include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifeq ($(CONFIG_MT_SND_SOC_6755),y)
EXTRA_CFLAGS += -I$(srctree)/sound/soc/mediatek/mt_soc_audio_6755
endif

obj-$(CONFIG_MTK_KEYPAD) := hal_kpd.o
