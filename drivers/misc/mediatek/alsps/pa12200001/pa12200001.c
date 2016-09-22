/*
 * Author: chester hsu (TXC) <chesterhsu@txc.com.tw>
 * Author: Alan Hsiao   (TXC) <alanhsiao@txc.com.tw>
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Version 1.92.1.01
 *	-Factory crosstalk calibration function for proximity sensor
 *	-Factory threshold calibration function for proximity sensor
 *	-Fast crosstalk calibration function when proximity sensor is enabled
 *	-Crosstalk calibration parameters are stored in rom,and are loaded when proximity sensor is enabled at first time
 *	-Add "version" attribute to check .c and .h version
 *	-Add condition to update continous lux value or discrete lux value
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h> 
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h> 
#include <linux/irqchip/mt-eic.h>
#include <linux/of_irq.h>

#include <linux/atomic.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include "mach/eint.h"
#include <alsps.h>


#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include <cust_alsps.h>

//static int first_zero_flag = 0;
int pa122_read_als(struct i2c_client *client, u16 *data);
static int suspend_irq_satus = 0;
int isl29125_lateresume_enable = 0;

static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 1,
    .polling_mode_ps =0,
    .polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    //.i2c_addr   = {0x0C, 0x48, 0x78, 0x00},
    .als_level  = {0, 471, 1058, 1703, 3883, 10171, 10443, 15445, 28499, 35153, 41421, 59194, 65535, 65535, 65535},
    .als_value  = {0, 133, 303, 501, 1002, 2001, 3355, 5001, 8008, 10010, 12000, 16010, 20010, 20010, 20010, 20010},
    .ps_threshold_high = 55,//Modify by huangzifan 2015.11.10
    .ps_threshold_low = 40,
    .is_batch_supported_ps = true,
    .is_batch_supported_als = true,
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}


#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/hwmsen_helper.h> 
#include <linux/sensors_io.h>
#include <linux/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include <linux/fs.h>
#include "pa12200001.h"

//Add new arch and old arch.We use the new arch. 
//Modify by huangzifan at 2015.10.22 
//#define OLD_ARCH
#define NEW_ARCH
#define PS_WAKEUP
#define PS_DYNAMIC_CALIBRATE
#define PS_DEBUG
#ifdef PS_DYNAMIC_CALIBRATE
static int cali_flag = 0;
static int ps_thd_val_low_g=0,	ps_thd_val_high_g=0 ;
static int ps_noise=0;
static int first_enable_flag = 1;
static int ps_offset_flag = 0;
#endif
static int set_crosstalk(int len);
static int write_block(char *buf, int len);
static int read_block(int len);
#define BASE_ADDR (3*1024*1024)
#define POS (10*1024)
#define STORE_FILE      "/dev/block/platform/mtk-msdc.0/by-name/proinfo"
char ps_buffer[8] ={0};
static int wakeup = 1;
#define MEIZU_CLASS_NAME  "meizu"
#define ALS_DEVICE_NAME  "als"
#define PS_DEVICE_NAME  "ps"
extern struct class *meizu_class;
static struct device *ps_device;
static struct device *als_device;
static int first_time = 0;
static int crosstalk_store_flag = 0;//Modify by huangzifan 2015.10.13
static int calibration_flag = 1 ;
/******************************************************************************
 * configuration
*******************************************************************************/
/**Global Variable**/
static int prevObj 	= 1;
static int intr_flag 	= 1;
static int bCal_flag	= 0;
static int pa122_has_load_cal_file = 0;
static int one_time = 1;
/*----------------------------------------------------------------------------*/
#define PA122_DEV_NAME		"pa122"
#define PA12_DRIVER_VERSION_C		"1.92.1.03"
/*----------------------------------------------------------------------------*/
#define PA12200001_DEBUG
#if defined(PA12200001_DEBUG)
#define APS_TAG		"[ALS/PS] "
#define APS_FUN(f)			pr_err(APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)	pr_err(APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)	pr_err(APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)	pr_err(APS_TAG fmt, ##args)    
#else
#define APS_FUN(f)
#define APS_ERR(fmt, args...)
#define APS_LOG(fmt, args...)
#define APS_DBG(fmt, args...)
#endif

#define I2C_FLAG_WRITE	0
#define I2C_FLAG_READ	1

/******************************************************************************
 * extern functions
*******************************************************************************/
#if 0
#ifdef CUST_EINT_ALS_TYPE
extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
extern void mt_eint_print_status(void);
#endif
#endif

