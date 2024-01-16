# Camera Files for Functional Changes
C Driver: kernel/nvidia/drivers/media/i2c/nv_imx477.c
Register Table and Video FPS and Resolution: kernel/nvidia/drivers/media/i2c/imx477_mode_tbls.h
Device Tree Camera: hardware/nvidia/platform/t23x/common/kernel-dts/t234-common-modules/tegra234-camera-imx477-a00.dtsi
Device Tree GPIO: hardware/nvidia/platform/t23x/concord/kernel-dts/cvb/tegra234-p3737-0000-camera-imx477-a00.dtsi

# Camera Files for Includes
Driver Include as module or in Preloaded: kernel/kernel-5.10/arch/arm64/configs/dsboard_agx_defconfig
Device Tree include: hardware/nvidia/platform/t23x/concord/kernel-dts/tegra234-p3701-0000-p3737-0000-dsboard-agx.dts

# Build Instructions
cd ./kernel/kernel-5.10/
source ../../environment_nvidia

make ARCH=arm64 O=$HOME/kernel_out_dsboard_agx dsboard_agx_defconfig
make ARCH=arm64 O=$HOME/kernel_out_dsboard_agx -j$(($(nproc)-1))

# Files to copy into the Linux_for_Tegra folder
cp $HOME/kernel_out_dsboard_agx/arch/arm64/boot/Image Linux_for_Tegra/kernel/Image
$HOME/kernel_out_dsboard_agx/arch/arm64/boot/dts/nvidia/tegra234-p3701-0005-p3737-0000-dsboard-agx.dtb Linux_for_Tegra/kernel/dtb/tegra234-p3701-0005-p3737-0000.dtb

# Location to copy into live Orin
$HOME/kernel_out_dsboard_agx/arch/arm64/boot/Image goes to heimdall:/boot/Image
$HOME/kernel_out_dsboard_agx/arch/arm64/boot/dts/nvidia/tegra234-p3701-0005-p3737-0000-dsboard-agx.dtb goes to heimdall:/boot/dtb/kernel_tegra234-p3701-0005-p3737-0000.dtb




