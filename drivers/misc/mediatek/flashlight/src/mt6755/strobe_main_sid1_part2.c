#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_camera_hw.h"
#include <cust_gpio_usage.h>
#include <cust_i2c.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
#include <linux/mutex.h>
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#endif

#include <linux/i2c.h>
#include <linux/leds.h>


/******************************************************************************
 * Debug configuration
******************************************************************************/
// availible parameter
// ANDROID_LOG_ASSERT
// ANDROID_LOG_ERROR
// ANDROID_LOG_WARNING
// ANDROID_LOG_INFO
// ANDROID_LOG_DEBUG
// ANDROID_LOG_VERBOSE
#define TAG_NAME "leds_HT_strobe_jerryxie_part2"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_WARN(fmt, arg...)        xlog_printk(ANDROID_LOG_WARNING, TAG_NAME, KERN_WARNING  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_NOTICE(fmt, arg...)      xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_NOTICE  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_INFO(fmt, arg...)        xlog_printk(ANDROID_LOG_INFO   , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_TRC_FUNC(f)              xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME,  "<%s>\n", __FUNCTION__);
#define PK_TRC_VERBOSE(fmt, arg...) xlog_printk(ANDROID_LOG_VERBOSE, TAG_NAME,  fmt, ##arg)
#define PK_ERROR(fmt, arg...)       xlog_printk(ANDROID_LOG_ERROR  , TAG_NAME, KERN_ERR "%s: " fmt, __FUNCTION__ ,##arg)


#define DEBUG_LEDS_STROBE
#ifdef  DEBUG_LEDS_STROBE
	//#define PK_DBG PK_DBG_FUNC
	#define PK_DBG PK_ERROR
	#define PK_VER PK_TRC_VERBOSE
	#define PK_ERR PK_ERROR
#else
	#define PK_DBG(a,...)
	#define PK_VER(a,...)
	#define PK_ERR(a,...)
#endif

/******************************************************************************
 * local variables
******************************************************************************/

static DEFINE_SPINLOCK(g_strobeSMPLock); /* cotta-- SMP proection */


static u32 strobe_Res = 0;
static BOOL g_strobe_On = 0;

static int g_timeOutTimeMs=0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
static DEFINE_MUTEX(g_strobeSem);
#else
static DECLARE_MUTEX(g_strobeSem);
#endif


static struct work_struct workTimeOut;

#define PART2_GPIO_LED_EN  		GPIO99
#define PART2_GPIO_LED_STROBE  	GPIO9
#define PART2_GPIO_LED_GPIO  		GPIO89

#if 1
//==================================
#define SY7806_REG_ENABLE      0x01
#define SY7806_REG_TIMING      0x08
#define SY7806_REG_FLASH_LED1  0x04
#define SY7806_REG_TORCH_LED1  0x06
#define SY7806_REG_FLASH_LED2  0x03
#define SY7806_REG_TORCH_LED2  0x05
#define SY7806_REG_DEVICE_ID   0x0C

//Reg0x01
#define SY7806_TORCH_MODE       0x08
#define SY7806_FLASH_MODE       0x0C
#define SY7806_LED1_ON       0x02
#define SY7806_LED2_ON       0x01
//----------------------
//Reg0x01 bit5
#define SY7806_FLEN_ON          0x20
//Reg0x01 bit6
#define SY7806_FLEN_LEVEL_TRIGER     0x00
#define SY7806_FLEN_EDGE_TRIGER      0x40
//==================================
#else
//==================================
#define SY7806_REG_ENABLE      0x01
#define SY7806_REG_TIMING      0x08
#define SY7806_REG_FLASH_LED1  0x03
#define SY7806_REG_TORCH_LED1  0x05
#define SY7806_REG_FLASH_LED2  0x04
#define SY7806_REG_TORCH_LED2  0x06
#define SY7806_REG_DEVICE_ID   0x0C

//Reg0x01
#define SY7806_TORCH_MODE       0x08
#define SY7806_FLASH_MODE       0x0C
#define SY7806_LED1_ON       0x01
#define SY7806_LED2_ON       0x02
//----------------------
//Reg0x01 bit5
#define SY7806_FLEN_ON          0x20
//Reg0x01 bit6
#define SY7806_FLEN_LEVEL_TRIGER     0x00
#define SY7806_FLEN_EDGE_TRIGER      0x40
//==================================
#endif


/*****************************************************************************
Functions
*****************************************************************************/
extern u32 get_strobe_Res_L(void);
static void work_timeOutFunc(struct work_struct *data);