extern int isl29125_als_read(void);
extern int isl29125_als_disable(void);
extern int isl29125_als_enable(void);
extern char p_sensor_info[20];
/*----------------------------------------------------------------------------*/
static int pa122_init_client(struct i2c_client *client);		
static int pa122_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int pa122_i2c_remove(struct i2c_client *client);
static int pa122_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int pa122_i2c_resume(struct i2c_client *client);

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id pa122_i2c_id[] = {{PA122_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_pa122={ I2C_BOARD_INFO(PA122_DEV_NAME,PA12_I2C_ADDRESS)};
/*----------------------------------------------------------------------------*/
struct pa122_priv {
	struct alsps_hw  *hw;
	struct i2c_client *client;
	struct work_struct	eint_work;
#if defined(CONFIG_OF)
	struct device_node *irq_node;
	int irq;
#endif

	/* misc */
	u16 		als_modulus;
	atomic_t	i2c_retry;
	atomic_t	als_suspend;
	atomic_t	als_debounce;	/*debounce time after enabling als*/
	atomic_t	als_deb_on; 		/*indicates if the debounce is on*/
	atomic_t	als_deb_end;	/*the jiffies representing the end of debounce*/
	atomic_t	ps_mask;		/*mask ps: always return far away*/
	atomic_t	ps_debounce;	/*debounce time after enabling ps*/
	atomic_t	ps_deb_on;		/*indicates if the debounce is on*/
	atomic_t	ps_deb_end; 	/*the jiffies representing the end of debounce*/
	atomic_t	ps_suspend;
	atomic_t 	trace;

	/* data */
	u16			als;
	u8 			ps;
	u8			_align;
	u16			als_level_num;
	u16			als_value_num;
	u32			als_level[C_CUST_ALS_LEVEL-1];
	u32			als_value[C_CUST_ALS_LEVEL];

	/* Mutex */
	struct mutex	update_lock;

	/* PS Calibration */
	u8 		crosstalk; 
	u8 		crosstalk_base; 

	/* threshold */
	u8		ps_thrd_low; 
	u8		ps_thrd_high; 

	atomic_t	als_cmd_val;		/*the cmd value can't be read, stored in ram*/
	atomic_t	ps_cmd_val; 		/*the cmd value can't be read, stored in ram*/
	atomic_t	ps_thd_val_high;	 	/*the cmd value can't be read, stored in ram*/
	atomic_t	ps_thd_val_low; 		/*the cmd value can't be read, stored in ram*/
	atomic_t	als_thd_val_high;	/*the cmd value can't be read, stored in ram*/
	atomic_t	als_thd_val_low; 	/*the cmd value can't be read, stored in ram*/
	atomic_t	ps_thd_val;
	ulong		enable; 			/*enable mask*/
	ulong		pending_intr;	/*pending interrupt*/
	u8			ps_enable_flag;
	
	/* early suspend */
	#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend	early_drv;
	#endif     
};
/*----------------------------------------------------------------------------*/

static struct i2c_driver pa122_i2c_driver = {	
	.probe	= pa122_i2c_probe,
	.remove	= pa122_i2c_remove,
#if !defined(CONFIG_HAS_EARLYSUSPEND)

	.detect	= pa122_i2c_detect,
	.suspend	= pa122_i2c_suspend,
	.resume	= pa122_i2c_resume,
#endif
	.id_table	= pa122_i2c_id,
	.driver = {
		.name = PA122_DEV_NAME,
	},
};


/*----------------------------------------------------------------------------*/

static struct i2c_client *pa122_i2c_client = NULL;
static struct pa122_priv *g_pa122_ptr = NULL; 
static struct pa122_priv *pa122_obj = NULL;
/*----------------------------------------------------------------------------*/

static struct wake_lock ps_lock; /* add for if ps run in polling mode, the system forbid to goto sleep mode. */

/*----------------------------------------------------------------------------*/
struct pa122_parameters {
	u16 als_th_high;
	u16 als_th_low;
	u8 ps_th_high;
	u8 ps_th_low;
	u8 ps_offset_default;
	u8 ps_offset_extra;
	u8 ps_offset_max;
	u8 ps_offset_min;
	
	int fast_cal;
	int fast_cal_once;
	
	int als_gain;
	int led_curr;
	int ps_prst;
	int als_prst;
	int int_set;
	int int_type;
	int ps_period;
	int als_period;
};
/*----------------------------------------------------------------------------*/
static struct pa122_parameters *pa122_param = NULL;
/*----------------------------------------------------------------------------*/
typedef enum {
	CMC_BIT_ALS	= 1,
	CMC_BIT_PS		= 2,
}CMC_BIT;
/*-----------------------------CMC for debugging-------------------------------*/
typedef enum {
    CMC_TRC_ALS_DATA	= 0x0001,
    CMC_TRC_PS_DATA	= 0x0002,
    CMC_TRC_EINT		= 0x0004,
    CMC_TRC_IOCTL		= 0x0008,
    CMC_TRC_I2C			= 0x0010,
    CMC_TRC_CVT_ALS	= 0x0020,
    CMC_TRC_CVT_PS		= 0x0040,
    CMC_TRC_DEBUG		= 0x8000,
} CMC_TRC;
/*-----------------------------------------------------------------------------*/
static int	pa122_init_flag = -1;	// 0<==>OK -1 <==> fail
static int  pa122_local_init(void);
static int  pa122_local_uninit(void);
static struct alsps_init_info pa122_init_info = {
	.name = "pa12200001",
	.init = pa122_local_init,
	.uninit = pa122_local_uninit,
};


/*----------------------------------------------------------------------------*/
// I2C Read
static int i2c_read_reg(struct i2c_client *client,u8 reg,u8 *data)
{
  u8 reg_value[1];
  u8 databuf[2]; 
	int res = 0;
	databuf[0]= reg;
	res = i2c_master_send(client,databuf,0x1);
	if(res <= 0)
	{
		APS_ERR("i2c_master_send function err\n");
		return res;
	}
	res = i2c_master_recv(client,reg_value,0x1);
	if(res <= 0)
	{
		APS_ERR("i2c_master_recv function err\n");
		return res;
	}
	return reg_value[0];
}
// I2C Write
static int i2c_write_reg(struct i2c_client *client,u8 reg,u8 value)
{
	u8 databuf[2];    
	int res = 0;
   
	databuf[0] = reg;   
	databuf[1] = value;
	res = i2c_master_send(client,databuf,0x2);

	if (res < 0){
		return res;
		APS_ERR("i2c_master_send function err\n");
	}
		return 0;
}

/*----------------------------------------------------------------------------*/
static int pa122_read_file(char *filename,u8* param) 
{
	struct file  *fop;
	mm_segment_t old_fs;

	fop = filp_open(filename,O_RDONLY,0);
	if(IS_ERR(fop))
	{
		APS_ERR("Filp_open error!! Path = %s\n",filename);
		return -1;
	}

	old_fs = get_fs();  
	set_fs(get_ds()); //set_fs(KERNEL_DS);  
	     
	fop->f_op->llseek(fop,0,0);
	fop->f_op->read(fop, param, strlen(param), &fop->f_pos);     

	set_fs(old_fs);  

	filp_close(fop,NULL);

	return 0;

}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_write_file(char *filename,u8* param) 
{
	struct file  *fop;
	mm_segment_t old_fs;	 

	fop = filp_open(filename,O_CREAT | O_RDWR,0666);
	if(IS_ERR(fop))
	{
		APS_ERR("Create file error!! Path = %s\n",filename);       
		return -1;
	}

	old_fs = get_fs();  
	set_fs(get_ds()); //set_fs(KERNEL_DS);  
	fop->f_op->write(fop, (char *)param, sizeof(param), &fop->f_pos);   
	set_fs(old_fs);  

	filp_close(fop,NULL);

	return 0;
}
/*----------------------------------------------------------------------------*/
void pa12_swap(u8 *x, u8 *y)
{
        u8 temp = *x;
        *x = *y;
        *y = temp;
}
static int pa122_thrd_calibration(struct i2c_client *client)
{
	struct pa122_priv *data = i2c_get_clientdata(client);
	int i,j,temp_diff;	
	u16 sum_of_pdata = 0;
    	u8 temp_pdata[10],buftemp[2],cfg0data = 0,temp_thrd;
  	unsigned int ArySize = 10;

	APS_LOG("START threshold calibration\n");

	hwmsen_read_byte(client, REG_CFG0, &cfg0data);
	/*PS On*/
	hwmsen_write_byte(client, REG_CFG0, cfg0data | 0x03); 
	
	for(i = 0; i < 10; i++)
	{
		mdelay(50);
    	hwmsen_read_byte(client,REG_PS_DATA,temp_pdata+i);
		//APS_LOG("ps temp_data = %d\n", temp_pdata[i]);	
	}

	/* pdata sorting */
 	for (i = 0; i < ArySize - 1; i++)
		for (j = i+1; j < ArySize; j++)
			if (temp_pdata[i] > temp_pdata[j])
				pa12_swap(temp_pdata + i, temp_pdata + j);	
	
	/* Calculate the cross-talk base using central 5 data */
	for (i = 3; i < 8; i++) 
	{
		//APS_LOG("%s: temp_pdata = %d\n", __func__, temp_pdata[i]);
		sum_of_pdata = sum_of_pdata + temp_pdata[i];
	}

	/* Restore CFG0 data */
	hwmsen_write_byte(client, REG_CFG0, &cfg0data);

	temp_thrd = sum_of_pdata/5;
	temp_diff = temp_thrd - PA12_PS_TH_HIGH_MINIMUM;
	if(temp_diff < 0)
	{
		APS_LOG("Threshold Cal fail \n");
		return -1 ;
	}
	else
	{
		buftemp[1] = temp_thrd;
		buftemp[0] = temp_thrd - PA12_PS_TH_INTERVAL;
	   
		data->ps_thrd_low = buftemp[0];
		data->ps_thrd_high = buftemp[1];

		if(pa122_write_file(THRD_CAL_FILE_PATH,buftemp)<0)  
			APS_LOG("Create PS Thredhold calibration file error!!");
		else
			APS_LOG("Create PS Thredhold calibration file Success!!");
	}
	return 0;
}

static void pa122_load_calibration_param(struct i2c_client *client)
{
	int res;
	u8 buftemp[2];

	struct pa122_priv *obj = i2c_get_clientdata(client);

	/* Check ps calibration file */

	if(pa122_read_file(PS_CAL_FILE_PATH,buftemp) < 0)
	{
		obj->crosstalk = PA12_PS_OFFSET_DEFAULT;
		APS_LOG("Use Default ps offset , x-talk = %d\n", obj->crosstalk);
	}
	else
	{
		APS_LOG("Use PS Cal file , x-talk = %d base = %d\n",buftemp[0],buftemp[1]);	
		obj->crosstalk = buftemp[0];
		obj->crosstalk_base = buftemp[1];
	}

	mutex_lock(&obj->update_lock);
	/* Write ps offset value to register 0x10 */
	hwmsen_write_byte(client, REG_PS_OFFSET, obj->crosstalk); 

	if(obj->hw->polling_mode_ps == 0)
	{
		/* Check threshold calibration file */
		if(pa122_read_file(THRD_CAL_FILE_PATH,buftemp) < 0)
		{
			APS_LOG("Use Default threhold , Low = %d , High = %d\n", obj->ps_thrd_low, obj->ps_thrd_high);
		}
		else
		{
			APS_LOG("Use Threshold Cal file , Low = %d , High = %d\n",buftemp[0],buftemp[1]);
			obj->ps_thrd_low = buftemp[0];
			obj->ps_thrd_high = buftemp[1];
		}	
		/* Set PS threshold */
		if(PA12_INT_TYPE == 0)
		{
			/*Window Type */	
			res=hwmsen_write_byte(client, REG_PS_TH, obj->ps_thrd_high);
			res=hwmsen_write_byte(client, REG_PS_TL, PA12_PS_TH_MIN);
		}
		else if(PA12_INT_TYPE == 1)
		{
			/*Hysteresis Type */
			res=hwmsen_write_byte(client, REG_PS_TH, obj->ps_thrd_high); 
			res=hwmsen_write_byte(client, REG_PS_TL, obj->ps_thrd_low); 
		}
	}

	mutex_unlock(&obj->update_lock);
}

static int pa122_run_calibration(struct i2c_client *client)
{
	struct pa122_priv *data = i2c_get_clientdata(client);
	int i, j;	
	int ret;
	u16 sum_of_pdata = 0;
	u8 temp_pdata[20],buftemp[2],cfg0data=0,cfg2data=0;
	unsigned int ArySize = 20;
	unsigned int cal_check_flag = 0;	
	
	APS_LOG("%s: START pa122_run_calibration  proximity sensor calibration\n", __func__);

RECALIBRATION:
	sum_of_pdata = 0;

	mutex_lock(&data->update_lock);
	ret = hwmsen_read_byte(client, REG_CFG0, &cfg0data);
	ret = hwmsen_read_byte(client, REG_CFG2, &cfg2data);
	
	/*PS On*/
	ret = hwmsen_write_byte(client, REG_CFG0, cfg0data | 0x03); 

	/*Set to offset mode & disable interrupt from ps*/
	ret = hwmsen_write_byte(client, REG_CFG2, cfg2data & 0x33); 

	/*Set crosstalk = 0*/
	ret = hwmsen_write_byte(client, REG_PS_OFFSET, 0x00); 	
	
	mdelay(100);
	for(i = 0; i < 20; i++)
	{
		mdelay(50);
		ret = hwmsen_read_byte(client,REG_PS_DATA,temp_pdata+i);
		//APS_LOG("temp_data = %d\n", temp_pdata[i]);	
	}	
	mutex_unlock(&data->update_lock);
	
	/* pdata sorting */
	for (i = 0; i < ArySize - 1; i++)
		for (j = i+1; j < ArySize; j++)
			if (temp_pdata[i] > temp_pdata[j])
				pa12_swap(temp_pdata + i, temp_pdata + j);	
	
	/* calculate the cross-talk using central 10 data */
	for (i = 5; i < 15; i++) 
	{
		//APS_LOG("%s: temp_pdata = %d\n", __func__, temp_pdata[i]);
		sum_of_pdata = sum_of_pdata + temp_pdata[i];
	}

	data->crosstalk = sum_of_pdata/10;
    	APS_LOG("%s: sum_of_pdata = %d   cross_talk = %d\n",
                        __func__, sum_of_pdata, data->crosstalk);
	
	/* Restore CFG2 (Normal mode) and Measure base x-talk */
	mutex_lock(&data->update_lock);
	ret = hwmsen_write_byte(client, REG_CFG0, cfg0data);
	ret = hwmsen_write_byte(client, REG_CFG2, cfg2data | 0xC0); 
	mutex_unlock(&data->update_lock);
 	
	if (data->crosstalk > PA12_PS_OFFSET_MAX)
	{
		APS_LOG("%s: invalid calibrated data\n", __func__);

		if(cal_check_flag == 0)
		{
			APS_LOG("%s: RECALIBRATION start\n", __func__);
			cal_check_flag = 1;
			goto RECALIBRATION;
		}
		else
		{
			APS_LOG("%s: CALIBRATION FAIL -> "
                               "cross_talk is set to DEFAULT\n", __func__);
			data->crosstalk = PA12_PS_OFFSET_DEFAULT;
			ret = hwmsen_write_byte(client, REG_PS_OFFSET, data->crosstalk);
			calibration_flag = 0;
			return -EINVAL;
         	}
	}	
/* Deleted by huangzifan at 2016.05.16. 
	if( data->crosstalk == 0){
		 APS_LOG("factroy calibration is not normal, data->crosstalk = 0\n");
		 calibration_flag = 0;
		 return -EINVAL;
	}
*/	
	data->crosstalk += PA12_PS_OFFSET_EXTRA;

CROSSTALKBASE_RECALIBRATION:
		
	
	mutex_lock(&data->update_lock);
	
	/*PS On*/
	ret = hwmsen_write_byte(client, REG_CFG0, cfg0data | 0x03); 

	/*Write offset value to register 0x10*/
	ret = hwmsen_write_byte(client, REG_PS_OFFSET, data->crosstalk);
	
	for(i = 0; i < 10; i++)
	{
		mdelay(50);
		ret = hwmsen_read_byte(client,REG_PS_DATA,temp_pdata+i);
		//APS_LOG("temp_data = %d\n", temp_pdata[i]);	
	}	

	mutex_unlock(&data->update_lock);
 
	/* calculate the cross-talk_base using central 5 data */

	sum_of_pdata = 0;

	for (i = 3; i < 8; i++) 
	{
		//APS_LOG("%s: temp_pdata = %d\n", __func__, temp_pdata[i]);
		sum_of_pdata = sum_of_pdata + temp_pdata[i];
	}
	
	data->crosstalk_base = sum_of_pdata/5;
    	APS_LOG("%s: sum_of_pdata = %d   cross_talk_base = %d\n",
                        __func__, sum_of_pdata, data->crosstalk_base);

	if(data->crosstalk_base > 0) 
	{
		data->crosstalk += 1;
		goto CROSSTALKBASE_RECALIBRATION;
	}
	
		 calibration_flag = 1;
	
  	 /* Restore CFG0  */
	mutex_lock(&data->update_lock);
	ret = hwmsen_write_byte(client, REG_CFG0, cfg0data);
	mutex_unlock(&data->update_lock);

	APS_LOG("%s: FINISH proximity sensor calibration\n", __func__);
	/*Write x-talk info to file*/  
	buftemp[0]=data->crosstalk;
    	buftemp[1]=data->crosstalk_base;
	return data->crosstalk;
/*
	if(pa122_write_file(PS_CAL_FILE_PATH,buftemp) < 0)
	{
		APS_LOG("Open PS x-talk calibration file error!!");
		return -1;
	}
	else
	{
		APS_LOG("Open PS x-talk calibration file Success!!");
		pa122_has_load_cal_file = 0;
		return data->crosstalk;
	}
*/
	
}
static int pa122_run_fast_calibration(struct i2c_client *client)
{

	struct pa122_priv *data = i2c_get_clientdata(client);
	int i = 0;
	int j = 0;	
	u16 sum_of_pdata = 0;
	u8  xtalk_temp = 0;
    	u8 temp_pdata[4], cfg0data = 0,cfg2data = 0,cfg3data = 0,xtalk_uplimit,cfg1data = 0;
   	unsigned int ArySize = 4;
	int res = 0;
	int diff_cnt = 0;	
	u16 als;

	if(bCal_flag & PA12_FAST_CAL_ONCE)
	{
		APS_LOG("Ignore Fast Calibration\n");
		return data->crosstalk;
	}
	
   	APS_LOG("START proximity sensor fast calibration\n");

	mutex_lock(&data->update_lock);

	hwmsen_read_byte(client, REG_CFG0, &cfg0data);
	hwmsen_read_byte(client, REG_CFG2, &cfg2data);
	hwmsen_read_byte(client, REG_CFG3, &cfg3data);

//	APS_DBG("huangzifan1 cfg3data = %d\n", cfg3data);	
	
	/*PS On*/
	res = hwmsen_write_byte(client, REG_CFG0, cfg0data | 0x03); 
	if( res < 0){
		APS_DBG("hwmsen_write_byte error 1\n");
	}
	
	/*Offset mode & disable intr from ps*/
	res = hwmsen_write_byte(client, REG_CFG2, cfg2data & 0x33); 
	if( res < 0){
		APS_DBG("hwmsen_write_byte error 2\n");
	}
	
	/*PS sleep time 6.5ms */
	res = hwmsen_write_byte(client, REG_CFG3, cfg3data & 0xC7); 	
	if( res < 0){
		APS_DBG("hwmsen_write_byte error 3\n");
	}
	
	/*Set crosstalk = 0*/
	res = hwmsen_write_byte(client, REG_PS_OFFSET, 0x00); 
	if( res < 0){
		APS_DBG("hwmsen_write_byte error 4\n");
	}

	hwmsen_read_byte(client, REG_CFG3, &cfg3data);

	//Modify by huangzifan 2015.9.29

	if( first_time == 0)
	{
		mdelay(100);
		first_time = 1;
	}
	mdelay(35);	
	for(i = 0; i < 4; i++)
	{
		mdelay(15);
		hwmsen_read_byte(client,REG_PS_DATA,temp_pdata+i);
		APS_DBG("huangzifan temp_data = %d\n", temp_pdata[i]);	
		if ( i > 0 )		
			diff_cnt += abs(temp_pdata[i] - temp_pdata[i-1]);								
	}	

	mutex_unlock(&data->update_lock);
	pa122_read_als(client,&als);	
	APS_ERR("huangzifan als_data = %d\n", als);	
	if (als>SUN_LIGHT_CONT)
	{
		APS_DBG("huangzifan at sun light mode\n");	
		mutex_lock(&data->update_lock);
		hwmsen_write_byte(client, REG_SUNLIGHT_MODE, 0x0C);
		hwmsen_read_byte(client, REG_CFG1, &cfg1data);
		cfg1data &= 0x0F;
		hwmsen_write_byte(client,REG_CFG1, cfg1data );		
		cfg2data &= 0xBF;
		hwmsen_write_byte(client, REG_CFG2, cfg2data);
		mutex_unlock(&data->update_lock);
		return pa122_obj->crosstalk;
	}		
	
	/* pdata sorting */
	for (i = 0; i < ArySize - 1; i++)
		for (j = i+1; j < ArySize; j++)
			if (temp_pdata[i] > temp_pdata[j])
				pa12_swap(temp_pdata + i, temp_pdata + j);	

	/* calculate the cross-talk using central 2 data */
	for (i = 1; i < 3; i++) 
	{
		//APS_LOG("%s: temp_pdata = %d\n", __func__, temp_pdata[i]);
		sum_of_pdata = sum_of_pdata + temp_pdata[i];
	}

	xtalk_temp = sum_of_pdata/2;
   	APS_LOG("%s: sum_of_pdata = %d   cross_talk = %d\n",
                        __func__, sum_of_pdata, data->crosstalk);
	
	/* Restore Data */
	mutex_lock(&data->update_lock);
	hwmsen_write_byte(client, REG_CFG0, cfg0data);
	hwmsen_write_byte(client, REG_CFG2, cfg2data | 0xC0); //make sure return normal mode
	hwmsen_write_byte(client, REG_CFG3, cfg3data);
	mutex_unlock(&data->update_lock);

    xtalk_uplimit = (data->crosstalk + PS_CALIB_LIMIT) > PA12_PS_OFFSET_MAX ? ( data->crosstalk + PS_CALIB_LIMIT) : PA12_PS_OFFSET_MAX ;
	if (xtalk_temp > (data->crosstalk - PA12_PS_OFFSET_EXTRA )&& xtalk_temp < xtalk_uplimit  && diff_cnt < 10)
	{ 	
		APS_LOG("Fast calibrated data=%d\n",xtalk_temp);
		bCal_flag=1;
		/* Write offset value to 0x10 */
		mutex_lock(&data->update_lock);
		hwmsen_write_byte(client, REG_PS_OFFSET, (xtalk_temp + PA12_PS_OFFSET_EXTRA));
		mutex_unlock(&data->update_lock);
		return (xtalk_temp + PA12_PS_OFFSET_EXTRA);
	}
	else
	{
		APS_LOG("Fast calibration fail, xtalk=%d\n",xtalk_temp);
		
		mutex_lock(&data->update_lock);

		if(PA12_FAST_CAL_ONCE)
		{
			if(xtalk_temp >= PA12_PS_OFFSET_MAX)
			hwmsen_write_byte(client, REG_PS_OFFSET, xtalk_temp + PA12_PS_OFFSET_EXTRA);
			else
			hwmsen_write_byte(client, REG_PS_OFFSET, PA12_PS_OFFSET_DEFAULT);
		}
		else
		{
			hwmsen_write_byte(client, REG_PS_OFFSET, data->crosstalk);
			xtalk_temp = data->crosstalk;
		}
		mutex_unlock(&data->update_lock);
		
		return xtalk_temp;
        }   	

}
/**********************************************************************************************/
static void pa122_power(struct alsps_hw *hw, unsigned int on) 
{
#ifdef __USE_LINUX_REGULATOR_FRAMEWORK__
#else

	static unsigned int power_on = 0;

	APS_LOG("power %s\n", on ? "on" : "off");

	if(hw->power_id != POWER_NONE_MACRO)
	{
		if(power_on == on)
		{
			APS_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "PA122")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "PA122")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}
	power_on = on;
#endif
}

/********************************************************************/
int pa122_enable_ps(struct i2c_client *client, int enable)
{
	struct pa122_priv *obj = i2c_get_clientdata(client);
	int res;
	u8 regdata0 = 0;
	u8 regdata2 = 0;
	u8 sendvalue = 0;
	hwm_sensor_data sensor_data;
	struct file  *fop;

	if ( obj->ps_enable_flag == 1 && enable == 1 )
	{
		APS_ERR("PS already enable \n");
		return -1;
	}
	obj->ps_enable_flag = enable;
	if( ( first_enable_flag == 1 ) && (enable == 1) && ( ps_offset_flag == 0)){
		first_enable_flag = 0;
		res = set_crosstalk(4);
		if(res < 0 ){
			APS_ERR("set_crosstalk error!\n");
		}
	}
	mutex_lock(&obj->update_lock);
	res = hwmsen_read_byte(client, REG_CFG0, &regdata0); 
	mutex_unlock(&obj->update_lock);	

	if(res<0)
	{
		APS_ERR("i2c_read function err\n");
		return -1;
	}

	if(enable == 1)
	{
	//Modify by huangzifan 2015.9.29
	//	APS_LOG("pa122 enable ps sensor\n");		
	/*		
		if(pa122_has_load_cal_file == 0)
		{
			pa122_has_load_cal_file = 1;
			fop = filp_open(PS_CAL_FILE_PATH, O_RDONLY, 0);
			if(IS_ERR(fop))
			{
				APS_ERR("pa122_enable_ps: open file error!! Path = %s\n", PS_CAL_FILE_PATH);
				if (PA12_FAST_CAL == 0)
					pa122_run_calibration(client);
			}
			else
			{
				filp_close(fop, NULL);
				pa122_load_calibration_param(client);
			}
			msleep(5);
		}
	*/
		if(PA12_FAST_CAL){
				pa122_run_fast_calibration(client);
			}
		/**** SET INTERRUPT FLAG AS FAR ****/
		if(obj->hw->polling_mode_ps == 0)
		{
			if(intr_flag == 0)
			{						
				intr_flag = 1; 
				if(PA12_INT_TYPE == 0)
				{
					mutex_lock(&obj->update_lock);
					hwmsen_write_byte(client, REG_PS_TL, PA12_PS_TH_MIN);
					hwmsen_write_byte(client, REG_PS_TH, obj->ps_thrd_high);
					hwmsen_write_byte(client,REG_CFG1,
							(PA12_LED_CURR << 4)| (PA12_PS_PRST << 2)| (PA12_ALS_PRST));
					mutex_unlock(&obj->update_lock);
				}
				else if(PA12_INT_TYPE == 1)
				{
				#if defined(CONFIG_OF)
					irq_set_irq_type(obj->irq, IRQ_TYPE_LEVEL_LOW);
				#elif defined(CUST_EINT_ALS_TYPE)
					mt_eint_set_polarity(CUST_EINT_ALS_NUM, 0); 
				#endif


					res = hwmsen_read_byte(client,REG_CFG2,&regdata2);		
					regdata2=regdata2 & 0xFD ; 
					mutex_lock(&obj->update_lock);
					res = hwmsen_write_byte(client,REG_CFG2,regdata2);
					res = hwmsen_write_byte(client,REG_CFG1,
								(PA12_LED_CURR << 4)| (PA12_PS_PRST << 2)| (PA12_ALS_PRST));
					mutex_unlock(&obj->update_lock);
				}

			}
				
			sensor_data.values[0] = intr_flag;
			sensor_data.value_divide = 1;
			sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;	
	//Modify by huangzifan 2015.10.22
	#ifdef OLD_ARCH
			if((res = ps_report_interrupt_data(sensor_data.values[0])))
			{
				APS_ERR("call ps_report_interrupt_data fail1 = %d\n", res);
			}
	#endif
	
	#ifdef NEW_ARCH
		if((res = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
		  	APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", res);
		}
	#endif
		}
		/***********************************/
		//APS_LOG("CFG0 Status: %d\n",regdata);
		//sendvalue = regdata & 0xFD;
		/* PS On */
		sendvalue = regdata0 | 0x03;

		mutex_lock(&obj->update_lock);
		res=hwmsen_write_byte(client,REG_CFG0,sendvalue); 
		mutex_unlock(&obj->update_lock);

		if(res<0)
		{
			APS_ERR("i2c_write function err\n");
			return res;
		}

		atomic_set(&obj->ps_deb_on, 1);
		atomic_set(&obj->ps_deb_end, jiffies+atomic_read(&obj->ps_debounce)/(1000/HZ));
	}
	else
	{
		APS_LOG("pa122 disaple ps sensor\n");
		
		//APS_LOG("CFG0 Status: %d\n",regdata);
		/* PS Off */
		sendvalue=regdata0 & 0xFC; 

		mutex_lock(&obj->update_lock);				
		res=hwmsen_write_byte(client,REG_CFG0,sendvalue); 
		hwmsen_write_byte(client,REG_SUNLIGHT_MODE,0x00);
		hwmsen_write_byte(client,REG_CFG1,
			(PA12_LED_CURR << 4)| (PA12_PS_PRST << 2)| (PA12_ALS_PRST));
		hwmsen_write_byte(client,REG_CFG2,
			(PA12_PS_MODE << 6)| (PA12_PS_SET << 2));		
		mutex_unlock(&obj->update_lock);

		if(res<0)
		{
			APS_ERR("i2c_write function err\n");
			return res;
		}	  	
		atomic_set(&obj->ps_deb_on, 0);
	}		
	APS_DBG("pa122 enable/disaple ps sensor finished!\n"); //add for test by wxj 2014.5.5

	return 0;
}


/********************************************************************/
int pa122_enable_als(struct i2c_client *client, int enable)
{
	struct pa122_priv *obj = i2c_get_clientdata(client);
	int res;
	u8 regdata=0;
	u8 sendvalue=0;

	mutex_lock(&obj->update_lock);
	res=hwmsen_read_byte(client,REG_CFG0,&regdata); 
	mutex_unlock(&obj->update_lock);

	if(res<0)
	{
		APS_ERR("i2c_read function err\n");
		return -1;
	}	

	//APS_LOG("CFG0 Status: %d\n",regdata);

	if(enable == 1)
	{
		APS_LOG("pa122 enable als sensor\n");
		//sendvalue=regdata & 0xFE; 
		/* ALS On */
		sendvalue=regdata | 0x01; 

		mutex_lock(&obj->update_lock);
		res=hwmsen_write_byte(client,REG_CFG0,sendvalue); 
		mutex_unlock(&obj->update_lock);

		if(res<0)
		{
			APS_ERR("i2c_write function err\n");
			return res;
		}


		atomic_set(&obj->als_deb_on, 1);
		atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
	}
	else
	{
		APS_LOG("pa122 disaple als sensor\n");
		/* ALS Off */
		sendvalue=regdata & 0xFE; 

		mutex_lock(&obj->update_lock);
		res=hwmsen_write_byte(client,REG_CFG0,sendvalue);
		mutex_unlock(&obj->update_lock);
		if(res<0)
		{
			APS_ERR("i2c_write function err\n");
			return res;
		}

		atomic_set(&obj->als_deb_on, 0);
	}
	APS_LOG("pa122 enable/disaple als sensor ok\n"); //add for test by wxj 2014.5.5

	return 0;
}

/********************************************************************/
int pa122_read_ps(struct i2c_client *client, u8 *data)
{
	int res;
	struct pa122_priv *obj = i2c_get_clientdata(client);
	
	if (atomic_read(&obj->trace) & CMC_TRC_CVT_PS)	
	{
		APS_FUN(f);
	}
	
	mutex_lock(&pa122_obj->update_lock);
	res = hwmsen_read_byte(client, REG_PS_DATA, data); 
	mutex_unlock(&pa122_obj->update_lock);

	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
	}
	if (atomic_read(&obj->trace) & CMC_TRC_CVT_PS)	
	{
		APS_LOG("PA122_PS_DATA value = %x\n",*data);	
	}
	return res;
}
/********************************************************************/
int pa122_read_als(struct i2c_client *client, u16 *data)
{
	int res;
	u8 dataLSB;
	u8 dataMSB;
	u16 count;
	struct pa122_priv *obj = i2c_get_clientdata(client);
	if (atomic_read(&obj->trace) & CMC_TRC_CVT_ALS) 
	{
		APS_FUN(f);
	}
	
	mutex_lock(&pa122_obj->update_lock);
	res = hwmsen_read_byte(client, REG_ALS_DATA_LSB, &dataLSB); 
	mutex_unlock(&pa122_obj->update_lock);

	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
		return res;
	}

	mutex_lock(&pa122_obj->update_lock);
	res = hwmsen_read_byte(client, REG_ALS_DATA_MSB, &dataMSB);
	mutex_unlock(&pa122_obj->update_lock);

	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
		return res;
	}

	count = ((dataMSB<<8)|dataLSB);

	if (atomic_read(&obj->trace) & CMC_TRC_CVT_ALS)	
	{
		APS_LOG("PA122_ALS_DATA count=%d\n ",count);
	}
	
	*data = count;

	return 0;
}

