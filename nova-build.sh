#!/bin/bash

set -euo pipefail

SECONDS=0

KERNEL_PATH="$PWD"
OUT_DIR="$KERNEL_PATH/out"
AK3_DIR="$KERNEL_PATH/Anykernel"
DEFCONFIG="${2:-begonia_user_defconfig}"
CLANG_DIR="$KERNEL_PATH/clang"
KSUN_DIR="$KERNEL_PATH/.ksun-src"

export LC_ALL=C
export USE_CCACHE=1
export ARCH=arm64
export KBUILD_BUILD_USER="Wahid7852"
export KBUILD_BUILD_HOST="NoVA"
export USE_HOST_LEX=yes

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
    make O="$OUT_DIR" ARCH=arm64 "$DEFCONFIG" savedefconfig
    cp "$OUT_DIR/defconfig" "arch/arm64/configs/$DEFCONFIG"
}

setup_ksun_worktree() {
    git fetch origin ksun

    if [[ ! -d "$KSUN_DIR" ]]; then
        echo "==> Setting up ksun worktree..."
        git worktree add -B ksun "$KSUN_DIR" origin/ksun
    else
        echo "==> Resetting ksun worktree to origin/ksun..."
        (
            cd "$KSUN_DIR"
            git checkout -- .
            git clean -fd
            git checkout ksun
            git reset --hard origin/ksun
        )
    fi
}

integrate_kernelsu() {
    local src_dir="$1"
    local defconfig="$2"

    echo "==> Integrating KernelSU-Next + SUSFS..."

    (
        cd "$src_dir"

        rm -rf ./KernelSU ./KernelSU-Next ./drivers/kernelsu

        curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
        rm -rf KernelSU

        git clone --recursive -j"$(nproc --all)" --branch legacy-susfs-v2 https://github.com/sidex15/KernelSU-Next KernelSU

        sed -i 's/# CONFIG_KSU is not set/CONFIG_KSU=y/' "arch/arm64/configs/$defconfig"
        sed -i 's/# CONFIG_KSU_MANUAL_HOOK is not set/CONFIG_KSU_MANUAL_HOOK=y/' "arch/arm64/configs/$defconfig"
        sed -i 's/# CONFIG_KSU_SUSFS is not set/CONFIG_KSU_SUSFS=y/' "arch/arm64/configs/$defconfig"
    )
}

_compile_and_package() {
    local src_dir="$1"
    local out_dir="$2"
    local defconfig="$3"
    local revision="$4"

    download_clang

    export PATH="$CLANG_DIR/bin:$PATH"

    mkdir -p "$out_dir"
    rm -f "$out_dir/error.log"

    make -C "$src_dir" O="$out_dir" ARCH=arm64 "$defconfig"

    (
        exec 2> >(tee -a "$out_dir/error.log" >&2)
        make -C "$src_dir" -j"$(nproc --all)" \
            O="$out_dir" \
            CC=clang \
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

    SUBREV="4.14.$(grep 'SUBLEVEL =' "$src_dir/Makefile" | awk '{print $3}')"
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
    _compile_and_package "$KERNEL_PATH" "$OUT_DIR" "$DEFCONFIG" "NoVA"
}

build_ksu() {
    setup_ksun_worktree
    integrate_kernelsu "$KSUN_DIR" "$DEFCONFIG"

    _compile_and_package "$KSUN_DIR" "$KSUN_DIR/out" "$DEFCONFIG" "NoVA-KSU"
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
    -r|--regen)
        regen_defconfig
        ;;
    *)
        echo
        echo "Usage: $0 [option] [defconfig]"
        echo
        echo "  -b, --build       Build normal kernel"
        echo "  -k, --build-ksu   Build KernelSU + SUSFS kernel (from ksun branch)"
        echo "  -a, --build-all   Build both normal and KernelSU kernels"
        echo "  -r, --regen       Regenerate defconfig"
        exit 1
        ;;
esac

echo "Time: $((SECONDS / 60)) min $((SECONDS % 60)) sec"
