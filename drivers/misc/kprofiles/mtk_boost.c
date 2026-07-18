// SPDX-License-Identifier: GPL-2.0
/*
 * Kprofile MTK boost.
 */

#define pr_fmt(fmt) "kprofiles_mtk: " fmt

#include <linux/cpufreq.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/power_supply.h>
#include <linux/sched/sysctl.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>

#include <mt-plat/cpu_ctrl.h>
#include <mt-plat/eas_ctrl.h>
#include <mt-plat/mtk_thermal_monitor.h>

/* Kprofiles core API (drivers/misc/kprofiles/main.c) */
extern int kp_active_mode(void);
extern void kp_set_mode(unsigned int level);
extern int kp_notifier_register_client(struct notifier_block *nb);
extern int kp_notifier_unregister_client(struct notifier_block *nb);
extern unsigned int KP_MODE_CHANGE;

/* CPU cluster topology */
extern int arch_get_nr_clusters(void);

/* GED GPU floor/boost/game hint (drivers/misc/mediatek/gpu/hal/mtk_gpu_utility.c,
 * drivers/misc/mediatek/gpu/ged/src/ged_kpi.c) - built in (=y) on this
 * tree, plain extern is sufficient without EXPORT_SYMBOL on the latter.
 */
extern bool mtk_set_bottom_gpu_freq(unsigned int ui32FreqLevel);
extern bool mtk_custom_boost_gpu_freq(unsigned int ui32FreqLevel);
extern void ged_kpi_set_game_hint(int mode);

/* EAS scheduler awareness (drivers/misc/mediatek/performance/boost_ctrl/
 * eas_ctrl/eas_ctrl.c) - real kicker-style API, same pattern as
 * update_userlimit_cpu_freq(). value is a 0-100 percentage, not raw
 * SCHED_CAPACITY_SCALE.
 */
extern int update_eas_uclamp_min(int kicker, int cgroup_idx, int value);
extern int update_prefer_idle_value(int kicker, int cgroup_idx, int value);

/*
 * update_userlimit_cpu_freq()'s .min/.max are raw kHz values passed
 * almost directly to cpufreq_update_policy(), which clamps them to
 * policy->cpuinfo.{min,max}_freq - so an oversized .min is a safe,
 * standard way to mean "floor at this cluster's real max frequency"
 * for the brief, self-expiring touch/launch kicks. The persistent
 * gaming floor below additionally goes through the thermal gate.
 */
#define KP_CPU_FREQ_MAX ((int)INT_MAX)
#define KP_CPU_FREQ_NONE (-1)

/*
 * AP sensor headroom window, millidegree C: 100% floor strength up to
 * KP_THERMAL_TAPER_START_MC, linearly taper to 0% by KP_THERMAL_HOT_MC.
 * Being a continuous function of temperature rather than a fixed
 * on/off pair, recovery from a fully-backed-off state ramps back up
 * smoothly on its own rather than snapping or flapping at an edge.
 */
#define KP_THERMAL_TAPER_START_MC 90000
#define KP_THERMAL_HOT_MC         110000
#define KP_THERMAL_RANGE_MC (KP_THERMAL_HOT_MC - KP_THERMAL_TAPER_START_MC)
#define KP_THERMAL_POLL_MS        1500

/* upper bound for mtk_set_bottom_gpu_freq()'s "strength" parameter -
 * self-clamped internally to the real DVFS table size, so this only
 * needs to be >= the largest table this SoC family could plausibly
 * have, not the exact count.
 */
#define KP_GPU_STRENGTH_MAX 32

static int kp_last_temp_mc;

/* returns 0-100: how much thermal headroom is left before KP_THERMAL_HOT_MC */
static unsigned int kp_thermal_headroom_pct(void)
{
	int temp = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP);

	kp_last_temp_mc = temp;

	if (temp <= KP_THERMAL_TAPER_START_MC)
		return 100;
	if (temp >= KP_THERMAL_HOT_MC)
		return 0;

	return 100 * (KP_THERMAL_HOT_MC - temp) / KP_THERMAL_RANGE_MC;
}

/* touch / app-launch transient boost durations by kp_active_mode() (0-3) */
static const unsigned int kp_touch_boost_ms[4]  = {   0, 150, 250, 350 };
static const unsigned int kp_launch_boost_ms[4] = {   0,   0, 100, 200 };