/**Change to near/far ****************************************************/
static int pa122_get_ps_value(struct pa122_priv *obj, u8 ps)
{
	int val = 0;
	int invalid = 0;
	int mask = atomic_read(&obj->ps_mask);

	if(ps > obj->ps_thrd_high)
	{
		val = 0;  /*close*/
		prevObj=0;
		//return 0;
	}
	else if(ps < obj->ps_thrd_low)
	{
		val = 1;  /*far away*/
		prevObj=1;
		//return 1;
	}
	else
	{
		val = prevObj;	//no change
	}

	//return prevObj;


	if(atomic_read(&obj->ps_suspend))
	{
		invalid = 1;
	}

	else if(1 == atomic_read(&obj->ps_deb_on))
	{
		unsigned long endt = atomic_read(&obj->ps_deb_end);

		if(time_after(jiffies, endt))
		{
			atomic_set(&obj->ps_deb_on, 0);
		}

		if (1 == atomic_read(&obj->ps_deb_on))
		{
			invalid = 1;
		}
	}

	if(!invalid)
	{
		if(atomic_read(&obj->trace) & CMC_TRC_CVT_PS)
		{
			if(mask)
			{
				APS_DBG("PS:  %05d => %05d [M] \n", ps, val);
			}
			else
			{
				APS_DBG("PS:  %05d => %05d\n", ps, val);
			}
		}
		if(0 == test_bit(CMC_BIT_PS,  &obj->enable))
		{
			//if ps is disable do not report value
			APS_DBG("PS: not enable and do not report this value\n");
			return -1;
		}
		else
		{
			return val;
		}
	}	
	else
	{
		if(atomic_read(&obj->trace) & CMC_TRC_CVT_PS)
		{
			APS_DBG("PS:  %05d => %05d (-1)\n", ps, val);    
		}
		return -1;
	}
}

