#ifndef __GF_SPI_TEE_H
#define __GF_SPI_TEE_H

#include <linux/types.h>

/**********************IO Magic**********************/
#define GF_IOC_MAGIC	'g'  //define magic number

struct gf_ioc_transfer {
	u8 cmd;
	u8 flag;
	u16 addr;
	u32 len;
	u8 *buf;
};

//send command to secure driver through DCI
struct gf_secdrv_cmd{
	u8 cmd;
	u8 reserve[3];
	u32 len;
	u8 buf[32];
};

struct spi_speed_setting{
	u32 high_speed;
	u32 low_speed;
};


//define commands
#define GF_IOC_CMD	_IOWR(GF_IOC_MAGIC, 1, struct gf_ioc_transfer)
#define GF_IOC_REINIT	_IO(GF_IOC_MAGIC, 0)
#define GF_IOC_SETSPEED	_IOW(GF_IOC_MAGIC, 2, u32)
#define GF_IOC_STOPTIMER	_IO(GF_IOC_MAGIC, 3)
#define GF_IOC_STARTTIMER	_IO(GF_IOC_MAGIC, 4)
#define GF_IOC_SETMODE	_IOW(GF_IOC_MAGIC, 6,u32)
#define GF_IOC_GETMODE	_IOR(GF_IOC_MAGIC, 7,u32)
#define GF_IOC_GETCHIPID	_IOR(GF_IOC_MAGIC, 8, unsigned int *)

/*for TEE version*/
#define GF_IOC_SECURE_INIT	  _IO(GF_IOC_MAGIC, 10)
#define GF_IOC_INIT_SPI_SPEED_VARIABLE _IOW(GF_IOC_MAGIC, 12, struct spi_speed_setting)
#define GF_IOC_HAS_ESD_WD	_IOW(GF_IOC_MAGIC, 13,u32)
#define GF_IOC_INIT_FP_EINT	_IOW(GF_IOC_MAGIC, 14,u32)

#define GF_IOC_CONNECT_SECURE_DRIVER	_IO(GF_IOC_MAGIC, 16)
#define GF_IOC_CLOSE_SECURE_DRIVER		_IO(GF_IOC_MAGIC, 17)
#define GF_IOC_DCI_TESTCASE	_IOW(GF_IOC_MAGIC, 18, struct gf_secdrv_cmd)
#define GF_IOC_SET_IRQ		_IOW(GF_IOC_MAGIC, 19, u32)
#define GF_IOC_GET_EVENT_TYPE		_IOR(GF_IOC_MAGIC, 20, u32)

#define GF_IOC_CLEAR_IRQ_STATUS		_IOW(GF_IOC_MAGIC, 22, u32)
#define GF_IOC_SPI_SETSPEED			_IOW(GF_IOC_MAGIC, 23, u32)
#define GF_IOC_RESET_SAMPLE_STATUS			_IOW(GF_IOC_MAGIC, 24, u32)

/* not used */
#define GF_IOC_SEND_SENSOR_CMD				 _IOW(GF_IOC_MAGIC, 25, u32)
#define SENSOR_CMD_SET_FACTORY_TEST_A_CONFIG (0x07)
#define SENSOR_CMD_SET_FACTORY_TEST_B_CONFIG (0x08)
#define SENSOR_CMD_RESUME_NORMAL_CONFIG          (0x09)

#define GF_IOC_SUSPEND_RESUME_SAMPLE_CMD	 _IOW(GF_IOC_MAGIC, 26, u32)

//#define  GF_IOC_MAXNR    27  //THIS MACRO IS NOT USED NOW...


#endif	//__GF_SPI_TEE_H
