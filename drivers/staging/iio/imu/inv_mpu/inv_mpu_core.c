/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#ifndef INV_KERNEL_3_10
#include <linux/android_alarm.h>
#endif
#include<linux/sensors_io.h>

#include "inv_mpu_iio.h"
#ifdef INV_KERNEL_3_10
#include <linux/iio/sysfs.h>
#else
#include "sysfs.h"
#endif
#include "inv_counters.h"

#ifdef CONFIG_DTS_INV_MPU_IIO
#include "inv_mpu_dts.h"
#endif
extern char g_sensor_info[20];
#define ACC_DYNAMIC_CALIBRATE
#ifdef ACC_DYNAMIC_CALIBRATE
extern struct class *meizu_class;
#define MEIZU_CLASS_NAME  "meizu"
#define ACC_DEVICE_NAME  "acc"
static struct device *acc_device;
static GSENSOR_VECTOR3D gsensor_gain;
void inv_accel_enable(struct inv_mpu_state *st);
int inv_get_accel_data(struct inv_mpu_state *st, short *acc);
int inv_get_accel_raw_data(struct inv_mpu_state *st, short *acc);
int inv_get_gyro_data(struct inv_mpu_state *st, short *gyr);
static int read_time = 0;
static int read_gdata_time = 0;
int gs_calib[3] = {0};
int gyro_calib[3] = {0};
static int acc_flag[3] = {0};
static int acc_cali_temp[3] = {0};
struct accgyr_calib{
int acc_fact_calib[3];
int gyr_fact_calib[3];
} acc_gyr_calib;
#define STORE_FILE      "/dev/block/platform/mtk-msdc.0/by-name/proinfo"
#define BASE_ADDR (3*1024*1024)
#define POS (10*1024+256)
static int init_calib_flag = 0;
static int init_factory_calibration(void);
static int calibration_operation_judge( int x,int y,int z);
static int bsp_calib_flag = 0;
#endif 


/******************************************/
struct iio_dev *mtk_indio_dev;
/******************************************/
 
s64 get_time_ns(void)
{
	struct timespec ts;

	/* TODO:
	 * Use the same API as what Android elapsedRealtimeNanos() is using
	 * for good timestamp synchronization with applications.
	 * This is system dependent and more information might be found
	 * in following files.
	 *
	 * <kernel root>/drivers/staging/android/alarm-dev.c
	 * <kernel root>/drivers/rtc/alarm-dev.c
	 */

#ifdef INV_KERNEL_3_10
	/* highly likely Kernel 3.10 uses get_monotonic_boottime(),
	 * but still need to check on each platform */
	get_monotonic_boottime(&ts);
#else
	ts = ktime_to_timespec(alarm_get_elapsed_realtime());
#endif

	return timespec_to_ns(&ts);
}

/* This is for compatibility for power state. Should remove once HAL
   does not use power_state sysfs entry */
static bool fake_asleep;

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	{119, "ITG3500"},
	{ 63, "MPU3050"},
	{117, "MPU6050"},
	{118, "MPU9150"},
	{128, "MPU6500"},
	{128, "MPU9250"},
	{128, "MPU9255"},
	{128, "MPU9350"},
	{128, "MPU6515"},
	{128, "ICM20608"},
};


static const u8 reg_gyro_offset[] = {REG_XG_OFFS_USRH,
					REG_XG_OFFS_USRH + 2,
					REG_XG_OFFS_USRH + 4};

const u8 reg_6050_accel_offset[] = {REG_XA_OFFS_H,
					REG_XA_OFFS_H + 2,
					REG_XA_OFFS_H + 4};

const u8 reg_6500_accel_offset[] = {REG_6500_XA_OFFS_H,
					REG_6500_YA_OFFS_H,
					REG_6500_ZA_OFFS_H};
#ifdef CONFIG_INV_TESTING
static bool suspend_state;
static int inv_mpu_suspend(struct device *dev);
static int inv_mpu_resume(struct device *dev);
struct test_data_out {
	bool gyro;
	bool accel;
	bool compass;
	bool pressure;
	bool LPQ;
	bool SIXQ;
	bool PEDQ;
};
static struct test_data_out data_out_control;
#endif

static void inv_setup_reg(struct inv_reg_map_s *reg)
{
	reg->sample_rate_div	= REG_SAMPLE_RATE_DIV;
	reg->lpf		= REG_CONFIG;
	reg->bank_sel		= REG_BANK_SEL;
	reg->user_ctrl		= REG_USER_CTRL;
	reg->fifo_en		= REG_FIFO_EN;
	reg->gyro_config	= REG_GYRO_CONFIG;
	reg->accel_config	= REG_ACCEL_CONFIG;
	reg->fifo_count_h	= REG_FIFO_COUNT_H;
	reg->fifo_r_w		= REG_FIFO_R_W;
	reg->raw_accel		= REG_RAW_ACCEL;
	reg->temperature	= REG_TEMPERATURE;
	reg->int_enable		= REG_INT_ENABLE;
	reg->int_status		= REG_INT_STATUS;
	reg->pwr_mgmt_1		= REG_PWR_MGMT_1;
	reg->pwr_mgmt_2		= REG_PWR_MGMT_2;
	reg->mem_start_addr	= REG_MEM_START_ADDR;
	reg->mem_r_w		= REG_MEM_RW;
	reg->prgm_strt_addrh	= REG_PRGM_STRT_ADDRH;
};


/******************************************************************/

static int sensor_read_data(struct i2c_client *client, u8 command, u8 *buf, int count)
{
#if 1
	int ret = 0;
	struct i2c_msg msgs[2] = {
			{
					.addr = (client->addr)  & I2C_MASK_FLAG,
					.flags = 0,
					.buf = buf,
					.len = 1,
					.timing = I2C_TRANS_TIMING,
					.ext_flag = 0x00,
			},
			{
					.addr = (client->addr)  & I2C_MASK_FLAG,
					.flags = I2C_M_RD,
					.buf = buf,
					.len = count,
					.timing = I2C_TRANS_TIMING,
					.ext_flag = 0x00,
			}
	};
	buf[0] = command;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if( ret!=2 ) {
		printk("invn: %s read error, reg:0x%x,count:%d, ext_flag:0x%x\n",__func__,command,count,client->addr&0xff00);
	}
#endif
	return ret;
}



/**
 *  inv_i2c_read_base() - Read one or more bytes from the device registers.
 *  @st:	Device driver instance.
 *  @i2c_addr:  i2c address of device.
 *  @reg:	First device register to be read from.
 *  @length:	Number of bytes to read.
 *  @data:	Data read from device.
 *  NOTE:This is not re-implementation of i2c_smbus_read because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_read_base(struct inv_mpu_state *st, u16 i2c_addr,
	u8 reg, u16 size, u8 *data)
{

	int i,retval = 0;
	u8 addr = reg;
	u8 buf[1024]={0,};
	int times = (size>1024?1024:size)/I2C_WR_MAX;

	for (i = 0; i < times; i++) {
		if( reg!=REG_FIFO_R_W ) {
			retval |= sensor_read_data(st->client, addr + I2C_WR_MAX*i, buf + I2C_WR_MAX*i, I2C_WR_MAX);
		} else {
			retval |= sensor_read_data(st->client, addr, buf + I2C_WR_MAX*i, I2C_WR_MAX);
		}
	}

	if (size % I2C_WR_MAX) {
		if( reg!=REG_FIFO_R_W ) {
			retval |= sensor_read_data(st->client, addr + I2C_WR_MAX*i, buf + I2C_WR_MAX*i, size % I2C_WR_MAX);
		} else {
			retval |= sensor_read_data(st->client, addr, buf + I2C_WR_MAX*i, size % I2C_WR_MAX);
		}
	}

	memcpy(data, buf, size);


	return (retval<0)?retval:0;
}



/**
 *  inv_i2c_single_write_base() - Write a byte to a device register.
 *  @st:	Device driver instance.
 *  @i2c_addr:  I2C address of the device.
 *  @reg:	Device register to be written to.
 *  @data:	Byte to write to device.
 *  NOTE:This is not re-implementation of i2c_smbus_write because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_single_write_base(struct inv_mpu_state *st,
	u16 i2c_addr, u8 reg, u8 data)
{

	int ret = 0;
	u8 buf[2];
	struct i2c_msg msgs[1] ={{0}};


#if 1

	buf[0] = reg;
	buf[1] = data;

	msgs[0].addr = i2c_addr & I2C_MASK_FLAG,
	msgs[0].flags = 0,
	msgs[0].buf = buf,
	msgs[0].len = sizeof(buf),
	msgs[0].timing = I2C_TRANS_TIMING,
	msgs[0].ext_flag = 0,

	ret = i2c_transfer(st->client->adapter, msgs, 1);
	if (ret!=1)
	{
		pr_err("invn: i2c_master_send data address 0x%x 0x%x error return %d\n",
				buf[0], buf[1], ret);
	}
#endif

	return (ret < 0) ? ret : 0;
}


int mpu_memory_write(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		     u32 size, u8 const *data)
{

#if 1
    int length = size;
    int writebytes = 0;
    int ret = 0;
    u8 const *ptr = data;
    unsigned char buf[I2C_WR_MAX + 1];

    struct i2c_msg msgs[3]={{0}, };


    memset(buf, 0, sizeof(buf));

    while(length > 0)
    {
              if (length > I2C_WR_MAX)
                       writebytes = I2C_WR_MAX;
              else
                       writebytes = length;


				buf[0] = REG_BANK_SEL;
				buf[1] = mem_addr >> 8;
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = 2;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

	          	ret = i2c_transfer(st->client->adapter, msgs, 1);
	              if (ret!=1)
	              {
	                       printk("invn: i2c transfer error ret:%d, write_bytes:%d, Line %d\n", ret, writebytes+1, __LINE__);
 				   return -1;
	              }

				buf[0] = REG_MEM_START_ADDR;
				buf[1] = mem_addr & 0xFF;
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = 2;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

	          	ret = i2c_transfer(st->client->adapter, msgs, 1);
	              if (ret!=1)
	              {
	                       printk("invn: i2c transfer error ret:%d, write_bytes:%d, Line %d\n", ret, writebytes+1, __LINE__);
 				   return -1;
	              }

				memset(buf, 0, sizeof(buf));
				buf[0] = REG_MEM_RW;
				memcpy(buf+1, ptr, writebytes);
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = writebytes+1;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

          	ret = i2c_transfer(st->client->adapter, msgs, 1);
              if (ret!=1)
              {
                       printk("invn: i2c transfer error ret:%d, write_bytes:%d, Line %d\n", ret, writebytes+1, __LINE__);
			    return -1;
              }

              length -= writebytes;
              mem_addr += writebytes;
              ptr +=writebytes;
    }

    return 0;

#endif
}

int mpu_memory_read(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		    u32 size, u8 *data)
{

    int length = size;
    int readbytes = 0;
    int ret = 0;
    unsigned char buf[I2C_WR_MAX];
    struct i2c_msg msgs[3] = {{0}, };


    while(length > 0)
    {
              if (length > I2C_WR_MAX)
                       readbytes = I2C_WR_MAX;
              else
                       readbytes = length;

              buf[0] = REG_BANK_SEL;
              buf[1] = mem_addr >> 8;
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = 2;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

	          	ret = i2c_transfer(st->client->adapter, msgs, 1);
	              if (ret!=1)
	              {
	                       printk("invn: i2c transfer error ret:%d, Line %d\n", ret, __LINE__);
				return -1;
	              }

              buf[0] = REG_MEM_START_ADDR;
              buf[1] = mem_addr & 0xFF;
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = 2;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

	          	ret = i2c_transfer(st->client->adapter, msgs, 1);
	              if (ret!=1)
	              {
	                       printk("invn: i2c transfer error ret:%d,  Line %d\n", ret,  __LINE__);
				return -1;
	              }


              buf[0] = REG_MEM_RW;
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = 1;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

	          	ret = i2c_transfer(st->client->adapter, msgs, 1);
	              if (ret!=1)
	              {
	                       printk("invn: i2c transfer error ret:%d, Line %d\n", ret, __LINE__);
				return -1;
	              }

				memset(buf, 0, sizeof(buf));
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = I2C_M_RD;
				msgs[0].buf = buf;
				msgs[0].len = readbytes;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

				ret = i2c_transfer(st->client->adapter, msgs, 1);
				  if (ret!=1)
				  {
						   printk("invn: i2c transfer error ret:%d, read_bytes:%d, Line %d\n", ret, readbytes, __LINE__);
						return -1;
				  }

              length -= readbytes;
              mem_addr += readbytes;
              memcpy(data, buf, readbytes);
              data += readbytes;
    }

    return 0;
}


int mpu_memory_write_unaligned(struct inv_mpu_state *st, u16 key, int len,
								u8 const *d)
{
	u32 addr;
	int start, end;
	int len1, len2;
	int result = 0;

	if (len > MPU_MEM_BANK_SIZE)
		return -EINVAL;
	addr = inv_dmp_get_address(key);
	if (addr > ICM20608_MAX_MPU_MEM)
		return -EINVAL;

	start = (addr >> 8);
	end   = ((addr + len - 1) >> 8);
	if (start == end) {
		result = mpu_memory_write(st, st->i2c_addr, addr, len, d);
	} else {
		end <<= 8;
		len1 = end - addr;
		len2 = len - len1;
		result = mpu_memory_write(st, st->i2c_addr, addr, len1, d);
		result |= mpu_memory_write(st, st->i2c_addr, end, len2,
								d + len1);
	}

	return result;
}

/******************************************************************/

#if 0
/**
 *  inv_i2c_read_base() - Read one or more bytes from the device registers.
 *  @st:	Device driver instance.
 *  @i2c_addr:  i2c address of device.
 *  @reg:	First device register to be read from.
 *  @length:	Number of bytes to read.
 *  @data:	Data read from device.
 *  NOTE:This is not re-implementation of i2c_smbus_read because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_read_base(struct inv_mpu_state *st, u16 i2c_addr,
	u8 reg, u16 length, u8 *data)
{
	struct i2c_msg msgs[2];
	int res;

	if (!data)
		return -EINVAL;

	msgs[0].addr = i2c_addr;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = i2c_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].len = length;

	res = i2c_transfer(st->sl_handle, msgs, 2);

	if (res < 2) {
		if (res >= 0)
			res = -EIO;
	} else
		res = 0;

	INV_I2C_INC_MPUWRITE(3);
	INV_I2C_INC_MPUREAD(length);
#if CONFIG_DYNAMIC_DEBUG
	{
		char *read = 0;
		pr_debug("%s RD%02X%02X%02X -> %s%s\n", st->hw->name,
			 i2c_addr, reg, length,
			 wr_pr_debug_begin(data, length, read),
			 wr_pr_debug_end(read));
	}
#endif
	return res;
}

/**
 *  inv_i2c_single_write_base() - Write a byte to a device register.
 *  @st:	Device driver instance.
 *  @i2c_addr:  I2C address of the device.
 *  @reg:	Device register to be written to.
 *  @data:	Byte to write to device.
 *  NOTE:This is not re-implementation of i2c_smbus_write because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_single_write_base(struct inv_mpu_state *st,
	u16 i2c_addr, u8 reg, u8 data)
{
	u8 tmp[2];
	struct i2c_msg msg;
	int res;
	tmp[0] = reg;
	tmp[1] = data;

	msg.addr = i2c_addr;
	msg.flags = 0;	/* write */
	msg.buf = tmp;
	msg.len = 2;

	pr_debug("%s WR%02X%02X%02X\n", st->hw->name, i2c_addr, reg, data);
	INV_I2C_INC_MPUWRITE(3);

	res = i2c_transfer(st->sl_handle, &msg, 1);
	if (res < 1) {
		if (res == 0)
			res = -EIO;
		return res;
	} else
		return 0;
}

#endif

static int inv_switch_engine(struct inv_mpu_state *st, bool en, u32 mask)
{
	struct inv_reg_map_s *reg;
	u8 data, mgmt_1;
	int result;

	reg = &st->reg;
	/* Only when gyro is on, can
	   clock source be switched to gyro. Otherwise, it must be set to
	   internal clock */
	if (BIT_PWR_GYRO_STBY == mask) {
		result = inv_i2c_read(st, reg->pwr_mgmt_1, 1, &mgmt_1);
		if (result)
			return result;

		mgmt_1 &= ~BIT_CLK_MASK;
	}

	if ((BIT_PWR_GYRO_STBY == mask) && (!en)) {
		/* turning off gyro requires switch to internal clock first.
		   Then turn off gyro engine */
		mgmt_1 |= INV_CLK_INTERNAL;
		result = inv_i2c_single_write(st, reg->pwr_mgmt_1,
						mgmt_1);
		if (result)
			return result;
	}

	result = inv_i2c_read(st, reg->pwr_mgmt_2, 1, &data);
	if (result)
		return result;
	if (en)
		data &= (~mask);
	else
		data |= mask;
	result = inv_i2c_single_write(st, reg->pwr_mgmt_2, data);
	if (result)
		return result;

	if ((BIT_PWR_GYRO_STBY == mask) && en) {
		/* only gyro on needs sensor up time */
		msleep(SENSOR_UP_TIME);
		/* after gyro is on & stable, switch internal clock to PLL */
		mgmt_1 |= INV_CLK_PLL;
		result = inv_i2c_single_write(st, reg->pwr_mgmt_1,
						mgmt_1);
		if (result)
			return result;
	}
	if ((BIT_PWR_ACCEL_STBY == mask) && en)
		msleep(REG_UP_TIME);

	return 0;
}

/*
 *  inv_lpa_freq() - store current low power frequency setting.
 */
static int inv_lpa_freq(struct inv_mpu_state *st, int lpa_freq)
{
	unsigned long result;
	u8 d;
	/* 2, 4, 6, 7 corresponds to 0.98, 3.91, 15.63, 31.25 */
	const u8 mpu6500_lpa_mapping[] = {2, 4, 6, 7};

	if (lpa_freq > MAX_LPA_FREQ_PARAM)
		return -EINVAL;

	if (INV_MPU6500 == st->chip_type || 
		INV_ICM20608 == st->chip_type) {
		d = mpu6500_lpa_mapping[lpa_freq];
		result = inv_i2c_single_write(st, REG_6500_LP_ACCEL_ODR, d);
		if (result)
			return result;
	}
	st->chip_config.lpa_freq = lpa_freq;

	return 0;
}

static int set_power_itg(struct inv_mpu_state *st, bool power_on)
{
	struct inv_reg_map_s *reg;
	u8 data;
	int result;

	if ((!power_on) == st->chip_config.is_asleep)
		return 0;
	reg = &st->reg;
	if (power_on)
		data = 0;
	else
		data = BIT_SLEEP;
	result = inv_i2c_single_write(st, reg->pwr_mgmt_1, data);
	if (result)
		return result;

	if (power_on)
		msleep(REG_UP_TIME);

	st->chip_config.is_asleep = !power_on;

	return 0;
}

/**
 *  inv_init_config() - Initialize hardware, disable FIFO.
 *  @indio_dev:	Device driver instance.
 *  Initial configuration:
 *  FSR: +/- 2000DPS
 *  DLPF: 42Hz
 *  FIFO rate: 50Hz
 */
