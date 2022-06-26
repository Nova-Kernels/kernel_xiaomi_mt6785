




OUTPUT_ARCH(aarch64)
ENTRY(_text)
jiffies = jiffies_64;
PECOFF_FILE_ALIGNMENT = 0x200;
SECTIONS
{
 /DISCARD/ : {
  KEEP(*(.exitcall.exit))
  *(.discard)
  *(.discard.*)
  *(.interp .dynamic)
  *(.dynsym .dynstr .hash .gnu.hash)
  *(.eh_frame)
 }
 . = ((((0xffffffffffffffff - (1 << (39)) + 1) + (0)) + (0x08000000))) + 0x00080000;
 .head.text : {
  _text = .;
  KEEP(*(.head.text))
 }
 .text : {
  _stext = .;
   __exception_text_start = .;
   *(.exception.text)
   __exception_text_end = .;
   . = ALIGN(8); __irqentry_text_start = .; *(.irqentry.text) __irqentry_text_end = .;
   . = ALIGN(8); __softirqentry_text_start = .; *(.softirqentry.text) __softirqentry_text_end = .;
   . = ALIGN(8); __entry_text_start = .; *(.entry.text) __entry_text_end = .;
   . = ALIGN(8); *(.text.hot .text.hot.*) *(.text .text.fixup) *(.text.unlikely .text.unlikely.*) *(.text.unknown .text.unknown.*) *(.text..refcount) *(.text..ftrace) *(.text.cfi) *(.ref.text) *(.text.asan.* .text.tsan.*)
   . = ALIGN(8); __sched_text_start = .; *(.sched.text) __sched_text_end = .;
   . = ALIGN(8); __cpuidle_text_start = .; *(.cpuidle.text) __cpuidle_text_end = .;
   . = ALIGN(8); __lock_text_start = .; *(.spinlock.text) __lock_text_end = .;
   . = ALIGN(8); __kprobes_text_start = .; *(.kprobes.text) __kprobes_text_end = .;
   . = ALIGN(0x00001000); __hyp_idmap_text_start = .; *(.hyp.idmap.text) __hyp_idmap_text_end = .; __hyp_text_start = .; *(.hyp.text) . = ALIGN(0x00000008); __start___kvm_ex_table = .; *(__kvm_ex_table) __stop___kvm_ex_table = .; __hyp_text_end = .;
   . = ALIGN(0x00001000); __idmap_text_start = .; *(.idmap.text) __idmap_text_end = .;
   . = ALIGN((1 << 12)); __entry_tramp_text_start = .; KEEP(*(.entry.tramp.text)) . = ALIGN((1 << 12)); __entry_tramp_text_end = .;
   *(.fixup)
   *(.gnu.warning)
  . = ALIGN(16);
  *(.got)
 }
 . = ALIGN(0x00010000);
 _etext = .;
 . = ALIGN(((1 << 12))); .rodata : AT(ADDR(.rodata) - 0) { __start_rodata = .; *(.rodata) *(.rodata.*) __start_ro_after_init = .; *(.data..ro_after_init) __end_ro_after_init = .; KEEP(*(__vermagic)) . = ALIGN(8); __start___tracepoints_ptrs = .; KEEP(*(__tracepoints_ptrs)) __stop___tracepoints_ptrs = .; *(__tracepoints_strings) } .rodata1 : AT(ADDR(.rodata1) - 0) { *(.rodata1) } .pci_fixup : AT(ADDR(.pci_fixup) - 0) { __start_pci_fixups_early = .; KEEP(*(.pci_fixup_early)) __end_pci_fixups_early = .; __start_pci_fixups_header = .; KEEP(*(.pci_fixup_header)) __end_pci_fixups_header = .; __start_pci_fixups_final = .; KEEP(*(.pci_fixup_final)) __end_pci_fixups_final = .; __start_pci_fixups_enable = .; KEEP(*(.pci_fixup_enable)) __end_pci_fixups_enable = .; __start_pci_fixups_resume = .; KEEP(*(.pci_fixup_resume)) __end_pci_fixups_resume = .; __start_pci_fixups_resume_early = .; KEEP(*(.pci_fixup_resume_early)) __end_pci_fixups_resume_early = .; __start_pci_fixups_suspend = .; KEEP(*(.pci_fixup_suspend)) __end_pci_fixups_suspend = .; __start_pci_fixups_suspend_late = .; KEEP(*(.pci_fixup_suspend_late)) __end_pci_fixups_suspend_late = .; } .builtin_fw : AT(ADDR(.builtin_fw) - 0) { __start_builtin_fw = .; KEEP(*(.builtin_fw)) __end_builtin_fw = .; } __ksymtab : AT(ADDR(__ksymtab) - 0) { __start___ksymtab = .; KEEP(*(SORT(___ksymtab+*))) __stop___ksymtab = .; } __ksymtab_gpl : AT(ADDR(__ksymtab_gpl) - 0) { __start___ksymtab_gpl = .; KEEP(*(SORT(___ksymtab_gpl+*))) __stop___ksymtab_gpl = .; } __ksymtab_unused : AT(ADDR(__ksymtab_unused) - 0) { __start___ksymtab_unused = .; KEEP(*(SORT(___ksymtab_unused+*))) __stop___ksymtab_unused = .; } __ksymtab_unused_gpl : AT(ADDR(__ksymtab_unused_gpl) - 0) { __start___ksymtab_unused_gpl = .; KEEP(*(SORT(___ksymtab_unused_gpl+*))) __stop___ksymtab_unused_gpl = .; } __ksymtab_gpl_future : AT(ADDR(__ksymtab_gpl_future) - 0) { __start___ksymtab_gpl_future = .; KEEP(*(SORT(___ksymtab_gpl_future+*))) __stop___ksymtab_gpl_future = .; } __kcrctab : AT(ADDR(__kcrctab) - 0) { __start___kcrctab = .; KEEP(*(SORT(___kcrctab+*))) __stop___kcrctab = .; } __kcrctab_gpl : AT(ADDR(__kcrctab_gpl) - 0) { __start___kcrctab_gpl = .; KEEP(*(SORT(___kcrctab_gpl+*))) __stop___kcrctab_gpl = .; } __kcrctab_unused : AT(ADDR(__kcrctab_unused) - 0) { __start___kcrctab_unused = .; KEEP(*(SORT(___kcrctab_unused+*))) __stop___kcrctab_unused = .; } __kcrctab_unused_gpl : AT(ADDR(__kcrctab_unused_gpl) - 0) { __start___kcrctab_unused_gpl = .; KEEP(*(SORT(___kcrctab_unused_gpl+*))) __stop___kcrctab_unused_gpl = .; } __kcrctab_gpl_future : AT(ADDR(__kcrctab_gpl_future) - 0) { __start___kcrctab_gpl_future = .; KEEP(*(SORT(___kcrctab_gpl_future+*))) __stop___kcrctab_gpl_future = .; } __ksymtab_strings : AT(ADDR(__ksymtab_strings) - 0) { *(__ksymtab_strings) } __init_rodata : AT(ADDR(__init_rodata) - 0) { *(.ref.rodata) } __param : AT(ADDR(__param) - 0) { __start___param = .; KEEP(*(__param)) __stop___param = .; } __modver : AT(ADDR(__modver) - 0) { __start___modver = .; KEEP(*(__modver)) __stop___modver = .; . = ALIGN(((1 << 12))); __end_rodata = .; } . = ALIGN(((1 << 12)));
 . = ALIGN(8); __ex_table : AT(ADDR(__ex_table) - 0) { __start___ex_table = .; KEEP(*(__ex_table)) __stop___ex_table = .; }
 .notes : AT(ADDR(.notes) - 0) { __start_notes = .; *(.note.*) __stop_notes = .; }
 . = ALIGN(0x00010000);
 __init_begin = .;
 __inittext_begin = .;
 . = ALIGN(8); .init.text : AT(ADDR(.init.text) - 0) { _sinittext = .; *(.init.text .init.text.*) *(.text.startup) *(.meminit.text) _einittext = .; }
 .exit.text : {
  *(.exit.text) *(.text.exit) *(.memexit.text)
 }
 . = ALIGN(4);
 .altinstructions : {
  __alt_instructions = .;
  KEEP(*(.altinstructions))
  __alt_instructions_end = .;
 }
 .altinstr_replacement : {
  *(.altinstr_replacement)
 }
 . = ALIGN((1 << 12));
 __inittext_end = .;
 __initdata_begin = .;
 .init.data : {
  KEEP(*(SORT(___kentry+*))) *(.init.data) *(.meminit.data) *(.init.rodata .init.rodata.*) . = ALIGN(8); __start_ftrace_events = .; KEEP(*(_ftrace_events)) __stop_ftrace_events = .; __start_ftrace_eval_maps = .; KEEP(*(_ftrace_eval_map)) __stop_ftrace_eval_maps = .; *(.meminit.rodata) . = ALIGN(8); __clk_of_table = .; KEEP(*(__clk_of_table)) KEEP(*(__clk_of_table_end)) . = ALIGN(8); __reservedmem_of_table = .; KEEP(*(__reservedmem_of_table)) KEEP(*(__reservedmem_of_table_end)) . = ALIGN(8); __timer_of_table = .; KEEP(*(__timer_of_table)) KEEP(*(__timer_of_table_end)) . = ALIGN(8); __iommu_of_table = .; KEEP(*(__iommu_of_table)) KEEP(*(__iommu_of_table_end)) . = ALIGN(8); __cpu_method_of_table = .; KEEP(*(__cpu_method_of_table)) KEEP(*(__cpu_method_of_table_end)) . = ALIGN(8); __cpuidle_method_of_table = .; KEEP(*(__cpuidle_method_of_table)) KEEP(*(__cpuidle_method_of_table_end)) . = ALIGN(32); __dtb_start = .; KEEP(*(.dtb.init.rodata)) __dtb_end = .; . = ALIGN(8); __irqchip_of_table = .; KEEP(*(__irqchip_of_table)) KEEP(*(__irqchip_of_table_end)) . = ALIGN(8); __earlycon_table = .; KEEP(*(__earlycon_table)) __earlycon_table_end = .;
  . = ALIGN(16); __setup_start = .; KEEP(*(.init.setup)) __setup_end = .;
  __initcall_start = .; KEEP(*(.initcallearly.init)) __initcall0_start = .; KEEP(*(.initcall0.init)) KEEP(*(.initcall0s.init)) __initcall1_start = .; KEEP(*(.initcall1.init)) KEEP(*(.initcall1s.init)) __initcall2_start = .; KEEP(*(.initcall2.init)) KEEP(*(.initcall2s.init)) __initcall3_start = .; KEEP(*(.initcall3.init)) KEEP(*(.initcall3s.init)) __initcall4_start = .; KEEP(*(.initcall4.init)) KEEP(*(.initcall4s.init)) __initcall5_start = .; KEEP(*(.initcall5.init)) KEEP(*(.initcall5s.init)) __initcallrootfs_start = .; KEEP(*(.initcallrootfs.init)) KEEP(*(.initcallrootfss.init)) __initcall6_start = .; KEEP(*(.initcall6.init)) KEEP(*(.initcall6s.init)) __initcall7_start = .; KEEP(*(.initcall7.init)) KEEP(*(.initcall7s.init)) __initcall_end = .;
  __con_initcall_start = .; KEEP(*(.con_initcall.init)) __con_initcall_end = .;
  __security_initcall_start = .; KEEP(*(.security_initcall.init)) __security_initcall_end = .;
  . = ALIGN(4); __initramfs_start = .; KEEP(*(.init.ramfs)) . = ALIGN(8); KEEP(*(.init.ramfs.info))
  *(.init.rodata.* .init.bss)
 }
 .exit.data : {
  *(.exit.data) *(.fini_array) *(.dtors) *(.memexit.data) *(.memexit.rodata)
 }
 . = ALIGN((1 << 12)); .data..percpu : AT(ADDR(.data..percpu) - 0) { __per_cpu_load = .; __per_cpu_start = .; *(.data..percpu..first) . = ALIGN((1 << 12)); *(.data..percpu..page_aligned) . = ALIGN((1 << 6)); *(.data..percpu..read_mostly) . = ALIGN((1 << 6)); *(.data..percpu) *(.data..percpu..shared_aligned) __per_cpu_end = .; }
 .rela.dyn : ALIGN(8) {
  *(.rela .rela*)
 }
 __rela_offset = ABSOLUTE(ADDR(.rela.dyn) - ((((0xffffffffffffffff - (1 << (39)) + 1) + (0)) + (0x08000000))));
 __rela_size = SIZEOF(.rela.dyn);
 . = ALIGN(0x00010000);
 __initdata_end = .;
 __init_end = .;
 _data = .;
 _sdata = .;
 . = ALIGN((1 << 12)); .data : AT(ADDR(.data) - 0) { . = ALIGN((2 * (1 << (14 + 0)))); __start_init_task = .; *(.data..init_task) __end_init_task = .; . = ALIGN((1 << 12)); __nosave_begin = .; *(.data..nosave) . = ALIGN((1 << 12)); __nosave_end = .; . = ALIGN((1 << 12)); *(.data..page_aligned) . = ALIGN((1 << 12)); . = ALIGN((1 << 6)); *(.data..cacheline_aligned) . = ALIGN((1 << 6)); *(.data..read_mostly) . = ALIGN((1 << 6)); *(.xiptext) *(.data) *(.ref.data) *(.data..shared_aligned) *(.data.unlikely) . = ALIGN(32); *(__tracepoints) . = ALIGN(8); __start___jump_table = .; KEEP(*(__jump_table)) __stop___jump_table = .; . = ALIGN(8); __start___verbose = .; KEEP(*(__verbose)) __stop___verbose = .; __start___trace_bprintk_fmt = .; KEEP(*(__trace_printk_fmt)) __stop___trace_bprintk_fmt = .; __start___tracepoint_str = .; KEEP(*(__tracepoint_str)) __stop___tracepoint_str = .; CONSTRUCTORS } . = ALIGN(8); __bug_table : AT(ADDR(__bug_table) - 0) { __start___bug_table = .; KEEP(*(__bug_table)) __stop___bug_table = .; }
 .mmuoff.data.write : ALIGN(0x00000800) {
  __mmuoff_data_start = .;
  *(.mmuoff.data.write)
 }
 . = ALIGN(0x00000800);
 .mmuoff.data.read : {
  *(.mmuoff.data.read)
  __mmuoff_data_end = .;
 }
 __pecoff_data_rawsize = ABSOLUTE(. - __initdata_begin);
 _edata = .;
 . = ALIGN(0); __bss_start = .; . = ALIGN(0); .sbss : AT(ADDR(.sbss) - 0) { *(.dynsbss) *(.sbss) *(.scommon) } . = ALIGN(0); .bss : AT(ADDR(.bss) - 0) { . = ALIGN((1 << 12)); *(.bss..page_aligned) . = ALIGN((1 << 12)); *(.dynbss) *(.bss) *(COMMON) } . = ALIGN(0); __bss_stop = .;
 . = ALIGN((1 << 12));
 idmap_pg_dir = .;
 . += ((((((48)) - 4) / (12 - 3)) - 1) * (1 << 12));
 swapper_pg_dir = .;
 . += ((3 - 1) * (1 << 12));
 reserved_ttbr0 = .;
 . += ((1 << 12));
 tramp_pg_dir = .;
 . += (1 << 12);
 __pecoff_data_size = ABSOLUTE(. - __initdata_begin);
 _end = .;
 .stab 0 : { *(.stab) } .stabstr 0 : { *(.stabstr) } .stab.excl 0 : { *(.stab.excl) } .stab.exclstr 0 : { *(.stab.exclstr) } .stab.index 0 : { *(.stab.index) } .stab.indexstr 0 : { *(.stab.indexstr) } .comment 0 : { *(.comment) }
 _kernel_size_le_lo32 = (((_end - _text) & 0xffffffff) & 0xffffffff); _kernel_size_le_hi32 = (((_end - _text) >> 32) & 0xffffffff); _kernel_offset_le_lo32 = (((0x00080000) & 0xffffffff) & 0xffffffff); _kernel_offset_le_hi32 = (((0x00080000) >> 32) & 0xffffffff); _kernel_flags_le_lo32 = (((((0 << 0) | (((12 - 10) / 2) << 1) | (1 << 3))) & 0xffffffff) & 0xffffffff); _kernel_flags_le_hi32 = (((((0 << 0) | (((12 - 10) / 2) << 1) | (1 << 3))) >> 32) & 0xffffffff);
}
ASSERT(__hyp_idmap_text_end - (__hyp_idmap_text_start & ~(0x00001000 - 1)) <= 0x00001000,
 "HYP init code too big or misaligned")
ASSERT(__idmap_text_end - (__idmap_text_start & ~(0x00001000 - 1)) <= 0x00001000,
 "ID map text too big or misaligned")
ASSERT((__entry_tramp_text_end - __entry_tramp_text_start) <= 3*(1 << 12),
 "Entry trampoline text too big")
ASSERT(_text == (((((0xffffffffffffffff - (1 << (39)) + 1) + (0)) + (0x08000000))) + 0x00080000), "HEAD is misaligned")