static int kp_cluster_num;
static unsigned int kp_cluster_max_freq[8];	/* kHz, indexed by cluster id */
static struct delayed_work kp_cpu_boost_release_work;
static struct delayed_work kp_gpu_boost_release_work;

static void kp_cache_cluster_max_freq(void)
{
	struct cpumask mask;
	int i;

	if (kp_cluster_num > ARRAY_SIZE(kp_cluster_max_freq))
		kp_cluster_num = ARRAY_SIZE(kp_cluster_max_freq);

	for (i = 0; i < kp_cluster_num; i++) {
		arch_get_cluster_cpus(&mask, i);
		kp_cluster_max_freq[i] = cpufreq_quick_get_max(cpumask_first(&mask));
	}
}

static void kp_cpu_userlimit_clear(int kicker)
{
	struct ppm_limit_data *pld;
	int i;

	if (kp_cluster_num <= 0)
		return;

	pld = kcalloc(kp_cluster_num, sizeof(*pld), GFP_KERNEL);
	if (!pld)
		return;

	for (i = 0; i < kp_cluster_num; i++) {
		pld[i].min = KP_CPU_FREQ_NONE;
		pld[i].max = KP_CPU_FREQ_NONE;
	}

	update_userlimit_cpu_freq(kicker, kp_cluster_num, pld);
	kfree(pld);
}

static void kp_cpu_userlimit_max(int kicker)
{
	struct ppm_limit_data *pld;
	int i;

	if (kp_cluster_num <= 0)
		return;

	pld = kcalloc(kp_cluster_num, sizeof(*pld), GFP_KERNEL);
	if (!pld)
		return;

	for (i = 0; i < kp_cluster_num; i++) {
		pld[i].min = KP_CPU_FREQ_MAX;
		pld[i].max = KP_CPU_FREQ_NONE;
	}

	update_userlimit_cpu_freq(kicker, kp_cluster_num, pld);
	kfree(pld);
}

static void kp_cpu_boost_release_fn(struct work_struct *work)
{
	kp_cpu_userlimit_clear(CPU_KIR_KPROFILES_BOOST);
}

static void kp_cpu_boost_kick(unsigned int duration_ms)
{
	if (!duration_ms || kp_cluster_num <= 0 || !kp_thermal_headroom_pct())
		return;

	kp_cpu_userlimit_max(CPU_KIR_KPROFILES_BOOST);

	mod_delayed_work(system_freezable_wq, &kp_cpu_boost_release_work,
			  msecs_to_jiffies(duration_ms));
}

static void kp_gpu_boost_release_fn(struct work_struct *work)
{
	/* clamped internally to the real table's lowest-priority index */
	mtk_custom_boost_gpu_freq(UINT_MAX);
}

static void kp_gpu_boost_kick(unsigned int duration_ms)
{
	if (!duration_ms || !kp_thermal_headroom_pct())
		return;

	mtk_custom_boost_gpu_freq(0);	/* 0 == highest frequency */

	mod_delayed_work(system_freezable_wq, &kp_gpu_boost_release_work,
			  msecs_to_jiffies(duration_ms));
}

/* app-launch boost, called from kernel/fork.c on zygote fork */
void kp_boost_on_fork(void)
{
	unsigned int ms = kp_launch_boost_ms[kp_active_mode() & 3];

	if (ms)
		kp_cpu_boost_kick(ms);
}

/* ---- touch boost: input_handler on BTN_TOUCH, driver-independent ---- */

/*
 * input core calls ->event() with dev->event_lock held via
 * spin_lock_irqsave() (drivers/input/input.c) - true atomic context.
 * kp_cpu_boost_kick()/kp_gpu_boost_kick() do GFP_KERNEL allocations and
 * update_userlimit_cpu_freq() takes a mutex, so none of that can run
 * directly in the event callback (real-world crash: "scheduling while
 * atomic" in the touch IRQ thread once a real finger triggered this
 * path). Defer the actual work to a workqueue instead.
 */
static unsigned int kp_touch_boost_pending_ms;
static struct work_struct kp_touch_boost_work;