static int inv_init_config(struct iio_dev *indio_dev)
{
	struct inv_reg_map_s *reg;
	int result, i;
	struct inv_mpu_state *st = iio_priv(indio_dev);
	const u8 *ch;
	u8 d[2];

	reg = &st->reg;

	result = inv_i2c_single_write(st, reg->gyro_config,
		INV_FSR_2000DPS << GYRO_CONFIG_FSR_SHIFT);
	if (result)
		return result;

	st->chip_config.fsr = INV_FSR_2000DPS;

	result = inv_i2c_single_write(st, reg->lpf, INV_FILTER_42HZ);
	if (result)
		return result;
	st->chip_config.lpf = INV_FILTER_42HZ;

	result = inv_i2c_single_write(st, reg->sample_rate_div,
					ONE_K_HZ / INIT_FIFO_RATE - 1);
	if (result)
		return result;
	st->chip_config.fifo_rate = INIT_FIFO_RATE;
	st->irq_dur_ns            = INIT_DUR_TIME;
	st->chip_config.prog_start_addr = DMP_START_ADDR;
	if (INV_MPU6050 == st->chip_type)
		st->self_test.samples = INIT_ST_MPU6050_SAMPLES;
	else
		st->self_test.samples = INIT_ST_SAMPLES;
	st->self_test.threshold = INIT_ST_THRESHOLD;
	st->batch.wake_fifo_on = true;
	st->suspend_state = false;
	if (INV_ITG3500 != st->chip_type) {
		st->chip_config.accel_fs = INV_FS_02G;
		result = inv_i2c_single_write(st, reg->accel_config,
			(INV_FS_02G << ACCEL_CONFIG_FSR_SHIFT));
		if (result)
			return result;
		st->tap.time = INIT_TAP_TIME;
		st->tap.thresh = INIT_TAP_THRESHOLD;
		st->tap.min_count = INIT_TAP_MIN_COUNT;
		st->sample_divider = INIT_SAMPLE_DIVIDER;
		st->smd.threshold = MPU_INIT_SMD_THLD;
		st->smd.delay     = MPU_INIT_SMD_DELAY_THLD;
		st->smd.delay2    = MPU_INIT_SMD_DELAY2_THLD;
		st->ped.int_thresh = INIT_PED_INT_THRESH;
		st->ped.step_thresh = INIT_PED_THRESH;
		st->sensor[SENSOR_STEP].rate = MAX_DMP_OUTPUT_RATE;

		result = inv_i2c_single_write(st, REG_ACCEL_MOT_DUR,
						INIT_MOT_DUR);
		if (result)
			return result;
		st->mot_int.mot_dur = INIT_MOT_DUR;

		result = inv_i2c_single_write(st, REG_ACCEL_MOT_THR,
						INIT_MOT_THR);
		if (result)
			return result;
		st->mot_int.mot_thr = INIT_MOT_THR;

		for (i = 0; i < 3; i++) {
			result = inv_i2c_read(st, reg_gyro_offset[i], 2, d);
			if (result)
				return result;
			st->rom_gyro_offset[i] =
					(short)be16_to_cpup((__be16 *)(d));
			st->input_gyro_offset[i] = 0;
			st->input_gyro_dmp_bias[i] = 0;
		}
		if (INV_MPU6050 == st->chip_type)
			ch = reg_6050_accel_offset;
		else
			ch = reg_6500_accel_offset;
		for (i = 0; i < 3; i++) {
			result = inv_i2c_read(st, ch[i], 2, d);
			if (result)
				return result;
			st->rom_accel_offset[i] =
					(short)be16_to_cpup((__be16 *)(d));
			st->input_accel_offset[i] = 0;
			st->input_accel_dmp_bias[i] = 0;
		}
		st->ped.step = 0;
		st->ped.time = 0;
	}

	return 0;
}

/*
 *  inv_write_fsr() - Configure the gyro's scale range.
 */
static int inv_write_fsr(struct inv_mpu_state *st, int fsr)
{
	struct inv_reg_map_s *reg;
	int result;

	reg = &st->reg;
	if ((fsr < 0) || (fsr > MAX_GYRO_FS_PARAM))
		return -EINVAL;
	if (fsr == st->chip_config.fsr)
		return 0;

	if (INV_MPU3050 == st->chip_type)
		result = inv_i2c_single_write(st, reg->lpf,
			(fsr << GYRO_CONFIG_FSR_SHIFT) | st->chip_config.lpf);
	else
		result = inv_i2c_single_write(st, reg->gyro_config,
			fsr << GYRO_CONFIG_FSR_SHIFT);

	if (result)
		return result;
	st->chip_config.fsr = fsr;

	return 0;
}

/*
 *  inv_write_accel_fs() - Configure the accelerometer's scale range.
 */
static int inv_write_accel_fs(struct inv_mpu_state *st, int fs)
{
	int result;
	struct inv_reg_map_s *reg;

	reg = &st->reg;
	if (fs < 0 || fs > MAX_ACCEL_FS_PARAM)
		return -EINVAL;
	if (fs == st->chip_config.accel_fs)
		return 0;
	if (INV_MPU3050 == st->chip_type)
		result = st->slave_accel->set_fs(st, fs);
	else
		result = inv_i2c_single_write(st, reg->accel_config,
				(fs << ACCEL_CONFIG_FSR_SHIFT));
	if (result)
		return result;

	st->chip_config.accel_fs = fs;

	return 0;
}

static int inv_set_offset_reg(struct inv_mpu_state *st, int reg, int val)
{
	int result;
	u8 d;

	d = ((val >> 8) & 0xff);
	result = inv_i2c_single_write(st, reg, d);
	if (result)
		return result;

	d = (val & 0xff);
	result = inv_i2c_single_write(st, reg + 1, d);

	return result;
}

int inv_reset_offset_reg(struct inv_mpu_state *st, bool en)
{
	const u8 *ch;
	int i, result;
	s16 gyro[3], accel[3];

	if (en) {
		for (i = 0; i < 3; i++) {
			gyro[i] = st->rom_gyro_offset[i];
			accel[i] = st->rom_accel_offset[i];
		}
	} else {
		for (i = 0; i < 3; i++) {
			gyro[i] = st->rom_gyro_offset[i] +
						st->input_gyro_offset[i];
			accel[i] = st->rom_accel_offset[i] +
					(st->input_accel_offset[i] << 1);
		}
	}
	if (INV_MPU6050 == st->chip_type)
		ch = reg_6050_accel_offset;
	else
		ch = reg_6500_accel_offset;

	for (i = 0; i < 3; i++) {
		result = inv_set_offset_reg(st, reg_gyro_offset[i], gyro[i]);
		if (result)
			return result;
		result = inv_set_offset_reg(st, ch[i], accel[i]);
		if (result)
			return result;
	}

	return 0;
}
/*
 *  inv_fifo_rate_store() - Set fifo rate.
 */
static int inv_fifo_rate_store(struct inv_mpu_state *st, int fifo_rate)
{
	if ((fifo_rate < MIN_FIFO_RATE) || (fifo_rate > MAX_FIFO_RATE))
		return -EINVAL;
	if (fifo_rate == st->chip_config.fifo_rate)
		return 0;

	st->irq_dur_ns = NSEC_PER_SEC / fifo_rate;
	st->chip_config.fifo_rate = fifo_rate;

	return 0;
}

/*
 *  inv_reg_dump_show() - Register dump for testing.
 */
static ssize_t inv_reg_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ii;
	char data;
	ssize_t bytes_printed = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);

//	mutex_lock(&indio_dev->mlock);
	for (ii = 0; ii < st->hw->num_reg; ii++) {
		/* don't read fifo r/w register */
		if (ii == st->reg.fifo_r_w)
			data = 0;
		else
			inv_i2c_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}
//	mutex_unlock(&indio_dev->mlock);

	return bytes_printed;
}

int write_be32_key_to_mem(struct inv_mpu_state *st,
					u32 data, int key)
{
	cpu_to_be32s(&data);
	return mem_w_key(key, sizeof(data), (u8 *)&data);
}

int inv_write_2bytes(struct inv_mpu_state *st, int k, int data)
{
	u8 d[2];

	if (data < 0 || data > USHRT_MAX)
		return -EINVAL;

	d[0] = (u8)((data >> 8) & 0xff);
	d[1] = (u8)(data & 0xff);

	return mem_w_key(k, ARRAY_SIZE(d), d);
}

static ssize_t _dmp_bias_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data, tmp;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	if (!st->chip_config.enable) {
		result = st->set_power_state(st, true);
		if (result)
			return result;
	}

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto dmp_bias_store_fail;
	switch (this_attr->address) {
	case ATTR_DMP_ACCEL_X_DMP_BIAS:
		tmp = st->input_accel_dmp_bias[0];
		st->input_accel_dmp_bias[0] = data;
		result = inv_set_accel_bias_dmp(st);
		if (result)
			st->input_accel_dmp_bias[0] = tmp;
		break;
	case ATTR_DMP_ACCEL_Y_DMP_BIAS:
		tmp = st->input_accel_dmp_bias[1];
		st->input_accel_dmp_bias[1] = data;
		result = inv_set_accel_bias_dmp(st);
		if (result)
			st->input_accel_dmp_bias[1] = tmp;
		break;
	case ATTR_DMP_ACCEL_Z_DMP_BIAS:
		tmp = st->input_accel_dmp_bias[2];
		st->input_accel_dmp_bias[2] = data;
		result = inv_set_accel_bias_dmp(st);
		if (result)
			st->input_accel_dmp_bias[2] = tmp;
		break;
	case ATTR_DMP_GYRO_X_DMP_BIAS:
		result = write_be32_key_to_mem(st, data,
					KEY_CFG_EXT_GYRO_BIAS_X);
		if (result)
			goto dmp_bias_store_fail;
		st->input_gyro_dmp_bias[0] = data;
		break;
	case ATTR_DMP_GYRO_Y_DMP_BIAS:
		result = write_be32_key_to_mem(st, data,
					KEY_CFG_EXT_GYRO_BIAS_Y);
		if (result)
			goto dmp_bias_store_fail;
		st->input_gyro_dmp_bias[1] = data;
		break;
	case ATTR_DMP_GYRO_Z_DMP_BIAS:
		result = write_be32_key_to_mem(st, data,
					KEY_CFG_EXT_GYRO_BIAS_Z);
		if (result)
			goto dmp_bias_store_fail;
		st->input_gyro_dmp_bias[2] = data;
		break;
	default:
		break;
	}

dmp_bias_store_fail:
	if (!st->chip_config.enable)
		result |= st->set_power_state(st, false);
	if (result)
		return result;

	return count;
}

static ssize_t inv_dmp_bias_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _dmp_bias_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t _dmp_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	if (st->chip_config.enable)
		return -EBUSY;
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	switch (this_attr->address) {
	/* power of chip is not turned on */
	case ATTR_DMP_ON:
		st->chip_config.dmp_on = !!data;
		break;
	case ATTR_DMP_INT_ON:
		st->chip_config.dmp_int_on = !!data;
		break;
	case ATTR_DMP_EVENT_INT_ON:
		st->chip_config.dmp_event_int_on = !!data;
		break;
	case ATTR_DMP_STEP_INDICATOR_ON:
		st->chip_config.step_indicator_on = !!data;
		break;
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		if (data < 0 || data > INT_MAX)
			return -EINVAL;
		st->batch.timeout = data;
		break;
	case ATTR_DMP_BATCHMODE_WAKE_FIFO_FULL:
		st->batch.wake_fifo_on = !!data;
		st->batch.overflow_on = 0;
		break;
	case ATTR_DMP_SIX_Q_ON:
		st->sensor[SENSOR_SIXQ].on = !!data;
		break;
	case ATTR_DMP_SIX_Q_WAKEUP_ENABLE:
		st->quat_wakeup_enabled = !!data;
		break;		
	case ATTR_DMP_SIX_Q_RATE:
		if (data > MPU_DEFAULT_DMP_FREQ || data < 0)
			return -EINVAL;
		st->sensor[SENSOR_SIXQ].rate = data;
		st->sensor[SENSOR_SIXQ].dur = MPU_DEFAULT_DMP_FREQ / data;
		st->sensor[SENSOR_SIXQ].dur *= DMP_INTERVAL_INIT;
		break;
	case ATTR_DMP_LPQ_ON:
		st->sensor[SENSOR_LPQ].on = !!data;
		break;
	case ATTR_DMP_LPQ_RATE:
		if (data > MPU_DEFAULT_DMP_FREQ || data < 0)
			return -EINVAL;
		st->sensor[SENSOR_LPQ].rate = data;
		st->sensor[SENSOR_LPQ].dur = MPU_DEFAULT_DMP_FREQ / data;
		st->sensor[SENSOR_LPQ].dur *= DMP_INTERVAL_INIT;
		break;
	case ATTR_DMP_PED_Q_ON:
		st->sensor[SENSOR_PEDQ].on = !!data;
		break;
	case ATTR_DMP_PED_Q_RATE:
		if (data > MPU_DEFAULT_DMP_FREQ || data < 0)
			return -EINVAL;
		st->sensor[SENSOR_PEDQ].rate = data;
		st->sensor[SENSOR_PEDQ].dur = MPU_DEFAULT_DMP_FREQ / data;
		st->sensor[SENSOR_PEDQ].dur *= DMP_INTERVAL_INIT;
		break;
	case ATTR_DMP_STEP_DETECTOR_ON:
		st->sensor[SENSOR_STEP].on = !!data;
		break;
	default:
		return -EINVAL;
	}

	return count;
}

/*
 * inv_dmp_attr_store() -  calling this function will store DMP attributes
 */
static ssize_t inv_dmp_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _dmp_attr_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t _dmp_mem_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	if (st->chip_config.enable)
		return -EBUSY;
	if (!st->chip_config.firmware_loaded)
		return -EINVAL;
	result = st->set_power_state(st, true);
	if (result)
		return result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto dmp_mem_store_fail;
	switch (this_attr->address) {
	case ATTR_DMP_PED_INT_ON:
		result = inv_enable_pedometer_interrupt(st, !!data);
		if (result)
			goto dmp_mem_store_fail;
		st->ped.int_on = !!data;
		break;
	case ATTR_DMP_PED_ON:
	{
		result = inv_enable_pedometer(st, !!data);
		if (result)
			goto dmp_mem_store_fail;
		st->ped.on = !!data;
		break;
	}
	case ATTR_DMP_PED_STEP_THRESH:
	{
		result = inv_write_2bytes(st, KEY_D_PEDSTD_SB, data);
		if (result)
			goto dmp_mem_store_fail;
		st->ped.step_thresh = data;
		break;
	}
	case ATTR_DMP_PED_INT_THRESH:
	{
		result = inv_write_2bytes(st, KEY_D_PEDSTD_SB2, data);
		if (result)
			goto dmp_mem_store_fail;
		st->ped.int_thresh = data;
		break;
	}
	case ATTR_DMP_SMD_ENABLE:
		result = inv_write_2bytes(st, KEY_SMD_ENABLE, !!data);
		if (result)
			goto dmp_mem_store_fail;
		st->chip_config.smd_enable = !!data;
		break;
	case ATTR_DMP_SMD_THLD:
		if (data < 0 || data > SHRT_MAX)
			goto dmp_mem_store_fail;
		result = write_be32_key_to_mem(st, data << 16,
						KEY_SMD_ACCEL_THLD);
		if (result)
			goto dmp_mem_store_fail;
		st->smd.threshold = data;
		break;
	case ATTR_DMP_SMD_DELAY_THLD:
		if (data < 0 || data > INT_MAX / MPU_DEFAULT_DMP_FREQ)
			goto dmp_mem_store_fail;
		result = write_be32_key_to_mem(st, data * MPU_DEFAULT_DMP_FREQ,
						KEY_SMD_DELAY_THLD);
		if (result)
			goto dmp_mem_store_fail;
		st->smd.delay = data;
		break;
	case ATTR_DMP_SMD_DELAY_THLD2:
		if (data < 0 || data > INT_MAX / MPU_DEFAULT_DMP_FREQ)
			goto dmp_mem_store_fail;
		result = write_be32_key_to_mem(st, data * MPU_DEFAULT_DMP_FREQ,
						KEY_SMD_DELAY2_THLD);
		if (result)
			goto dmp_mem_store_fail;
		st->smd.delay2 = data;
		break;
#if 0 //tab		
	case ATTR_DMP_TAP_ON:
		result = inv_enable_tap_dmp(st, !!data);
		if (result)
			goto dmp_mem_store_fail;
		st->tap.on = !!data;
		break;
	case ATTR_DMP_TAP_THRESHOLD:
		if (data < 0 || data > USHRT_MAX) {
			result = -EINVAL;
			goto dmp_mem_store_fail;
		}
		result = inv_set_tap_threshold_dmp(st, data);
		if (result)
			goto dmp_mem_store_fail;
		st->tap.thresh = data;
		break;
	case ATTR_DMP_TAP_MIN_COUNT:
		if (data < 0 || data > USHRT_MAX) {
			result = -EINVAL;
			goto dmp_mem_store_fail;
		}
		result = inv_set_min_taps_dmp(st, data);
		if (result)
			goto dmp_mem_store_fail;
		st->tap.min_count = data;
		break;
	case ATTR_DMP_TAP_TIME:
		if (data < 0 || data > USHRT_MAX) {
			result = -EINVAL;
			goto dmp_mem_store_fail;
		}
		result = inv_set_tap_time_dmp(st, data);
		if (result)
			goto dmp_mem_store_fail;
		st->tap.time = data;
		break;
#endif		
	case ATTR_DMP_DISPLAY_ORIENTATION_ON:
		result = inv_set_display_orient_interrupt_dmp(st, !!data);
		if (result)
			goto dmp_mem_store_fail;
		st->chip_config.display_orient_on = !!data;
		break;
#ifdef CONFIG_INV_TESTING
	case ATTR_DEBUG_SMD_ENABLE_TESTP1:
	{
		u8 d[] = {0x42};
		result = st->set_power_state(st, true);
		if (result)
			goto dmp_mem_store_fail;
		result = mem_w_key(KEY_SMD_ENABLE_TESTPT1, ARRAY_SIZE(d), d);
		if (result)
			goto dmp_mem_store_fail;
	}
		break;
	case ATTR_DEBUG_SMD_ENABLE_TESTP2:
	{
		u8 d[] = {0x42};
		result = st->set_power_state(st, true);
		if (result)
			goto dmp_mem_store_fail;
		result = mem_w_key(KEY_SMD_ENABLE_TESTPT2, ARRAY_SIZE(d), d);
		if (result)
			goto dmp_mem_store_fail;
	}
		break;
#endif
	default:
		result = -EINVAL;
		goto dmp_mem_store_fail;
	}

dmp_mem_store_fail:
	result |= st->set_power_state(st, false);
	if (result)
		return result;

	return count;
}

/*
 * inv_dmp_mem_store() -  calling this function will store DMP memory data
 */
static ssize_t inv_dmp_mem_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _dmp_mem_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t inv_attr64_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	u64 tmp;
	u32 ped;

	mutex_lock(&indio_dev->mlock);
	if (!st->chip_config.enable || !st->chip_config.dmp_on) {
		mutex_unlock(&indio_dev->mlock);
		return -EINVAL;
	}
	result = 0;
	switch (this_attr->address) {
	case ATTR_DMP_PEDOMETER_STEPS:
		result = inv_get_pedometer_steps(st, &ped);
		result |= inv_read_pedometer_counter(st);
		tmp = st->ped.step + ped;
		break;
	case ATTR_DMP_PEDOMETER_TIME:
		result = inv_get_pedometer_time(st, &ped);
		tmp = (u64)st->ped.time + ((u64)ped) * MS_PER_PED_TICKS;
		break;
	case ATTR_DMP_PEDOMETER_COUNTER:
		tmp = st->ped.last_step_time;
		break;
	default:
		result = -EINVAL;
		break;
	}
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return -EINVAL;
	return sprintf(buf, "%lld\n", tmp);
}

static ssize_t inv_attr64_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	u64 data;

	mutex_lock(&indio_dev->mlock);
	if (st->chip_config.enable || (!st->chip_config.firmware_loaded)) {
		mutex_unlock(&indio_dev->mlock);
		return -EINVAL;
	}
	result = st->set_power_state(st, true);
	if (result) {
		mutex_unlock(&indio_dev->mlock);
		return result;
	}
	result = kstrtoull(buf, 10, &data);
	if (result)
		goto attr64_store_fail;
	switch (this_attr->address) {
	case ATTR_DMP_PEDOMETER_STEPS:
		result = write_be32_key_to_mem(st, 0, KEY_D_PEDSTD_STEPCTR);
		if (result)
			goto attr64_store_fail;
		st->ped.step = data;
		break;
	case ATTR_DMP_PEDOMETER_TIME:
		result = write_be32_key_to_mem(st, 0, KEY_D_PEDSTD_TIMECTR);
		if (result)
			goto attr64_store_fail;
		st->ped.time = data;
		break;
	default:
		result = -EINVAL;
		break;
	}
attr64_store_fail:
	mutex_unlock(&indio_dev->mlock);
	result = st->set_power_state(st, false);
	if (result)
		return result;

	return count;
}
/*
 * inv_attr_show() -  calling this function will show current
 *                        dmp parameters.
 */
