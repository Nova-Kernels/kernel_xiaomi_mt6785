#!/bin/bash

set -euo pipefail

SECONDS=0
KERNEL_PATH=$PWD
AK3_DIR="$KERNEL_PATH/Anykernel"
DEFCONFIG="${2:-begonia_user_defconfig}"
BUILD_USER="Abdul7852"
BUILD_HOST="NoVA"
TOOLCHAIN_DIR="$KERNEL_PATH/toolchain"
OUT_DIR="$KERNEL_PATH/out"

export KBUILD_BUILD_USER="$BUILD_USER"
export KBUILD_BUILD_HOST="$BUILD_HOST"
export ARCH=arm64
export PATH="$TOOLCHAIN_DIR/bin:$PATH"
export USE_HOST_LEX=yes

install_tools() {
    mkdir -p "$TOOLCHAIN_DIR" && cd "$TOOLCHAIN_DIR"
    curl -LO "https://raw.githubusercontent.com/Neutron-Toolchains/antman/main/antman"
    chmod +x antman && ./antman -S
    cd "$KERNEL_PATH"
}

regen_defconfig() {
    make O="$OUT_DIR" ARCH=arm64 "$DEFCONFIG" savedefconfig
    cp "$OUT_DIR/defconfig" "arch/arm64/configs/$DEFCONFIG"
}

build_kernel() {
    [[ ! -d "$TOOLCHAIN_DIR/bin" ]] && install_tools
    mkdir -p "$OUT_DIR"
    make O="$OUT_DIR" CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 "$DEFCONFIG"
    exec 2> >(tee -a "$OUT_DIR/error.log" >&2)
    make -j"$(nproc)" \
        O="$OUT_DIR" \
        CC=clang LLVM=1 LLVM_IAS=1 \
        AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy \
        OBJDUMP=llvm-objdump STRIP=llvm-strip \
        LD=ld.lld \
        CROSS_COMPILE=aarch64-linux-gnu- \
        CROSS_COMPILE_ARM32=arm-linux-gnueabi-

    KERNEL_IMG="$OUT_DIR/arch/arm64/boot/Image.gz-dtb"
    [[ ! -f "$KERNEL_IMG" ]] && exit 1

    rm -f ./*.zip
    SUBREV="4.14.$(grep "SUBLEVEL =" Makefile | awk '{print $3}')"
    REVISION="NoVA-Begonia"
    ZIPBASE="${REVISION}-${SUBREV}"
    ZIPNAME="${ZIPBASE}.zip"
    i=1
    while [[ -f "$ZIPNAME" ]]; do
        ZIPNAME="${ZIPBASE}_v${i}.zip"
        ((i++))
    done

    if [[ ! -d Anykernel ]]; then
        git clone https://github.com/Wahid7852/Anykernel Anykernel
    fi

    cp "$KERNEL_IMG" Anykernel/
    rm -rf "$OUT_DIR/arch/arm64/boot"
    cd Anykernel && git checkout master &> /dev/null
    zip -r9 "../$ZIPNAME" * -x .git README.md *placeholder
    cd ..

    echo -e "\nBuild complete: $ZIPNAME"
    echo -e "Time: $((SECONDS / 60)) min $((SECONDS % 60)) sec"
}

case "${1:-}" in
    -r|--regen) regen_defconfig ;;
    -b|--build) build_kernel ;;
    *) echo -e "\nUsage: $0 [option] [defconfig]\n  -b, --build    Build kernel\n  -r, --regen    Regenerate defconfig\n"; exit 1 ;;
esac
