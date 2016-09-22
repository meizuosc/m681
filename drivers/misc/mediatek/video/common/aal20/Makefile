include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += ddp_aal.o
obj-y += ddp_pwm.o

ifeq ($(strip $(CHINATELECOM_CUSTOMIZE)),true)		
CFLAGS_ddp_pwm.o := -DMTK_REDUCE_BACKLIGHT_SUPPORT	
endif