static ssize_t inv_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, axis;
	s8 *m;
	int err = 0;
/*
	if ( init_calib_flag == 0){
	err = init_factory_calibration();
	printk("init_factory_calibration inv_attr_show!\n");
	init_calib_flag = 1;
	if (err < 0 )
    	{	
        	printk("init_factory_calibration failed!\n");
    	}
	}
*/
	switch (this_attr->address) {
	case ATTR_GYRO_SCALE:
	{
		const s16 gyro_scale[] = {250, 500, 1000, 2000};

		return sprintf(buf, "%d\n", gyro_scale[st->chip_config.fsr]);
	}
	case ATTR_ACCEL_SCALE:
	{
		const s16 accel_scale[] = {2, 4, 8, 16};
		return sprintf(buf, "%d\n",
					accel_scale[st->chip_config.accel_fs] *
					st->chip_info.multi);
	}
	case ATTR_COMPASS_SCALE:
		st->slave_compass->get_scale(st, &result);

		return sprintf(buf, "%d\n", result);
	case ATTR_ACCEL_X_CALIBBIAS:
	case ATTR_ACCEL_Y_CALIBBIAS:
	case ATTR_ACCEL_Z_CALIBBIAS:
		axis = this_attr->address - ATTR_ACCEL_X_CALIBBIAS;
		return sprintf(buf, "%d\n", st->accel_bias[axis] *
						st->chip_info.multi);
	case ATTR_GYRO_X_CALIBBIAS:
	case ATTR_GYRO_Y_CALIBBIAS:
	case ATTR_GYRO_Z_CALIBBIAS:
		axis = this_attr->address - ATTR_GYRO_X_CALIBBIAS;
		return sprintf(buf, "%d\n", st->gyro_bias[axis]);
	case ATTR_SELF_TEST_GYRO_SCALE:
		return sprintf(buf, "%d\n", SELF_TEST_GYRO_FULL_SCALE);
	case ATTR_SELF_TEST_ACCEL_SCALE:
		if (INV_MPU6500 == st->chip_type)
			return sprintf(buf, "%d\n", SELF_TEST_ACCEL_6500_SCALE);
		else
			return sprintf(buf, "%d\n", SELF_TEST_ACCEL_FULL_SCALE);
	case ATTR_GYRO_X_OFFSET:
	case ATTR_GYRO_Y_OFFSET:
	case ATTR_GYRO_Z_OFFSET:
		axis = this_attr->address - ATTR_GYRO_X_OFFSET;
		return sprintf(buf, "%d\n", st->input_gyro_offset[axis]);
	case ATTR_ACCEL_X_OFFSET:
	case ATTR_ACCEL_Y_OFFSET:
	case ATTR_ACCEL_Z_OFFSET:
		axis = this_attr->address - ATTR_ACCEL_X_OFFSET;
		return sprintf(buf, "%d\n", st->input_accel_offset[axis]);
	case ATTR_DMP_ACCEL_X_DMP_BIAS:
		return sprintf(buf, "%d\n", st->input_accel_dmp_bias[0]);
	case ATTR_DMP_ACCEL_Y_DMP_BIAS:
		return sprintf(buf, "%d\n", st->input_accel_dmp_bias[1]);
	case ATTR_DMP_ACCEL_Z_DMP_BIAS:
		return sprintf(buf, "%d\n", st->input_accel_dmp_bias[2]);
	case ATTR_DMP_GYRO_X_DMP_BIAS:
		return sprintf(buf, "%d\n", st->input_gyro_dmp_bias[0]);
	case ATTR_DMP_GYRO_Y_DMP_BIAS:
		return sprintf(buf, "%d\n", st->input_gyro_dmp_bias[1]);
	case ATTR_DMP_GYRO_Z_DMP_BIAS:
		return sprintf(buf, "%d\n", st->input_gyro_dmp_bias[2]);
	case ATTR_DMP_PED_INT_ON:
		return sprintf(buf, "%d\n", st->ped.int_on);
	case ATTR_DMP_PED_ON:
		return sprintf(buf, "%d\n", st->ped.on);
	case ATTR_DMP_PED_STEP_THRESH:
		return sprintf(buf, "%d\n", st->ped.step_thresh);
	case ATTR_DMP_PED_INT_THRESH:
		return sprintf(buf, "%d\n", st->ped.int_thresh);
	case ATTR_DMP_SMD_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.smd_enable);
	case ATTR_DMP_SMD_THLD:
		return sprintf(buf, "%d\n", st->smd.threshold);
	case ATTR_DMP_SMD_DELAY_THLD:
		return sprintf(buf, "%d\n", st->smd.delay);
	case ATTR_DMP_SMD_DELAY_THLD2:
		return sprintf(buf, "%d\n", st->smd.delay2);
	case ATTR_DMP_TAP_ON:
		return sprintf(buf, "%d\n", st->tap.on);
	case ATTR_DMP_TAP_THRESHOLD:
		return sprintf(buf, "%d\n", st->tap.thresh);
	case ATTR_DMP_TAP_MIN_COUNT:
		return sprintf(buf, "%d\n", st->tap.min_count);
	case ATTR_DMP_TAP_TIME:
		return sprintf(buf, "%d\n", st->tap.time);
	case ATTR_DMP_DISPLAY_ORIENTATION_ON:
		return sprintf(buf, "%d\n",
			st->chip_config.display_orient_on);
	case ATTR_DMP_ON:
		return sprintf(buf, "%d\n", st->chip_config.dmp_on);
	case ATTR_DMP_INT_ON:
		return sprintf(buf, "%d\n", st->chip_config.dmp_int_on);
	case ATTR_DMP_EVENT_INT_ON:
		return sprintf(buf, "%d\n", st->chip_config.dmp_event_int_on);
	case ATTR_DMP_STEP_INDICATOR_ON:
		return sprintf(buf, "%d\n", st->chip_config.step_indicator_on);
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		return sprintf(buf, "%d\n",
				st->batch.timeout);
	case ATTR_DMP_BATCHMODE_WAKE_FIFO_FULL:
		return sprintf(buf, "%d\n",
				st->batch.wake_fifo_on);
	case ATTR_DMP_SIX_Q_ON:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_SIXQ].on);
	case ATTR_DMP_SIX_Q_WAKEUP_ENABLE:
		return sprintf(buf, "%d\n", st->quat_wakeup_enabled);
	case ATTR_DMP_SIX_Q_RATE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_SIXQ].rate);
	case ATTR_DMP_LPQ_ON:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_LPQ].on);
	case ATTR_DMP_LPQ_RATE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_LPQ].rate);
	case ATTR_DMP_PED_Q_ON:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_PEDQ].on);
	case ATTR_DMP_PED_Q_RATE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_PEDQ].rate);
	case ATTR_DMP_STEP_DETECTOR_ON:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_STEP].on);
	case ATTR_MOTION_LPA_ON:
		return sprintf(buf, "%d\n", st->mot_int.mot_on);
	case ATTR_MOTION_LPA_FREQ:{
		const char *f[] = {"1.25", "5", "20", "40"};
		return sprintf(buf, "%s\n", f[st->chip_config.lpa_freq]);
	}
	case ATTR_MOTION_LPA_THRESHOLD:
		return sprintf(buf, "%d\n", st->mot_int.mot_thr);

	case ATTR_SELF_TEST_SAMPLES:
		return sprintf(buf, "%d\n", st->self_test.samples);
	case ATTR_SELF_TEST_THRESHOLD:
		return sprintf(buf, "%d\n", st->self_test.threshold);
	case ATTR_GYRO_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.gyro_enable);
	case ATTR_GYRO_FIFO_ENABLE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_GYRO].on);
	case ATTR_GYRO_RATE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_GYRO].rate);
	case ATTR_ACCEL_ENABLE:	
		return sprintf(buf, "%d\n", st->chip_config.accel_enable);
	case ATTR_ACCEL_FIFO_ENABLE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_ACCEL].on);
	case ATTR_ACCEL_RATE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_ACCEL].rate);
#ifdef CONFIG_INV_AUX_COMPASS
	case ATTR_COMPASS_ENABLE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_COMPASS].on);
	case ATTR_COMPASS_RATE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_COMPASS].rate);
#endif
#ifdef CONFIG_INV_AUX_PRESSURE
	case ATTR_PRESSURE_ENABLE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_PRESSURE].on);
	case ATTR_PRESSURE_RATE:
		return sprintf(buf, "%d\n", st->sensor[SENSOR_PRESSURE].rate);
#endif		
	case ATTR_POWER_STATE:
		return sprintf(buf, "%d\n", !fake_asleep);
	case ATTR_FIRMWARE_LOADED:
		return sprintf(buf, "%d\n", st->chip_config.firmware_loaded);
	case ATTR_SAMPLING_FREQ:
		return sprintf(buf, "%d\n", st->chip_config.fifo_rate);
	case ATTR_SELF_TEST:
		mutex_lock(&indio_dev->mlock);
		if (st->chip_config.enable) {
			mutex_unlock(&indio_dev->mlock);
			return -EBUSY;
		}
		if (INV_MPU3050 == st->chip_type)
			result = 1;
		else
			result = inv_hw_self_test(st);
		mutex_unlock(&indio_dev->mlock);
		return sprintf(buf, "%d\n", result);
	case ATTR_GYRO_MATRIX:
		m = st->plat_data.orientation;
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_ACCEL_MATRIX:
		if (st->plat_data.sec_slave_type ==
						SECONDARY_SLAVE_TYPE_ACCEL)
			m =
			st->plat_data.secondary_orientation;
		else
			m = st->plat_data.orientation;
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_COMPASS_MATRIX:
		if (st->plat_data.sec_slave_type ==
				SECONDARY_SLAVE_TYPE_COMPASS)
			m =
			st->plat_data.secondary_orientation;
		else
			return -ENODEV;
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_SECONDARY_NAME:
	{
		const char *n[] = {"NULL", "AK8975", "AK8972", "AK8963",
					"BMA250", "MLX90399",
					"AK09911", "AK09912"};
		switch (st->plat_data.sec_slave_id) {
		case COMPASS_ID_AK8975:
			return sprintf(buf, "%s\n", n[1]);
		case COMPASS_ID_AK8972:
			return sprintf(buf, "%s\n", n[2]);
		case COMPASS_ID_AK8963:
			return sprintf(buf, "%s\n", n[3]);
		case ACCEL_ID_BMA250:
			return sprintf(buf, "%s\n", n[4]);
		case COMPASS_ID_MLX90399:
			return sprintf(buf, "%s\n", n[5]);
		case COMPASS_ID_AK09911:
			return sprintf(buf, "%s\n", n[6]);
		case COMPASS_ID_AK09912:
			return sprintf(buf, "%s\n", n[7]);
		default:
			return sprintf(buf, "%s\n", n[0]);
		}
	}
#ifdef CONFIG_INV_TESTING
	case ATTR_REG_WRITE:
		return sprintf(buf, "1\n");
	case ATTR_COMPASS_SENS:
	{
		/* these 2 conditions should never be met, since the
		   'compass_sens' sysfs entry should be hidden if the compass
		   is not an AKM */
		if (st->plat_data.sec_slave_type !=
					SECONDARY_SLAVE_TYPE_COMPASS)
			return -ENODEV;
		if (st->plat_data.sec_slave_id != COMPASS_ID_AK8975 &&
		    st->plat_data.sec_slave_id != COMPASS_ID_AK8972 &&
		    st->plat_data.sec_slave_id != COMPASS_ID_AK8963)
			return -ENODEV;
		m = st->chip_info.compass_sens;
		return sprintf(buf, "%d,%d,%d\n", m[0], m[1], m[2]);
	}
	case ATTR_DEBUG_SMD_EXE_STATE:
	{
		u8 d[2];

		result = st->set_power_state(st, true);
		mpu_memory_read(st, st->i2c_addr,
				inv_dmp_get_address(KEY_SMD_EXE_STATE), 2, d);
		return sprintf(buf, "%d\n", (short)be16_to_cpup((__be16 *)(d)));
	}
	case ATTR_DEBUG_SMD_DELAY_CNTR:
	{
		u8 d[4];

		result = st->set_power_state(st, true);
		mpu_memory_read(st, st->i2c_addr,
				inv_dmp_get_address(KEY_SMD_DELAY_CNTR), 4, d);
		return sprintf(buf, "%d\n", (int)be32_to_cpup((__be32 *)(d)));
	}
#endif
	default:
		return -EPERM;
	}
}

/*
 * inv_dmp_display_orient_show() -  calling this function will
 *			show orientation This event must use poll.
 */
static ssize_t inv_dmp_display_orient_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st = iio_priv(dev_get_drvdata(dev));

	return sprintf(buf, "%d\n", st->display_orient_data);
}

/*
 * inv_accel_motion_show() -  calling this function showes motion interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_accel_motion_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

/*
 * inv_smd_show() -  calling this function showes smd interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_smd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

/*
 * inv_ped_show() -  calling this function showes pedometer interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_ped_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

/*
 * inv_dmp_tap_show() -  calling this function will show tap
 *                         This event must use poll.
 */
static ssize_t inv_dmp_tap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st = iio_priv(dev_get_drvdata(dev));
	return sprintf(buf, "%d\n", st->tap_data);
}

/*
 *  inv_temperature_show() - Read temperature data directly from registers.
 */
static ssize_t inv_temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct inv_reg_map_s *reg;
	int result, cur_scale, cur_off;
	short temp;
	long scale_t;
	u8 data[2];
	const long scale[] = {3834792L, 3158064L, 3340827L};
	const long offset[] = {5383314L, 2394184L, 1376256L};

	reg = &st->reg;
	mutex_lock(&indio_dev->mlock);
	if (!st->chip_config.enable)
		result = st->set_power_state(st, true);
	else
		result = 0;
	if (result) {
		mutex_unlock(&indio_dev->mlock);
		return result;
	}
	result = inv_i2c_read(st, reg->temperature, 2, data);
	if (!st->chip_config.enable)
		result |= st->set_power_state(st, false);
	mutex_unlock(&indio_dev->mlock);
	if (result) {
		pr_err("Could not read temperature register.\n");
		return result;
	}
	temp = (signed short)(be16_to_cpup((short *)&data[0]));
	switch (st->chip_type) {
	case INV_MPU3050:
		cur_scale = scale[0];
		cur_off   = offset[0];
		break;
	case INV_MPU6050:
		cur_scale = scale[1];
		cur_off   = offset[1];
		break;
	case INV_MPU6500:
	case INV_ICM20608:
		cur_scale = scale[2];
		cur_off   = offset[2];
		break;
	default:
		return -EINVAL;
	};
	scale_t = cur_off +
		inv_q30_mult((int)temp << MPU_TEMP_SHIFT, cur_scale);

	INV_I2C_INC_TEMPREAD(1);

	return sprintf(buf, "%ld %lld\n", scale_t, get_time_ns());
}

static ssize_t inv_flush_batch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;
	bool has_data = false;

	mutex_lock(&indio_dev->mlock);
	result = inv_flush_batch_data(indio_dev, &has_data);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return sprintf(buf, "%d\n", result);
	else
		return sprintf(buf, "%d\n", has_data);
}

/*
 * inv_firmware_loaded() -  calling this function will change
 *                        firmware load
 */
static int inv_firmware_loaded(struct inv_mpu_state *st, int data)
{
	if (data)
		return -EINVAL;
	st->chip_config.firmware_loaded = 0;

	return 0;
}

static int inv_switch_gyro_engine(struct inv_mpu_state *st, bool en)
{
	return inv_switch_engine(st, en, BIT_PWR_GYRO_STBY);
}

static int inv_switch_accel_engine(struct inv_mpu_state *st, bool en)
{
	return inv_switch_engine(st, en, BIT_PWR_ACCEL_STBY);
}

static int inv_check_wake_sensor_on(struct inv_mpu_state *st)
{
	st->chip_config.wake_on = false;
	st->chip_config.wake_on |= st->gyro_wakeup_enabled;
	st->chip_config.wake_on |= st->accel_wakeup_enabled;
	st->chip_config.wake_on |= st->quat_wakeup_enabled;

	return 0;
}

static ssize_t _attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data;
	u8 d, axis;
	int result;
	int err = 0;

//Modify by huangzifan at 2016.1.25
#if 0
	if ( init_calib_flag == 0){
	err = init_factory_calibration();
	 printk("init_factory_calibration at _attr_store!\n");
	init_calib_flag = 1;
	if (err < 0 )
    	{	
        	printk("init_factory_calibration failed!\n");
    	}
	}
#endif
	result = 0;
	if (st->chip_config.enable)
		return -EBUSY;
	if (this_attr->address <= ATTR_MOTION_LPA_THRESHOLD) {
		result = st->set_power_state(st, true);
		if (result)
			return result;
	}

#if 0
	if ( init_calib_flag == 0){
	err = init_factory_calibration();
	 printk("init_factory_calibration at _attr_store!\n");
//nit_calib_flag = 1
	if (err < 0 )
    	{	
        	printk("init_factory_calibration failed!\n");
    	}
	}
