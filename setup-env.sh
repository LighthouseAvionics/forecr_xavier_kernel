

# check if the L4T_DIR is already set
if [ -z $L4T_DIR ]; then
    L4T_DIR=$1
fi



GLIBC_FOLDER=$(pwd)/glibc

# check if glibc folder exists
if [ ! -d "$GLIBC_FOLDER" ]; then
    mkdir $GLIBC_FOLDER
    wget https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v3.0/toolchain/aarch64--glibc--stable-2022.08-1.tar.bz2 -O $GLIBC_FOLDER/aarch64--glibc--stable-2022.08-1.tar.bz2
    tar -xvf $GLIBC_FOLDER/aarch64--glibc--stable-2022.08-1.tar.bz2 -C $GLIBC_FOLDER --strip-components=1
fi

CROSS_COMPILE=$GLIBC_FOLDER/bin/aarch64-buildroot-linux-gnu-
INSTALL_MOD_PATH=$L4T_DIR/rootfs
KERNEL_OUTPUT=$(pwd)/kernel_out
KERNEL_DEF_CONFIG=dsboard_agx_defconfig


export NPROC=$(($(nproc)-2))
export LOCALVERSION=-tegra
export ARCH=arm64 
export KERNEL_HEADERS=$PWD/kernel/kernel-jammy-src
export CROSS_COMPILE
export INSTALL_MOD_PATH
export KERNEL_OUTPUT
export KERNEL_DEF_CONFIG
export OOT_SOURCE_LIST
export KERNEL_MODULAR_BUILD
export L4T_DIR




