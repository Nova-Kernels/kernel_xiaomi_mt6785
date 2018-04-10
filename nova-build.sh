#!/bin/bash

set -euo pipefail

SECONDS=0

KERNEL_PATH="$PWD"
OUT_DIR="$KERNEL_PATH/out"
AK3_DIR="$KERNEL_PATH/Anykernel"
DEFCONFIG="${2:-begonia_user_defconfig}"
CLANG_DIR="$KERNEL_PATH/clang"

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

build_kernel() {
    download_clang

    export PATH="$CLANG_DIR/bin:$PATH"

    mkdir -p "$OUT_DIR"
    rm -f "$OUT_DIR/error.log"

    make O="$OUT_DIR" ARCH=arm64 "$DEFCONFIG"

    exec 2> >(tee -a "$OUT_DIR/error.log" >&2)

    make -j"$(nproc --all)" \
        O="$OUT_DIR" \
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
        CROSS_COMPILE_ARM32=arm-linux-gnueabi-

    KERNEL_IMG="$OUT_DIR/arch/arm64/boot/Image.gz-dtb"

    if [[ ! -f "$KERNEL_IMG" ]]; then
        echo "Kernel image not found!"
        exit 1
    fi

    rm -f ./*.zip

    SUBREV="4.14.$(grep 'SUBLEVEL =' Makefile | awk '{print $3}')"
    REVISION="NoVA-Begonia"
    ZIPBASE="${REVISION}-${SUBREV}"
    ZIPNAME="${ZIPBASE}.zip"

    i=1
    while [[ -f "$ZIPNAME" ]]; do
        ZIPNAME="${ZIPBASE}_v${i}.zip"
        ((i++))
    done

    if [[ ! -d "$AK3_DIR" ]]; then
        git clone --depth=1 https://github.com/Wahid7852/Anykernel "$AK3_DIR"
    fi

    cp "$KERNEL_IMG" "$AK3_DIR/"

    rm -rf "$OUT_DIR/arch/arm64/boot"

    (
        cd "$AK3_DIR"
        git checkout master &>/dev/null || true
        zip -r9 "../$ZIPNAME" * -x .git README.md "*placeholder"
    )

    echo
    echo "Build complete: $ZIPNAME"
    echo "Time: $((SECONDS / 60)) min $((SECONDS % 60)) sec"
}

case "${1:-}" in
    -b|--build)
        build_kernel
        ;;
    -r|--regen)
        regen_defconfig
        ;;
    *)
        echo
        echo "Usage: $0 [option] [defconfig]"
        echo
        echo "  -b, --build    Build kernel"
        echo "  -r, --regen    Regenerate defconfig"
        exit 1
        ;;
esac
