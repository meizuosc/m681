include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ft5436
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/

obj-y += focaltech_ctl.o  
obj-y += focaltech_driver.o
obj-y += focaltech_ex_fun.o
obj-y += self-test/
# mcap_5x46_test_lib.a: self-test/mcap_5x46_test_lib.o self-test/ini.o
#	$(AR) rcs $@ $^