static int readReg(int reg);
static int writeReg(int reg, int val);

static struct i2c_client *SY7806_i2c_client = NULL;

struct SY7806_platform_data {
	u8 torch_pin_enable;    // 1:  TX1/TORCH pin isa hardware TORCH enable
	u8 pam_sync_pin_enable; // 1:  TX2 Mode The ENVM/TX2 is a PAM Sync. on input
	u8 thermal_comp_mode_enable;// 1: LEDI/NTC pin in Thermal Comparator Mode
	u8 strobe_pin_disable;  // 1 : STROBE Input disabled
	u8 vout_mode_enable;  // 1 : Voltage Out Mode enable
};

struct SY7806_chip_data {
	struct i2c_client *client;

	//struct led_classdev cdev_flash;
	//struct led_classdev cdev_torch;
	//struct led_classdev cdev_indicator;

	struct SY7806_platform_data *pdata;
	struct mutex lock;

	u8 last_flag;
	u8 no_pdata;
};

static int SY7806_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret=0;
	struct SY7806_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret =  i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		PK_ERR("failed writting at 0x%02x\n", reg);
	return ret;
}

static int SY7806_read_reg(struct i2c_client *client, u8 reg)
{
	int val=0;
	struct SY7806_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val =  i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);


	return val;
}

static int readReg(int reg)
{
	int val;
	val = SY7806_read_reg(SY7806_i2c_client, reg);
	return (int)val;
}

static int writeReg(int reg, int val)
{
    int err;

    PK_DBG("writeReg  ---1--- reg=0x%x\n",reg);

    err = SY7806_write_reg(SY7806_i2c_client, reg, val);
    return (int)val;
}


#define e_DutyNum 27