static void kp_touch_boost_work_fn(struct work_struct *work)
{
	unsigned int ms = kp_touch_boost_pending_ms;

	kp_cpu_boost_kick(ms);
	kp_gpu_boost_kick(ms);
}

static void kp_touch_input_event(struct input_handle *handle,
				  unsigned int type, unsigned int code,
				  int value)
{
	if (type != EV_KEY || code != BTN_TOUCH || !value)
		return;

	kp_touch_boost_pending_ms = kp_touch_boost_ms[kp_active_mode() & 3];
	schedule_work(&kp_touch_boost_work);
}

static int kp_touch_input_connect(struct input_handler *handler,
				   struct input_dev *dev,
				   const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "kprofiles";

	error = input_register_handle(handle);
	if (error)
		goto free_handle;

	error = input_open_device(handle);
	if (error)
		goto unregister_handle;

	return 0;

unregister_handle:
	input_unregister_handle(handle);
free_handle:
	kfree(handle);
	return error;
}

static void kp_touch_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id kp_touch_input_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler kp_touch_input_handler = {
	.event = kp_touch_input_event,
	.connect = kp_touch_input_connect,
	.disconnect = kp_touch_input_disconnect,
	.name = "kprofiles_touch",
	.id_table = kp_touch_input_ids,
};

/* ---- battery-aware auto mode: force Battery below a threshold ---- */

#define KP_BATTERY_LOW_PCT     15
#define KP_BATTERY_RECOVER_PCT 20
#define KP_BATTERY_POLL_MS     30000

static bool kp_battery_low;
static unsigned int kp_battery_saved_mode;
static struct delayed_work kp_battery_poll_work;

static int kp_battery_capacity(void)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (!psy)
		return -1;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(psy);

	return ret ? -1 : val.intval;
}

static void kp_battery_poll_fn(struct work_struct *work)
{
	int cap = kp_battery_capacity();

	if (cap >= 0) {
		/* mode 0 (disabled) must never be touched, and Performance
		 * is deliberately exempt from the low-battery downgrade too
		 * - the point of Performance is to ignore battery cost.
		 */
		if (!kp_battery_low && cap <= KP_BATTERY_LOW_PCT &&
		    kp_active_mode() != 0 && kp_active_mode() != 3) {
			/* snapshot whatever was active before forcing Battery,
			 * same "restore what it was, don't track live changes
			 * during the override" contract as the mode this
			 * itself forces (kp_set_mode(1)) uses for screen-off.
			 */
			kp_battery_saved_mode = kp_active_mode();
			kp_battery_low = true;
			kp_set_mode(1);
		} else if (kp_battery_low && cap >= KP_BATTERY_RECOVER_PCT) {
			kp_battery_low = false;
			kp_set_mode(kp_battery_saved_mode);
		}
	}

	mod_delayed_work(system_freezable_wq, &kp_battery_poll_work,
			  msecs_to_jiffies(KP_BATTERY_POLL_MS));
}

/* ---- gaming boost: thermal-proportional CPU/GPU floor + sched tuning ---- */

static bool kp_gaming_wanted;		/* kp_active_mode() == 3 */
static unsigned int kp_gaming_pct;	/* floor strength currently applied, 0-100 */
static struct delayed_work kp_gaming_thermal_work;

static void kp_cpu_userlimit_level(int kicker, unsigned int pct)
{
	struct ppm_limit_data *pld;
	int i;

	if (kp_cluster_num <= 0)
		return;

	pld = kcalloc(kp_cluster_num, sizeof(*pld), GFP_KERNEL);
	if (!pld)
		return;

	for (i = 0; i < kp_cluster_num; i++) {
		pld[i].min = pct ? (int)(kp_cluster_max_freq[i] * pct / 100)
				  : KP_CPU_FREQ_NONE;
		pld[i].max = KP_CPU_FREQ_NONE;
	}

	update_userlimit_cpu_freq(kicker, kp_cluster_num, pld);
	kfree(pld);
}

static unsigned int kp_sched_latency_orig;
static unsigned int kp_sched_min_gran_orig;
static unsigned int kp_sched_wakeup_gran_orig;
static unsigned int kp_sched_migration_cost_orig;
static bool kp_sched_tuned;