#endif

	/* check the input and validate it's format */
	switch (this_attr->address) {
#ifdef CONFIG_INV_TESTING
	/* these inputs are strings */
	case ATTR_COMPASS_MATRIX:
	case ATTR_COMPASS_SENS:
		break;
#endif
	/* these inputs are integers */
	default:
		result = kstrtoint(buf, 10, &data);
		if (result)
			goto attr_store_fail;
		break;
	}

	switch (this_attr->address) {
	case ATTR_GYRO_X_OFFSET:
	case ATTR_GYRO_Y_OFFSET:
	case ATTR_GYRO_Z_OFFSET:
		if ((data > MPU_MAX_G_OFFSET_VALUE) ||
				(data < MPU_MIN_G_OFFSET_VALUE))
			return -EINVAL;
		axis = this_attr->address - ATTR_GYRO_X_OFFSET;
		result = inv_set_offset_reg(st,
				reg_gyro_offset[axis],
				st->rom_gyro_offset[axis] + data);

		if (result)
			goto attr_store_fail;
		st->input_gyro_offset[axis] = data;
		break;
	case ATTR_ACCEL_X_OFFSET:
	case ATTR_ACCEL_Y_OFFSET:
	case ATTR_ACCEL_Z_OFFSET:
	{
		const u8 *ch;

		if ((data > MPU_MAX_A_OFFSET_VALUE) ||
			(data < MPU_MIN_A_OFFSET_VALUE))
			return -EINVAL;

		axis = this_attr->address - ATTR_ACCEL_X_OFFSET;
		if (INV_MPU6050 == st->chip_type)
			ch = reg_6050_accel_offset;
		else
			ch = reg_6500_accel_offset;

		result = inv_set_offset_reg(st, ch[axis],
			st->rom_accel_offset[axis] + (data << 1));
		if (result)
			goto attr_store_fail;
		st->input_accel_offset[axis] = data;
		break;
	}
	case ATTR_GYRO_SCALE:
		result = inv_write_fsr(st, data);
		break;
	case ATTR_ACCEL_SCALE:
		result = inv_write_accel_fs(st, data);
		break;
	case ATTR_COMPASS_SCALE:
		result = st->slave_compass->set_scale(st, data);
		break;
	case ATTR_MOTION_LPA_ON:
		if (INV_MPU6500 == st->chip_type ||
			INV_ICM20608 == st->chip_type) {
			if (data)
				/* enable and put in MPU6500 mode */
				d = BIT_ACCEL_INTEL_ENABLE
					| BIT_ACCEL_INTEL_MODE;
			else
				d = 0;
			result = inv_i2c_single_write(st,
						REG_6500_ACCEL_INTEL_CTRL, d);
			if (result)
				goto attr_store_fail;
		}
		st->mot_int.mot_on = !!data;
		st->chip_config.lpa_mode = !!data;
		break;
	case ATTR_MOTION_LPA_FREQ:
		result = inv_lpa_freq(st, data);
		break;
	case ATTR_MOTION_LPA_THRESHOLD:
		if ((data > MPU6XXX_MAX_MOTION_THRESH) || (data < 0)) {
			result = -EINVAL;
			goto attr_store_fail;
		}
		if (INV_MPU6500 == st->chip_type ||
			INV_ICM20608 == st->chip_type) {
			d = (u8)(data >> MPU6500_MOTION_THRESH_SHIFT);
			data = (d << MPU6500_MOTION_THRESH_SHIFT);
		} else {
			d = (u8)(data >> MPU6050_MOTION_THRESH_SHIFT);
			data = (d << MPU6050_MOTION_THRESH_SHIFT);
		}

		result = inv_i2c_single_write(st, REG_ACCEL_MOT_THR, d);
		if (result)
			goto attr_store_fail;
		st->mot_int.mot_thr = data;
		break;
	/* from now on, power is not turned on */
	case ATTR_SELF_TEST_SAMPLES:
		if (data > ST_MAX_SAMPLES || data < 0) {
			result = -EINVAL;
			goto attr_store_fail;
		}
		st->self_test.samples = data;
		break;
	case ATTR_SELF_TEST_THRESHOLD:
		if (data > ST_MAX_THRESHOLD || data < 0) {
			result = -EINVAL;
			goto attr_store_fail;
		}
		st->self_test.threshold = data;
	case ATTR_GYRO_ENABLE:
		st->chip_config.gyro_enable = !!data;
		break;
	case ATTR_GYRO_FIFO_ENABLE:
		st->sensor[SENSOR_GYRO].on = !!data;
		break;
	case ATTR_GYRO_WAKEUP_ENABLE:
		st->gyro_wakeup_enabled = !!data;
		break;
	case ATTR_GYRO_RATE:
		st->sensor[SENSOR_GYRO].rate = data;
		st->sensor[SENSOR_GYRO].dur  = MPU_DEFAULT_DMP_FREQ / data;
		st->sensor[SENSOR_GYRO].dur  *= DMP_INTERVAL_INIT;
		break;
	case ATTR_ACCEL_ENABLE:
		st->chip_config.accel_enable = !!data;
		break;
	case ATTR_ACCEL_FIFO_ENABLE:
		st->sensor[SENSOR_ACCEL].on = !!data;
		break;
	case ATTR_ACCEL_WAKEUP_ENABLE:
		st->accel_wakeup_enabled = !!data;
		break;
	case ATTR_ACCEL_RATE:
		st->sensor[SENSOR_ACCEL].rate = data;
		st->sensor[SENSOR_ACCEL].dur  = MPU_DEFAULT_DMP_FREQ / data;
		st->sensor[SENSOR_ACCEL].dur  *= DMP_INTERVAL_INIT;
		break;
#ifdef CONFIG_INV_AUX_COMPASS
	case ATTR_COMPASS_ENABLE:
		st->sensor[SENSOR_COMPASS].on = !!data;
		break;
	case ATTR_COMPASS_RATE:
		if (data <= 0) {
			result = -EINVAL;
			goto attr_store_fail;
		}
		if ((MSEC_PER_SEC / st->slave_compass->rate_scale) < data)
			data = MSEC_PER_SEC / st->slave_compass->rate_scale;

		st->sensor[SENSOR_COMPASS].rate = data;
		st->sensor[SENSOR_COMPASS].dur  = MPU_DEFAULT_DMP_FREQ / data;
		st->sensor[SENSOR_COMPASS].dur  *= DMP_INTERVAL_INIT;
		break;
#endif
#ifdef CONFIG_INV_AUX_PRESSURE
	case ATTR_PRESSURE_ENABLE:
		st->sensor[SENSOR_PRESSURE].on = !!data;
		break;
	case ATTR_PRESSURE_RATE:
		if (data <= 0) {
			result = -EINVAL;
			goto attr_store_fail;
		}
		if ((MSEC_PER_SEC / st->slave_pressure->rate_scale) < data)
			data = MSEC_PER_SEC / st->slave_pressure->rate_scale;

		st->sensor[SENSOR_PRESSURE].rate = data;
		st->sensor[SENSOR_PRESSURE].dur  = MPU_DEFAULT_DMP_FREQ / data;
		st->sensor[SENSOR_PRESSURE].dur  *= DMP_INTERVAL_INIT;
		break;
#endif		
	case ATTR_POWER_STATE:
		fake_asleep = !data;
		break;
	case ATTR_FIRMWARE_LOADED:
		result = inv_firmware_loaded(st, data);
		break;
	case ATTR_SAMPLING_FREQ:
		result = inv_fifo_rate_store(st, data);
		break;
#ifdef CONFIG_INV_TESTING
	case ATTR_COMPASS_MATRIX:
	{
		char *str;
		__s8 m[9];
		d = 0;
		if (st->plat_data.sec_slave_type == SECONDARY_SLAVE_TYPE_NONE)
			return -ENODEV;
		while ((str = strsep((char **)&buf, ","))) {
			if (d >= 9) {
				result = -EINVAL;
				goto attr_store_fail;
			}
			result = kstrtoint(str, 10, &data);
			if (result)
				goto attr_store_fail;
			if (data < -1 || data > 1) {
				result = -EINVAL;
				goto attr_store_fail;
			}
			m[d] = data;
			d++;
		}
		if (d < 9) {
			result = -EINVAL;
			goto attr_store_fail;
		}
		memcpy(st->plat_data.secondary_orientation, m, sizeof(m));
		pr_debug(KERN_INFO
			 "compass_matrix: %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			 m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
		break;
	}
	case ATTR_COMPASS_SENS:
	{
		char *str;
		__s8 s[3];
		d = 0;
		/* these 2 conditions should never be met, since the
		   'compass_sens' sysfs entry should be hidden if the compass
		   is not an AKM */
		if (st->plat_data.sec_slave_type == SECONDARY_SLAVE_TYPE_NONE)
			return -ENODEV;
		if (st->plat_data.sec_slave_id != COMPASS_ID_AK8975 &&
		    st->plat_data.sec_slave_id != COMPASS_ID_AK8972 &&
		    st->plat_data.sec_slave_id != COMPASS_ID_AK8963)
			return -ENODEV;
		/* read the input data, expecting 3 comma separated values */
		while ((str = strsep((char **)&buf, ","))) {
			if (d >= 3) {
				result = -EINVAL;
				goto attr_store_fail;
			}
			result = kstrtoint(str, 10, &data);
			if (result)
				goto attr_store_fail;
			if (data < 0 || data > 255) {
				result = -EINVAL;
				goto attr_store_fail;
			}
			s[d] = data;
			d++;
		}
		if (d < 3) {
			result = -EINVAL;
			goto attr_store_fail;
		}
		/* store the new compass sensitivity adjustment */
		memcpy(st->chip_info.compass_sens, s, sizeof(s));
		pr_debug(KERN_INFO
			 "compass_sens: %d,%d,%d\n", s[0], s[1], s[2]);
		break;
	}
#endif
	default:
		result = -EINVAL;
		goto attr_store_fail;
	};

attr_store_fail:
	if (this_attr->address <= ATTR_MOTION_LPA_THRESHOLD)
		result |= st->set_power_state(st, false);
	if (result)
		return result;

	inv_check_wake_sensor_on(st);

	return count;
}

/*
 * inv_attr_store() -  calling this function will store current
 *                        non-dmp parameter settings
 */
static ssize_t inv_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _attr_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t inv_master_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int data;
	int result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;

	mutex_lock(&indio_dev->mlock);
	if (st->chip_config.enable == (!!data)) {
		result = count;
		goto end_enable;
	}
	if (!!data) {
		result = st->set_power_state(st, true);
		if (result)
			goto end_enable;
	}
	result = set_inv_enable(indio_dev, !!data);
	if (result)
		goto end_enable;
	if (!data) {
		result = st->set_power_state(st, false);
		if (result)
			goto end_enable;
	}
	result = count;

end_enable:
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t inv_master_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st = iio_priv(dev_get_drvdata(dev));

	return sprintf(buf, "%d\n", st->chip_config.enable);
}

#ifdef CONFIG_INV_TESTING
static ssize_t inv_test_suspend_resume_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int data;
	int result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	if (data)
		inv_mpu_suspend(dev);
	else
		inv_mpu_resume(dev);
	suspend_state = !!data;

	return count;
}

static ssize_t inv_test_suspend_resume_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%d\n", suspend_state);
}

/*
 * inv_reg_write_store() - register write command for testing.
 *                         Format: WSRRDD, where RR is the register in hex,
 *                                         and DD is the data in hex.
 */
static ssize_t inv_reg_write_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	u32 result;
	u8 wreg, wval;
	int temp;
	char local_buf[10];

	if ((buf[0] != 'W' && buf[0] != 'w') ||
	    (buf[1] != 'S' && buf[1] != 's'))
		return -EINVAL;
	if (strlen(buf) < 6)
		return -EINVAL;

	strncpy(local_buf, buf, 7);
	local_buf[6] = 0;
	result = sscanf(&local_buf[4], "%x", &temp);
	if (result == 0)
		return -EINVAL;
	wval = temp;
	local_buf[4] = 0;
	sscanf(&local_buf[2], "%x", &temp);
	if (result == 0)
		return -EINVAL;
	wreg = temp;

	result = inv_i2c_single_write(st, wreg, wval);
	if (result)
		return result;

	return count;
}

static ssize_t inv_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data;
	u8 *m;
	int result;

	if (st->chip_config.enable)
		return -EBUSY;
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;

	result = st->set_power_state(st, true);
	if (result)
		return result;

	switch (this_attr->address) {
	case ATTR_DEBUG_ACCEL_COUNTER:
	{
		u8 D[6] = {0xf2, 0xb0, 0x80, 0xc0, 0xc8, 0xc2};
		u8 E[6] = {0xf3, 0xb1, 0x88, 0xc0, 0xc0, 0xc0};

		if (data)
			m = E;
		else
			m = D;
		result = mem_w_key(KEY_TEST_01, ARRAY_SIZE(D), m);
		data_out_control.accel = !!data;
		break;
	}
	case ATTR_DEBUG_GYRO_COUNTER:
	{
		u8 D[6] = {0xf2, 0xb0, 0x80, 0xc4, 0xcc, 0xc6};
		u8 E[6] = {0xf3, 0xb1, 0x88, 0xc0, 0xc0, 0xc0};

		if (data)
			m = E;
		else
			m = D;
		result = mem_w_key(KEY_TEST_02, ARRAY_SIZE(D), m);
		data_out_control.gyro = !!data;
		break;
	}
	case ATTR_DEBUG_COMPASS_COUNTER:
	{
		u8 D[6] = {0xf2, 0xb0, 0x81, 0xc0, 0xc8, 0xc2};
		u8 E[6] = {0xf3, 0xb1, 0x88, 0xc0, 0xc0, 0xc0};

		if (data)
			m = E;
		else
			m = D;
		result = mem_w_key(KEY_TEST_03, ARRAY_SIZE(D), m);
		data_out_control.compass = !!data;
		break;
	}
	case ATTR_DEBUG_PRESSURE_COUNTER:
	{
		u8 D[6] = {0xf2, 0xb0, 0x81, 0xc4, 0xcc, 0xc6};
		u8 E[6] = {0xf3, 0xb1, 0x88, 0xc0, 0xc0, 0xc0};

		if (data)
			m = E;
		else
			m = D;
		result = mem_w_key(KEY_TEST_04, ARRAY_SIZE(D), m);
		data_out_control.pressure = !!data;
		break;
	}
	case ATTR_DEBUG_LPQ_COUNTER:
	{
		u8 D[6] = {0xf1, 0xb1, 0x83, 0xc2, 0xc4, 0xc6};
		u8 E[6] = {0xf1, 0xb1, 0x88, 0xc0, 0xc0, 0xc0};

		if (data)
			m = E;
		else
			m = D;
		result = mem_w_key(KEY_TEST_05, ARRAY_SIZE(D), m);
		data_out_control.LPQ = !!data;
		break;
	}
	case ATTR_DEBUG_SIXQ_COUNTER:
	{
		u8 D[6] = {0xf1, 0xb1, 0x89, 0xc2, 0xc4, 0xc6};
		u8 E[6] = {0xf1, 0xb1, 0x88, 0xc0, 0xc0, 0xc0};

		if (data)
			m = E;
		else
			m = D;
		result = mem_w_key(KEY_TEST_06, ARRAY_SIZE(D), m);
		data_out_control.SIXQ = !!data;
		break;
	}
	case ATTR_DEBUG_PEDQ_COUNTER:
	{
		u8 D[6] = {0xf2, 0xf2, 0x88, 0xc2, 0xc4, 0xc6};
		u8 E[6] = {0xf3, 0xb1, 0x88, 0xc0, 0xc0, 0xc0};

		if (data)
			m = E;
		else
			m = D;
		result = mem_w_key(KEY_TEST_07, ARRAY_SIZE(D), m);
		data_out_control.PEDQ = !!data;
		break;
	}
	default:
		return -EINVAL;
	}

	return count;
}

static ssize_t inv_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	switch (this_attr->address) {
	case ATTR_DEBUG_ACCEL_COUNTER:
		return sprintf(buf, "%d\n", data_out_control.accel);
	case ATTR_DEBUG_GYRO_COUNTER:
		return sprintf(buf, "%d\n", data_out_control.gyro);
	case ATTR_DEBUG_COMPASS_COUNTER:
		return sprintf(buf, "%d\n", data_out_control.compass);
	case ATTR_DEBUG_PRESSURE_COUNTER:
		return sprintf(buf, "%d\n", data_out_control.pressure);
	case ATTR_DEBUG_LPQ_COUNTER:
		return sprintf(buf, "%d\n", data_out_control.LPQ);
	case ATTR_DEBUG_SIXQ_COUNTER:
		return sprintf(buf, "%d\n", data_out_control.SIXQ);
	case ATTR_DEBUG_PEDQ_COUNTER:
		return sprintf(buf, "%d\n", data_out_control.PEDQ);
	default:
		return -EINVAL;
	}
}

#endif /* CONFIG_INV_TESTING */

static const struct iio_chan_spec inv_mpu_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(INV_MPU_SCAN_TIMESTAMP),
};

/*constant IIO attribute */
static IIO_CONST_ATTR_SAMP_FREQ_AVAIL("10 20 50 100 200 500");

/* special sysfs */
static DEVICE_ATTR(reg_dump, S_IRUGO, inv_reg_dump_show, NULL);
static DEVICE_ATTR(temperature, S_IRUGO, inv_temperature_show, NULL);

/* event based sysfs, needs poll to read */
static DEVICE_ATTR(event_tap, S_IRUGO, inv_dmp_tap_show, NULL);
static DEVICE_ATTR(event_display_orientation, S_IRUGO,
	inv_dmp_display_orient_show, NULL);
static DEVICE_ATTR(event_accel_motion, S_IRUGO, inv_accel_motion_show, NULL);
static DEVICE_ATTR(event_smd, S_IRUGO, inv_smd_show, NULL);
static DEVICE_ATTR(event_pedometer, S_IRUGO, inv_ped_show, NULL);

/* master enable method */
static DEVICE_ATTR(master_enable, S_IRUGO | S_IWUGO, inv_master_enable_show,
					inv_master_enable_store);

/* special run time sysfs entry, read only */
static DEVICE_ATTR(flush_batch, S_IRUGO, inv_flush_batch_show, NULL);

/* DMP sysfs with power on/off */
static IIO_DEVICE_ATTR(in_accel_x_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store, ATTR_DMP_ACCEL_X_DMP_BIAS);
static IIO_DEVICE_ATTR(in_accel_y_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store, ATTR_DMP_ACCEL_Y_DMP_BIAS);
static IIO_DEVICE_ATTR(in_accel_z_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store, ATTR_DMP_ACCEL_Z_DMP_BIAS);

static IIO_DEVICE_ATTR(in_anglvel_x_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store, ATTR_DMP_GYRO_X_DMP_BIAS);
static IIO_DEVICE_ATTR(in_anglvel_y_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store, ATTR_DMP_GYRO_Y_DMP_BIAS);
static IIO_DEVICE_ATTR(in_anglvel_z_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store, ATTR_DMP_GYRO_Z_DMP_BIAS);

static IIO_DEVICE_ATTR(pedometer_int_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_PED_INT_ON);
static IIO_DEVICE_ATTR(pedometer_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_PED_ON);
static IIO_DEVICE_ATTR(pedometer_step_thresh, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_PED_STEP_THRESH);
static IIO_DEVICE_ATTR(pedometer_int_thresh, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_PED_INT_THRESH);

static IIO_DEVICE_ATTR(smd_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_SMD_ENABLE);
static IIO_DEVICE_ATTR(smd_threshold, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_SMD_THLD);
static IIO_DEVICE_ATTR(smd_delay_threshold, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_SMD_DELAY_THLD);
static IIO_DEVICE_ATTR(smd_delay_threshold2, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_SMD_DELAY_THLD2);

static IIO_DEVICE_ATTR(pedometer_steps, S_IRUGO | S_IWUGO, inv_attr64_show,
	inv_attr64_store, ATTR_DMP_PEDOMETER_STEPS);
static IIO_DEVICE_ATTR(pedometer_time, S_IRUGO | S_IWUGO, inv_attr64_show,
	inv_attr64_store, ATTR_DMP_PEDOMETER_TIME);
static IIO_DEVICE_ATTR(pedometer_counter, S_IRUGO | S_IWUGO, inv_attr64_show,
	NULL, ATTR_DMP_PEDOMETER_COUNTER);

static IIO_DEVICE_ATTR(tap_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_TAP_ON);
static IIO_DEVICE_ATTR(tap_threshold, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_TAP_THRESHOLD);
static IIO_DEVICE_ATTR(tap_min_count, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_TAP_MIN_COUNT);
static IIO_DEVICE_ATTR(tap_time, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_TAP_TIME);
static IIO_DEVICE_ATTR(display_orientation_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_mem_store, ATTR_DMP_DISPLAY_ORIENTATION_ON);

/* DMP sysfs without power on/off */
static IIO_DEVICE_ATTR(dmp_on, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_ON);
static IIO_DEVICE_ATTR(dmp_int_on, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_INT_ON);
static IIO_DEVICE_ATTR(dmp_event_int_on, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_EVENT_INT_ON);
static IIO_DEVICE_ATTR(step_indicator_on, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_STEP_INDICATOR_ON);
static IIO_DEVICE_ATTR(batchmode_timeout, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_attr_store, ATTR_DMP_BATCHMODE_TIMEOUT);
static IIO_DEVICE_ATTR(batchmode_wake_fifo_full_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_attr_store, ATTR_DMP_BATCHMODE_WAKE_FIFO_FULL);

static IIO_DEVICE_ATTR(six_axes_q_on, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_SIX_Q_ON);
static IIO_DEVICE_ATTR(six_axes_q_wakeup_enable, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_SIX_Q_WAKEUP_ENABLE);
static IIO_DEVICE_ATTR(six_axes_q_rate, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_SIX_Q_RATE);

static IIO_DEVICE_ATTR(three_axes_q_on, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_LPQ_ON);
static IIO_DEVICE_ATTR(three_axes_q_rate, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_LPQ_RATE);

static IIO_DEVICE_ATTR(ped_q_on, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_PED_Q_ON);
static IIO_DEVICE_ATTR(ped_q_rate, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_PED_Q_RATE);

static IIO_DEVICE_ATTR(step_detector_on, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_STEP_DETECTOR_ON);

/* non DMP sysfs with power on/off */
static IIO_DEVICE_ATTR(motion_lpa_on, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_MOTION_LPA_ON);
static IIO_DEVICE_ATTR(motion_lpa_freq, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_MOTION_LPA_FREQ);
static IIO_DEVICE_ATTR(motion_lpa_threshold, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_MOTION_LPA_THRESHOLD);

static IIO_DEVICE_ATTR(in_accel_scale, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_ACCEL_SCALE);
static IIO_DEVICE_ATTR(in_anglvel_scale, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_GYRO_SCALE);

static IIO_DEVICE_ATTR(in_anglvel_x_offset, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_GYRO_X_OFFSET);
static IIO_DEVICE_ATTR(in_anglvel_y_offset, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_GYRO_Y_OFFSET);
static IIO_DEVICE_ATTR(in_anglvel_z_offset, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_GYRO_Z_OFFSET);

