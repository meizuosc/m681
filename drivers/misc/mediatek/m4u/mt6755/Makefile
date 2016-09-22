include $(srctree)/drivers/misc/mediatek/Makefile.custom

#ccflags-y += -O0 -gdwarf-2
ccflags-y += -I$(srctree)/drivers/staging/android/ion
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/trustzone/m4u
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/mach

obj-$(CONFIG_MTK_M4U) += m4u.o m4u_mva.o m4u_pgtable.o m4u_hw.o m4u_debug.o

