include $(srctree)/drivers/misc/mediatek/Makefile.custom


ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/ppm/inc \
	-I$(srctree)/drivers/misc/mediatek/ppm/src/mach/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/mach/$(CONFIG_MTK_PLATFORM)/include

ifneq ($(CONFIG_MTK_FPGA),y)
# main
obj-y := mt_ppm_main.o
obj-y += mt_ppm_interface.o
obj-y += mt_ppm_api.o

# policies
obj-y += mt_ppm_policy_dlpt.o
obj-y += mt_ppm_policy_perfserv.o
obj-y += mt_ppm_policy_sys_boost.o
obj-y += mt_ppm_policy_ptpod.o
obj-y += mt_ppm_policy_pwr_thro.o
obj-y += mt_ppm_policy_thermal.o
obj-y += mt_ppm_policy_lcm_off.o
obj-y += mt_ppm_policy_hica.o
obj-y += mt_ppm_policy_user_limit.o
# for test purpose
obj-y += mt_ppm_policy_ut.o

# platform dependent files
obj-y += mach/$(subst ",,$(CONFIG_MTK_PLATFORM))/

else
# dummy driver to avoid build error
obj-y := mt_ppm_dummy.o
endif