/**Change to luxr************************************************/
static int pa122_get_als_value(struct pa122_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;	
	u64 lux=0;

	for(idx = 0; idx < obj->als_level_num; idx++)
	{
		if(als < obj->hw->als_level[idx])
		{
			break;
		}
	}
	if(idx >= obj->als_value_num)
	{
		APS_ERR("exceed range\n"); 
		idx = obj->als_value_num - 1;
	}

	if(1 == atomic_read(&obj->als_deb_on))
	{
		unsigned long endt = atomic_read(&obj->als_deb_end);

		if(time_after(jiffies, endt))
		{
			atomic_set(&obj->als_deb_on, 0);
		}

		if(1 == atomic_read(&obj->als_deb_on))
		{
			invalid = 1;
		}
	}

	if(!invalid)
	{
		if (atomic_read(&obj->trace) & CMC_TRC_CVT_ALS)	
		{
			APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);
		}
		if(PA12_ALS_ADC_TO_LUX_USE_LEVEL)
		{
			return obj->hw->als_value[idx];
		}
		else
		{
			lux = (als * PA12_ALS_ADC_TO_LUX_NUMERATOR) / PA12_ALS_ADC_TO_LUX_DENOMINATOR;		
			if(lux > 10240)		    
				return 10240;  
			else 
				return (int)lux;
		}
	}
	else
	{
		if(atomic_read(&obj->trace) & CMC_TRC_CVT_ALS)
		{
			APS_DBG("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);	  
		}
		return -1;
	}
}


/*-------------------------------attribute file for debugging----------------------------------*/

/******************************************************************************
 * Sysfs attributes
*******************************************************************************/
static ssize_t pa122_show_version(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}
	
	res = snprintf(buf, PAGE_SIZE, ".H Ver: %s\n.C Ver: %s\n",PA12_DRIVER_VERSION_H,PA12_DRIVER_VERSION_C); 
	return res;    
}
static ssize_t pa122_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}
	
	res = snprintf(buf, PAGE_SIZE, "(%d %d %d %d %d)\n", 
		atomic_read(&pa122_obj->i2c_retry), atomic_read(&pa122_obj->als_debounce), 
		atomic_read(&pa122_obj->ps_mask), atomic_read(&pa122_obj->ps_thd_val), atomic_read(&pa122_obj->ps_debounce));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_store_config(struct device_driver *ddri, const char *buf, size_t count)
{
	int retry, als_deb, ps_deb, mask, thres;
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return -EINVAL;
	}
	
	if(5 == sscanf(buf, "%d %d %d %d %d", &retry, &als_deb, &mask, &thres, &ps_deb))
	{ 
		atomic_set(&pa122_obj->i2c_retry, retry);
		atomic_set(&pa122_obj->als_debounce, als_deb);
		atomic_set(&pa122_obj->ps_mask, mask);
		atomic_set(&pa122_obj->ps_thd_val, thres);        
		atomic_set(&pa122_obj->ps_debounce, ps_deb);
	}
	else
	{
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&pa122_obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_store_trace(struct device_driver *ddri, const char *buf, size_t count)
{
    int trace;
    if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return -EINVAL;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&pa122_obj->trace, trace);
	}
	else 
	{
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_als(struct device_driver *ddri, char *buf)
{
	int res;
	
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}
	if((res = pa122_read_als(pa122_obj->client, &pa122_obj->als)))
	{
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	}
	else
	{
		return snprintf(buf, PAGE_SIZE, "%d\n", pa122_obj->als);     
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_ps(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}
	
	if((res = pa122_read_ps(pa122_obj->client, &pa122_obj->ps)))
	{
		return snprintf(buf, PAGE_SIZE, "ERROR: %zd\n", res);
	}
	else
	{
		return snprintf(buf, PAGE_SIZE, "%d\n", pa122_obj->ps);     
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_reg(struct device_driver *ddri, char *buf)
{
	u8 regdata;
	int res=0;
	int count=0;
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}
	int i=0	;

	mutex_lock(&pa122_obj->update_lock);
	for(i;i <17 ;i++)
	{
		res=hwmsen_read_byte(pa122_obj->client,0x00+i,&regdata);

		if(res<0)
		{
		   break;
		}
		else
		count+=sprintf(buf+count,"[%x] = (%x)\n",0x00+i,regdata);
	}
	mutex_unlock(&pa122_obj->update_lock);

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_send(struct device_driver *ddri, char *buf)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_store_send(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr, cmd;


	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return -EINVAL;
	}
	else if(2 != sscanf(buf, "%x %x", &addr, &cmd))
	{
		APS_ERR("invalid format: '%s'\n", buf);
		return -EINVAL;
	}

	mutex_lock(&pa122_obj->update_lock);		
	hwmsen_write_byte(pa122_obj->client,addr,cmd);
	mutex_unlock(&pa122_obj->update_lock);
	//****************************
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_recv(struct device_driver *ddri, char *buf)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_store_recv(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr;
	//u8 dat;
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return -EINVAL;
	}
	else if(1 != sscanf(buf, "%x", &addr))
	{
		APS_ERR("invalid format: '%s'\n", buf);
		return -EINVAL;
	}

	//****************************
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}
	
	if(pa122_obj->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d, (%d %d)\n", 
			pa122_obj->hw->i2c_num, pa122_obj->hw->power_id, pa122_obj->hw->power_vol);
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	
	len += snprintf(buf+len, PAGE_SIZE-len, "REGS: %02X %02X %02X %02lX %02lX\n", 
				atomic_read(&pa122_obj->als_cmd_val), atomic_read(&pa122_obj->ps_cmd_val), 
				atomic_read(&pa122_obj->ps_thd_val),pa122_obj->enable, pa122_obj->pending_intr);
	
	len += snprintf(buf+len, PAGE_SIZE-len, "MISC: %d %d\n", atomic_read(&pa122_obj->als_suspend), atomic_read(&pa122_obj->ps_suspend));

	return len;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
/*----------------------------------------------------------------------------*/
static int read_int_from_buf(struct pa122_priv *obj, const char* buf, size_t count, u32 data[], int len)
{
	int idx = 0;
	char *cur = (char*)buf, *end = (char*)(buf+count);

	while(idx < len)
	{
		while((cur < end) && IS_SPACE(*cur))
		{
			cur++;        
		}

		if(1 != sscanf(cur, "%d", &data[idx]))
		{
			break;
		}

		idx++; 
		while((cur < end) && !IS_SPACE(*cur))
		{
			cur++;
		}
	}
	return idx;
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}
	
	for(idx = 0; idx < pa122_obj->als_level_num; idx++)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", pa122_obj->hw->als_level[idx]);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;    
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_store_alslv(struct device_driver *ddri, const char *buf, size_t count)
{
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return -EINVAL;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(pa122_obj->als_level, pa122_obj->hw->als_level, sizeof(pa122_obj->als_level));
	}
	else if(pa122_obj->als_level_num != read_int_from_buf(pa122_obj, buf, count, 
			pa122_obj->hw->als_level, pa122_obj->als_level_num))
	{
		APS_ERR("invalid format: '%s'\n", buf);
	}    
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}
	
	for(idx = 0; idx < pa122_obj->als_value_num; idx++)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", pa122_obj->hw->als_value[idx]);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;    
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_store_alsval(struct device_driver *ddri, const char *buf, size_t count)
{
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return -EINVAL;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(pa122_obj->als_value, pa122_obj->hw->als_value, sizeof(pa122_obj->als_value));
	}
	else if(pa122_obj->als_value_num != read_int_from_buf(pa122_obj, buf, count, 
			pa122_obj->hw->als_value, pa122_obj->als_value_num))
	{
		APS_ERR("invalid format: '%s'\n", buf);
	}    
	return count;
}