static IIO_DEVICE_ATTR(in_accel_x_offset, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_ACCEL_X_OFFSET);
static IIO_DEVICE_ATTR(in_accel_y_offset, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_ACCEL_Y_OFFSET);
static IIO_DEVICE_ATTR(in_accel_z_offset, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_ACCEL_Z_OFFSET);

/* non DMP sysfs without power on/off */
static IIO_DEVICE_ATTR(self_test_samples, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_SELF_TEST_SAMPLES);
static IIO_DEVICE_ATTR(self_test_threshold, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_SELF_TEST_THRESHOLD);

static IIO_DEVICE_ATTR(gyro_enable, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_GYRO_ENABLE);
static IIO_DEVICE_ATTR(gyro_fifo_enable, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_GYRO_FIFO_ENABLE);
static IIO_DEVICE_ATTR(gyro_wakeup_enable, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_GYRO_WAKEUP_ENABLE);
static IIO_DEVICE_ATTR(gyro_rate, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_GYRO_RATE);

static IIO_DEVICE_ATTR(accel_enable, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_ACCEL_ENABLE);
static IIO_DEVICE_ATTR(accel_fifo_enable, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_ACCEL_FIFO_ENABLE);
static IIO_DEVICE_ATTR(accel_wakeup_enable, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_ACCEL_WAKEUP_ENABLE);
static IIO_DEVICE_ATTR(accel_rate, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_ACCEL_RATE);

static IIO_DEVICE_ATTR(power_state, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_POWER_STATE);
static IIO_DEVICE_ATTR(firmware_loaded, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_FIRMWARE_LOADED);
static IIO_DEVICE_ATTR(sampling_frequency, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_attr_store, ATTR_SAMPLING_FREQ);

/* show method only sysfs but with power on/off */
static IIO_DEVICE_ATTR(self_test, S_IRUGO, inv_attr_show, NULL,
	ATTR_SELF_TEST);

/* show method only sysfs */
static IIO_DEVICE_ATTR(in_accel_x_calibbias, S_IRUGO, inv_attr_show,
	NULL, ATTR_ACCEL_X_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_y_calibbias, S_IRUGO, inv_attr_show,
	NULL, ATTR_ACCEL_Y_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_z_calibbias, S_IRUGO, inv_attr_show,
	NULL, ATTR_ACCEL_Z_CALIBBIAS);

static IIO_DEVICE_ATTR(in_anglvel_x_calibbias, S_IRUGO,
		inv_attr_show, NULL, ATTR_GYRO_X_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_y_calibbias, S_IRUGO,
		inv_attr_show, NULL, ATTR_GYRO_Y_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_z_calibbias, S_IRUGO,
		inv_attr_show, NULL, ATTR_GYRO_Z_CALIBBIAS);

static IIO_DEVICE_ATTR(in_anglvel_self_test_scale, S_IRUGO,
		inv_attr_show, NULL, ATTR_SELF_TEST_GYRO_SCALE);
static IIO_DEVICE_ATTR(in_accel_self_test_scale, S_IRUGO,
		inv_attr_show, NULL, ATTR_SELF_TEST_ACCEL_SCALE);

static IIO_DEVICE_ATTR(gyro_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_GYRO_MATRIX);
static IIO_DEVICE_ATTR(accel_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_ACCEL_MATRIX);

static IIO_DEVICE_ATTR(secondary_name, S_IRUGO, inv_attr_show, NULL,
	ATTR_SECONDARY_NAME);

#ifdef CONFIG_INV_TESTING
static IIO_DEVICE_ATTR(reg_write, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_reg_write_store, ATTR_REG_WRITE);
/* smd debug related sysfs */
static IIO_DEVICE_ATTR(debug_smd_enable_testp1, S_IWUGO, NULL,
	inv_dmp_attr_store, ATTR_DEBUG_SMD_ENABLE_TESTP1);
static IIO_DEVICE_ATTR(debug_smd_enable_testp2, S_IWUGO, NULL,
	inv_dmp_attr_store, ATTR_DEBUG_SMD_ENABLE_TESTP2);
static IIO_DEVICE_ATTR(debug_smd_exe_state, S_IRUGO, inv_attr_show,
	NULL, ATTR_DEBUG_SMD_EXE_STATE);
static IIO_DEVICE_ATTR(debug_smd_delay_cntr, S_IRUGO, inv_attr_show,
	NULL, ATTR_DEBUG_SMD_DELAY_CNTR);
static DEVICE_ATTR(test_suspend_resume, S_IRUGO | S_IWUGO,
		inv_test_suspend_resume_show, inv_test_suspend_resume_store);

static IIO_DEVICE_ATTR(test_gyro_counter, S_IRUGO | S_IWUGO, inv_test_show,
	inv_test_store, ATTR_DEBUG_GYRO_COUNTER);
static IIO_DEVICE_ATTR(test_accel_counter, S_IRUGO | S_IWUGO, inv_test_show,
	inv_test_store, ATTR_DEBUG_ACCEL_COUNTER);
static IIO_DEVICE_ATTR(test_compass_counter, S_IRUGO | S_IWUGO, inv_test_show,
	inv_test_store, ATTR_DEBUG_COMPASS_COUNTER);
static IIO_DEVICE_ATTR(test_pressure_counter, S_IRUGO | S_IWUGO, inv_test_show,
	inv_test_store, ATTR_DEBUG_PRESSURE_COUNTER);
static IIO_DEVICE_ATTR(test_LPQ_counter, S_IRUGO | S_IWUGO, inv_test_show,
	inv_test_store, ATTR_DEBUG_LPQ_COUNTER);
static IIO_DEVICE_ATTR(test_SIXQ_counter, S_IRUGO | S_IWUGO, inv_test_show,
	inv_test_store, ATTR_DEBUG_SIXQ_COUNTER);
static IIO_DEVICE_ATTR(test_PEDQ_counter, S_IRUGO | S_IWUGO, inv_test_show,
	inv_test_store, ATTR_DEBUG_PEDQ_COUNTER);
#endif

static const struct attribute *inv_gyro_attributes[] = {
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	&dev_attr_reg_dump.attr,
	&dev_attr_temperature.attr,
	&dev_attr_master_enable.attr,
	&iio_dev_attr_in_anglvel_scale.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_self_test_scale.dev_attr.attr,
	&iio_dev_attr_self_test_samples.dev_attr.attr,
	&iio_dev_attr_self_test_threshold.dev_attr.attr,
	&iio_dev_attr_gyro_enable.dev_attr.attr,
	&iio_dev_attr_gyro_fifo_enable.dev_attr.attr,
	&iio_dev_attr_gyro_wakeup_enable.dev_attr.attr,
	&iio_dev_attr_gyro_rate.dev_attr.attr,
	&iio_dev_attr_power_state.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_dev_attr_self_test.dev_attr.attr,
	&iio_dev_attr_gyro_matrix.dev_attr.attr,
	&iio_dev_attr_secondary_name.dev_attr.attr,
#ifdef CONFIG_INV_TESTING
	&iio_dev_attr_reg_write.dev_attr.attr,
	&iio_dev_attr_debug_smd_enable_testp1.dev_attr.attr,
	&iio_dev_attr_debug_smd_enable_testp2.dev_attr.attr,
	&iio_dev_attr_debug_smd_exe_state.dev_attr.attr,
	&iio_dev_attr_debug_smd_delay_cntr.dev_attr.attr,
	&dev_attr_test_suspend_resume.attr,
	&iio_dev_attr_test_gyro_counter.dev_attr.attr,
	&iio_dev_attr_test_accel_counter.dev_attr.attr,
	&iio_dev_attr_test_compass_counter.dev_attr.attr,
	&iio_dev_attr_test_pressure_counter.dev_attr.attr,
	&iio_dev_attr_test_LPQ_counter.dev_attr.attr,
	&iio_dev_attr_test_SIXQ_counter.dev_attr.attr,
	&iio_dev_attr_test_PEDQ_counter.dev_attr.attr,
#endif
};

static const struct attribute *inv_mpu6xxx_attributes[] = {
	&dev_attr_event_accel_motion.attr,
	&dev_attr_event_smd.attr,
	&dev_attr_event_pedometer.attr,
	&dev_attr_flush_batch.attr,
	&iio_dev_attr_in_accel_scale.dev_attr.attr,
	&iio_dev_attr_in_accel_x_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_self_test_scale.dev_attr.attr,
	&iio_dev_attr_in_accel_x_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_y_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_z_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_x_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_dmp_bias.dev_attr.attr,
	&iio_dev_attr_pedometer_int_on.dev_attr.attr,
	&iio_dev_attr_pedometer_on.dev_attr.attr,
	&iio_dev_attr_pedometer_steps.dev_attr.attr,
	&iio_dev_attr_pedometer_time.dev_attr.attr,
	&iio_dev_attr_pedometer_counter.dev_attr.attr,
	&iio_dev_attr_pedometer_step_thresh.dev_attr.attr,
	&iio_dev_attr_pedometer_int_thresh.dev_attr.attr,
	&iio_dev_attr_smd_enable.dev_attr.attr,
	&iio_dev_attr_smd_threshold.dev_attr.attr,
	&iio_dev_attr_smd_delay_threshold.dev_attr.attr,
	&iio_dev_attr_smd_delay_threshold2.dev_attr.attr,
	&iio_dev_attr_dmp_on.dev_attr.attr,
	&iio_dev_attr_dmp_int_on.dev_attr.attr,
	&iio_dev_attr_dmp_event_int_on.dev_attr.attr,
	&iio_dev_attr_step_indicator_on.dev_attr.attr,
	&iio_dev_attr_batchmode_timeout.dev_attr.attr,
	&iio_dev_attr_batchmode_wake_fifo_full_on.dev_attr.attr,
	&iio_dev_attr_six_axes_q_on.dev_attr.attr,
	&iio_dev_attr_six_axes_q_wakeup_enable.dev_attr.attr, 
	&iio_dev_attr_six_axes_q_rate.dev_attr.attr,
	&iio_dev_attr_three_axes_q_on.dev_attr.attr,
	&iio_dev_attr_three_axes_q_rate.dev_attr.attr,
	&iio_dev_attr_ped_q_on.dev_attr.attr,
	&iio_dev_attr_ped_q_rate.dev_attr.attr,
	&iio_dev_attr_step_detector_on.dev_attr.attr,
	&iio_dev_attr_accel_enable.dev_attr.attr,
	&iio_dev_attr_accel_fifo_enable.dev_attr.attr,
	&iio_dev_attr_accel_wakeup_enable.dev_attr.attr,
	&iio_dev_attr_accel_rate.dev_attr.attr,
	&iio_dev_attr_firmware_loaded.dev_attr.attr,
	&iio_dev_attr_accel_matrix.dev_attr.attr,
};

static const struct attribute *inv_mpu6500_attributes[] = {
	&iio_dev_attr_motion_lpa_on.dev_attr.attr,
	&iio_dev_attr_motion_lpa_freq.dev_attr.attr,
	&iio_dev_attr_motion_lpa_threshold.dev_attr.attr,
};

static const struct attribute *inv_tap_attributes[] = {
	&dev_attr_event_tap.attr,
	&iio_dev_attr_tap_on.dev_attr.attr,
	&iio_dev_attr_tap_threshold.dev_attr.attr,
	&iio_dev_attr_tap_min_count.dev_attr.attr,
	&iio_dev_attr_tap_time.dev_attr.attr,
};

static const struct attribute *inv_display_orient_attributes[] = {
	&dev_attr_event_display_orientation.attr,
	&iio_dev_attr_display_orientation_on.dev_attr.attr,
};

static struct attribute *inv_attributes[
	ARRAY_SIZE(inv_gyro_attributes) +
	ARRAY_SIZE(inv_mpu6xxx_attributes) +
	ARRAY_SIZE(inv_mpu6500_attributes) +
	ARRAY_SIZE(inv_tap_attributes) +
	ARRAY_SIZE(inv_display_orient_attributes) +
	1
];

static const struct attribute_group inv_attribute_group = {
	.name = "mpu",
	.attrs = inv_attributes
};

static const struct iio_info mpu_info = {
	.driver_module = THIS_MODULE,
	.attrs = &inv_attribute_group,
};

static void inv_setup_func_ptr(struct inv_mpu_state *st)
{
	if (st->chip_type == INV_MPU3050) {
		st->set_power_state    = set_power_mpu3050;
		st->switch_gyro_engine = inv_switch_3050_gyro_engine;
		st->switch_accel_engine = inv_switch_3050_accel_engine;
		st->init_config        = inv_init_config_mpu3050;
		st->setup_reg          = inv_setup_reg_mpu3050;
	} else {
		st->set_power_state    = set_power_itg;
		st->switch_gyro_engine = inv_switch_gyro_engine;
		st->switch_accel_engine = inv_switch_accel_engine;
		st->init_config        = inv_init_config;
		st->setup_reg          = inv_setup_reg;
	}
}

/*
 *  inv_check_chip_type() - check and setup chip type.
 */
static int inv_check_chip_type(struct inv_mpu_state *st,
		const struct i2c_device_id *id, bool reset_needed)
{
	struct inv_reg_map_s *reg;
	int result;
	int t_ind;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;

	conf = &st->chip_config;
	plat = &st->plat_data;
	printk("mpu: name : %s\n", id->name);
	if (!strcmp(id->name, "mpu6515"))
		st->chip_type = INV_MPU6500;
	else if (!strcmp(id->name, "icm20608"))
		st->chip_type = INV_ICM20608;
	else
		return -EPERM;
	inv_setup_func_ptr(st);
	st->hw  = &hw_info[st->chip_type];
	reg = &st->reg;
	st->setup_reg(reg);

	if (reset_needed) {
		/* reset to make sure previous state are not there */
		result = inv_i2c_single_write(st, reg->pwr_mgmt_1, BIT_H_RESET);
		if (result)
			return result;
		msleep(POWER_UP_TIME);
	}
	/* toggle power state */
	result = st->set_power_state(st, false);
	if (result)
		return result;

	result = st->set_power_state(st, true);
	if (result)
		return result;

	printk("mpu: type= %d\n", st->chip_type);
	switch (st->chip_type) {
	case INV_MPU6500:
	case INV_ICM20608:
		result = inv_get_silicon_rev_mpu6500(st);
		break;
	default:
		result = 0;
		break;
	}
	if (result) {
		pr_err("read silicon rev error\n");
		st->set_power_state(st, false);
		return result;
	}

	/* turn off the gyro engine after OTP reading */
	result = st->switch_gyro_engine(st, false);
	if (result)
		return result;
	result = st->switch_accel_engine(st, false);
	if (result)
		return result;

	t_ind = 0;
	memcpy(&inv_attributes[t_ind], inv_gyro_attributes,
		sizeof(inv_gyro_attributes));
	t_ind += ARRAY_SIZE(inv_gyro_attributes);

	/* all MPU6xxx based parts */
	if ((INV_MPU6050 == st->chip_type) || 
		(INV_MPU6500 == st->chip_type) ||
		(INV_ICM20608 == st->chip_type)) {
		memcpy(&inv_attributes[t_ind], inv_mpu6xxx_attributes,
		       sizeof(inv_mpu6xxx_attributes));
		t_ind += ARRAY_SIZE(inv_mpu6xxx_attributes);
	}

	/* MPU6500 only */
	if ((INV_MPU6500 == st->chip_type) ||
		(INV_ICM20608 == st->chip_type)) {
		memcpy(&inv_attributes[t_ind], inv_mpu6500_attributes,
		       sizeof(inv_mpu6500_attributes));
		t_ind += ARRAY_SIZE(inv_mpu6500_attributes);
	}

	inv_attributes[t_ind] = NULL;

	return 0;
}

/*
 *  inv_create_dmp_sysfs() - create binary sysfs dmp entry.
 */
static const struct bin_attribute dmp_firmware = {
	.attr = {
		.name = "dmp_firmware",
		.mode = S_IRUGO | S_IWUGO
	},
	.size = DMP_IMAGE_SIZE + 32,
	.read = inv_dmp_firmware_read,
	.write = inv_dmp_firmware_write,
};

static const struct bin_attribute six_q_value = {
	.attr = {
		.name = "six_axes_q_value",
		.mode = S_IWUGO
	},
	.size = QUATERNION_BYTES,
	.read = NULL,
	.write = inv_six_q_write,
};

static int inv_create_dmp_sysfs(struct iio_dev *ind)
{
	int result;

	result = sysfs_create_bin_file(&ind->dev.kobj, &dmp_firmware);
	if (result)
		return result;
	result = sysfs_create_bin_file(&ind->dev.kobj, &six_q_value);

	return result;
}

#if 1
static int MPU6050_ReadChipInfo( char *buf, int bufsize)
{
    u8 databuf[10];    

    memset(databuf, 0, sizeof(u8)*10);

    if ((NULL == buf)||(bufsize<=30))
    {
        return -1;
    }


    sprintf(buf, "MPU6050 Chip");
    return 0;
}



static int icm20608d_gyro_open(struct inode *inode, struct file *file)
{


    if (file->private_data == NULL)
    {
        return -EINVAL;
    }
    return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int icm20608d_gyro_release(struct inode *inode, struct file *file)
{
    file->private_data = NULL;
    return 0;
}
/*----------------------------------------------------------------------------*/
static long icm20608d_gyro_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
struct inv_mpu_state *st = iio_priv(mtk_indio_dev);

    char strbuf[256] = {0};
    void __user *data;
   SENSOR_DATA sensor_data;
    long err = 0;
    int cali[3] = {0};
    short gyro[3] = {0};
    int gyro_cal[3] = {0};

    if (_IOC_DIR(cmd) & _IOC_READ)
    {
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
    {
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }

    if (err)
    {
 	printk("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
        return -EFAULT;
    }

    switch (cmd)
    {
    case GYROSCOPE_IOCTL_INIT:
        break;

    case GYROSCOPE_IOCTL_SMT_DATA:
		/*
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        SMTdata = kzalloc(sizeof(*SMTdata) * 800, GFP_KERNEL);
        if (SMTdata == NULL)
        {
            err = -ENOMEM;
            break;
        }
        memset(SMTdata, 0, sizeof(*SMTdata) * 800);
        MPU6050_SMTReadSensorData(client, SMTdata, 800);

        GYRO_LOG("gyroscope read data from kernel OK: SMTdata[0]:%d, copied packet:%zd!\n", SMTdata[0],
                 ((SMTdata[0]*MPU6050_AXES_NUM+2)*sizeof(s16)+1));

        smtRes = MPU6050_PROCESS_SMT_DATA(client,SMTdata);
        GYRO_LOG("ioctl smtRes: %d!\n", smtRes);
        copy_cnt = copy_to_user(data, &smtRes,  sizeof(smtRes));
        kfree(SMTdata);
        if (copy_cnt)
        {
            err = -EFAULT;
  //          GYRO_ERR("copy gyro data to user failed!\n");
        }

		*/	
  //      GYRO_LOG("copy gyro data to user OK: %d!\n", copy_cnt);
        break;

    case GYROSCOPE_IOCTL_READ_SENSORDATA:
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }
					
	if (!st->chip_config.gyro_enable)
	{
		printk("Inv accel enable in factory mode\n");
		inv_accel_enable(st);
		st->chip_config.gyro_enable = 1;
	}
 
	inv_get_gyro_data(st, gyro);	
	gyro_cal[0] = gyro[0] * 1064;    //10^6 times for angle speed (rad/s),  1064 = 1 LSB * ( PI * 10^6 / (16.4 * 180))
	gyro_cal[1] = gyro[1] * 1064;
	gyro_cal[2] = gyro[2] * 1064;
	
//	printk( "inv gyr %d %d %d\n", gyro_cal[0], gyro_cal[1], gyro_cal[2]);
//	printk( "inv gyro_fact  %d %d %d\n", gyro_cal[0], gyro_cal[1], gyro_cal[2]);
	sprintf(strbuf, "%04x %04x %04x", gyro_cal[0], gyro_cal[1], gyro_cal[2]);

        if (copy_to_user(data, strbuf, strlen(strbuf)+1))
        {
            err = -EFAULT;
            break;    
        }
        break;

    case GYROSCOPE_IOCTL_SET_CALI:
        data = (void __user*)arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }			

        if (copy_from_user(&sensor_data, data, sizeof(sensor_data)))
        {
            err = -EFAULT;
            break;    
        }	

	#if 0
	printk("huangzifan  set calib x =%d,y = %d z = %d\n",st->gyro_bias[0],st->gyro_bias[1],st->gyro_bias[2]);
   //     cali[MPU6050_AXIS_Y] = sensor_data.y * MPU6050_DEFAULT_LSB / MPU6050_FS_MAX_LSB;
   	   	st->gyro_bias[0] = sensor_data.x * 33 / 131;
           	st->gyro_bias[1] = sensor_data.y * 33 / 131;
          	st->gyro_bias[2] = sensor_data.z * 33 / 131;      
//		st->gyro_bias[0] = sensor_data.x ;
 //          	st->gyro_bias[1] = sensor_data.y;
 //         	st->gyro_bias[2] = sensor_data.z;      
	#endif
        break;

    case GYROSCOPE_IOCTL_CLR_CALI:
		st->gyro_bias[0] = 0;
		st->gyro_bias[1] = 0;
		st->gyro_bias[2] = 0; 
        break;

    case GYROSCOPE_IOCTL_GET_CALI:
        data = (void __user*)arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }
/*
        sensor_data.x = st->gyro_bias[0] * 131 / 33;
        sensor_data.y = st->gyro_bias[1] * 131 / 33;
        sensor_data.z = st->gyro_bias[2] * 131 / 33;
*/		
	 sensor_data.x = st->gyro_bias[0] ;
        sensor_data.y = st->gyro_bias[1] ;
        sensor_data.z = st->gyro_bias[2] ;
        if (copy_to_user(data, &sensor_data, sizeof(sensor_data)))
        {
            err = -EFAULT;
            break;
        }

        break;

#if INV_GYRO_AUTO_CALI==1    
    case GYROSCOPE_IOCTL_READ_SENSORDATA_RAW:
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        break;
        
    case GYROSCOPE_IOCTL_READ_TEMPERATURE:
        data = (void __user *) arg;
		
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        break;

    case GYROSCOPE_IOCTL_GET_POWER_STATUS:
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        if (copy_to_user(data, strbuf, sizeof(strbuf)))
        {
            err = -EFAULT;
            break;    
        }
        break;        
#endif

    default:
        err = -ENOIOCTLCMD;
        break;          
    }
    return err;
}

