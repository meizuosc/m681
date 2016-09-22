ifeq ($(MTK_TABLET_HARDWARE),mt8752)
ccflags-y += -DMTK_TABLET_TURBO
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic_wrap/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(MTK_CUSTOM_PATH)/videox/$(MTK_PLATFORM)

include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y	+= mt_spm.o
obj-y	+= mt_spm_internal.o
obj-$(CONFIG_PM)	+= mt_spm_fs.o
obj-y	+= mt_spm_sleep.o
obj-y	+= mt_spm_vcore_dvfs.o
obj-y	+= mt_spm_pmic_wrap.o
ifneq ($(CONFIG_MTK_FPGA),y)
obj-y	+= mt_spm_dpidle.o
obj-y	+= mt_spm_sodi3p0.o
obj-y	+= mt_spm_sodi.o
obj-y	+= mt_spm_mcdi.o
#obj-y	+= mt_spm_talking.o
endif