/*---Offset At-------------------------------------------------------------------------*/
static ssize_t pa122_show_ps_offset(struct device_driver *ddri, char *buf)
{
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", pa122_obj->crosstalk);     

}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_set_ps_offset(struct device_driver *ddri, const char *buf, size_t count)
{
	int ret;
	ret = pa122_run_calibration(pa122_obj->client);
	return ret;
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_reg_add(struct device_driver *ddri, char *buf)
{return 0;}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_reg_value(struct device_driver *ddri, char *buf)
{return 0;}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_store_dev_init(struct device_driver *ddri, const char *buf, size_t count)
{
	int ret;
	ret = pa122_init_client(pa122_obj->client);
	return count;
}
/*----------------------------------------------------------------------------*/
/* PS Threshold Calibration */
static ssize_t pa122_store_pthreshold_calibration(struct device_driver *ddri, const char *buf, size_t count)
{
	pa122_thrd_calibration(pa122_obj->client);	
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_show_pthreshold_calibration(struct device_driver *ddri, char *buf)
{
	struct pa122_priv *data = i2c_get_clientdata(pa122_obj->client);
	
	return sprintf(buf, "Low threshold = %d , High threshold = %d\n",data->ps_thrd_low,data->ps_thrd_high);	
}
/*----------------------------------------------------------------------------*/
/* X-talk Calibration file */
static ssize_t pa122_show_calibration_file(struct device_driver *ddri, char *buf)
{
    struct file  *fop;
    mm_segment_t old_fs;
    u8 readtemp[4];

    fop = filp_open(PS_CAL_FILE_PATH,O_RDONLY,0);
    if(IS_ERR(fop)){
        APS_ERR("Filp_open error!!");
        return sprintf(buf, "Open File Error\n");
    }

     old_fs = get_fs();  
     set_fs(get_ds()); //set_fs(KERNEL_DS);  
	     
     fop->f_op->llseek(fop,0,0);
     fop->f_op->read(fop, readtemp, strlen(readtemp), &fop->f_pos);     

     set_fs(old_fs);  

     filp_close(fop,NULL);
     return sprintf(buf, "X-talk data= %d\n",readtemp[0]);
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_store_calibration_file(struct device_driver *ddri, const char *buf, size_t count)
{
    struct file  *fop;
    mm_segment_t old_fs;	 
    u8 tempbuf[2];
    int temp;
    sscanf(buf, "%d",&temp); 
    tempbuf[0]=(u8)temp;

    fop = filp_open(PS_CAL_FILE_PATH,O_CREAT | O_RDWR,0644);
    if(IS_ERR(fop)){
        APS_ERR("Test: filp_open error!!");
        return -1;
    }

     old_fs = get_fs();  
     set_fs(get_ds()); //set_fs(KERNEL_DS);  
     fop->f_op->write(fop, (char *)tempbuf, sizeof(tempbuf), &fop->f_pos);  
     set_fs(old_fs);  

     filp_close(fop,NULL);

    return count;
}
/*----------------------------------------------------------------------------*/
/* Threshold Calibration file */
static ssize_t pa122_show_pthrd_file(struct device_driver *ddri, char *buf)
{
     u8 readtemp[2];
     if(pa122_read_file(THRD_CAL_FILE_PATH,readtemp)<0){
  
       return sprintf(buf, "Open File Error\n");
     }
     
     return sprintf(buf, "Low threshold = %d , High threshold = %d\n",readtemp[0],readtemp[1]);
}
/*----------------------------------------------------------------------------*/
static ssize_t pa122_store_pthrd_file(struct device_driver *ddri, const char *buf, size_t count)
{	 
    u8 buftemp[2];
    int temp[2];


    if(2 != sscanf(buf, "%d %d",&temp[0],&temp[1]))
    {
	APS_ERR("invalid format: '%s'\n", buf);
	return -1;
    }
    buftemp[0]=(u8)temp[0];
    buftemp[1]=(u8)temp[1];

    if(pa122_write_file(THRD_CAL_FILE_PATH,buftemp)<0)
    	APS_LOG("Create PS Thredhold calibration file error!!");
    else
    	APS_LOG("Create PS Thredhold calibration file Success!!");
 
    return count;
} 
/*---------------------------------------------------------------------------------------*/
static DRIVER_ATTR(version,     S_IRUGO, pa122_show_version, NULL);
static DRIVER_ATTR(als,     S_IRUGO, pa122_show_als, NULL);
static DRIVER_ATTR(ps,      S_IRUGO, pa122_show_ps, NULL);
static DRIVER_ATTR(config,  S_IWUGO | S_IRUGO, pa122_show_config,	pa122_store_config);
static DRIVER_ATTR(alslv,   S_IWUGO | S_IRUGO, pa122_show_alslv, pa122_store_alslv);
static DRIVER_ATTR(alsval,  S_IWUGO | S_IRUGO, pa122_show_alsval, pa122_store_alsval);
static DRIVER_ATTR(trace,   S_IWUGO | S_IRUGO, pa122_show_trace,		pa122_store_trace);
static DRIVER_ATTR(status,  S_IRUGO, pa122_show_status, NULL);
static DRIVER_ATTR(send,    S_IWUGO | S_IRUGO, pa122_show_send, pa122_store_send); // No func
static DRIVER_ATTR(recv,    S_IWUGO | S_IRUGO, pa122_show_recv, pa122_store_recv);    // No func
static DRIVER_ATTR(reg,     S_IRUGO, pa122_show_reg, NULL);

static DRIVER_ATTR(pscalibration, S_IWUGO | S_IRUGO, pa122_show_ps_offset,pa122_set_ps_offset);
static DRIVER_ATTR(dev_init,S_IWUGO | S_IRUGO, NULL, pa122_store_dev_init);
static DRIVER_ATTR(pthredcalibration, S_IRUGO,pa122_show_pthreshold_calibration, NULL);

//static DRIVER_ATTR(pthredcalibration, S_IWUGO | S_IRUGO,pa122_show_pthreshold_calibration, pa122_store_pthreshold_calibration);
static DRIVER_ATTR(xtalk_param, S_IWUGO | S_IRUGO,pa122_show_calibration_file, pa122_store_calibration_file);
static DRIVER_ATTR(pthrd_param, S_IWUGO | S_IRUGO,pa122_show_pthrd_file, pa122_store_pthrd_file);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *pa122_attr_list[] = {
	&driver_attr_version,    
	&driver_attr_als,
    &driver_attr_ps,    
    &driver_attr_trace,        /*trace log*/
  //  &driver_attr_config,
    &driver_attr_alslv,
    &driver_attr_alsval,
    &driver_attr_status,
    &driver_attr_send,
    &driver_attr_recv,
    &driver_attr_reg,
    &driver_attr_pscalibration,
    &driver_attr_dev_init,
    &driver_attr_pthredcalibration,
  //  &driver_attr_xtalk_param,
//    &driver_attr_pthrd_param,
};

/*----------------------------------------------------------------------------*/
static int pa122_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(pa122_attr_list);
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, pa122_attr_list[idx])))
		{            
			APS_ERR("driver_create_file (%s) = %d\n", pa122_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
	static int pa122_delete_attr(struct device_driver *driver)
	{
	int idx ,err = 0;
	int num = (int)ARRAY_SIZE(pa122_attr_list);

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++) 
	{
		driver_remove_file(driver, pa122_attr_list[idx]);
	}
	
	return err;
}

/*----------------------------------interrupt functions--------------------------------*/

static int pa122_check_intr(struct i2c_client *client) 
{
	struct pa122_priv *obj = i2c_get_clientdata(client);
	int res;
	u8 databuf[2];
	u8 psdata=0;
	u8 cfgdata=0;
	int i;
	int Num = 5;
	u16 als = 0;

	
	APS_ERR("pa122_check_intr start...\n");//test
	mutex_lock(&obj->update_lock);		
	res = hwmsen_read_byte(client, REG_PS_DATA, &psdata);
	if (intr_flag == 1 )
	{
		for( i = 0 ; i < Num ; i++ )
		{
			res = hwmsen_read_byte(client, REG_PS_DATA, &psdata);
			APS_ERR("check psdata = %d\n",psdata);
			/*
			APS_ERR("check psdata = %d\n",psdata);
			mutex_unlock(&obj->update_lock);
			pa122_read_als(client,&als);	
			APS_ERR("huangzifan2 als_data = %d\n", als);	
			mutex_lock(&obj->update_lock);		
			*/
			if (psdata < obj->ps_thrd_high )
			{
				mutex_unlock(&obj->update_lock);
				goto ENTER_CLEAR_FLAG;
			}
			msleep(13);
		}
		
	}
/*
	if (intr_flag == 0 && psdata == 0  && first_zero_flag == 1)
	{
		first_zero_flag = 0;
		mutex_unlock(&obj->update_lock);
		goto ENTER_CLEAR_FLAG;
	}
*/
	mutex_unlock(&obj->update_lock);

	if(res<0)
	{
			APS_ERR("i2c_read function err res = %d\n",res);
			return -1;
	}
	APS_LOG("ps = %d\n",psdata);

	switch (PA12_INT_TYPE)
	{
		case 1: /* Hysteresis Type */

			if(psdata > obj->ps_thrd_high){
	 			intr_flag = 0;
				#if defined(CONFIG_OF)
					irq_set_irq_type(obj->irq, IRQ_TYPE_LEVEL_HIGH);
				#elif defined(CUST_EINT_ALS_TYPE)
            		mt_eint_set_polarity(CUST_EINT_ALS_NUM, 1);	
				#endif
				mutex_lock(&obj->update_lock);
				hwmsen_write_byte(client,REG_CFG1,
								(PA12_LED_CURR << 4)| (1 << 2)| (PA12_ALS_PRST));
				mutex_unlock(&obj->update_lock);
				if(atomic_read(&obj->trace) & CMC_TRC_CVT_PS)
				{
					APS_LOG("--Hysteresis Type----near------ps = %d\n",psdata);
				}
//				first_zero_flag = 1;
			}else if(psdata < obj->ps_thrd_low){
				intr_flag = 1;
				#if defined(CONFIG_OF)
					irq_set_irq_type(obj->irq, IRQ_TYPE_LEVEL_LOW);
				#elif defined(CUST_EINT_ALS_TYPE)
            		mt_eint_set_polarity(CUST_EINT_ALS_NUM, 0);	
				#endif

				mutex_lock(&obj->update_lock);
				hwmsen_write_byte(client,REG_CFG1,
								(PA12_LED_CURR << 4)| (PA12_PS_PRST << 2)| (PA12_ALS_PRST));
				mutex_unlock(&obj->update_lock);
				if(atomic_read(&obj->trace) & CMC_TRC_CVT_PS)
				{
					APS_LOG("--Hysteresis Type----far------ps = %d\n",psdata);
				}
			}
			/* No need to clear interrupt flag !! */
			goto EXIT_CHECK_INTR;
			break;			
		case 0: /* Window Type */
			
			if(intr_flag == 1){
				if(psdata > obj->ps_thrd_high){
					intr_flag = 0;

					mutex_lock(&obj->update_lock);
					hwmsen_write_byte(client, REG_PS_TL, obj->ps_thrd_low);
					hwmsen_write_byte(client, REG_PS_TH, PA12_PS_TH_MAX);
					hwmsen_write_byte(client,REG_CFG1,
								(PA12_LED_CURR << 4)| (1 << 2)| (PA12_ALS_PRST));
					mutex_unlock(&obj->update_lock);
					if(atomic_read(&obj->trace) & CMC_TRC_CVT_PS)
					{
						APS_LOG("--Window Type----near------ps = %d\n",psdata);
					}
				}
			}else if(intr_flag == 0){
				if(psdata < obj->ps_thrd_low){
					intr_flag = 1;

					mutex_lock(&obj->update_lock);
					hwmsen_write_byte(client, REG_PS_TL, PA12_PS_TH_MIN);
					hwmsen_write_byte(client, REG_PS_TH, obj->ps_thrd_high);
					hwmsen_write_byte(client,REG_CFG1,
								(PA12_LED_CURR << 4)| (PA12_PS_PRST << 2)| (PA12_ALS_PRST));
					mutex_unlock(&obj->update_lock);
					if(atomic_read(&obj->trace) & CMC_TRC_CVT_PS)
					{
						APS_LOG("--Window Type----far------ps = %d\n",psdata);
					}
				}		
			}
			break;
	}
ENTER_CLEAR_FLAG:
	/* Clear PS INT FLAG */
	mutex_lock(&obj->update_lock);
	res = hwmsen_read_byte(client, REG_CFG2, &cfgdata);
	mutex_unlock(&obj->update_lock);

	if(res<0)
	{
		APS_ERR("i2c_read function err res = %d\n",res);
		return -1;
	}
	cfgdata = cfgdata & 0xFD ; 
	mutex_lock(&obj->update_lock);
	res = hwmsen_write_byte(client,REG_CFG2,cfgdata);
	mutex_unlock(&obj->update_lock);
  	if(res<0)
  	{		
		APS_ERR("i2c_send function err res = %d\n",res);
		return -1;
	}
	APS_ERR("pa122_check_intr end...\n");//test	
EXIT_CHECK_INTR:
	return 0;
}
/*----------------------------------------------------------------------------*/
static void pa122_eint_work(struct work_struct *work)
{
	struct pa122_priv *obj = (struct pa122_priv *)container_of(work, struct pa122_priv, eint_work);
	hwm_sensor_data sensor_data;
	int res = 0;

#if 1
	APS_ERR("pa122_ps int work start...0\n");//test

	res = pa122_check_intr(obj->client);
//	APS_LOG("pa122_ps int work !\n");

	if(res != 0){
		goto EXIT_INTR_ERR;
	}else{
		sensor_data.values[0] = intr_flag;
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;	

	}
#ifdef NEW_ARCH
	if((res = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
	{
		  	APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", res);
	}
#endif
#ifdef OLD_ARCH
	if((res = ps_report_interrupt_data(sensor_data.values[0])))
	{	
		APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", res);
	}
#endif

#endif
#if defined(CONFIG_OF)
	enable_irq(obj->irq);
#elif defined(CUST_EINT_ALS_TYPE)
	mt_eint_unmask(CUST_EINT_ALS_NUM);
#endif	
	APS_ERR("pa122_ps int work finish...0\n");//test
	return;

EXIT_INTR_ERR:
#if defined(CONFIG_OF)
	enable_irq(obj->irq);
#elif defined(CUST_EINT_ALS_TYPE)
	mt_eint_unmask(CUST_EINT_ALS_NUM);
#endif	

	APS_ERR("pa122_eint_work err: %d\n", res);
}

/*----------------------------------------------------------------------------*/
static void pa122_eint_func(void)
{
	struct pa122_priv *obj = g_pa122_ptr;
	if(!obj)
	{
		return;
	}	
	schedule_work(&obj->eint_work);
}
#if defined(CONFIG_OF)
static irqreturn_t pa122_eint_handler(int irq, void *desc)
{
	struct pa122_priv *obj = g_pa122_ptr;
	/*APS_FUN(f);	//for debug */
	if(!obj)
		return IRQ_HANDLED;

	disable_irq_nosync(obj->irq);
	pa122_eint_func();
	
	return IRQ_HANDLED;
}
#endif

int pa122_setup_eint(struct i2c_client *client)
{
	struct pa122_priv *obj = i2c_get_clientdata(client);        

#if defined(CONFIG_OF)
	int ints[2] = {0};
	int err = 0;
#endif
	if(obj == NULL){
		APS_ERR("epl259x_obj is null!\n");
		return -EINVAL;
	}

	g_pa122_ptr = obj;

	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, GPIO_PULL_ENABLE);
	//mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);

//	APS_DBG("huangzifan pa122_setup_eint\n");
#if defined(CONFIG_OF)
	obj->irq_node = of_find_compatible_node(NULL, NULL, "mediatek, ALS-eint");

	if(obj->irq_node != NULL){
		of_property_read_u32_array(obj->irq_node, "debounce", ints, ARRAY_SIZE(ints));
		APS_LOG("ins[0] = %d, ints[1] = %d\n", ints[0], ints[1]);
		mt_gpio_set_debounce(ints[0], ints[1]);

		obj->irq = irq_of_parse_and_map(obj->irq_node, 0);
		if(obj->irq != 0){
			err = request_irq(obj->irq, pa122_eint_handler, IRQF_TRIGGER_NONE, "ALS-eint", NULL);
			if(err < 0){
				APS_ERR("request_irq failed!\n");
				return err;
			}else{
			//	enable_irq(obj->irq);
			}
		}else{
			APS_ERR("irq_of_parse_and_map failed!\n");
			return -EFAULT;
		}
	}else{
		APS_ERR("obj->irq_node is null!\n");
		return -EFAULT;
	}
#elif defined(CUST_EINT_ALS_TYPE)
	mt_eint_set_hw_debounce(GPIO_ALS_EINT_PIN, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, pa122_eint_func, 0);
	mt_eint_mask(CUST_EINT_ALS_NUM);

#endif

	return 0;
}
/*-------------------------------MISC device related------------------------------------------*/



/************************************************************/
static int pa122_open(struct inode *inode, struct file *file)
{
	file->private_data = pa122_i2c_client;

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/************************************************************/

static int pa122_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/************************************************************/
static long pa122_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
		struct i2c_client *client = (struct i2c_client*)file->private_data;
		struct pa122_priv *obj = i2c_get_clientdata(client);  
		long err = 0;
		void __user *ptr = (void __user*) arg;
		int dat;
		uint32_t enable;
		int ps_result;
		int lux = 0;
		switch (cmd)
		{
			case ALSPS_SET_PS_MODE:
				if(copy_from_user(&enable, ptr, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}			
				if(enable)
				{
					if((err = pa122_enable_ps(obj->client, 1)))
					{
						APS_ERR("enable ps fail: %ld\n", err); 
						goto err_out;
					}
					set_bit(CMC_BIT_PS, &obj->enable);	
				}
				else
				{		
					if((err = pa122_enable_ps(obj->client, 0)))
					{
						APS_ERR("disable ps fail: %ld\n", err); 
						goto err_out;
					}
					clear_bit(CMC_BIT_PS, &obj->enable);
				}
				break;

			case ALSPS_GET_PS_MODE:
				enable = test_bit(CMC_BIT_PS, &obj->enable) ? (1) : (0);
				if(copy_to_user(ptr, &enable, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;

			case ALSPS_GET_PS_DATA:    
				if((err = pa122_read_ps(obj->client, &obj->ps)))
				{
					goto err_out;
				}

				dat = pa122_get_ps_value(obj, obj->ps);
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;

			case ALSPS_GET_PS_RAW_DATA:    
				if((err = pa122_read_ps(obj->client, &obj->ps)))
				{
					goto err_out;
				}

				dat = obj->ps;
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;			  

			case ALSPS_SET_ALS_MODE:

				if(copy_from_user(&enable, ptr, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}
				if(enable)
				{
					err = isl29125_als_enable();
					if(err < 0){
						APS_DBG("pa122_unlocked_ioctl isl29125_als_enable error\n");
						goto err_out;
					}
				/*
					if((err = pa122_enable_als(obj->client, 1)))
					{
						APS_ERR("enable als fail: %ld\n", err); 
						goto err_out;
					}
				*/
					set_bit(CMC_BIT_ALS, &obj->enable);
				
				}
				else
				{
					err = isl29125_als_disable();
					if(err < 0){
						APS_DBG("pa122_unlocked_ioctl isl29125_als_disable error\n");	
						goto err_out;
					}
				/*
					if((err = pa122_enable_als(obj->client, 0)))
					{
						APS_ERR("disable als fail: %ld\n", err); 
						goto err_out;
					}
				*/
					clear_bit(CMC_BIT_ALS, &obj->enable);
				}
				break;

			case ALSPS_GET_ALS_MODE:
				enable = test_bit(CMC_BIT_ALS, &obj->enable) ? (1) : (0);
				if(copy_to_user(ptr, &enable, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;

			case ALSPS_GET_ALS_DATA: 
				/*
				if((err = pa122_read_als(obj->client, &obj->als)))
				{
					goto err_out;
				}
	
				dat = pa122_get_als_value(obj, obj->als);
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}*/
				lux = isl29125_als_read();
				if(lux < 0){
					APS_DBG("huangzifan pa122_unlocked_ioctl ALSPS_GET_ALS_DATA isl29125_als_read error\n");
					goto err_out;
				}
				if(copy_to_user(ptr, &lux, sizeof(lux)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;

			case ALSPS_GET_ALS_RAW_DATA:
				/*
				if((err = pa122_read_als(obj->client, &obj->als)))
				{
					goto err_out;
				}
				*/
				lux = isl29125_als_read();
				if(lux < 0){
					APS_DBG("huangzifan pa122_unlocked_ioctl ALSPS_GET_ALS_RAW_DATA isl29125_als_read error\n");
					goto err_out;
				}
				if(copy_to_user(ptr, &lux, sizeof(lux)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;

			case ALSPS_IOCTL_CLR_CALI:
				APS_LOG("%s ALSPS_IOCTL_CLR_CALI\n", __func__);
				if(copy_from_user(&dat, ptr, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;

			case ALSPS_IOCTL_GET_CALI:
				dat = obj->crosstalk ;
				APS_LOG("%s set ps_cali %x\n", __func__, dat);
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;

			case ALSPS_IOCTL_SET_CALI:
				APS_LOG("%s set ps_cali %x\n", __func__, obj->crosstalk); 
				break;

			default:
				APS_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
				err = -ENOIOCTLCMD;
				break;
		}

err_out:
		return err;    
}

/********************************************************************/
/*------------------------------misc device related operation functions------------------------------------*/
static struct file_operations pa122_fops = {
	.owner = THIS_MODULE,
	.open = pa122_open,
	.release = pa122_release,
	.unlocked_ioctl = pa122_unlocked_ioctl,
};

static struct miscdevice pa122_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &pa122_fops,
};

/*--------------------------------------------------------------------------------------*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
static void pa122_early_suspend(struct early_suspend *h)
{
	struct pa122_priv *obj = container_of(h, struct pa122_priv, early_drv);	
	int err;
	APS_FUN();	  

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}
	if( 0 == wakeup){
			APS_ERR("pa122_early_suspend enable_irq!\n");		
			disable_irq_nosync(obj->irq);	
			suspend_irq_satus = 1;
	}
	atomic_set(&obj->als_suspend, 1);
	if(err = isl29125_als_disable())
	{
		APS_ERR("disable als fail: %d\n", err);
	}
	 isl29125_lateresume_enable = 0;
}

static void pa122_late_resume(struct early_suspend *h) 
{
		/*early_suspend is only applied for ALS*/
		struct pa122_priv *obj = container_of(h, struct pa122_priv, early_drv);		  
		int err;
		hwm_sensor_data sensor_data;
		memset(&sensor_data, 0, sizeof(sensor_data));
		APS_FUN();
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return;
		}

		if( 0 == wakeup ||suspend_irq_satus == 1){//When irq is disabled at early_suspend.We should enable irq at late_resume.
			APS_ERR("pa122_late_resume enable_irq!\n");		
			enable_irq(obj->irq);
			suspend_irq_satus = 0;
		}
		atomic_set(&obj->als_suspend, 0);
		if(test_bit(CMC_BIT_ALS, &obj->enable))
		{
			if( isl29125_lateresume_enable ==1)
			{
				APS_ERR("isl29125 already enabled!, do nothing\n");	
			}else if(err = isl29125_als_enable())
			{
				APS_ERR("enable als fail: %d\n", err);		  
			}
		}
}
#endif
/*--------------------------------------------------------------------------------*/
static int pa122_init_client(struct i2c_client *client)
{
	struct pa122_priv *obj = i2c_get_clientdata(client);
	u8 sendvalue=0;
	int res = 0;

	 char *info = p_sensor_info;

		 
	// Initialize Sensor
	mutex_lock(&obj->update_lock);
	res=hwmsen_write_byte(client,REG_CFG0,
		PA12_ALS_GAIN << 4);
	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
		goto EXIT_ERR;
	}

	res=hwmsen_write_byte(client,REG_CFG1,
		(PA12_LED_CURR << 4)| (PA12_PS_PRST << 2)| (PA12_ALS_PRST));
	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
		goto EXIT_ERR;
	}

	res=hwmsen_write_byte(client,REG_CFG2,
		(PA12_PS_MODE << 6)| (PA12_PS_SET << 2));
	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
		goto EXIT_ERR;
	}

	res=hwmsen_write_byte(client,REG_CFG3,
		(PA12_INT_TYPE << 6)| (PA12_PS_PERIOD << 3)| (PA12_ALS_PERIOD));
	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
		goto EXIT_ERR;
	}

	res=hwmsen_write_byte(client,REG_PS_SET,0x03); 
	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
		goto EXIT_ERR;
	}

	obj->crosstalk_base = 10;
	obj->crosstalk = PA12_PS_OFFSET_DEFAULT;
	res = hwmsen_write_byte(client, REG_PS_OFFSET, obj->crosstalk); 
	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
		goto EXIT_ERR;
	}
#if 0
	obj->ps_thrd_low = PA12_PS_TH_LOW;
	obj->ps_thrd_high = PA12_PS_TH_HIGH;
#endif
	if(obj->hw->polling_mode_als == 0)
	{
		/* Set ALS threshold */
		res=hwmsen_write_byte(client, REG_ALS_TH_MSB, PA12_ALS_TH_HIGH >> 8);
		if(res < 0)
		{
			APS_ERR("i2c_send function err\n");
			goto EXIT_ERR;
		}
		res=hwmsen_write_byte(client, REG_ALS_TH_LSB, PA12_ALS_TH_HIGH & 0xFF); 
		if(res < 0)
		{
			APS_ERR("i2c_send function err\n");
			goto EXIT_ERR;
		}

		res=hwmsen_write_byte(client, REG_ALS_TL_MSB, PA12_ALS_TH_LOW >> 8); 
		if(res < 0)
		{
			APS_ERR("i2c_send function err\n");
			goto EXIT_ERR;
		}

		res=hwmsen_write_byte(client, REG_ALS_TL_LSB, PA12_ALS_TH_LOW & 0xFF);
		if(res < 0)
		{
			APS_ERR("i2c_send function err\n");
			goto EXIT_ERR;
		}

	}

	if(obj->hw->polling_mode_ps == 0)
	{
		/* Set PS threshold */
		if(PA12_INT_TYPE == 0)
		{
			/*Window Type */	
			res=hwmsen_write_byte(client, REG_PS_TH, obj->ps_thrd_high);
			if(res < 0)
			{
				APS_ERR("i2c_send function err\n");
				goto EXIT_ERR;
			}

			res=hwmsen_write_byte(client, REG_PS_TL, PA12_PS_TH_MIN); 
			if(res < 0)
			{
				APS_ERR("i2c_send function err\n");
				goto EXIT_ERR;
			}

		}
		else if(PA12_INT_TYPE == 1)
		{
			/*Hysteresis Type */
			res=hwmsen_write_byte(client, REG_PS_TH, obj->ps_thrd_high); 
			if(res < 0)
			{
				APS_ERR("i2c_send function err\n");
				goto EXIT_ERR;
			}

			res=hwmsen_write_byte(client, REG_PS_TL, obj->ps_thrd_low); 
			if(res < 0)
			{
				APS_ERR("i2c_send function err\n");
				goto EXIT_ERR;
			}

		}
	}

	// Polling Setting	
	int intmode=((obj->hw->polling_mode_ps)<<1)|((obj->hw->polling_mode_als));

	res=hwmsen_read_byte(client,REG_CFG2,&sendvalue);
	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
		goto EXIT_ERR;
	}

	sendvalue=sendvalue & 0xF0; //clear int set

	switch(intmode)
	{

		case 0:
			sendvalue=sendvalue | 0x0C; //Both Interrupt
			res=hwmsen_write_byte(client,REG_CFG2,sendvalue); //set int mode
			break;
		case 1:
			sendvalue=sendvalue | 0x04; //PS Interrupt
			res=hwmsen_write_byte(client,REG_CFG2,sendvalue); //set int mode
			break;
		case 2:
			sendvalue=sendvalue | 0x00; //ALS Interrupt
			res=hwmsen_write_byte(client,REG_CFG2,sendvalue); //set int mode
			break;
		default:
			sendvalue=sendvalue | 0x04; //No Interupt 
			res=hwmsen_write_byte(client,REG_CFG2,sendvalue); //set int mode
			break;

	}

	if(res < 0)
	{
		APS_ERR("i2c_send function err\n");
		goto EXIT_ERR;
	}
	mutex_unlock(&obj->update_lock);

	// Regsit int
	res = pa122_setup_eint(client);
	if(res!=0)
	{
		APS_ERR("PA122 setup eint: %d\n", res);
		return res;
	}
	 info += sprintf(info,"PAL122");
    	 info += sprintf(info,"TXC,");

	return 0;

EXIT_ERR:
	mutex_unlock(&obj->update_lock);
	APS_ERR("pa122 init dev fail!!!!: %d\n", res);
	return res;
}
/*--------------------------------------------------------------------------------*/
static int pa122_als_open_report_data(int open)
{
	//should queuq work to report event if  is_report_input_direct=true
	return 0;
}
/*--------------------------------------------------------------------------------*/
// if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL
static int pa122_als_enable_nodata(int en)
{
	int err = 0;
	int value = en;

	struct pa122_priv *obj = pa122_obj;

	if(NULL == obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return -1;
	}
	APS_LOG("pa122_obj als enable value = %d\n", en);

	if(value)
	{
		err = isl29125_als_enable();
		if( err < 0){
			APS_DBG("huangzifan isl29125_als_enable error\n");
			return -1;
		}
	/*
		if((err = pa122_enable_als(obj->client, 1)))
		{
			APS_ERR("enable als fail: %d\n", err); 
			return -1;
		}
	*/
		set_bit(CMC_BIT_ALS, &obj->enable);
	
	}
	else
	{
		err = isl29125_als_disable();
		if(err < 0){
			APS_DBG("huangzifan isl29125_als_disable error\n");
			return -1;
		}
	/*
		if((err = pa122_enable_als(obj->client, 0)))
		{
			APS_ERR("disable als fail: %d\n", err); 
			return -1;
		}
	*/
		clear_bit(CMC_BIT_ALS, &obj->enable);
	
	}


	return 0;
}
/*--------------------------------------------------------------------------------*/
static int pa122_als_set_delay(u64 ns)
{
	return 0;
}
/*--------------------------------------------------------------------------------*/
static int pa122_als_get_data(int* value, int* status)
{
	int err = 0;
/*
//	u8 flag = 0;
	struct pa122_priv *obj = pa122_obj;

	if(NULL == obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return -1;
	}


	if((err = pa122_read_als(obj->client, &obj->als)))
	{
		err = -1;
	}
	else
	{

		*value = pa122_get_als_value(obj, obj->als);
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
		if(atomic_read(&obj->trace) & CMC_TRC_ALS_DATA)
			APS_LOG("als = %d!,value=%d\n", obj->als, *value);
	}	
*/
	*value = isl29125_als_read();
	if(*value < 0)
			APS_DBG("huangzifan pa122_als_get_data  error\n");
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	return err;
}
/*--------------------------------------------------------------------------------*/
// if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL
static int pa122_ps_open_report_data(int open)
{
	//should queuq work to report event if  is_report_input_direct=true
	return 0;
}
/*--------------------------------------------------------------------------------*/
// if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL
static int pa122_ps_enable_nodata(int en)
{
	int err = 0;
	int value = en;
	struct pa122_priv *obj = pa122_obj;

	if(NULL == obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return -1;
	}
	APS_DBG("pa122_obj ps enable value = %d\n", en);
		
	if(value)
	{				
		if((err = pa122_enable_ps(obj->client, 1)))
		{
			APS_ERR("enable ps fail: %d\n", err); 
			return -1;
		}
		set_bit(CMC_BIT_PS, &obj->enable);
		if(1 == obj->hw->polling_mode_ps)
		{
			wake_lock(&ps_lock);
			//APS_ERR("wake_lock(&ps_lock)\n"); //test
		}
		else
		{
#if defined(CONFIG_OF)
//	APS_DBG("huangzifan pa122_obj ps enable value enable_irq1\n");
			enable_irq(obj->irq);
#elif defined(CUST_EINT_ALS_TYPE)
			mt_eint_unmask(CUST_EINT_ALS_NUM);
#endif	
		}
		if ( one_time == 1){
			enable_irq(obj->irq);
			one_time = 0;
		}
	}
	else
	{
		if((err = pa122_enable_ps(obj->client, 0)))
		{
			APS_ERR("disable ps fail: %d\n", err); 
			return -1;
		}
		clear_bit(CMC_BIT_PS, &obj->enable);
	
		if(1 == obj->hw->polling_mode_ps)
		{
			wake_unlock(&ps_lock);
			//APS_ERR("wake_unlock(&ps_lock)\n"); //test
		}
		else
		{
#if defined(CONFIG_OF)
			disable_irq_nosync(obj->irq);
#elif defined(CUST_EINT_ALS_TYPE)
			mt_eint_mask(CUST_EINT_ALS_NUM);
#endif	
		}
	}

	return 0;

}
/*--------------------------------------------------------------------------------*/
static int pa122_ps_set_delay(u64 ns)
{
	return 0;
}
/*--------------------------------------------------------------------------------*/
static int pa122_ps_get_data(int* value, int* status)
{
	int err = 0;
	int temp_value = -1;
	struct pa122_priv *obj = pa122_obj;
	
	if(NULL == obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return -1;
	}

	if((err = pa122_read_ps(obj->client, &obj->ps)))
	{
		APS_DBG("huangzifan pa122_ps_get_data obj->ps= %d\n",(int)(obj->ps));
		err = -1;
	}
	else
	{
		*value = pa122_get_ps_value(obj, obj->ps);
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
//		APS_DBG("huangzifan pa122_ps_get_data *value = %d\n",*value);
		if(atomic_read(&obj->trace) & CMC_TRC_PS_DATA)
			APS_LOG("%s:ps raw 0x%x -> value 0x%x \n",__FUNCTION__, obj->ps, *value);								
	}
    
	return err;
}


/*--------------------------------------------------------------------------------*/

static ssize_t alsps_calibration_write(struct file *f, char __user *buf, size_t count, loff_t *pos)
{
	if(pa122_run_calibration(pa122_i2c_client) < 0)
	{
		return -EFAULT;
	}

	pa122_has_load_cal_file = 1;

	return count;
}

static int alsps_calibration_open(struct inode *inode, struct file *f)
{
	return 0;
}

static int alsps_calibration_release(struct inode *inode, struct file *f)
{
	return 0;
}

static const struct file_operations alsps_calibration_fops = {
	.owner = THIS_MODULE,
	.open = alsps_calibration_open,
	.release = alsps_calibration_release,
	.write = alsps_calibration_write,
};

static struct miscdevice alsps_calibration_struct = {
	.name = "alsps_cali",
	.fops = &alsps_calibration_fops,
	.minor = MISC_DYNAMIC_MINOR,
};

static int set_crosstalk(int len)
{
	int res;
	struct pa122_priv *obj = pa122_obj;
	int *value =(int *)ps_buffer ;
	res = read_block(len);
	if( res < 0){
			APS_DBG("Oh sorry,  read_block error\n");
			return -1;
	}else{
		if( *value < 0 || *value > 100 || *value == 0){//Modify by huangzifan. 2015.12.7
			APS_DBG(" Value is not at normal range.It is error\n");
			*value  = PA12_PS_OFFSET_DEFAULT;
		}
			obj->crosstalk  = *value;
			APS_DBG("pa122_store_ps_offset obj->crosstalk = %d\n",obj->crosstalk);
	}
	return 0;
}
static int read_block(int len)
{
    struct file *f;
    mm_segment_t fs;
    int ret;
    int *value = NULL;
    f = filp_open(STORE_FILE, O_RDONLY, 0644);
    if (IS_ERR(f)) {
        APS_DBG("read_block open file %s error!\n", STORE_FILE);
        return -1;
    }
    fs = get_fs(); set_fs(KERNEL_DS);
    f->f_pos=BASE_ADDR+POS;
    ret = f->f_op->read(f, ps_buffer, len, &f->f_pos);
    set_fs(fs);
    filp_close(f, NULL);
    if (ret < 0 ) {
        printk("%s: read_block read file %s error!\n", __func__, STORE_FILE);
        ret = -1;
    }
	value = (int *)ps_buffer;
   APS_DBG("read_block ps_buffer = %d\n",*value);
    return ret;
}
static int write_block(char * buf, int len)
{
    struct file *f;
    mm_segment_t fs;
    int res;
     char *buffer = buf;
    f = filp_open(STORE_FILE, O_RDWR, 0644);
    if (IS_ERR(f)) {
        APS_ERR("%s: write_block open file %s error!\n", __func__, STORE_FILE);
	calibration_flag = 0;
        return -1;
    }
    f->f_pos=BASE_ADDR+POS;
    fs = get_fs(); set_fs(KERNEL_DS);
    APS_ERR("set value = %d\n",*buffer);
    res = f->f_op->write(f, buffer, len, &f->f_pos);
    set_fs(fs);
    filp_close(f, NULL);
    if ( res < 0){
		APS_DBG("write_block error ret =%d\n", res);
		calibration_flag = 0;
    	}
    return res;
}
#ifdef PS_DYNAMIC_CALIBRATE
//Modify by huangzifan 2015.9.29
static ssize_t pa122_store_ps_calibration(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int status1;
	u8 data = 0;
	if(1 == sscanf(buf, "%d", &status1))
	{ 
		if(1== status1){
		data = pa122_run_calibration(pa122_i2c_client);
		if ( data < 0 ){
			    	APS_ERR("PS pa122_store_ps_calibration error huangzifan \n");
				return -1;//Modify by huangzifan 2015.11.12
			}
		}
	}
	return size;    
}
//Modify by huangzifan 2015.9.29
static ssize_t pa122_show_ps_calibbias(struct device *dev,struct device_attribute *attr, char *buf)
{
		struct pa122_priv *obj = pa122_obj;
		APS_DBG("pa122_show_ps_calibbias %d\n",obj->crosstalk);
		if( calibration_flag == 0){//Modify by huangzifan 2015.11.12
			return sprintf(buf, "%d\n", -1); 
		}else{
			return sprintf(buf, "%d\n",(int)(obj->crosstalk)); 
		}
}

//Modify by huangzifan 2015.9.29
static ssize_t pa122_store_ps_offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int status1,error,res;
	struct pa122_priv *obj = pa122_obj;

	if(1 == sscanf(buf, "%d", &status1))
	{ 
	 	if(status1 > 0 || status1 == 0 ){

			ps_offset_flag  = 1;
			if( status1 > PA12_PS_OFFSET_MAX || status1 == 0){//Modify by huangzifan 2015.12.18
				ps_offset_flag  = 0;
				APS_DBG("pa122_store_ps_offset offset value is invalid,do nothing\n");
				return size;
	}
	
	obj->crosstalk  = status1;
	APS_DBG("pa122_store_ps_offset obj->crosstalk = %d\n",obj->crosstalk);

	mutex_lock(&obj->update_lock);
	/* Write ps offset value to register 0x10 */
	hwmsen_write_byte(pa122_i2c_client, REG_PS_OFFSET, obj->crosstalk); 
	if(obj->hw->polling_mode_ps == 0)
	{
		/* Set PS threshold */
		if(PA12_INT_TYPE == 0)
		{
			/*Window Type */	
			res=hwmsen_write_byte(pa122_i2c_client, REG_PS_TH, obj->ps_thrd_high);
			res=hwmsen_write_byte(pa122_i2c_client, REG_PS_TL, PA12_PS_TH_MIN);
		}
		else if(PA12_INT_TYPE == 1)
		{
			/*Hysteresis Type */
			res=hwmsen_write_byte(pa122_i2c_client, REG_PS_TH, obj->ps_thrd_high); 
			res=hwmsen_write_byte(pa122_i2c_client, REG_PS_TL, obj->ps_thrd_low); 
		}
	}
		mutex_unlock(&obj->update_lock);			
	 	}
	}
	else
	{
		APS_DBG("PS huangzifan2  : '%s'\n", buf);
	}
	return size;    
	
}


//Modify by huangzifan 2015.9.29
static ssize_t pa122_store_ps_enable_irq(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int status1,error,res;
	struct pa122_priv *obj = pa122_obj;

	if(1 == sscanf(buf, "%d", &status1))
	{ 
	 if(status1 == 0){
			disable_irq_nosync(obj->irq);				
	 	}
	 if(status1 == 1) 
	 	{
			enable_irq(obj->irq);
	 	}
	}
	else
	{
		APS_DBG("PS huangzifan2  : '%s'\n", buf);
	}
	return size;    
	
}

//Modify by huangzifan 2015.9.29
static ssize_t pa122_store_ps_rw_data(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int  status1 = 0;

	if(1 == sscanf(buf, "%d", &status1))
	{ 
		write_block(&status1,4); 
	 	}
	else
	 	{
		APS_DBG("PS huangzifan2  : '%s'\n", buf);
	 	}
	return size;    
	}
static ssize_t pa122_show_ps_rw_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
		int res;
		int *value =(int *)ps_buffer ;
		res = read_block(4);
		if( res < 0){
			return sprintf(buf, "%d\n",-1);
		}else{
			return sprintf(buf, "%d\n",*value);
		}
}
static ssize_t pa122_store_ps_factory_calibration(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
	{
	int status1;
	int res;
	int value =  0;
	u8 data = 0;
	if(1 == sscanf(buf, "%d", &status1))
	{ 
		if(1== status1){
			data = pa122_run_calibration(pa122_i2c_client);
			if ( data < 0 || data > 100){
			    	APS_ERR("PS pa122_store_ps_factory_calibration error, data = %d\n",(int)data);
				return size;
	}
			value = (int) data;
			res = write_block(&value,4);
			if( res < 0){
				  APS_ERR("PS pa122_store_ps_factory_calibration write_block error\n");
				  return size;
			}
		}
	}
	return size;    
}
static ssize_t pa122_show_ps_calib_flag(struct device *dev,struct device_attribute *attr, char *buf)
{
		if( calibration_flag == 0){
			return sprintf(buf, "%d\n", -1); 
		}else{
			return sprintf(buf, "%d\n",0); 
		}
}

static ssize_t pa122_show_ps_reg(struct device *dev,struct device_attribute *attr, char *buf)
{
	u8 regdata;
	int res=0;
	int count=0;
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}
	int i=0	;

	mutex_lock(&pa122_obj->update_lock);
	for(i;i <17 ;i++)
	{
		res=hwmsen_read_byte(pa122_obj->client,0x00+i,&regdata);

		if(res<0)
		{
		   break;
		}
		else
		count+=sprintf(buf+count,"[%x] = (%x)\n",0x00+i,regdata);
	}
	mutex_unlock(&pa122_obj->update_lock);

	return count;
}

static ssize_t pa122_show_ps_value(struct device *dev,struct device_attribute *attr, char *buf)
{
	ssize_t res;
	if(!pa122_obj)
	{
		APS_ERR("pa122_obj is null!!\n");
		return 0;
	}
	
	if((res = pa122_read_ps(pa122_obj->client, &pa122_obj->ps)))
	{
		return snprintf(buf, PAGE_SIZE, "ERROR: %zd\n", res);
	}
	else
	{
		return snprintf(buf, PAGE_SIZE, "%d\n", pa122_obj->ps);     
	}
}
#endif


static DEVICE_ATTR(ps_calibration, S_IWUSR | S_IRUGO,NULL, pa122_store_ps_calibration);
static DEVICE_ATTR(ps_calibbias, S_IWUSR | S_IRUGO,pa122_show_ps_calibbias, NULL);
static DEVICE_ATTR(ps_offset, S_IWUSR | S_IRUGO,NULL, pa122_store_ps_offset);
static DEVICE_ATTR(ps_enable_irq, S_IWUSR | S_IRUGO,NULL, pa122_store_ps_enable_irq);
static DEVICE_ATTR(ps_rw_data, S_IWUSR | S_IRUGO,pa122_show_ps_rw_data, pa122_store_ps_rw_data);
static DEVICE_ATTR(ps_factory_calibration, S_IWUSR | S_IRUGO,NULL, pa122_store_ps_factory_calibration);
static DEVICE_ATTR(ps_calib_flag, S_IWUSR | S_IRUGO,pa122_show_ps_calib_flag, NULL);
static DEVICE_ATTR(ps_reg, S_IWUSR | S_IRUGO,pa122_show_ps_reg, NULL);
static DEVICE_ATTR(ps_value, S_IWUSR | S_IRUGO,pa122_show_ps_value, NULL);

static struct device_attribute *calibrate_dev_attributes[] = {
	&dev_attr_ps_calibration,
	&dev_attr_ps_calibbias,
	&dev_attr_ps_offset,
	&dev_attr_ps_enable_irq,
	&dev_attr_ps_rw_data,//Add by huangzifan 2015.11.23. For proximity calibration at factory functional test.
	&dev_attr_ps_factory_calibration,
	&dev_attr_ps_calib_flag,
	&dev_attr_ps_reg,
	&dev_attr_ps_value,
};


#ifdef PS_WAKEUP
static ssize_t pa122_store_ps_wakeup_enable(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int status1;
	struct pa122_priv *obj = pa122_obj;
	
	if(1 == sscanf(buf, "%d", &status1))
	{ 
		if(0== status1){
			APS_DBG("PA122	disable nonwakeup ps\n");
	//		enable_irq(obj->irq);
	//		mt_eint_unmask(CUST_EINT_ALS_NUM);    
			wakeup = 1;
		}
		else if(1 == status1){	
			APS_DBG("PA122 enable nonwakeup ps\n");
	//		mt_eint_mask(CUST_EINT_ALS_NUM);
			wakeup = 0;
		};
	}
	return size;    
	
}

static ssize_t pa122_show_ps_wakeup_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
		return sprintf(buf, "%d\n",!wakeup);
}


static DEVICE_ATTR(ps_wakeup_enable, S_IWUSR | S_IRUGO,pa122_show_ps_wakeup_enable,pa122_store_ps_wakeup_enable);

static struct device_attribute *wakeup_enable_dev_attributes[] = {
	&dev_attr_ps_wakeup_enable,
};
#endif

//Add by huangzifan at 2015.10.22
int pa122_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct pa122_priv *obj = (struct pa122_priv *)self;

//	int err = 0;
	int temp_value = -1;
//	struct pa122_priv *obj = pa122_obj;

	
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{				
				value = *(int *)buff_in;				
				if(value)
				{
					pa122_ps_enable_nodata(1);
				/*
					err = ltr559_ps_enable(ps_gainrange);
					if(err < 0)
					{
						APS_ERR("enable ps fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_PS, &obj->enable);
					calibrate_flag = 0;
					APS_ERR("huangzifan calibrate_flag =  %d\n", calibrate_flag); 		   	
				*/
				}
				else
				{
					pa122_ps_enable_nodata(0);
				/*
				    err = ltr559_ps_disable();
					if(err < 0)
					{
						APS_ERR("disable ps fail: %d\n", err); 
						return -1;
					}
					clear_bit(CMC_BIT_PS, &obj->enable);
				*/
				}
				
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				
				sensor_data = (hwm_sensor_data *)buff_out;
	
				if(NULL == obj)
				{
					APS_ERR("pa122_obj is null!!\n");
					return -1;
				}


				if((err = pa122_read_ps(obj->client, &obj->ps)))
				{
					APS_DBG("huangzifan pa122_ps_get_data obj->ps= %d\n",(int)(obj->ps));
					err = -1;
				}
				
    				if(obj->ps < 0)
    				{
    					err = -1;
    					break;
    				}

				sensor_data->values[0] = pa122_get_ps_value(obj, obj->ps);
				//sensor_data->values[1] = obj->ps;		//steven polling mode *#*#3646633#*#*
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
				APS_ERR("get sensor ps data !value = %d\n",sensor_data->values[0]);
			}
			break;
		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

//Add by huangzifan at 2015.10.22
int pa122_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct pa122_priv *obj = (struct pa122_priv *)self;

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;				
				if(value)
				{
					pa122_als_enable_nodata(1);
				/*
				    err = ltr559_als_enable(als_gainrange);
					if(err < 0)
					{
						APS_ERR("enable als fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_ALS, &obj->enable);
				*/
				}
				else
				{
					pa122_als_enable_nodata(0);
				/*
				    err = ltr559_als_disable();
					if(err < 0)
					{
						APS_ERR("disable als fail: %d\n", err); 
						return -1;
					}
					clear_bit(CMC_BIT_ALS, &obj->enable);
				*/
				}
				
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;
				obj->als = isl29125_als_read();
                #if defined(MTK_AAL_SUPPORT)
				//sensor_data->values[0] = ltr559_get_als_value(obj, obj->als);//wisky-lxh@20150206
				sensor_data->values[0] = obj->als;
				#else
				//sensor_data->values[0] = ltr559_get_als_value(obj, obj->als);
				sensor_data->values[0] = obj->als;
				#endif
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;

				if( obj->als < 0)
					APS_ERR("huangzifan pa122_als_get_data  error\n");			
			}
			break;
		default:
			APS_ERR("light sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

/*-----------------------------------i2c operations----------------------------------*/
static int pa122_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct pa122_priv *obj = NULL;
    struct als_control_path als_ctl = {0};
	struct als_data_path als_data = {0};
	struct ps_control_path ps_ctl = {0};
	struct ps_data_path ps_data = {0};
	struct hwmsen_object obj_ps, obj_als;

	int err = 0;

	APS_LOG("%s: start...\n", __func__); //add for test by wxj 2014.5.5
	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
     //VIO18_PMU-LS_EINT 0.2ma
/*
        mt_set_gpio_mode(GPIO_LS_EINT_PIN,GPIO_MODE_GPIO);  
        mt_set_gpio_dir(GPIO_LS_EINT_PIN,GPIO_DIR_IN);
        //mt_set_gpio_out(GPIO120,GPIO_OUT_ZERO);
        mt_set_gpio_pull_enable(GPIO_LS_EINT_PIN, GPIO_PULL_ENABLE);
        mt_set_gpio_pull_select(GPIO_LS_EINT_PIN, GPIO_PULL_UP);
*/	
	pa122_obj = obj;

	obj->hw = get_cust_alsps_hw();//get custom file data struct

	mutex_init(&obj->update_lock); 
	wake_lock_init(&ps_lock,WAKE_LOCK_SUSPEND,"ps wakelock");

	INIT_WORK(&obj->eint_work, pa122_eint_work);
	
	obj->client = client;
	i2c_set_clientdata(client, obj);

	/*-----------------------------value need to be confirmed-----------------------------------------*/
	atomic_set(&obj->als_debounce, 200);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 200);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_cmd_val, 0xDF);
	atomic_set(&obj->ps_cmd_val,  0xC1);
	//atomic_set(&obj->ps_thd_val_high,  obj->hw->ps_threshold_high);
	//atomic_set(&obj->ps_thd_val_low,  obj->hw->ps_threshold_low);
	atomic_set(&obj->als_thd_val_high,  obj->hw->als_threshold_high);
	atomic_set(&obj->als_thd_val_low,  obj->hw->als_threshold_low);

	obj->ps_thrd_high = obj->hw->ps_threshold_high;
	obj->ps_thrd_low = obj->hw->ps_threshold_low;

	obj->enable = 0;
	obj->pending_intr = 0;
	obj->ps_enable_flag = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);
	/*-----------------------------value need to be confirmed-----------------------------------------*/