#ifdef CONFIG_COMPAT
static long icm20608d_gyro_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	void __user *arg32 = compat_ptr(arg);
	
	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;
	
    //printk("akm8963_compat_ioctl arg: 0x%lx, arg32: 0x%p\n",arg, arg32);
	
	switch (cmd) {
		 case COMPAT_GYROSCOPE_IOCTL_INIT:
		 	 //printk("akm8963_compat_ioctl COMPAT_ECS_IOCTL_WRITE\n");
			 if(arg32 == NULL)
			 {
		//		 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_INIT,
							(unsigned long)arg32);
			 if (ret){
//			 	GYRO_ERR("GYROSCOPE_IOCTL_INIT unlocked_ioctl failed.");
				return ret;
			 }			 

			 break;

		 case COMPAT_GYROSCOPE_IOCTL_SET_CALI:
			 if(arg32 == NULL)
			 {
//				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_SET_CALI,
							(unsigned long)arg32);
			 if (ret){
//			 	GYRO_ERR("GYROSCOPE_IOCTL_SET_CALI unlocked_ioctl failed.");
				return ret;
			 }			 

			 break;

		 case COMPAT_GYROSCOPE_IOCTL_CLR_CALI:
			 if(arg32 == NULL)
			 {
//				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_CLR_CALI,
							(unsigned long)arg32);
			 if (ret){
//			 	GYRO_ERR("GYROSCOPE_IOCTL_CLR_CALI unlocked_ioctl failed.");
				return ret;
			 }			 

			 break;

		 case COMPAT_GYROSCOPE_IOCTL_GET_CALI:
			 if(arg32 == NULL)
			 {
//				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_GET_CALI,
							(unsigned long)arg32);
			 if (ret){
//			 	GYRO_ERR("GYROSCOPE_IOCTL_GET_CALI unlocked_ioctl failed.");
				return ret;
			 }			 

			 break;

		 case COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA:
			 if(arg32 == NULL)
			 {
//				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_READ_SENSORDATA,
							(unsigned long)arg32);
			 if (ret){
//			 	GYRO_ERR("GYROSCOPE_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
				return ret;
			 }			 

			 break;	
			 
		 default:
			 printk(KERN_ERR "%s not supported = 0x%04x", __FUNCTION__, cmd);
			 return -ENOIOCTLCMD;
			 break;
	}
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static struct file_operations icm20608d_gyro_fops = {
    .open = icm20608d_gyro_open,
    .release = icm20608d_gyro_release,
	.unlocked_ioctl = icm20608d_gyro_unlocked_ioctl,
#ifdef CONFIG_COMPAT
			.compat_ioctl = icm20608d_gyro_compat_ioctl,
#endif
};
/*----------------------------------------------------------------------------*/
static struct miscdevice icm20608d_gyroscope_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "gyroscope",
    .fops = &icm20608d_gyro_fops,
};

#endif

#if 1
static int icm20608d_gsensor_open(struct inode *inode, struct file *file)
{
   

    if (file->private_data == NULL)
    {
 //       GSE_ERR("null pointer!!\n");
        return -EINVAL;
    }
    return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int icm20608d_gsensor_release(struct inode *inode, struct file *file)
{
    file->private_data = NULL;
    return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_COMPAT
static long icm20608d_gsensor_compat_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
    long err = 0;

	void __user *arg32 = compat_ptr(arg);
	
	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;
	
    switch (cmd)
    {
        case COMPAT_GSENSOR_IOCTL_READ_SENSORDATA:
            if (arg32 == NULL)
            {
                err = -EINVAL;
                break;    
            }
		
		    err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_READ_SENSORDATA, (unsigned long)arg32);
		    if (err){
//		        GSE_ERR("GSENSOR_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
		        return err;
		    }
        break;
        case COMPAT_GSENSOR_IOCTL_SET_CALI:
            if (arg32 == NULL)
            {
                err = -EINVAL;
                break;    
            }
		
		    err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_SET_CALI, (unsigned long)arg32);
		    if (err){
	//	        GSE_ERR("GSENSOR_IOCTL_SET_CALI unlocked_ioctl failed.");
		        return err;
		    }
        break;
        case COMPAT_GSENSOR_IOCTL_GET_CALI:
            if (arg32 == NULL)
            {
                err = -EINVAL;
                break;    
            }
		
		    err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_GET_CALI, (unsigned long)arg32);
		    if (err){
	//	        GSE_ERR("GSENSOR_IOCTL_GET_CALI unlocked_ioctl failed.");
		        return err;
		    }
        break;
        case COMPAT_GSENSOR_IOCTL_CLR_CALI:
            if (arg32 == NULL)
            {
                err = -EINVAL;
                break;    
            }
		
		    err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_CLR_CALI, (unsigned long)arg32);
		    if (err){
	//	        GSE_ERR("GSENSOR_IOCTL_CLR_CALI unlocked_ioctl failed.");
		        return err;
		    }
        break;

        default:
   //         GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
            err = -ENOIOCTLCMD;
        break;

    }

    return err;
}
#endif

static long icm20608d_gsensor_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct inv_mpu_state *st = iio_priv(mtk_indio_dev);


    char strbuf[256] = {0};
    void __user *data;
   SENSOR_DATA sensor_data;
    long err = 0;
    int cali[3] = {0};
    short accel[3] = {0};
    int accel_fact[3] = {0};
    gsensor_gain.x = 1;
    gsensor_gain.y = 1;
    gsensor_gain.z = 1;

    if (_IOC_DIR(cmd) & _IOC_READ)
    {
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
    {
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }

    if (err)
    {
   //     GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
        return -EFAULT;
    }

    switch (cmd)
    {
    case GSENSOR_IOCTL_INIT:
 		printk("inv GSENSOR_IOCTL_INIT\n");
    
        break;

    case GSENSOR_IOCTL_READ_CHIPINFO:
				printk("inv GSENSOR_IOCTL_READ_CHIPINFO\n");
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }
    	 sprintf(strbuf, "MPU6515 Chip");
        if (copy_to_user(data, strbuf, strlen(strbuf)+1))
        {
            err = -EFAULT;
            break;
        }
        break;    

    case GSENSOR_IOCTL_READ_SENSORDATA:
	if (!st->chip_config.accel_enable)
	{
		printk("Inv accel enable in factory mode\n");
		inv_accel_enable(st);
		st->chip_config.accel_enable = 1;
	}
 
	inv_get_accel_data(st, accel);	
//	accel_fact[0] = accel[0];
//	accel_fact[1] = accel[1];
//	accel_fact[2] = accel[2];

//	accel_fact[0] = (-1)*accel_fact[0]*4*9807/65536;
//	accel_fact[1] = accel_fact[1]*4*9807/65536;
//	accel_fact[2] = (-1)*accel_fact[2]*4*9807/65536;
	accel_fact[0] = (-1)*accel[0]*4*9807/65536;
	accel_fact[1] = accel[1]*4*9807/65536;
	accel_fact[2] = (-1)*accel[2]*4*9807/65536;
	
	 sprintf(strbuf, "%04x %04x %04x", accel_fact[0], accel_fact[1], accel_fact[2]);

        data = (void __user *)arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        if (copy_to_user(data, strbuf, strlen(strbuf)+1))
        {
            err = -EFAULT;
            break;    
        }
        break;

    case GSENSOR_IOCTL_READ_GAIN:
	printk("inv GSENSOR_IOCTL_READ_GAIN\n");
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        if (copy_to_user(data, &gsensor_gain, sizeof(GSENSOR_VECTOR3D)))
        {
            err = -EFAULT;
            break;
        }

        break;

    case GSENSOR_IOCTL_READ_RAW_DATA:
	printk("Inv GSENSOR_IOCTL_READ_RAW_DATA\n"); 		
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

	inv_get_accel_raw_data(st, accel);	
//	printk( "inv raw accel %d %d %d\n", accel[0], accel[1], accel[2]);
//	printk( "inv raw accel %04x %04x %04x\n", accel[0], accel[1], accel[2]);
//	accel[0] = accel[0] *4*9.8*1000/65536;
//	accel[0] = accel[0] *4*9.8*1000/65536;
//	accel[0] = accel[0] *4*9.8*1000/65536;


	 sprintf(strbuf, "%04x %04x %04x", accel[0], accel[1], accel[2]);	

        if (copy_to_user(data, strbuf, strlen(strbuf)+1))
        {
            err = -EFAULT;
            break;    
        }	

        break;    

    case GSENSOR_IOCTL_SET_CALI:
	printk("Inv GSENSOR_IOCTL_SET_CALI\n");
        data = (void __user*)arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        if (copy_from_user(&sensor_data, data, sizeof(sensor_data)))
        {
            err = -EFAULT;
            break;    
        }	
/*
	st->accel_bias[0] = sensor_data.x;
	st->accel_bias[1] = sensor_data.y;
	st->accel_bias[2] = sensor_data.z;
*/
 //       printk("huangzifan  get calib x =%d,y = %d z = %d\n",sensor_data.x, sensor_data.y,sensor_data.z);
#if 0
	gs_calib[0] = sensor_data.x*16384/9807;
	gs_calib[1] = sensor_data.y*16384/9807;
	gs_calib[2] = sensor_data.z*16384/9807;
	st->accel_bias[0] =gs_calib[0];
	st->accel_bias[1] =(-1)* gs_calib[1];
	st->accel_bias[2] = gs_calib[2];
	
	printk("huangzifan  set calib x =%d,y = %d z = %d\n",st->accel_bias[0],st->accel_bias[1],st->accel_bias[2]);
#endif
        break;

    case GSENSOR_IOCTL_CLR_CALI:
#if 0
	printk("huangzifan_inv 6\n");
	st->accel_bias[0] = 0;
	st->accel_bias[1] = 0;
	st->accel_bias[2] = 0;
#endif
        break;

    case GSENSOR_IOCTL_GET_CALI:
	 printk("inv GSENSOR_IOCTL_GET_CALI \n");
        data = (void __user*)arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }
#if 0
	 printk("huangzifan_gsensor GET_CALI !\n");
        sensor_data.x = st->accel_bias[0]*9807/16384;
        sensor_data.y = (-1)*st->accel_bias[1]*9807/16384;
        sensor_data.z = st->accel_bias[2]*9807/16384;
        printk("huangzifan  get calib x =%d,y = %d z = %d\n",sensor_data.x, sensor_data.y,sensor_data.z);
#endif		
        if (copy_to_user(data, &sensor_data, sizeof(sensor_data)))
        {
            err = -EFAULT;
            break;
        }
        
        break;


    default:
	       printk("unknown IOCTL: 0x%08x\n", cmd);
        err = -ENOIOCTLCMD;
        break;

    }

    return err;
}

static struct file_operations icm20608d_gsensor_fops = {
    .open = icm20608d_gsensor_open,
    .release = icm20608d_gsensor_release,
    .unlocked_ioctl = icm20608d_gsensor_unlocked_ioctl,
    #ifdef CONFIG_COMPAT
	.compat_ioctl = icm20608d_gsensor_compat_ioctl,
	#endif
};

static struct miscdevice icm20608d_gsensor_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "gsensor",
    .fops = &icm20608d_gsensor_fops,
};

#endif


#if 0
int inv_accel_gyro_calibration(struct inv_mpu_state *st)
{
	int result;
	int gyro_bias_regular[THREE_AXIS];
	int accel_bias_regular[THREE_AXIS];
	int test_times, i;

	result = inv_power_up_self_test(st);
	if (result)
		return result;
	result = inv_reset_offset_reg(st, true);
	if (result)
		return result;

	test_times = DEF_ST_TRY_TIMES;
	while (test_times > 0) {
		result = inv_do_test(st, 0, gyro_bias_regular,
			accel_bias_regular);
		if (result == -EAGAIN)
			test_times--;
		else
			test_times = 0;
	}
	if (result)
		return -1;
	pr_debug("%s self_test accel bias_regular - %+d %+d %+d\n",
		 st->hw->name, accel_bias_regular[0],
		 accel_bias_regular[1], accel_bias_regular[2]);
	pr_debug("%s self_test gyro bias_regular - %+d %+d %+d\n",
		 st->hw->name, gyro_bias_regular[0], gyro_bias_regular[1],
		 gyro_bias_regular[2]);

	for (i = 0; i < 3; i++) {
		st->gyro_bias[i] = gyro_bias_regular[i]/1000;
		st->accel_bias[i] = accel_bias_regular[i]/1000;
	}

	if (st->accel_bias[2] < 0)
		st->accel_bias[2] += INV_ACCEL_SENSITIVITY; 
	else
		st->accel_bias[2] -= INV_ACCEL_SENSITIVITY; 

	inv_recover_setting(st);
	msleep(50);
		inv_power_up_self_test(st);


	return 0;
}
#endif


#ifdef ACC_DYNAMIC_CALIBRATE
#define INV_ACCEL_SENSITIVITY 16384
#define CAL_READ_TIMES 30
void inv_accel_enable(struct inv_mpu_state *st)
{
	inv_i2c_single_write(st, 0x6C, 0x00);
	inv_i2c_single_write(st, 0x6B, 0x00);

}

int inv_get_accel_raw_data(struct inv_mpu_state *st, short *acc)
{
	int result = 0;
	unsigned char accel[6];

	result = inv_i2c_read(st, st->reg.raw_accel, 6, accel);
	if (result < 0) {
		printk("i2c read error\n");
		return -1;
	}
	
	acc[0] = (short)(accel[0] << 8 | accel[1] );
	acc[1] = (short)(accel[2] << 8 | accel[3] );
	acc[2] = (short)(accel[4] << 8 | accel[5] );

	return 0;
}

int inv_get_accel_data(struct inv_mpu_state *st, short *acc)
{

	int result = 0;
	u8 accel[6] = {0};
	s16 temp = {0};
       
	result = inv_i2c_read(st, st->reg.raw_accel, 6, accel);
	if (result < 0) {
		printk("i2c read error\n");
		return -1;
	}
	
        /* write then burst read */
	acc[0] = (s16)( (accel[0] << 8) | accel[1] );
	acc[1] = (s16)( (accel[2] << 8) | accel[3] );
	acc[2] = (s16)( (accel[4] << 8) | accel[5] );
 //	printk( "inv inv_get_accel_data %d %d %d\n", accel[0], accel[1], accel[2]);

	acc[0] -= st->accel_bias[0];
	acc[1] -= st->accel_bias[1];
	acc[2] -= st->accel_bias[2];
//	printk("huangzifan  inv_get_accel_data bias x =%d,y = %d z = %d\n",st->accel_bias[0],st->accel_bias[1],st->accel_bias[2]);

	return 0;
}

int inv_get_gyro_data(struct inv_mpu_state *st, short *gyr)
{

	int result = 0;
	u8 gyroscope[6] = {0};
	s16 temp = {0};
       
	result = inv_i2c_read(st, 0x43, 6, gyr);
	if (result < 0) {
		printk("i2c read error\n");
		return -1;
	}
	
        /* write then burst read */
	gyroscope[0] = (s16)( (gyr[0] << 8) | gyr[1] );
	gyroscope[1] = (s16)( (gyr[2] << 8) | gyr[3] );
	gyroscope[2] = (s16)( (gyr[4] << 8) | gyr[5] );

	gyroscope[0] -= st->gyro_bias[0];
	gyroscope[1] -= st->gyro_bias[1];
	gyroscope[2] -= st->gyro_bias[2];
	
	return 0;
}

void inv_set_accel_offset_reg(struct inv_mpu_state *st, int axis, int data)
{
	const u8 *ch;
//	printk("inv pre data is 0 axis = %d, st->input_accel_offset =%d \n",axis,st->input_accel_offset[axis]);
		if ((data > MPU_MAX_A_OFFSET_VALUE) ||
			(data < MPU_MIN_A_OFFSET_VALUE))
			return -EINVAL;
	printk("inv pre data is 1 axis = %d, st->input_accel_offset =%d \n",axis,st->input_accel_offset[axis]);
		if (INV_MPU6050 == st->chip_type)
			ch = reg_6050_accel_offset;
		else
			ch = reg_6500_accel_offset;

		inv_set_offset_reg(st, ch[axis],
			st->rom_accel_offset[axis] + (data << 1));
		st->input_accel_offset[axis] = data;
	printk("inv pre data is 2 axis = %d, st->input_accel_offset =%d \n",axis,st->input_accel_offset[axis]);
}

static int calibration_operation_judge(int x, int y,int z)
{
	x = x*4*9807/65536;
	y = y*4*9807/65536;
	z = z*4*9807/65536;

	printk("inv calibration_operation_judge x = %d, y= %d, z=%d\n",x,y,z);
	if(  x > 2500 || x < (-2500))
		return -1;
	if(  y > 2500 || y < (-2500))
		return -2;
//	if(  z < 8000 || z > 14000)
//		return -3;
	return 0;
}