/* top-app uclamp_min floor while gaming boost is active, 0-100 scale */
#define KP_PERF_UCLAMP_MIN 60

static void kp_sched_tune_set(bool tighten)
{
	if (tighten == kp_sched_tuned)
		return;

	if (tighten) {
		kp_sched_latency_orig = sysctl_sched_latency;
		kp_sched_min_gran_orig = sysctl_sched_min_granularity;
		kp_sched_wakeup_gran_orig = sysctl_sched_wakeup_granularity;
		kp_sched_migration_cost_orig = sysctl_sched_migration_cost;

		/* tighter preemption/migration response for foreground game
		 * thread latency; halved, not zeroed, to avoid excess
		 * context-switch/migration overhead offsetting the gain.
		 */
		sysctl_sched_latency = kp_sched_latency_orig / 2;
		sysctl_sched_min_granularity = kp_sched_min_gran_orig / 2;
		sysctl_sched_wakeup_granularity = kp_sched_wakeup_gran_orig / 2;
		sysctl_sched_migration_cost = kp_sched_migration_cost_orig / 2;
	} else {
		sysctl_sched_latency = kp_sched_latency_orig;
		sysctl_sched_min_granularity = kp_sched_min_gran_orig;
		sysctl_sched_wakeup_granularity = kp_sched_wakeup_gran_orig;
		sysctl_sched_migration_cost = kp_sched_migration_cost_orig;
	}

	/* EAS task-placement awareness for top-app: guaranteed minimum
	 * capacity so the scheduler doesn't undersize it, and prefer
	 * placing it on an idle CPU over a busy-but-sufficient one.
	 */
	update_eas_uclamp_min(EAS_UCLAMP_KIR_KPROFILES, CGROUP_TA,
			       tighten ? KP_PERF_UCLAMP_MIN : 0);
	update_prefer_idle_value(EAS_PREFER_IDLE_KIR_KPROFILES, CGROUP_TA,
				  tighten ? 1 : 0);

	kp_sched_tuned = tighten;
}

static void kp_gaming_apply_pct(unsigned int pct)
{
	if (pct == kp_gaming_pct)
		return;

	kp_cpu_userlimit_level(CPU_KIR_KPROFILES_GAME, pct);
	mtk_set_bottom_gpu_freq(KP_GPU_STRENGTH_MAX * pct / 100);
	ged_kpi_set_game_hint(pct ? 1 : 0);
	kp_sched_tune_set(pct != 0);

	kp_gaming_pct = pct;
}

static void kp_gaming_thermal_fn(struct work_struct *work)
{
	if (!kp_gaming_wanted)
		return;

	kp_gaming_apply_pct(kp_thermal_headroom_pct());

	mod_delayed_work(system_freezable_wq, &kp_gaming_thermal_work,
			  msecs_to_jiffies(KP_THERMAL_POLL_MS));
}

static void kp_gaming_set_wanted(bool wanted)
{
	if (wanted == kp_gaming_wanted)
		return;

	kp_gaming_wanted = wanted;

	if (wanted) {
		kp_gaming_apply_pct(kp_thermal_headroom_pct());
		mod_delayed_work(system_freezable_wq, &kp_gaming_thermal_work,
				  msecs_to_jiffies(KP_THERMAL_POLL_MS));
	} else {
		cancel_delayed_work_sync(&kp_gaming_thermal_work);
		kp_gaming_apply_pct(0);
	}
}

/*
 * Battery mode: cap CPU frequency instead of just skipping boosts.
 * Reuses the same CPU_KIR_KPROFILES_GAME kicker as the Performance
 * floor above (they're mutually exclusive - only one mode is active
 * at a time) but sets .max instead of .min. No thermal gating needed
 * here: a ceiling only ever reduces demand, it can't create the
 * kicker-arbitration override risk the floor has, so it's always
 * safe to apply immediately and unconditionally.
 */
#define KP_BATTERY_CPU_CEILING_PCT 50

static bool kp_battery_ceiling_active;