//	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
//	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	set_bit(CMC_BIT_ALS, &obj->enable);
	set_bit(CMC_BIT_PS, &obj->enable);

	pa122_i2c_client = client;
//	APS_ERR("huangzifan pa122_i2c_probe 1 \n");
	if((err = pa122_init_client(client)))
	{
		goto exit_init_failed;
	}
	APS_LOG("pa122_init_client() OK!\n");

	if((err = misc_register(&pa122_device)))
	{
		APS_ERR("pa122_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	APS_LOG("pa122_device misc_register OK!\n");

	/*------------------------sl22201001 attribute file for debug--------------------------------------*/
/*
	if((err = pa122_create_attr(&pa122_init_info.platform_diver_addr->driver)))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
*/
	/*------------------------s12201001 attribute file for debug--------------------------------------*/
//Modify by huangzifan at 2015.10.22.
#ifdef OLD_ARCH
	als_ctl.open_report_data= pa122_als_open_report_data;
	als_ctl.enable_nodata = pa122_als_enable_nodata;
	als_ctl.set_delay  = pa122_als_set_delay;
	als_ctl.is_report_input_direct = false;
	als_ctl.is_polling_mode = true;
	als_ctl.is_support_batch = obj->hw->is_batch_supported_als;
	als_ctl.is_use_common_factory = false;
	err = als_register_control_path(&als_ctl);
	if(err != 0){
		APS_ERR("als_register_control_path fail = %d\n", err);
		goto exit_register_path_fail;
	}

	als_data.get_data = pa122_als_get_data;
	als_data.vender_div = 1;

	err = als_register_data_path(&als_data);	
	if(err != 0){
		APS_ERR("als_register_data_path fail = %d\n", err);
		goto exit_register_path_fail;
	}


//	APS_ERR("huangzifan pa122_i2c_probe 2\n");
	
	ps_ctl.open_report_data= pa122_ps_open_report_data;
	ps_ctl.enable_nodata = pa122_ps_enable_nodata;
	ps_ctl.set_delay  = pa122_ps_set_delay;
	ps_ctl.is_support_batch = obj->hw->is_batch_supported_ps;
	ps_ctl.is_use_common_factory = false;
	if(obj->hw->polling_mode_ps == 0)	/*interrupt mode*/
	{
		ps_ctl.is_polling_mode = false;
		ps_ctl.is_report_input_direct = true;

		//APS_LOG("%s: enable PS interrupt\n", __FUNCTION__);
	}
	else					/*polling mode*/
	{
		ps_ctl.is_polling_mode = true;
		ps_ctl.is_report_input_direct = false;
		//wake_lock_init(&ps_lock,WAKE_LOCK_SUSPEND,"ps wakelock");
	}

	err = ps_register_control_path(&ps_ctl);
	if(err != 0){
		APS_ERR("register fail = %d\n", err);
		goto exit_register_path_fail;
	}

	ps_data.get_data = pa122_ps_get_data;
	ps_data.vender_div = 1;

	err = ps_register_data_path(&ps_data);	
	if(err != 0){
		APS_ERR("tregister fail = %d\n", err);
		goto exit_register_path_fail;
	}
#endif

//Add new arch. Modify by huangzifan at 2015.10.22
#ifdef NEW_ARCH
	obj_ps.self = pa122_obj;
	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	if(1 == obj->hw->polling_mode_ps)
	{
		obj_ps.polling = 1;
	}
	else
	{
		obj_ps.polling = 0;
	}
		obj_ps.polling = 0;
	obj_ps.sensor_operate = pa122_ps_operate;
	if(err = hwmsen_attach(ID_PROXIMITY, &obj_ps))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	obj_als.self = pa122_obj;
	obj_als.polling = 1;
	obj_als.sensor_operate = pa122_als_operate;
	if(err = hwmsen_attach(ID_LIGHT, &obj_als))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend  = pa122_early_suspend,
	obj->early_drv.resume   = pa122_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif
	if (misc_register(&alsps_calibration_struct) < 0)
	{
		APS_ERR("Creat alsps_calibration_struct device file error!!\n");
	}	
	pa122_init_flag = 0;

//Modify by huangzifan 2015.9.29
#ifdef PS_WAKEUP
	int j=0;
	int result =0;
	struct device *wakeup_dev = &(client->dev);
	result = device_create_file(wakeup_dev,  wakeup_enable_dev_attributes[j]);
	if (result<0){ 
        	device_remove_file(wakeup_dev,  wakeup_enable_dev_attributes[j]);
		goto exit_create_attr_failed;
	}
#endif

//Modify by huangzifan 2015.9.29
#ifdef PS_DYNAMIC_CALIBRATE
	int i;
	int ps_result;

	ps_device= kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!ps_device) {
        	APS_DBG("huangzifan cannot allocate hall device\n");        
		goto exit_register_path_fail;
    	}
      ps_device->init_name = PS_DEVICE_NAME;
      ps_device->class = meizu_class;
      ps_device->release = (void (*)(struct device *))kfree;
      ps_result = device_register(ps_device);
	if (ps_result) {
         	APS_DBG("HZF cannot register ps_device\n");        
  	  	goto exit_register_path_fail;
	}
		  
      ps_result = 0;
	for (i = 0; i < ARRAY_SIZE(calibrate_dev_attributes); i++) {
		ps_result = device_create_file(ps_device,  calibrate_dev_attributes[i]);
		if (ps_result)
            break;
	}
	if (ps_result) {
        while (--i >= 0)
            device_remove_file(ps_device, calibrate_dev_attributes[i]);
	     APS_ERR("huangzifan ltr559 device remove file  = %d\n", ps_result);      
        	goto exit_create_attr_failed;
	}    	
#endif
	
//	APS_ERR("huangzifan pa122_i2c_probe is OK \n");
	APS_ERR("%s: OK\n", __func__);
	return 0;
	
exit_register_path_fail:

exit_create_attr_failed:
exit_sensor_obj_attach_fail:
exit_misc_device_register_failed:
		misc_deregister(&pa122_device);
exit_init_failed:
	kfree(obj);
exit:
	pa122_i2c_client = NULL;           
	APS_ERR("%s: err = %d\n", __func__, err);
	return err;
}