int inv_accel_calibration(struct inv_mpu_state *st)
{
	int result = 0;
	int sum[3];
	short x, y, z;
	unsigned char accel[6];
	unsigned char gyro[6];
	unsigned char state[2];
	int i = 0;

	memset(sum, 0, sizeof(sum));
	st->accel_bias[0] = 0; 
	st->accel_bias[1] = 0; 
	st->accel_bias[2] = 0; 

	
	inv_i2c_read(st, 0x6B, 2, state);

	printk("inv pre data is 0x%x 0x%x\n", state[0], state[1]);
	inv_i2c_single_write(st, 0x6B, 0);
	inv_i2c_single_write(st, 0x6C, 0x07);

	msleep(100);

	inv_reset_offset_reg(st, 1);

	printk("inv i2c address is 0x%x\n", st->i2c_addr);

	for (i = 0; i < 20; i++) {
		result = inv_i2c_read(st, st->reg.raw_accel, 6, accel);
		if (result < 0)
			printk("i2c read error\n");
		
		x = (short)(accel[0] << 8 | accel[1] );
		y = (short)(accel[2] << 8 | accel[3] );
		z = (short)(accel[4] << 8 | accel[5] );

		printk("inv: accel %d time read x y z is %d %d %d\n", i, x, y, z);

		msleep(40);
	}

	
	for (i = 0; i < CAL_READ_TIMES; i++) {
		result = inv_i2c_read(st, st->reg.raw_accel, 6, accel);
		if (result < 0)
			printk("i2c read error\n");
		
		x = (short)(accel[0] << 8 | accel[1] );
		y = (short)(accel[2] << 8 | accel[3] );
		z = (short)(accel[4] << 8 | accel[5] );

		printk("inv: accel %d time read x y z is %d %d %d\n", i, x, y, z);
		sum[0] += x;
		sum[1] += y;
		sum[2] += z;
		msleep(40);
	}
	
	for(i = 0; i < 3; i++) {
		sum[i] = sum[i]/CAL_READ_TIMES;
	}

//Add by huangzifan at 2016.1.14. Add calibration operation judage.
	result = calibration_operation_judge(sum[0],sum[1],sum[2]);
	if( result < 0){
		printk("inv:calibration operation is wrong,error = %d\n",result);
		return -1;
	}
	

	for(i = 0; i < 3; i++) {
		st->accel_bias[i] = sum[i];
		printk("inv sum[%d] is %d\n", i, sum[i]);
	}


	if (st->accel_bias[2] < 0)
		st->accel_bias[2] += INV_ACCEL_SENSITIVITY; 
	else
		st->accel_bias[2] -= INV_ACCEL_SENSITIVITY; 

	printk("inv: accel calibration is %d %d %d\n", st->accel_bias[0], st->accel_bias[1], st->accel_bias[2]);

	for(i=0; i< 3; i++)
	{
		st->accel_bias[i] = (-1)*(st->accel_bias[i] >> 4);
		inv_set_accel_offset_reg(st, i, st->accel_bias[i] );
	}

	inv_i2c_single_write(st, 0x6B, state[0]);
	inv_i2c_single_write(st, 0x6C, state[1]);
		
	return 0;
	


}


int inv_gyro_calibration(struct inv_mpu_state *st)
{
#if 0
	int result = 0;
	int sum[3];
	short x, y, z;
	unsigned char gyro[6];
	unsigned char state[2];
	int i = 0;

	memset(sum, 0, sizeof(sum));
	st->gyro_bias[0] = 0; 
	st->gyro_bias[1] = 0; 
	st->gyro_bias[2] = 0; 
	
	inv_i2c_read(st, 0x6B, 2, state);

	printk("inv pre data is 0x%x 0x%x\n", state[0], state[1]);
	inv_i2c_single_write(st, 0x6B, 0);
	inv_i2c_single_write(st, 0x6C, 0x38);
	msleep(100);
	printk("inv i2c address is 0x%x\n", st->i2c_addr);
	for (i = 0; i < CAL_READ_TIMES; i++) {
		result = inv_i2c_read(st, 0x43, 6, gyro);
		if (result < 0)
			printk("i2c read error\n");
		
		x = (short)(gyro[0] << 8 | gyro[1] );
		y = (short)(gyro[2] << 8 | gyro[3] );
		z = (short)(gyro[4] << 8 | gyro[5] );

		printk("inv: gyro %d time read x y z is %d %d %d\n", i, x, y, z);
		sum[0] += x;
		sum[1] += y;
		sum[2] += z;
		msleep(40);
	}

	for(i = 0; i < 3; i++) {
		st->gyro_bias[i] = sum[i]/CAL_READ_TIMES;
		printk("inv sum[%d] is %d\n", i, sum[i]);
	}

	printk("inv: gyrol calibration is %d %d %d\n", st->gyro_bias[0], st->gyro_bias[1], st->gyro_bias[2]);
	inv_i2c_single_write(st, 0x6B, state[0]);
	inv_i2c_single_write(st, 0x6C, state[1]);
#endif
	return 0;

}


static ssize_t icm20608d_store_acc_calibration(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct inv_mpu_state *st;

	int accel_bias[3];
	int gyro_bias[3];
	int result = 0;
	int status1 = 0;

	memset(accel_bias, 0, sizeof(accel_bias));

	st = iio_priv(mtk_indio_dev);
	if(1 == sscanf(buf, "%d", &status1))
	{ 
		if(1== status1){
			result = inv_accel_calibration(st);
			if (result < 0)
				printk(KERN_ERR"Inv accel and gyro calibration failed\n");
		}
	}

	return 0;    
	
}

static ssize_t icm20608d_show_acc_calibration(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st;
	int accel_bias[3];
	int gyro_bias[3];
	int result = 0;

	st = iio_priv(mtk_indio_dev);

	memset(accel_bias, 0, sizeof(accel_bias));

	result = inv_accel_calibration(st);

	if (result < 0)
		printk(KERN_ERR"Inv accel and gyro calibration failed\n");
	

//	return sprintf(buf, "INV: accel %d %d %d gyro %d %d %d\n", st->accel_bias[0],
//		st->accel_bias[1], st->accel_bias[2], st->gyro_bias[0], st->gyro_bias[1], st->gyro_bias[2]);
	return sprintf(buf, "INV: accel %d %d %d\n", st->accel_bias[0],st->accel_bias[1], st->accel_bias[2]);

}


static ssize_t icm20608d_show_acc_x_calibbias(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st;

	st = iio_priv(mtk_indio_dev);
	return sprintf(buf, "%d\n",st->accel_bias[0]);
}

static ssize_t icm20608d_show_acc_y_calibbias(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st;

	st = iio_priv(mtk_indio_dev);
	return sprintf(buf, "%d\n",st->accel_bias[1]);
}

static ssize_t icm20608d_show_acc_z_calibbias(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st;

	st = iio_priv(mtk_indio_dev);
	return sprintf(buf, "%d\n",st->accel_bias[2]);
}

static ssize_t icm20608d_store_acc_x_offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct inv_mpu_state *st;
	int result;
	int data;
	
	st = iio_priv(mtk_indio_dev);
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;

	acc_cali_temp[0] = data;

	return size;    
	
}

static ssize_t icm20608d_store_acc_y_offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct inv_mpu_state *st;
	int result;
	int data;
	
	st = iio_priv(mtk_indio_dev);
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	
	acc_cali_temp[1] = data;

	return size;    
}

static ssize_t icm20608d_store_acc_z_offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct inv_mpu_state *st;
	int result;
	int data;
	
	st = iio_priv(mtk_indio_dev);
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	
	acc_cali_temp[2] = data;
	if( acc_cali_temp[2] == 0 && acc_cali_temp[1] == 0 && acc_cali_temp[0] == 0) {
		return size;
	}
	inv_set_accel_offset_reg(st, 0, acc_cali_temp[0] );
	inv_set_accel_offset_reg(st, 1, acc_cali_temp[1] );
	inv_set_accel_offset_reg(st, 2, acc_cali_temp[2] );
	st->accel_bias[0] = acc_cali_temp[0];
	st->accel_bias[1] = acc_cali_temp[1];
	st->accel_bias[2] = acc_cali_temp[2];
	bsp_calib_flag = 1;
	printk("Inv icm20608d_store_acc_z_offset finished\n");
	return size;
	 
}

static ssize_t icm20608d_show_gyro_calibration(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st;
	int accel_bias[3];
	int gyro_bias[3];
	int result = 0;

	st = iio_priv(mtk_indio_dev);

	memset(gyro_bias, 0, sizeof(gyro_bias));


	result = inv_gyro_calibration(st);

	if (result < 0)
		printk(KERN_ERR"Inv accel and gyro calibration failed\n");
	
	return sprintf(buf, "INV: accel %d %d %d\n", st->gyro_bias[0],st->gyro_bias[1], st->gyro_bias[2]);

}

static ssize_t icm20608d_show_gyro_x_calibbias(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st;

	st = iio_priv(mtk_indio_dev);
	return sprintf(buf, "%d\n",st->gyro_bias[0]);
}

static ssize_t icm20608d_show_gyro_y_calibbias(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st;

	st = iio_priv(mtk_indio_dev);
	return sprintf(buf, "%d\n",st->gyro_bias[1]);
}

static ssize_t icm20608d_show_gyro_z_calibbias(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st;

	st = iio_priv(mtk_indio_dev);
	return sprintf(buf, "%d\n",st->gyro_bias[2]);
}

static ssize_t icm20608d_store_gyro_x_offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct inv_mpu_state *st;
	int result;
	int data;
	
	st = iio_priv(mtk_indio_dev);
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;

	st->gyro_bias[0] = data;
	return size;    
	
}

static ssize_t icm20608d_store_gyro_y_offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct inv_mpu_state *st;
	int result;
	int data;
	
	st = iio_priv(mtk_indio_dev);
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;

	st->gyro_bias[1] = data;
	return size;    
}

static ssize_t icm20608d_store_gyro_z_offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct inv_mpu_state *st;
	int result;
	int data;
	
	st = iio_priv(mtk_indio_dev);
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;

	st->gyro_bias[2] = data;
	return size;    
}

static int write_block(char * buf, int len)
{
    struct file *f;
    mm_segment_t fs;
    int res;
     char *buffer = buf;
    f = filp_open(STORE_FILE, O_RDWR, 0644);
    if (IS_ERR(f)) {
        printk("%s: write_block open file %s error!\n", __func__, STORE_FILE);

        return -1;
    }
    f->f_pos=BASE_ADDR+POS;
    fs = get_fs(); set_fs(KERNEL_DS);
    printk("set value = %d\n",*buffer);
    res = f->f_op->write(f, buffer, len, &f->f_pos);
    set_fs(fs);
    filp_close(f, NULL);
    if ( res < 0){
		printk("write_block error ret =%d\n", res);

    	}
    return res;
}

static int read_block(int len)
{
    struct file *f;
    mm_segment_t fs;
    int ret;
    int *value = NULL;
    f = filp_open(STORE_FILE, O_RDWR, 0);
    if (IS_ERR(f)) {
        printk("read_block open file %s error!num =%ld\n", STORE_FILE,IS_ERR(f));
        return -1;
    }
    fs = get_fs(); set_fs(KERNEL_DS);
    f->f_pos=BASE_ADDR+POS;
    ret = f->f_op->read(f, &acc_gyr_calib, len, &f->f_pos);
    set_fs(fs);
    filp_close(f, NULL);
    if (ret < 0 ) {
        printk("%s: read_block read file %s error!\n", __func__, STORE_FILE);
        ret = -1;
    }
    return ret;
}

static ssize_t icm20608d_show_acc_gyro_factory_test_calibration(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st;
	int accel_bias[3];
	int gyro_bias[3];
	int result = 0;
	

	st = iio_priv(mtk_indio_dev);

	memset(accel_bias, 0, sizeof(accel_bias));
	memset(gyro_bias, 0, sizeof(gyro_bias));
	memset(&acc_gyr_calib, 0, sizeof(acc_gyr_calib));

	printk("INV_block0-0: printk accel %d %d %d Gyr:  %d %d %d \n", accel_bias[0],accel_bias[1], accel_bias[2],gyro_bias[0],gyro_bias[1],gyro_bias[2]);
//	printk("INV_block0-1: printk accel %d %d %d Gyr:  %d %d %d \n",  st->accel_bias[0], st->accel_bias[1],  st->accel_bias[2], st->gyro_bias[0], st->gyro_bias[1], st->gyro_bias[2]);
		
	result = inv_accel_calibration(st);
	if (result < 0){
		printk(KERN_ERR"Inv accel  calibration failed\n");
	}
	result = inv_gyro_calibration(st);
	if (result < 0){
		printk(KERN_ERR"Inv gyro calibration failed\n");
	}
	acc_gyr_calib.acc_fact_calib[0] = st->accel_bias[0];
	acc_gyr_calib.acc_fact_calib[1] = st->accel_bias[1];
	acc_gyr_calib.acc_fact_calib[2] = st->accel_bias[2];
	acc_gyr_calib.gyr_fact_calib[0] = st->gyro_bias[0];
	acc_gyr_calib.gyr_fact_calib[1] = st->gyro_bias[1];
	acc_gyr_calib.gyr_fact_calib[2] = st->gyro_bias[2];
	printk("INV_block1: accel %d %d %d Gyr:  %d %d %d \n",acc_gyr_calib.acc_fact_calib[0],acc_gyr_calib.acc_fact_calib[1],
		acc_gyr_calib.acc_fact_calib[2],acc_gyr_calib.gyr_fact_calib[0],acc_gyr_calib.gyr_fact_calib[1],acc_gyr_calib.gyr_fact_calib[2]);
	
	result = write_block(&acc_gyr_calib,sizeof(acc_gyr_calib));
	if( result < 0){
				  printk("icm20608d_show_acc_gyro_factory_calibration write error\n");
				  return -1;
	}
	acc_gyr_calib.acc_fact_calib[0] = 0;
	acc_gyr_calib.acc_fact_calib[1] = 0;
	acc_gyr_calib.acc_fact_calib[2] = 0;
	acc_gyr_calib.gyr_fact_calib[0] = 0;
	acc_gyr_calib.gyr_fact_calib[1] = 0;
	acc_gyr_calib.gyr_fact_calib[2] = 0;
	result = read_block( sizeof(acc_gyr_calib));
	if( result < 0){
				  printk("icm20608d_show_acc_gyro_factory_calibration read error\n");
				  return -1;
	}
	accel_bias[0] = acc_gyr_calib.acc_fact_calib[0]; 
	accel_bias[1] = acc_gyr_calib.acc_fact_calib[1]; 
	accel_bias[2] = acc_gyr_calib.acc_fact_calib[2]; 
	gyro_bias[0] = acc_gyr_calib.gyr_fact_calib[0]; 
	gyro_bias[1] = acc_gyr_calib.gyr_fact_calib[1]; 
	gyro_bias[2] = acc_gyr_calib.gyr_fact_calib[2]; 
	
	printk("INV_block2: printk accel %d %d %d Gyr:  %d %d %d \n", accel_bias[0],accel_bias[1], accel_bias[2],gyro_bias[0],gyro_bias[1],gyro_bias[2]);

	return sprintf(buf, "INV_block2: accel %d %d %d Gyr:  %d %d %d \n", accel_bias[0],accel_bias[1], accel_bias[2],gyro_bias[0],gyro_bias[1],gyro_bias[2]);

}

static ssize_t icm20608d_show_acc_gyro_factory_calibration(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st;
	int accel_bias[3];
	int gyro_bias[3];
	int result = 0;
	

	st = iio_priv(mtk_indio_dev);

	memset(accel_bias, 0, sizeof(accel_bias));
	memset(gyro_bias, 0, sizeof(gyro_bias));
	memset(&acc_gyr_calib, 0, sizeof(acc_gyr_calib));

	printk("INV_block0-0: printk accel %d %d %d Gyr:  %d %d %d \n", accel_bias[0],accel_bias[1], accel_bias[2],gyro_bias[0],gyro_bias[1],gyro_bias[2]);
	printk("INV_block0-1: printk accel %d %d %d Gyr:  %d %d %d \n",  st->accel_bias[0], st->accel_bias[1],  st->accel_bias[2], st->gyro_bias[0], st->gyro_bias[1], st->gyro_bias[2]);
		
	result = inv_accel_calibration(st);
	if (result < 0){
		printk(KERN_ERR"Inv accel  calibration failed\n");
		return -1;
		
	}
	result = inv_gyro_calibration(st);
	if (result < 0){
		printk(KERN_ERR"Inv gyro calibration failed\n");
	}
	acc_gyr_calib.acc_fact_calib[0] = st->accel_bias[0];
	acc_gyr_calib.acc_fact_calib[1] = st->accel_bias[1];
	acc_gyr_calib.acc_fact_calib[2] = st->accel_bias[2];
//	acc_gyr_calib.gyr_fact_calib[0] = st->gyro_bias[0];
//	acc_gyr_calib.gyr_fact_calib[1] = st->gyro_bias[1];
//	acc_gyr_calib.gyr_fact_calib[2] = st->gyro_bias[2];
	printk("INV_block1: accel %d %d %d Gyr:  %d %d %d \n",acc_gyr_calib.acc_fact_calib[0],acc_gyr_calib.acc_fact_calib[1],
		acc_gyr_calib.acc_fact_calib[2],acc_gyr_calib.gyr_fact_calib[0],acc_gyr_calib.gyr_fact_calib[1],acc_gyr_calib.gyr_fact_calib[2]);
	
	result = write_block(&acc_gyr_calib,sizeof(acc_gyr_calib));
	if( result < 0){
				  printk("icm20608d_show_acc_gyro_factory_calibration write error\n");
				  return -1;
	}
	#if 0
	acc_gyr_calib.acc_fact_calib[0] = 0;
	acc_gyr_calib.acc_fact_calib[1] = 0;
	acc_gyr_calib.acc_fact_calib[2] = 0;
	acc_gyr_calib.gyr_fact_calib[0] = 0;
	acc_gyr_calib.gyr_fact_calib[1] = 0;
	acc_gyr_calib.gyr_fact_calib[2] = 0;
	result = read_block( sizeof(acc_gyr_calib));
	if( result < 0){
				  printk("icm20608d_show_acc_gyro_factory_calibration read error\n");
				  return -1;
	}
	accel_bias[0] = acc_gyr_calib.acc_fact_calib[0]; 
	accel_bias[1] = acc_gyr_calib.acc_fact_calib[1]; 
	accel_bias[2] = acc_gyr_calib.acc_fact_calib[2]; 
	gyro_bias[0] = acc_gyr_calib.gyr_fact_calib[0]; 
	gyro_bias[1] = acc_gyr_calib.gyr_fact_calib[1]; 
	gyro_bias[2] = acc_gyr_calib.gyr_fact_calib[2]; 
	
	printk("INV_block2: printk accel %d %d %d Gyr:  %d %d %d \n", accel_bias[0],accel_bias[1], accel_bias[2],gyro_bias[0],gyro_bias[1],gyro_bias[2]);
	#endif
	return sprintf(buf, "INV_block2: accel %d %d %d Gyr:  %d %d %d \n", st->accel_bias[0],st->accel_bias[1], st->accel_bias[2],st->gyro_bias[0],st->gyro_bias[1],st->gyro_bias[2]);

}

//Add and modify by huangzifan at 2016.1.25. For gsensor calibration. 
static ssize_t icm20608d_show_acc_gyro_factory_read_calibbias(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int err = 0;

	err = init_factory_calibration();
	if (err < 0)
    	{
        	printk("init_factory_calibration failed!\n");
    	}

     	printk("factory_read_calibbias!\n");

	return sprintf(buf, "factory_read_calibbias: result =  %d\n",err );

}

static ssize_t icm20608d_store_acc_gyro_factory_read_calibbias(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int err = 0;
	err = init_factory_calibration();
	if (err < 0)
    	{
        	printk("init_factory_calibration store failed!\n");
    	}
     	printk("factory_read_calibbias store!\n");

	return sprintf(buf, "factory_read_calibbias:store  result =  %d\n",err );
}

