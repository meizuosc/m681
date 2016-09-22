#add kernel source code path as head file seach path
include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree) -I$(srctree)/drivers/mmc

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
ccflags-y += -DFEATURE_MET_MMC_INDEX
endif

obj-$(CONFIG_MTK_MMC) += msdc.o

msdc-y += sd.o mt_sd_misc.o
msdc-y += msdc_tune.o
msdc-y += dbg.o
msdc-y += msdc_partition.o
msdc-y += autok.o
msdc-y += autok_dvfs.o

msdc-y += msdc_io.o
ifeq ($(CONFIG_MTK_FPGA),y)
msdc-y += msdc_fpga_io.o
endif

ifeq ($(CONFIG_MTK_AEE_IPANIC),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic_wrap/$(MTK_PLATFORM)
msdc-y += mt_dump.o
endif

ifeq ($(CONFIG_MTK_EMMC_SUPPORT_OTP),y)
msdc-y += emmc_otp.o
endif