static int pa122_i2c_remove(struct i2c_client *client)
{
	int err;	
	/*------------------------pa122 attribute file for debug--------------------------------------*/	
	if((err = pa122_delete_attr(&pa122_init_info.platform_diver_addr->driver)))
	{
		APS_ERR("pa122_delete_attr fail: %d\n", err);
	} 
	/*----------------------------------------------------------------------------------------*/
	
	if((err = misc_deregister(&pa122_device)))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}
		
	pa122_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;

}
#if !defined(CONFIG_HAS_EARLYSUSPEND)

static int pa122_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	APS_FUN();
	return 0;
}

static int pa122_i2c_resume(struct i2c_client *client)
{
	APS_FUN();
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int  pa122_local_uninit(void)
{
	//APS_FUN(); 
	struct alsps_hw *hw = get_cust_alsps_hw();
	
	pa122_power(hw, 0);//*****************  
	
	i2c_del_driver(&pa122_i2c_driver);
	return 0;

}

/*----------------------------------------------------------------------------*/
static int pa122_local_init(void) 
{
	//APS_FUN();  
	struct alsps_hw *hw = get_cust_alsps_hw();

	pa122_power(hw, 1); //*****************   
	
	if(i2c_add_driver(&pa122_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	} 


    if(-1 == pa122_init_flag)
    {
    	APS_ERR("init failed pa122_init_flag = %d\n", pa122_init_flag);
        return -1;
    }
	return 0;
}


/*----------------------------------------------------------------------------*/
static int __init pa122_init(void)
{
	//APS_FUN();
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_pa122, 1);
	alsps_driver_add(&pa122_init_info);

	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit pa122_exit(void)
{
	APS_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(pa122_init);
module_exit(pa122_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("TXC Corp");
MODULE_DESCRIPTION("pa122 driver");
MODULE_LICENSE("GPL");

