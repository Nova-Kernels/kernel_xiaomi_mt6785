#!/bin/bash

set -euo pipefail

SECONDS=0

KERNEL_PATH="$PWD"
OUT_DIR="$KERNEL_PATH/out"
KSU_OUT_DIR="$KERNEL_PATH/out-ksu"
AK3_DIR="$KERNEL_PATH/Anykernel"
DEFCONFIG="begonia_user_defconfig"
KSU_DEFCONFIG="${DEFCONFIG%_defconfig}_ksu_defconfig"
CLANG_DIR="$KERNEL_PATH/clang"
CCACHE_DIR="$KERNEL_PATH/.ccache"
KSU_MANIFEST="$KERNEL_PATH/.ksu-manifest"
KSU_BEFORE_FILES=()

export LC_ALL=C
export ARCH=arm64
export KBUILD_BUILD_USER="Wahid7852"
export KBUILD_BUILD_HOST="NoVA"
export USE_HOST_LEX=yes

CC_CMD="clang"
if command -v ccache &>/dev/null; then
    export CCACHE_DIR
    ccache -M 20G &>/dev/null
    CC_CMD="ccache clang"
else
    echo "==> ccache not found, builds will not be cached (sudo pacman -S ccache to enable)"
fi

force_rm() {
    local tries=5
    while ! rm -rf "$@" 2>/dev/null; do
        ((--tries)) || { rm -rf "$@"; return; }
        sleep 0.5
    done
}

download_clang() {
    if [[ ! -d "$CLANG_DIR/bin" ]]; then
        echo "==> Downloading AOSP Clang..."

        wget -q \
        https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/tags/android-14.0.0_r50/clang-r510928.tar.gz \
        -O aosp-clang.tar.gz

        mkdir -p "$CLANG_DIR"
        tar -xf aosp-clang.tar.gz -C "$CLANG_DIR"
        rm -f aosp-clang.tar.gz
    fi
}

regen_defconfig() {
    local defconfig="${1:-$DEFCONFIG}"
    make O="$OUT_DIR" ARCH=arm64 "$defconfig" savedefconfig
    cp "$OUT_DIR/defconfig" "arch/arm64/configs/$defconfig"
}

# returns 1 if already integrated (no-op)
integrate_kernelsu() {
    if [[ -d ./KernelSU ]]; then
        return 1
    fi

    echo "==> Integrating KernelSU-Next + SUSFS (one-time)..."

    mapfile -t KSU_BEFORE_FILES < <(git diff --name-only)

    curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
    force_rm KernelSU

    git clone --recursive -j"$(nproc --all)" --branch legacy-susfs-v2 https://github.com/sidex15/KernelSU-Next KernelSU
}

remove_ksu() {
    echo "==> Removing KernelSU-Next integration..."
    force_rm ./KernelSU ./KernelSU-Next ./drivers/kernelsu

    if [[ -f "$KSU_MANIFEST" ]]; then
        local touched
        mapfile -t touched < "$KSU_MANIFEST"
        if [[ ${#touched[@]} -gt 0 ]]; then
            git checkout -- "${touched[@]}"
        fi
        rm -f "$KSU_MANIFEST"
    fi
}

_compile_and_package() {
    local out_dir="$1"
    local defconfig="$2"
    local revision="$3"

    download_clang

    export PATH="$CLANG_DIR/bin:$PATH"

    mkdir -p "$out_dir"
    rm -f "$out_dir/error.log"

    make O="$out_dir" ARCH=arm64 "$defconfig"

    (
        exec 2> >(tee -a "$out_dir/error.log" >&2)
        make -j"$(nproc --all)" \
            O="$out_dir" \
            CC="$CC_CMD" \
            LLVM=1 \
            LLVM_IAS=1 \
            AR=llvm-ar \
            NM=llvm-nm \
            OBJCOPY=llvm-objcopy \
            OBJDUMP=llvm-objdump \
            STRIP=llvm-strip \
            LD=ld.lld \
            CROSS_COMPILE=aarch64-linux-gnu- \
            CROSS_COMPILE_COMPAT=arm-linux-gnueabi-
    )

    KERNEL_IMG="$out_dir/arch/arm64/boot/Image.gz"

    if [[ ! -f "$KERNEL_IMG" ]]; then
        echo "Kernel image not found!"
        exit 1
    fi

    SUBREV="4.14.$(grep 'SUBLEVEL =' Makefile | awk '{print $3}')"
    ZIPBASE="${revision}-${SUBREV}"
    ZIPNAME="${ZIPBASE}.zip"

    i=1
    while [[ -f "$ZIPNAME" ]]; do
        ZIPNAME="${ZIPBASE}_v${i}.zip"
        ((i++))
    done

    if [[ ! -d "$AK3_DIR" ]]; then
        git clone --depth=1 https://github.com/Wahid7852/Anykernel "$AK3_DIR"
    fi

    rm -f "$AK3_DIR"/Image* "$AK3_DIR"/zImage*
    cp "$KERNEL_IMG" "$AK3_DIR/"

    rm -rf "$out_dir/arch/arm64/boot"

    (
        cd "$AK3_DIR"
        git checkout master &>/dev/null || true
        zip -r9 "../$ZIPNAME" * -x .git README.md "*placeholder"
    )

    echo
    echo "Build complete: $ZIPNAME"
}

build_kernel() {
    _compile_and_package "$OUT_DIR" "$DEFCONFIG" "NoVA"
}

build_ksu() {
    local fresh=false
    integrate_kernelsu && fresh=true

    _compile_and_package "$KSU_OUT_DIR" "$KSU_DEFCONFIG" "NoVA-KSU"

    # KernelSU-Next's Kbuild touches the normal defconfig too, revert that
    git checkout -- "arch/arm64/configs/$DEFCONFIG"

    if $fresh; then
        comm -13 <(printf '%s\n' "${KSU_BEFORE_FILES[@]}" | sort) <(git diff --name-only | sort) > "$KSU_MANIFEST"
    fi
}

case "${1:-}" in
    -b|--build)
        rm -f ./*.zip
        build_kernel
        ;;
    -k|--build-ksu)
        rm -f ./*.zip
        build_ksu
        ;;
    -a|--build-all)
        rm -f ./*.zip
        build_kernel
        build_ksu
        ;;
    -x|--remove-ksu)
        remove_ksu
        ;;
    -r|--regen)
        regen_defconfig "${2:-}"
        ;;
    *)
        echo
        echo "Usage: $0 [option] [defconfig]"
        echo
        echo "  -b, --build       Build normal kernel"
        echo "  -k, --build-ksu   Build KernelSU + SUSFS kernel"
        echo "  -a, --build-all   Build both normal and KernelSU kernels"
        echo "  -x, --remove-ksu  Remove the KernelSU-Next integration"
        echo "  -r, --regen       Regenerate defconfig"
        exit 1
        ;;
esac

echo "Time: $((SECONDS / 60)) min $((SECONDS % 60)) sec"
