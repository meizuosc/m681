include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifeq ($(CONFIG_USB_DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG
endif

#For USB HQA Driving Tuning Mode 1 Settings
#EXTRA_CFLAGS += -DMTK_USB_MODE1

obj-$(CONFIG_PROJECT_PHY) += mtk-phy-asic.o