//Add and modify by huangzifan. For gsensor calibration. 
static DEVICE_ATTR(acc_calibration, S_IWUSR | S_IRUGO,icm20608d_show_acc_calibration, icm20608d_store_acc_calibration);
static DEVICE_ATTR(acc_x_calibbias, S_IWUSR | S_IRUGO,icm20608d_show_acc_x_calibbias, NULL);
static DEVICE_ATTR(acc_y_calibbias, S_IWUSR | S_IRUGO,icm20608d_show_acc_y_calibbias, NULL);
static DEVICE_ATTR(acc_z_calibbias, S_IWUSR | S_IRUGO,icm20608d_show_acc_z_calibbias, NULL);
static DEVICE_ATTR(acc_x_offset, S_IWUSR | S_IRUGO,NULL, icm20608d_store_acc_x_offset);
static DEVICE_ATTR(acc_y_offset, S_IWUSR | S_IRUGO,NULL, icm20608d_store_acc_y_offset);
static DEVICE_ATTR(acc_z_offset, S_IWUSR | S_IRUGO,NULL, icm20608d_store_acc_z_offset);
static DEVICE_ATTR(gyro_calibration, S_IWUSR | S_IRUGO,icm20608d_show_gyro_calibration, NULL);
static DEVICE_ATTR(gyro_x_calibbias, S_IWUSR | S_IRUGO,icm20608d_show_gyro_x_calibbias, NULL);
static DEVICE_ATTR(gyro_y_calibbias, S_IWUSR | S_IRUGO,icm20608d_show_gyro_y_calibbias, NULL);
static DEVICE_ATTR(gyro_z_calibbias, S_IWUSR | S_IRUGO,icm20608d_show_gyro_z_calibbias, NULL);
static DEVICE_ATTR(gyro_x_offset, S_IWUSR | S_IRUGO,NULL, icm20608d_store_gyro_x_offset);
static DEVICE_ATTR(gyro_y_offset, S_IWUSR | S_IRUGO,NULL, icm20608d_store_gyro_y_offset);
static DEVICE_ATTR(gyro_z_offset, S_IWUSR | S_IRUGO,NULL, icm20608d_store_gyro_z_offset);
static DEVICE_ATTR(acc_gyro_factory_test_calibration, S_IWUSR | S_IRUGO,icm20608d_show_acc_gyro_factory_test_calibration,NULL);
static DEVICE_ATTR(acc_gyro_factory_calibration, S_IWUSR | S_IRUGO,icm20608d_show_acc_gyro_factory_calibration, NULL);
static DEVICE_ATTR(acc_gyro_factory_read_calibbias, S_IWUSR | S_IRUGO,icm20608d_show_acc_gyro_factory_read_calibbias, icm20608d_store_acc_gyro_factory_read_calibbias);

static struct device_attribute *calibrate_dev_attributes[] = {
	&dev_attr_acc_calibration,
	&dev_attr_acc_x_calibbias,
	&dev_attr_acc_y_calibbias,
	&dev_attr_acc_z_calibbias,
	&dev_attr_acc_x_offset,
	&dev_attr_acc_y_offset,
	&dev_attr_acc_z_offset,
	&dev_attr_gyro_calibration,
	&dev_attr_gyro_x_calibbias,
	&dev_attr_gyro_y_calibbias,
	&dev_attr_gyro_z_calibbias,
	&dev_attr_gyro_x_offset,
	&dev_attr_gyro_y_offset,
	&dev_attr_gyro_z_offset,
	&dev_attr_acc_gyro_factory_test_calibration,
	&dev_attr_acc_gyro_factory_calibration,
	&dev_attr_acc_gyro_factory_read_calibbias,
};
#endif

static int init_factory_calibration(void){
	struct inv_mpu_state *st;
	int accel_bias[3];
	int result = 0;
	int i = 0;
	st = iio_priv(mtk_indio_dev);

//Add by huangzifan at 2016.02.01. For BSP calibration and factory calibration.
	if( bsp_calib_flag == 1){
		printk("INV BSP calibration have done. Dothing.\n");
		return 0;
	}
	memset(accel_bias, 0, sizeof(accel_bias));
	memset(&acc_gyr_calib, 0, sizeof(acc_gyr_calib));

	result = read_block( sizeof(acc_gyr_calib));
	if( result < 0){
				  printk("INV icm20608d_show_acc_gyro_factory_calibration read error\n");
				  return -1;
	}
	st->accel_bias[0] = acc_gyr_calib.acc_fact_calib[0]; 
	st->accel_bias[1] = acc_gyr_calib.acc_fact_calib[1]; 
	st->accel_bias[2] = acc_gyr_calib.acc_fact_calib[2]; 
	
	printk("INV_block2: init_factory_calibration  accel %d %d %d Gyr:  %d %d %d \n", st->accel_bias[0],st->accel_bias[1], st->accel_bias[2],st->gyro_bias[0],st->gyro_bias[1],st->gyro_bias[2]);	
	return 0;
}


/*
 *  inv_mpu_probe() - probe function.
 */
static int inv_mpu_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct inv_mpu_state *st;
	struct iio_dev *indio_dev;
	int err = 0;
	int result;
	char *info = g_sensor_info;
	/*
	 * If we're not coming from a power-off condition, we need to
	 * reset the chip as we may have gotten here via a watchdog
	 * reboot, in which case the status of the chip is unknown
	 * (i.e. chip is not reset by hardware on a watchdog reboot).
	 */
	bool reset_needed = true;

#ifdef CONFIG_DTS_INV_MPU_IIO
	enable_irq_wake(client->irq);
#endif

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		result = -ENOSYS;
		pr_err("I2c function error\n");
		goto out_no_free;
	}

	mdelay(100);
#ifdef INV_KERNEL_3_10
	indio_dev = iio_device_alloc(sizeof(*st));
#else
	indio_dev = iio_allocate_device(sizeof(*st));
#endif

	if (indio_dev == NULL) {
		pr_err("memory allocation failed\n");
		result =  -ENOMEM;
		goto out_no_free;
	}

	st = iio_priv(indio_dev);
	st->client = client;
	st->sl_handle = client->adapter;
	st->i2c_addr = client->addr;
	
	printk("mpu: 00000000000\n");
#ifdef CONFIG_DTS_INV_MPU_IIO
	result = invensense_mpu_parse_dt(&client->dev, &st->plat_data);
	if (result)
		goto out_free;
	/*Power on device.*/
	if (st->plat_data.power_on) {
		result = st->plat_data.power_on(&st->plat_data);
		if (result < 0) {
			dev_err(&client->dev,
					"power_on failed: %d\n", result);
			return result;
		}
		msleep(POWER_UP_TIME);
		/*
		 * We don't need subsequent reset of chip as it's coming
		 * from a power-off condition
		 */
		reset_needed = false;
	}
#else
	/* power is turned on inside check chip type */
	st->plat_data =
	*(struct mpu_platform_data *)dev_get_platdata(&client->dev);
#endif


	result = inv_check_chip_type(st, id, reset_needed);
	if (result){	
 		client->addr = 0x68;
		st->i2c_addr = 0x68;
		result = 0;
		result = inv_check_chip_type(st, id, reset_needed);
		if(result){
			printk("Inv: inv_check_chip_type error = %d\n",result);
			goto out_free;
		}
	}

     info += sprintf(info,"Icm20608d,");
     info += sprintf(info,"Invensen ");
	
	result = st->init_config(indio_dev);
	if (result) {
		dev_err(&client->adapter->dev,
			"Could not initialize device.\n");
		goto out_free;
	}
	/* Make state variables available to all _show and _store functions. */
	i2c_set_clientdata(client, indio_dev);
	indio_dev->dev.parent = &client->dev;

	indio_dev->name = id->name;
	indio_dev->channels = inv_mpu_channels;
	indio_dev->num_channels = ARRAY_SIZE(inv_mpu_channels);

	indio_dev->info = &mpu_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;

	result = inv_mpu_configure_ring(indio_dev);
	if (result) {
		pr_err("configure ring buffer fail\n");
		goto out_free;
	}
	result = iio_buffer_register(indio_dev, indio_dev->channels,
					indio_dev->num_channels);
	if (result) {
		pr_err("ring buffer register fail\n");
		goto out_unreg_ring;
	}
	st->irq = client->irq;
	result = inv_mpu_probe_trigger(indio_dev);
	if (result) {
		pr_err("trigger probe fail\n");
		goto out_remove_ring;
	}

	/* Tell the i2c counter, we have an IRQ */
	INV_I2C_SETIRQ(IRQ_MPU, client->irq);

	result = iio_device_register(indio_dev);
	if (result) {
		pr_err("IIO device register fail\n");
		goto out_remove_trigger;
	}

	if (INV_MPU6050 == st->chip_type ||
		INV_MPU6500 == st->chip_type ||
		INV_ICM20608 == st->chip_type) {
		result = inv_create_dmp_sysfs(indio_dev);
		if (result) {
			pr_err("create dmp sysfs failed\n");
			goto out_unreg_iio;
		}
	}
	INIT_KFIFO(st->timestamps);
	spin_lock_init(&st->time_stamp_lock);
	wake_lock_init(&st->wake_lock, WAKE_LOCK_SUSPEND, "inv_mpu");
	mutex_init(&st->suspend_resume_lock);

#if 0
     	printk("init_factory_calibration0!\n");
	err = init_factory_calibration();
	if (err)
    	{
        	printk("init_factory_calibration failed!\n");
    	}
     	printk("init_factory_calibration1!\n");
#endif

	result = st->set_power_state(st, false);
	if (result) {
		dev_err(&client->adapter->dev,
			"%s could not be turned off.\n", st->hw->name);
		goto out_remove_wakelock;
	}
	inv_init_sensor_struct(st);
	dev_info(&client->dev, "%s is ready to go!\n",
					indio_dev->name);


    if ((err = misc_register(&icm20608d_gsensor_device)))
    {
     		printk("icm20608d_device register failed\n");
        goto out_remove_wakelock;
    }

    err = misc_register(&icm20608d_gyroscope_device);
    if (err)
    {
        	printk("icm20608d_device misc register failed!\n");
        goto out_remove_wakelock;
    }


	 mtk_indio_dev = indio_dev;
#ifdef ACC_DYNAMIC_CALIBRATE
	int i;
	int ps_result;
      acc_device= kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!acc_device) {
        	printk("Inv cannot allocate hall device\n");        
	}

      acc_device->init_name = ACC_DEVICE_NAME;
      acc_device->class = meizu_class;
      acc_device->release = (void (*)(struct device *))kfree;
      ps_result = device_register(acc_device);
	if (ps_result) {
         	printk("Inv cannot register acc_device\n");        
  	      goto out_remove_wakelock;
	}

 	ps_result = 0;
	for (i = 0; i < ARRAY_SIZE(calibrate_dev_attributes); i++) {
		ps_result = device_create_file(acc_device,  calibrate_dev_attributes[i]);
		if (ps_result)
            	break;
	}
	if (ps_result) {
        while (--i >= 0)
            device_remove_file(acc_device, calibrate_dev_attributes[i]);
	     printk("Inv mpu6515 device remove file  = %d\n", ps_result);      
           goto out_remove_wakelock;
	} 
#endif



	return 0;
out_remove_wakelock:
	wake_lock_destroy(&st->wake_lock);
out_unreg_iio:
	iio_device_unregister(indio_dev);
out_remove_trigger:
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_mpu_remove_trigger(indio_dev);
out_remove_ring:
	iio_buffer_unregister(indio_dev);
out_unreg_ring:
	inv_mpu_unconfigure_ring(indio_dev);
out_free:
#ifdef INV_KERNEL_3_10
	iio_device_free(indio_dev);
#else
	iio_free_device(indio_dev);
#endif
out_no_free:
	dev_err(&client->adapter->dev, "%s failed %d\n", __func__, result);
	return -EIO;
}

static void inv_mpu_shutdown(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct inv_reg_map_s *reg;
	int result;

	reg = &st->reg;
	mutex_lock(&indio_dev->mlock);
	dev_dbg(&client->adapter->dev, "Shutting down %s...\n", st->hw->name);

	/* reset to make sure previous state are not there */
	result = inv_i2c_single_write(st, reg->pwr_mgmt_1, BIT_H_RESET);
	if (result)
		dev_err(&client->adapter->dev, "Failed to reset %s\n",
			st->hw->name);
	msleep(POWER_UP_TIME);
	/* turn off power to ensure gyro engine is off */
	result = st->set_power_state(st, false);
	if (result)
		dev_err(&client->adapter->dev, "Failed to turn off %s\n",
			st->hw->name);
	mutex_unlock(&indio_dev->mlock);
}

/*
 *  inv_mpu_remove() - remove function.
 */
static int inv_mpu_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_mpu_state *st = iio_priv(indio_dev);

	kfifo_free(&st->timestamps);
	wake_lock_destroy(&st->wake_lock);
	iio_device_unregister(indio_dev);
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_mpu_remove_trigger(indio_dev);
	iio_buffer_unregister(indio_dev);
	inv_mpu_unconfigure_ring(indio_dev);
#ifdef INV_KERNEL_3_10
	iio_device_free(indio_dev);
#else
	iio_free_device(indio_dev);
#endif
	dev_info(&client->adapter->dev, "inv-mpu-iio module removed.\n");

	return 0;
}

static int inv_setup_suspend_batchmode(struct iio_dev *indio_dev, bool suspend)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;
	int counter;

	if (st->chip_config.dmp_on &&
		st->chip_config.enable) {
		if (!st->chip_config.dmp_event_int_on &&
				!st->chip_config.wake_on) {
			/* turn off data interrupt in suspend mode
			 * if no wake-up sensor is enabled;
			 * turn on resume */
			result = inv_set_interrupt_on_gesture_event(st, suspend);
			if (result)
				return result;
		}
		if (suspend && !st->chip_config.wake_on)
			counter = INT_MAX;
		else
			counter = st->batch.counter;
		result = write_be32_key_to_mem(st, counter, KEY_BM_BATCH_THLD);
		if (result)
			return result;
	}

	return 0;
}

#ifdef CONFIG_PM
#if 0
static void inv_disable_nonwake_sensors(struct inv_mpu_state *st)
{
	int err = 0;
	if (st->chip_config.gyro_enable) {
		err = inv_switch_gyro_engine(st, false);
		if (err)
			pr_err("%s: ERROR %d disabling gyro\n", __func__, err);
	}

	/* don't disable accel if pedometer or significant motion is enabled */
	if (!st->ped.on && !st->chip_config.smd_enable &&
					st->chip_config.accel_enable) {
		err = inv_switch_accel_engine(st, false);
		if (err)
			pr_err("%s: ERROR %d disabling accelerometer\n",
							__func__, err);
	}

	if (st->sensor[SENSOR_COMPASS].on) {
		err = st->slave_compass->suspend(st);
		if (err)
			pr_err("%s: ERROR %d disabling compass\n",
							__func__, err);
	}
}

static void inv_enable_nonwake_sensors(struct inv_mpu_state *st)
{
	int err = 0;
	if (st->chip_config.gyro_enable) {
		err = inv_switch_gyro_engine(st, true);
		if (err)
			pr_err("%s: ERROR %d restoring gyro state\n",
							__func__, err);
	}

	if (!st->ped.on && !st->chip_config.smd_enable &&
					st->chip_config.accel_enable) {
		err = inv_switch_accel_engine(st, true);
		if (err)
			pr_err("%s: ERROR %d restoring accelerometer state\n",
							__func__, err);
	}

	if (st->sensor[SENSOR_COMPASS].on) {
		err = st->slave_compass->resume(st);
		if (err)
			pr_err("%s: ERROR %d restoring compass state\n",
							__func__, err);
	}
}
#endif
/* Uncomment to utilize suspend_noirq.
   It is platform dependent whether or not suspend_irq is called */
#define USE_SUSPEND_NOIRQ

extern irqreturn_t inv_read_fifo(int, void *);
/*
 * inv_mpu_resume(): resume method for this driver.
 *    This method can be modified according to the request of different
 *    customers. It basically undo everything suspend_noirq is doing
 *    and recover the chip to what it was before suspend.
 */
static int inv_mpu_resume(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	/* add code according to different request Start */
	pr_debug("%s inv_mpu_resume\n", st->hw->name);
	mutex_lock(&indio_dev->mlock);
	st->suspend_state = false;

	result = 0;
	if (st->chip_config.dmp_on && st->chip_config.enable) {
		result = st->set_power_state(st, true);
		result |= inv_read_time_and_ticks(st, true);
		if (st->ped.int_on)
			result |= inv_enable_pedometer_interrupt(st, true);
		if (st->chip_config.display_orient_on)
			result |= inv_set_display_orient_interrupt_dmp(st,
								true);
		result |= inv_setup_suspend_batchmode(indio_dev, false);
	} else if (st->chip_config.enable) {
		result = st->set_power_state(st, true);
	}
	mutex_unlock(&indio_dev->mlock);
	/* add code according to different request End */
	mutex_unlock(&st->suspend_resume_lock);
	inv_read_fifo(0, (void *)st);
	return result;
}

/*
 * inv_mpu_suspend(): suspend method for this driver.
 *    This method can be modified according to the request of different
 *    customers. If customer want some events, such as SMD to wake up the CPU,
 *    then data interrupt should be disabled in this interrupt to avoid
 *    unnecessary interrupts. If customer want pedometer running while CPU is
 *    asleep, then pedometer should be turned on while pedometer interrupt
 *    should be turned off.
 */
static int inv_mpu_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	/* add code according to different request Start */
	pr_debug("%s inv_mpu_suspend\n", st->hw->name);

	result = 0;
	if (st->chip_config.dmp_on && st->chip_config.enable) {
		/* turn off pedometer interrupt during suspend */
		if (st->ped.int_on)
			result |= inv_enable_pedometer_interrupt(st, false);
		/* turn off orientation interrupt during suspend */
		if (st->chip_config.display_orient_on)
			result |= inv_set_display_orient_interrupt_dmp(st,
								false);
		/* setup batch mode related during suspend */
		result = inv_setup_suspend_batchmode(indio_dev, true);
		/* only in DMP non-batch data mode, turn off the power */
		if ((!st->batch.on) && (!st->chip_config.smd_enable) &&
					(!st->ped.on) && (!st->chip_config.wake_on))
			result |= st->set_power_state(st, false);
	} else if (st->chip_config.enable) {
		/* in non DMP case, just turn off the power */
		result |= st->set_power_state(st, false);
	}
	st->suspend_state = true;
	/* add code according to different request End */
#ifndef USE_SUSPEND_NOIRQ
	mutex_lock(&st->suspend_resume_lock);
#endif

	return 0;
}

#ifdef USE_SUSPEND_NOIRQ
static int inv_mpu_suspend_noirq(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);

	pr_err("inv_mpu_suspend_noirq\n");
	mutex_lock(&st->suspend_resume_lock);
	st->suspend_state = false;

	return 0;
}
#endif
static const struct dev_pm_ops inv_mpu_pmops = {
	.suspend       = inv_mpu_suspend,
#ifdef USE_SUSPEND_NOIRQ
	.suspend_noirq = inv_mpu_suspend_noirq,
#endif
	.resume        = inv_mpu_resume,
};
#define INV_MPU_PMOPS (&inv_mpu_pmops)
#else
#define INV_MPU_PMOPS NULL
#endif /* CONFIG_PM */

static const u16 normal_i2c[] = { I2C_CLIENT_END };
/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct i2c_device_id inv_mpu_id[] = {
	{"icm20608", INV_ICM20608},
	{}
};

//MODULE_DEVICE_TABLE(i2c, inv_mpu_id);

static struct i2c_driver inv_mpu_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		=	inv_mpu_probe,
	.remove		=	inv_mpu_remove,
	.shutdown	=	inv_mpu_shutdown,
	.id_table	=	inv_mpu_id,
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"icm20608",
		.pm     =       INV_MPU_PMOPS,
	},
	.address_list = normal_i2c,
};


static struct mpu_platform_data gyro_platform_data = {
        .int_config  = 0x00,
        .level_shifter = 0,
        .orientation = {  -1,  0,  0,
                           0,  1,  0,
                           0,  0, -1 },

};


static struct i2c_board_info __initdata acvilon_i2c_devs[] = {
        {
                I2C_BOARD_INFO("icm20608", 0x69),
                .platform_data = &gyro_platform_data,
        }
};


static int __init inv_mpu_board_init(void) 
{
	int result;
	result = 	i2c_register_board_info(1, acvilon_i2c_devs,
				ARRAY_SIZE(acvilon_i2c_devs));
	printk("board info result:%d\n",result);
	return 0;
}

static void __exit inv_mpu_board_exit(void)
{
}

static int __init inv_mpu_init(void)
{	
	int result = i2c_add_driver(&inv_mpu_driver);
	if ( result < 0 ){
			printk("Inv i2c_add_driver,result = %d\n",result);	
	}
	if (result) {
		pr_err("failed\n");
		return result;
	}
	return 0;
}

static void __exit inv_mpu_exit(void)
{
	i2c_del_driver(&inv_mpu_driver);
}


postcore_initcall(inv_mpu_board_init);
module_init(inv_mpu_init);
module_exit(inv_mpu_exit);
module_exit(inv_mpu_board_exit);

//late_initcall(inv_mpu_init);
//module_exit(inv_mpu_exit);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("inv-mpu-iio");