static void kp_battery_ceiling_set(bool on)
{
	struct ppm_limit_data *pld;
	int i;

	if (on == kp_battery_ceiling_active || kp_cluster_num <= 0)
		return;

	pld = kcalloc(kp_cluster_num, sizeof(*pld), GFP_KERNEL);
	if (!pld)
		return;

	for (i = 0; i < kp_cluster_num; i++) {
		pld[i].min = KP_CPU_FREQ_NONE;
		pld[i].max = on ? (int)(kp_cluster_max_freq[i] *
					 KP_BATTERY_CPU_CEILING_PCT / 100)
				 : KP_CPU_FREQ_NONE;
	}

	update_userlimit_cpu_freq(CPU_KIR_KPROFILES_GAME, kp_cluster_num, pld);
	kfree(pld);

	kp_battery_ceiling_active = on;
}

static int kp_boost_notifier_cb(struct notifier_block *self,
				 unsigned long event, void *data)
{
	unsigned int mode = (unsigned int)(uintptr_t)data;

	if (event != KP_MODE_CHANGE)
		return NOTIFY_DONE;

	kp_gaming_set_wanted(mode == 3);
	kp_battery_ceiling_set(mode == 1);

	return NOTIFY_OK;
}

static struct notifier_block kp_boost_notifier = {
	.notifier_call = kp_boost_notifier_cb,
};

/* ---- status: /sys/kernel/kprofiles_mtk/status, read-only ---- */

static struct kobject *kp_mtk_kobj;

static ssize_t kp_status_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
			  "gaming_pct=%u battery_low=%u battery_ceiling=%u temp_mc=%d\n",
			  kp_gaming_pct, kp_battery_low, kp_battery_ceiling_active,
			  kp_last_temp_mc);
}

static struct kobj_attribute kp_status_attribute =
	__ATTR(status, 0444, kp_status_show, NULL);

static struct attribute *kp_mtk_attrs[] = {
	&kp_status_attribute.attr,
	NULL,
};

static struct attribute_group kp_mtk_attr_group = {
	.attrs = kp_mtk_attrs,
};

void kp_mtk_boost_init(void)
{
	int ret;

	kp_cluster_num = arch_get_nr_clusters();
	kp_cache_cluster_max_freq();
	INIT_DELAYED_WORK(&kp_cpu_boost_release_work, kp_cpu_boost_release_fn);
	INIT_DELAYED_WORK(&kp_gpu_boost_release_work, kp_gpu_boost_release_fn);
	INIT_DELAYED_WORK(&kp_gaming_thermal_work, kp_gaming_thermal_fn);
	INIT_DELAYED_WORK(&kp_battery_poll_work, kp_battery_poll_fn);
	INIT_WORK(&kp_touch_boost_work, kp_touch_boost_work_fn);

	kp_mtk_kobj = kobject_create_and_add("kprofiles_mtk", kernel_kobj);
	if (kp_mtk_kobj)
		sysfs_create_group(kp_mtk_kobj, &kp_mtk_attr_group);

	ret = kp_notifier_register_client(&kp_boost_notifier);
	if (ret)
		pr_err("Failed to register mode-change notifier, err: %d\n", ret);

	/* sync gaming boost / battery ceiling to whatever mode is already
	 * active at boot
	 */
	kp_gaming_set_wanted(kp_active_mode() == 3);
	kp_battery_ceiling_set(kp_active_mode() == 1);

	mod_delayed_work(system_freezable_wq, &kp_battery_poll_work, 0);

	ret = input_register_handler(&kp_touch_input_handler);
	if (ret)
		pr_err("Failed to register touch input handler, err: %d\n", ret);
}

void kp_mtk_boost_exit(void)
{
	input_unregister_handler(&kp_touch_input_handler);
	kp_notifier_unregister_client(&kp_boost_notifier);
	cancel_work_sync(&kp_touch_boost_work);
	cancel_delayed_work_sync(&kp_battery_poll_work);

	if (kp_mtk_kobj) {
		sysfs_remove_group(kp_mtk_kobj, &kp_mtk_attr_group);
		kobject_put(kp_mtk_kobj);
	}

	cancel_delayed_work_sync(&kp_cpu_boost_release_work);
	kp_cpu_userlimit_clear(CPU_KIR_KPROFILES_BOOST);
	cancel_delayed_work_sync(&kp_gpu_boost_release_work);
	mtk_custom_boost_gpu_freq(UINT_MAX);

	kp_gaming_set_wanted(false);
	kp_battery_ceiling_set(false);
}
