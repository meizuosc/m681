#
# Makefile for Invensense inv-mpu-iio device.
#
include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_INV_MPU_IIO) += inv-mpu-iio.o
inv-mpu-iio-objs := inv_mpu_core.o
#inv-mpu-iio-objs += inv_counters.o
inv-mpu-iio-objs += inv_mpu_ring.o
inv-mpu-iio-objs += inv_mpu_trigger.o
inv-mpu-iio-objs += inv_mpu_misc.o
inv-mpu-iio-objs += inv_mpu3050_iio.o
inv-mpu-iio-objs += dmp3Default_20608D.o
ifeq ($(CONFIG_INV_AUX_COMPASS), y)
inv-mpu-iio-objs += inv_slave_compass.o
endif
ifeq ($(CONFIG_INV_AUX_PRESSURE), y)
inv-mpu-iio-objs += inv_slave_pressure.o
endif

ifeq ($(VERSION),3)
ifeq ($(PATCHLEVEL),10)
#$(info "KERNELVERSION=3.10")
#--yd ifeq ($(CONFIG_INV_KERNEL_3_10), y)
CFLAGS_inv_mpu_core.o      += -Idrivers/iio    -Iinclude/linux/iio  -DINV_KERNEL_3_10
CFLAGS_inv_mpu_ring.o      += -Idrivers/iio    -Iinclude/linux/iio  -DINV_KERNEL_3_10
CFLAGS_inv_mpu_trigger.o   += -Idrivers/iio    -Iinclude/linux/iio  -DINV_KERNEL_3_10
CFLAGS_inv_mpu_misc.o      += -Idrivers/iio    -Iinclude/linux/iio  -DINV_KERNEL_3_10
CFLAGS_inv_mpu3050_iio.o   += -Idrivers/iio    -Iinclude/linux/iio  -DINV_KERNEL_3_10
CFLAGS_dmp3Default_20608D.o += -Idrivers/iio    -Iinclude/linux/iio  -DINV_KERNEL_3_10
ifeq ($(CONFIG_INV_AUX_COMPASS), y)
CFLAGS_inv_slave_compass.o   += -Idrivers/iio  -Iinclude/linux/iio  -DINV_KERNEL_3_10
endif
ifeq ($(CONFIG_INV_AUX_PRESSURE), y)
CFLAGS_inv_slave_pressure.o   += -Idrivers/iio -Iinclude/linux/iio  -DINV_KERNEL_3_10
endif
else 
CFLAGS_inv_mpu_core.o      += -Idrivers/staging/iio
CFLAGS_inv_mpu_ring.o      += -Idrivers/staging/iio
CFLAGS_inv_mpu_trigger.o   += -Idrivers/staging/iio
CFLAGS_inv_mpu_misc.o      += -Idrivers/staging/iio
CFLAGS_inv_mpu3050_iio.o   += -Idrivers/staging/iio
CFLAGS_dmp3Default_20608D.o += -Idrivers/staging/iio
ifeq ($(CONFIG_INV_AUX_COMPASS), y)
CFLAGS_inv_slave_compass.o   += -Idrivers/staging/iio
endif
ifeq ($(CONFIG_INV_AUX_PRESSURE), y)
CFLAGS_inv_slave_pressure.o   += -Idrivers/staging/iio
endif
endif
endif

# the Bosch BMA250 driver is added to the inv-mpu device driver because it
# must be connected to an MPU3050 device on the secondary slave bus.
ifeq ($(CONFIG_INV_IIO_MPU3050_ACCEL_SLAVE_BMA250), y)
inv-mpu-iio-objs += inv_slave_bma250.o
ifeq ($(VERSION),3)
ifeq ($(PATCHLEVEL),10)
#--yd ifeq ($(CONFIG_INV_KERNEL_3_10), y)
CFLAGS_inv_slave_bma250.o   += -Idrivers/iio
else
CFLAGS_inv_slave_bma250.o   += -Idrivers/staging/iio
endif
endif
endif

# compile Invensense MPU IIO driver as DTS
ifeq ($(CONFIG_DTS_INV_MPU_IIO), y)
inv-mpu-iio-objs += inv_mpu_dts.o
ifeq ($(VERSION),3)
ifeq ($(PATCHLEVEL),10)
#--yd ifeq ($(CONFIG_INV_KERNEL_3_10), y)
CFLAGS_inv_mpu_dts.o   += -Idrivers/iio
else
CFLAGS_inv_mpu_dts.o   += -Idrivers/staging/iio
endif
endif
endif

