include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += cqdma.o

ifeq ($(CONFIG_MT_ENG_BUILD),y)
CFLAGS_cqdma.o := -DDEBUG
endif