static int isMovieModeLED1[e_DutyNum+1] = {-1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static int isMovieModeLED2[e_DutyNum+1] = {-1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
/*
static int torchLED1Reg[e_DutyNum+1] = {0,25,38,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static int torchLED2Reg[e_DutyNum+1] = {0,25,38,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static int flashLED1Reg[e_DutyNum+1] = {0,2,4,5,7,8,10,11,13,15,16,18,19,21,23,25,29,32,35,38,41,44,48,51,54,57,60,66};
static int flashLED2Reg[e_DutyNum+1] = {0,2,4,5,7,8,10,11,13,15,16,18,19,21,23,25,29,32,35,38,41,44,48,51,54,57,60,66};
*/
static int torchLED1Reg[e_DutyNum+1] = {0,26,39,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static int torchLED2Reg[e_DutyNum+1] = {0,26,39,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static int flashLED1Reg[e_DutyNum+1] = {0,2,4,6,8,9,11,12,14,16,17,19,20,22,24,26,30,33,36,39,42,45,48,52,55,58,61,67};
static int flashLED2Reg[e_DutyNum+1] = {0,2,4,6,8,9,11,12,14,16,17,19,20,22,24,26,30,33,36,39,42,45,48,52,55,58,61,67};



int m_part2_duty1=0;
int m_part2_duty2=0;
int LED1Closeflag_part2 = 1;
int LED2Closeflag_part2 = 1;

int flashEnable_SY7806_2(void)
{
	int enable_value =0;

	PK_DBG("---lijin---flashEnable_SY7806_2 LED1Closeflag_part2 = %d, LED2Closeflag_part2 = %d\n", LED1Closeflag_part2, LED2Closeflag_part2);

	enable_value = readReg(SY7806_REG_ENABLE);
	PK_DBG(" LED1&2_enable_value S =0x%x\n",enable_value);

	if((LED1Closeflag_part2 == 1) && (LED2Closeflag_part2 == 1))
	{
		writeReg(SY7806_REG_ENABLE, (enable_value & 0x80));//close
		//SY7806_FlashIc_Disable();
	}
	else if(LED1Closeflag_part2 == 1)	//led1 close
	{
		if(isMovieModeLED2[m_part2_duty2+1] == 1) //torch
		{
			if(mt_set_gpio_out(PART2_GPIO_LED_STROBE,GPIO_OUT_ZERO)){PK_DBG(" set strobe failed!! \n");}
			//Assist light mode with continuous LED current
			writeReg(SY7806_REG_TIMING, 0x1F);
			//set additional feature
			writeReg(SY7806_REG_TORCH_LED1,0x00);		//led1 torch = 0
			writeReg(SY7806_REG_FLASH_LED1,0x00);		//led1 flash = 0
			writeReg(SY7806_REG_ENABLE, (enable_value & 0x80) | (SY7806_TORCH_MODE | SY7806_LED2_ON));	//led2 on
			if(mt_set_gpio_out(PART2_GPIO_LED_GPIO,GPIO_OUT_ONE)){PK_DBG(" set gpio failed!! \n");}
			PK_DBG("flashEnable_SY7806_2 ---1--- LED1Closeflag_part2 = 1  led2 torch mode start enable_value=0x%x\n",enable_value);
		}
		else                             //flash
		{
			if(mt_set_gpio_out(PART2_GPIO_LED_GPIO,GPIO_OUT_ZERO)){PK_DBG(" set gpio failed!! \n");}
			//flash mode with LED current to 1A available for up to 1.6sec
			writeReg(SY7806_REG_TIMING, 0x1F);
			//set additional feature
			writeReg(SY7806_REG_TORCH_LED1,0x00);		//led1 torch = 0
			writeReg(SY7806_REG_FLASH_LED1,0x00);		//led1 flash = 0
			writeReg(SY7806_REG_ENABLE, (enable_value & 0x80) | (SY7806_FLASH_MODE | SY7806_FLEN_ON | SY7806_LED2_ON));	//led2 on
			if(mt_set_gpio_out(PART2_GPIO_LED_STROBE,GPIO_OUT_ONE)){PK_DBG(" set strobe failed!! \n");}
			PK_DBG("flashEnable_SY7806_2 ---2--- LED1Closeflag_part2 = 1  led2 flash mode start enable_value=0x%x\n",enable_value);
		}
	}
	else if(LED2Closeflag_part2 == 1)	//led2 close
	{
		if(isMovieModeLED1[m_part2_duty1+1] == 1) //torch
		{
			if(mt_set_gpio_out(PART2_GPIO_LED_STROBE,GPIO_OUT_ZERO)){PK_DBG(" set strobe failed!! \n");}
			//Assist light mode with continuous LED current
			writeReg(SY7806_REG_TIMING, 0x1F);
			//set additional feature
			writeReg(SY7806_REG_TORCH_LED2,0x00);		//led2 torch = 0
			writeReg(SY7806_REG_FLASH_LED2,0x00);		//led2 flash = 0
			writeReg(SY7806_REG_ENABLE, (enable_value & 0x80) | (SY7806_TORCH_MODE | SY7806_LED1_ON));	//led1 on
			if(mt_set_gpio_out(PART2_GPIO_LED_GPIO,GPIO_OUT_ONE)){PK_DBG(" set gpio failed!! \n");}
			PK_DBG("flashEnable_SY7806_2 ---3--- LED2Closeflag_part2 = 1  led1 torch mode start enable_value=0x%x\n",enable_value);
		}
		else                              //flash
		{
			if(mt_set_gpio_out(PART2_GPIO_LED_GPIO,GPIO_OUT_ZERO)){PK_DBG(" set gpio failed!! \n");}
			//flash mode with LED current to 1A available for up to 1.6sec
			writeReg(SY7806_REG_TIMING, 0x1F);
			//set additional feature
			writeReg(SY7806_REG_TORCH_LED2,0x00);		//led2 torch = 0
			writeReg(SY7806_REG_FLASH_LED2,0x00);		//led2 flash = 0
			writeReg(SY7806_REG_ENABLE, (enable_value & 0x80) | (SY7806_FLASH_MODE | SY7806_FLEN_ON | SY7806_LED1_ON));	//led1 on
			if(mt_set_gpio_out(PART2_GPIO_LED_STROBE,GPIO_OUT_ONE)){PK_DBG(" set strobe failed!! \n");}
			PK_DBG("flashEnable_SY7806_2 ---4--- LED2Closeflag_part2 = 1  led1 flash mode start enable_value=0x%x\n",enable_value);
		}
	}
	else
	{
		if((isMovieModeLED1[m_part2_duty1+1] == 1)&&(isMovieModeLED2[m_part2_duty2+1] == 1)) //torch
		{
			//writeReg(0x0F, 0x00);
			if(mt_set_gpio_out(PART2_GPIO_LED_STROBE,GPIO_OUT_ZERO)){PK_DBG(" set strobe failed!! \n");}
			//Assist light mode with continuous LED current
			writeReg(SY7806_REG_TIMING, 0x1F);
			//set additional feature
			writeReg(SY7806_REG_ENABLE, (enable_value & 0x80) | (SY7806_TORCH_MODE | SY7806_LED1_ON | SY7806_LED2_ON));	//led1,led2 on
			if(mt_set_gpio_out(PART2_GPIO_LED_GPIO,GPIO_OUT_ONE)){PK_DBG(" set gpio failed!! \n");}
			PK_DBG("flashEnable_SY7806_2  ---5--- led1,led2 torch mode start enable_value=0x%x\n",enable_value);
		}
		else                                     //flash
		{
			if(mt_set_gpio_out(PART2_GPIO_LED_GPIO,GPIO_OUT_ZERO)){PK_DBG(" set strobe failed!! \n");}
			//flash mode with LED current to 1A available for up to 1.6sec
			writeReg(SY7806_REG_TIMING, 0x1F);
			//set additional feature
			writeReg(SY7806_REG_ENABLE, (enable_value & 0x80) | (SY7806_FLASH_MODE | SY7806_FLEN_ON | SY7806_LED1_ON | SY7806_LED2_ON));	//led1,led2 on
			if(mt_set_gpio_out(PART2_GPIO_LED_STROBE,GPIO_OUT_ONE)){PK_DBG(" set strobe failed!! \n");}
			PK_DBG("flashEnable_SY7806_2  ---6--- led1,led2 flash mode start enable_value=0x%x\n",enable_value);
		}
	}

	enable_value = readReg(SY7806_REG_ENABLE);
	PK_DBG(" LED1&2_enable_value E =0x%x\n",enable_value);

	return 0;
}

int flashDisable_SY7806_2(void)
{
	PK_DBG("flashDisable_SY7806_2 S line=%d\n",__LINE__);

	flashEnable_SY7806_2();

	PK_DBG("flashDisable_SY7806_2 E line=%d\n",__LINE__);

	return 0;
}

int setDuty_SY7806_2(int duty)
{

	PK_DBG("setDuty_SY7806_2:m_part2_duty1 = %d, m_part2_duty2 = %d!\n", m_part2_duty1, m_part2_duty2);
	PK_DBG("LED1Closeflag_part2 = %d, LED2Closeflag_part2 = %d\n", LED1Closeflag_part2, LED2Closeflag_part2);

	if((LED1Closeflag_part2 == 1) && (LED2Closeflag_part2 == 1))
	{
		
	}
	else if(LED1Closeflag_part2 == 1)		//led1 close
	{
		if(isMovieModeLED2[m_part2_duty2+1] == 1)		//torch
		{
			writeReg(SY7806_REG_TORCH_LED1,torchLED1Reg[0]);		//write value to led1 torch mode
			writeReg(SY7806_REG_TORCH_LED2,torchLED2Reg[m_part2_duty2+1]);	//write value to led2 torch mode
			PK_DBG("setDuty_SY7806_2:----1----SY7806_REG_TORCH_LED1 = %d, SY7806_REG_TORCH_LED2 = %d\n", torchLED1Reg[0], torchLED2Reg[m_part2_duty2+1]);
		}
		else									//flash
		{
			writeReg(SY7806_REG_FLASH_LED1,flashLED1Reg[0]);			//write value to led1 flash mode
			writeReg(SY7806_REG_FLASH_LED2,flashLED2Reg[m_part2_duty2+1]);	//write value to led2 flash mode
			PK_DBG("setDuty_SY7806_2:----2----SY7806_REG_FLASH_LED1 = %d, SY7806_REG_FLASH_LED2 = %d\n", flashLED1Reg[0], flashLED2Reg[m_part2_duty2+1]);		
		}
	}
	else if(LED2Closeflag_part2 == 1)		//led2 close
	{
		if(isMovieModeLED1[m_part2_duty1+1] == 1)  //torch
		{
			writeReg(SY7806_REG_TORCH_LED1,torchLED1Reg[m_part2_duty1+1]);			//write value to led1 torch mode
			writeReg(SY7806_REG_TORCH_LED2,torchLED2Reg[0]);				//write value to led2 torch mode
			PK_DBG("setDuty_SY7806_2:----3----SY7806_REG_TORCH_LED1 = %d, SY7806_REG_TORCH_LED2 = %d\n", torchLED1Reg[m_part2_duty1+1], torchLED2Reg[0]);
		}
		else                 //flash
		{
			writeReg(SY7806_REG_FLASH_LED1,flashLED1Reg[m_part2_duty1+1]);			//write value to led1 flash mode
			writeReg(SY7806_REG_FLASH_LED2,flashLED2Reg[0]);				//write value to led2 flash mode			
			PK_DBG("setDuty_SY7806_2:----4----SY7806_REG_FLASH_LED1 = %d, SY7806_REG_FLASH_LED2 = %d\n", flashLED1Reg[m_part2_duty1+1], flashLED2Reg[0]);
		}
	}
	else
	{
		if((isMovieModeLED1[m_part2_duty1+1] == 1)&&(isMovieModeLED2[m_part2_duty2+1] == 1))		//torch
		{
			writeReg(SY7806_REG_TORCH_LED1,torchLED1Reg[m_part2_duty1+1]);			//write value to led1 torch mode
			writeReg(SY7806_REG_TORCH_LED2,torchLED2Reg[m_part2_duty2+1]);			//write value to led2 torch mode
			PK_DBG("setDuty_SY7806_2:----5----SY7806_REG_TORCH_LED1 = %d, SY7806_REG_TORCH_LED2 = %d\n", torchLED1Reg[m_part2_duty1+1], torchLED2Reg[m_part2_duty2+1]);
		}
		else											//flash
		{
			writeReg(SY7806_REG_FLASH_LED1,flashLED1Reg[m_part2_duty1+1]);			//write value to led1 flash mode
			writeReg(SY7806_REG_FLASH_LED2,flashLED2Reg[m_part2_duty2+1]);			//write value to led2 flash mode
			PK_DBG("setDuty_SY7806_2:----6----SY7806_REG_FLASH_LED1 = %d, SY7806_REG_FLASH_LED2 = %d\n", flashLED1Reg[m_part2_duty1+1], flashLED2Reg[m_part2_duty2+1]);
		}
	}

	return 0;
}

static int SY7806_chip_init(struct SY7806_chip_data *chip)
{

	return 0;
}

//========================================================
static int g_part2_id;

int get_part2_id(void)
{
    int value = -1;
    
    if(g_part2_id == 2) {
        PK_DBG("JerryXie NT50572 Flash IC.\n");
        value = 2;
    } else if(g_part2_id == 3) {
        PK_DBG("JerryXie SY7806 Flash IC.\n");
        value = 2;
    } else {
        value = g_part2_id;
    }
    
    return value;
}

int get_SY7806_device_id(void)
{
    int value = -1;

    if(mt_set_gpio_mode(PART2_GPIO_LED_EN,GPIO_MODE_00)){PK_DBG(" set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(PART2_GPIO_LED_EN,GPIO_DIR_OUT)){PK_DBG(" set gpio dir failed!! \n");}
    if(mt_set_gpio_out(PART2_GPIO_LED_EN,GPIO_OUT_ONE)){PK_DBG(" set gpio failed!! \n");}
    mdelay(5);

    if(SY7806_i2c_client->addr == 0x63) { //SY7806 & NT50572 i2c 7bit addr
    	  //read register
        value = SY7806_read_reg(SY7806_i2c_client, SY7806_REG_DEVICE_ID);
        if(value > 0) {
            value &= 0x3F;
            if(value == 0x02) { //NT50572 Device ID
            	  PK_DBG("JerryXie NT50572 Device ID = 0x%x\n", value);
                value = 2;
            } else if (value == 0x18){ //SY7806 Device ID
            	  PK_DBG("JerryXie SY7806 Device ID = 0x%x\n", value);
                value = 3;
            }
        } else {
        	  PK_DBG("JerryXie SY7806 Get Device ID Fail \n");
        	  value = -1;
        }
    }

    if(mt_set_gpio_out(PART2_GPIO_LED_EN,GPIO_OUT_ZERO)){PK_DBG(" set gpio failed!! \n");}
    PK_DBG("JerryXie SY7806 Get Device ID Fail \n");
    return value;
}
//========================================================

static int SY7806_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct SY7806_chip_data *chip;
	struct SY7806_platform_data *pdata = client->dev.platform_data;

	int err = -1;

	PK_DBG("SY7806_probe start--->.\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		printk(KERN_ERR  "SY7806 i2c functionality check fail.\n");
		return err;
	}

	chip = kzalloc(sizeof(struct SY7806_chip_data), GFP_KERNEL);
	chip->client = client;

	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);

	if(pdata == NULL){ //values are set to Zero.
		PK_ERR("SY7806 Platform data does not exist\n");
		pdata = kzalloc(sizeof(struct SY7806_platform_data),GFP_KERNEL);
		chip->pdata  = pdata;
		chip->no_pdata = 1;
	}

	chip->pdata  = pdata;
	if(SY7806_chip_init(chip)<0)
		goto err_chip_init;

	SY7806_i2c_client = client;

  g_part2_id = get_SY7806_device_id();

	PK_DBG("SY7806 Initializing is done \n");

	return 0;

err_chip_init:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
	PK_ERR("SY7806 probe is failed \n");
	return -ENODEV;
}

static int SY7806_remove(struct i2c_client *client)
{
	struct SY7806_chip_data *chip = i2c_get_clientdata(client);

    if(chip->no_pdata)
		kfree(chip->pdata);
	kfree(chip);
	return 0;
}


#define SY7806_NAME "leds-SY7806"
#define I2C_BUS_NUM  1
static const struct i2c_device_id SY7806_id[] = {
	{SY7806_NAME, 0},
	{}
};

static struct i2c_driver SY7806_i2c_driver = {
	.driver = {
		.name  = SY7806_NAME,
	},
	.probe	= SY7806_probe,
	.remove   = SY7806_remove,
	.id_table = SY7806_id,
};

struct SY7806_platform_data SY7806_pdata = {0, 0, 0, 0, 0};
static struct i2c_board_info __initdata i2c_SY7806={ I2C_BOARD_INFO(SY7806_NAME, 0x63), .platform_data = &SY7806_pdata };


static int __init SY7806_init(void)
{
	printk("SY7806_init\n");

	i2c_register_board_info(I2C_BUS_NUM, &i2c_SY7806, 1);

	return i2c_add_driver(&SY7806_i2c_driver);
}

static void __exit SY7806_exit(void)
{
	i2c_del_driver(&SY7806_i2c_driver);
}


module_init(SY7806_init);
module_exit(SY7806_exit);

MODULE_DESCRIPTION("Flash driver for SY7806");
MODULE_AUTHOR("LiJin <lijin@malatamobile.com>");
MODULE_LICENSE("GPL v2");


int SY7806_FlashIc_Enable(void)
{
  if(mt_set_gpio_out(PART2_GPIO_LED_EN,GPIO_OUT_ONE)){PK_DBG(" set gpio failed!! \n");}
	PK_DBG("SY7806_FlashIc_Enable!\n");
}

int SY7806_FlashIc_Disable(void)
{
	if((!get_strobe_Res_L())&&(!strobe_Res))
		if(mt_set_gpio_out(PART2_GPIO_LED_EN,GPIO_OUT_ZERO)){PK_DBG(" set gpio failed!! \n");}
	PK_DBG("SY7806_FlashIc_Disable!\n");
}


int flashEnable_SY7806_1(void)
{
	return 0;
}
int flashDisable_SY7806_1(void)
{
	return 0;
}
/*
int readjust_part2_Duty_2(int duty)
{
  PK_DBG(" setDuty_SY7806_2 S line=%d\n",__LINE__);

  if(duty<0)
    duty=0;
  else if(duty>=e_DutyNum)
    duty=e_DutyNum-1;
  m_part2_duty2=duty;

  PK_DBG(" setDuty_SY7806_2 E line=%d\n",__LINE__);
  
  return 0;
}

int setDuty_SY7806_1(int duty)
{
  PK_DBG(" setDuty_SY7806_1 S line=%d\n",__LINE__);

	if(duty<0)
		duty=0;
	else if(duty>=e_DutyNum)
		duty=e_DutyNum-1;
	m_part2_duty1=duty;

  PK_DBG(" setDuty_SY7806_1 S line=%d\n",__LINE__);

	return 0;
}
*/

static int FL_Enable(void)
{
	PK_DBG(" FL_Enable line=%d\n", __LINE__);
	flashEnable_SY7806_1();
	return 0;
}

static int FL_Disable(void)
{
	PK_DBG(" FL_Disable line=%d\n", __LINE__);
	flashDisable_SY7806_1();
	return 0;
}
/*
static int FL_dim_duty(kal_uint32 duty)
{
	PK_DBG(" FL_dim_duty line=%d\n", __LINE__);
	setDuty_SY7806_1(duty);
	return 0;
}
*/

int g_part2_flash_gpio_init =0;
int part2_gpio_init(void){
    if(mt_set_gpio_mode(PART2_GPIO_LED_EN,GPIO_MODE_00)){PK_DBG(" set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(PART2_GPIO_LED_EN,GPIO_DIR_OUT)){PK_DBG(" set gpio dir failed!! \n");}
    if(mt_set_gpio_out(PART2_GPIO_LED_EN,GPIO_OUT_ZERO)){PK_DBG(" set gpio failed!! \n");}

    if(mt_set_gpio_mode(PART2_GPIO_LED_STROBE,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(PART2_GPIO_LED_STROBE,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(PART2_GPIO_LED_STROBE,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}

    if(mt_set_gpio_mode(PART2_GPIO_LED_GPIO,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(PART2_GPIO_LED_GPIO,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(PART2_GPIO_LED_GPIO,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
    return 0;
}


static int FL_Init(void)
{
    PK_DBG("LED1_FL_Init!\n");
    if(0==g_part2_flash_gpio_init){
        g_part2_flash_gpio_init=g_part2_flash_gpio_init+1;
       	part2_gpio_init();
    }

    INIT_WORK(&workTimeOut, work_timeOutFunc);
    PK_DBG(" FL_Init line=%d\n",__LINE__);
    return 0;
}


static int FL_Uninit(void)
{
	PK_DBG("LED1_FL_Uninit!\n");
	SY7806_FlashIc_Disable();
    return 0;
}

/*****************************************************************************
User interface
*****************************************************************************/

static void work_timeOutFunc(struct work_struct *data)
{
    FL_Disable();
    PK_DBG("LED1TimeOut_callback\n");
}



static enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
    schedule_work(&workTimeOut);
    return HRTIMER_NORESTART;
}
static struct hrtimer g_timeOutTimer;
static void timerInit(void)
{
	static int init_flag;
	if (init_flag==0){
		init_flag=1;
  INIT_WORK(&workTimeOut, work_timeOutFunc);
	g_timeOutTimeMs=1000; //1s
	hrtimer_init( &g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	g_timeOutTimer.function=ledTimeOutCallback;
}
}



static int constant_flashlight_ioctl(unsigned int cmd, unsigned long arg)
{
	int i4RetValue = 0;
	int ior_shift;
	int iow_shift;
	int iowr_shift;
	ior_shift = cmd - (_IOR(FLASHLIGHT_MAGIC,0, int));
	iow_shift = cmd - (_IOW(FLASHLIGHT_MAGIC,0, int));
	iowr_shift = cmd - (_IOWR(FLASHLIGHT_MAGIC,0, int));
	PK_DBG("SY7806_LED1_constant_flashlight_ioctl() line=%d ior_shift=%d, iow_shift=%d iowr_shift=%d arg=%d\n",__LINE__, ior_shift, iow_shift, iowr_shift, arg);
    switch(cmd)
    {

		case FLASH_IOC_SET_TIME_OUT_TIME_MS:
			PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n",arg);
			g_timeOutTimeMs=arg;
			break;


    	case FLASH_IOC_SET_DUTY:
    		PK_DBG("FLASHLIGHT_DUTY: %d\n",arg);
      //#if 1
    		m_part2_duty1 = arg; //wingtech
      //#else
    	//  FL_dim_duty(arg); //JerryXie
      //#endif
    		break;


    	case FLASH_IOC_SET_STEP:
    		PK_DBG("FLASH_IOC_SET_STEP: %d\n",arg);

    		break;

    	case FLASH_IOC_SET_ONOFF:
    		PK_DBG("FLASHLIGHT_ONOFF: %d\n",arg);
    		if(arg==1)
    		{
				if(g_timeOutTimeMs!=0)
	            {
	            	ktime_t ktime;
					ktime = ktime_set( 0, g_timeOutTimeMs*1000000 );
					hrtimer_start( &g_timeOutTimer, ktime, HRTIMER_MODE_REL );
	            }
				LED1Closeflag_part2 = 0;
    			FL_Enable();
    		}
    		else
    		{
    			LED1Closeflag_part2 = 1;
    			FL_Disable();
				hrtimer_cancel( &g_timeOutTimer );
    		}
    		break;
    	case FLASH_IOC_SET_REG_ADR:
    	    break;
    	case FLASH_IOC_SET_REG_VAL:
    	    break;
    	case FLASH_IOC_SET_REG:
    	    break;
    	case FLASH_IOC_GET_REG:
    	    break;



		default :
    		PK_DBG(" No such command \n");
    		i4RetValue = -EPERM;
    		break;
    }
    return i4RetValue;
}




static int constant_flashlight_open(void *pArg)
{
    int i4RetValue = 0;
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

	if (0 == strobe_Res)
	{
    FL_Init();
    timerInit();
	}
	PK_DBG("constant_flashlight_open line=%d\n", __LINE__);
	spin_lock_irq(&g_strobeSMPLock);


    if(strobe_Res)
    {
        PK_ERR(" busy!\n");
        i4RetValue = -EBUSY;
    }
    else
    {
        strobe_Res += 1;
    }


    spin_unlock_irq(&g_strobeSMPLock);
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

    return i4RetValue;

}


static int constant_flashlight_release(void *pArg)
{
    PK_DBG(" constant_flashlight_release\n");

    if (strobe_Res)
    {
        spin_lock_irq(&g_strobeSMPLock);

        strobe_Res = 0;

        /* LED On Status */
        g_strobe_On = FALSE;

        spin_unlock_irq(&g_strobeSMPLock);

        FL_Uninit();
    }

    PK_DBG(" Done\n");

    return 0;

}

static FLASHLIGHT_FUNCTION_STRUCT strobeFunc = {
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl
};

MUINT32 strobeInit_main_sid1_part2(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc != NULL)
    {
		  *pfFunc = &strobeFunc;
    }
    return 0;
}


//===============================================================
static int hight_cct_dutu = 0;
static int low_cct_dutu = 0;
int hight_cct_led_storbe_enable_part2(void){
	SY7806_FlashIc_Enable();
}

int hight_cct_led_storbe_setduty_part2(int duty){
	hight_cct_dutu = duty;
	writeReg(SY7806_REG_TORCH_LED1,hight_cct_dutu);			//write value to led1 torch mode
	writeReg(SY7806_REG_TORCH_LED2,low_cct_dutu);				//write value to led2 torch mode
	PK_DBG("hight_cct_led_storbe_setduty:----3----SY7806_REG_TORCH_LED1 = %d, SY7806_REG_TORCH_LED2 = %d\n", duty, 0);
	return 0;
}

int hight_cct_led_storbe_on_part2(int onoff){
	unsigned int temp;
	
	if(mt_set_gpio_out(PART2_GPIO_LED_STROBE,GPIO_OUT_ZERO)){PK_DBG(" set strobe failed!! \n");}
	//Assist light mode with continuous LED current
	writeReg(SY7806_REG_TIMING, 0x1F);
	//set additional feature
  
	if(onoff){
		temp = readReg(SY7806_REG_ENABLE);
		writeReg(SY7806_REG_ENABLE, (temp & 0x80) | (SY7806_TORCH_MODE | SY7806_LED1_ON));	//led1 on
	}else {
		temp = readReg(SY7806_REG_ENABLE);
		writeReg(SY7806_REG_ENABLE, (temp & 0x80));	//led1 off
	}
	if(mt_set_gpio_out(PART2_GPIO_LED_GPIO,GPIO_OUT_ONE)){PK_DBG(" set gpio failed!! \n");}
	PK_DBG("flashEnable_SY7806_2 ---3--- LED2Closeflag = 1  led1 torch mode start temp=0x%x\n",temp);
	return 0;
}


int low_cct_led_storbe_enable_part2(void){
	SY7806_FlashIc_Enable();
}

int low_cct_led_storbe_setduty_part2(int duty){
	low_cct_dutu = duty;
	writeReg(SY7806_REG_TORCH_LED1,hight_cct_dutu);			//write value to led1 torch mode
	writeReg(SY7806_REG_TORCH_LED2,low_cct_dutu);				//write value to led2 torch mode
	PK_DBG("hight_cct_led_storbe_setduty:----3----SY7806_REG_TORCH_LED1 = %d, SY7806_REG_TORCH_LED2 = %d\n", duty, 0);
	return 0;
}

int low_cct_led_storbe_on_part2(int onoff){
	unsigned int temp;
	
	if(mt_set_gpio_out(PART2_GPIO_LED_STROBE,GPIO_OUT_ZERO)){PK_DBG(" set strobe failed!! \n");}
	//Assist light mode with continuous LED current
	writeReg(SY7806_REG_TIMING, 0x1F);
	//set additional feature
	
	if(onoff){
		temp = readReg(SY7806_REG_ENABLE);
		writeReg(SY7806_REG_ENABLE, (temp & 0x80) | (SY7806_TORCH_MODE | SY7806_LED2_ON));	//led2 on
	}else{
		temp = readReg(SY7806_REG_ENABLE);
		writeReg(SY7806_REG_ENABLE, (temp & 0x80));	//led2 off
	}
	if(mt_set_gpio_out(PART2_GPIO_LED_GPIO,GPIO_OUT_ONE)){PK_DBG(" set gpio failed!! \n");}
	PK_DBG("flashEnable_SY7806_2 ---3--- LED2Closeflag = 1  led1 torch mode start temp=0x%x\n",temp);
	return 0;
}
//===============================================================
