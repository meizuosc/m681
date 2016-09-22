obj-  := dummy.o
ifeq ($(CONFIG_THERMAL), y)
else
    obj-y += mtk_thermal_dummy.o
endif

obj-$(CONFIG_THERMAL) += mtk_thermal_platform.o

obj-y += thermal_zones/
obj-y += coolers/
