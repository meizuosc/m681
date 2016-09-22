/*
 * 
 * Author: MingHsien Hsieh <minghsien.hsieh@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#define GN_MTK_BSP_PS_DYNAMIC_CALI
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include <linux/hwmsen_helper.h>
#include "ltr579.h"
#include "linux/mutex.h"
#include <alsps.h>
#include <linux/irqchip/mt-eic.h>
#include <linux/of_irq.h>
#include "mach/eint.h"

extern char l_sensor_info[20];
extern char p_sensor_info[20];
extern int als_isl29125; 
extern int tp_type_flag;
extern int board_version_flag;

static int ltr579_als_enable(int gainrange);

#define POWER_NONE_MACRO MT65XX_POWER_NONE

static int ltr579_factory_calibrate(void);
static int ltr579_wtfactory_calibrate(void);
static int ltr579_ps_factory_disable();
static int calib_value = 0;
static int suspend_irq_satus = 0;
static int ps_calibrate_by_factory = 1;
static int ps_facroty_value = 0;
static int ps_state_for_store = 1;

/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/

#define LTR579_DEV_NAME   "LTR_579ALS"

/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(KERN_ERR APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_ERR APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_ERR APS_TAG fmt, ##args)                 
/******************************************************************************
 * extern functions
*******************************************************************************/

extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
extern void mt_eint_print_status(void);
/*----------------------------------------------------------------------------*/

static struct i2c_client *ltr579_i2c_client = NULL;
static int isadjust=0;
static int dynamic_cali = 200;
static int first_enable = 1;
#define PS_DYNAMIC_CALIBRATE
#define PS_WAKEUP
static int set_crosstalk(int len);
static int write_block(char *buf, int len);
static int read_block(int len);
#define BASE_ADDR (3*1024*1024)
#define POS (10*1024)
#define STORE_FILE      "/dev/block/platform/mtk-msdc.0/by-name/proinfo"
char psrw_buffer[8] ={0};
static int wakeup = 1;
#define MEIZU_CLASS_NAME  "meizu"
#define ALS_DEVICE_NAME  "als"
#define PS_DEVICE_NAME  "ps"
extern struct class *meizu_class;
static struct device *ps_device;
static struct device *als_device;
static int calibration_flag = 1 ;
static int first_enable_flag = 1;
static int ps_offset_flag = 0;

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id ltr579_i2c_id[] = {{LTR579_DEV_NAME,0},{}};
/*the adapter id & i2c address will be available in customization*/
static struct i2c_board_info __initdata i2c_ltr579={ I2C_BOARD_INFO("LTR_579ALS", 0x53)};

//static unsigned short ltr579_force[] = {0x00, 0x46, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const ltr579_forces[] = { ltr579_force, NULL };
//static struct i2c_client_address_data ltr579_addr_data = { .forces = ltr579_forces,};
/*----------------------------------------------------------------------------*/
static int ltr579_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int ltr579_i2c_remove(struct i2c_client *client);
static int ltr579_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
static int ltr579_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int ltr579_i2c_resume(struct i2c_client *client);
static int ltr579_ps_enable();
static int ltr579_ps_factory_enable();
static int  ltr579_local_init(void);
static int  ltr579_local_uninit(void);
static struct alsps_init_info ltr579_init_info = {
	.name = "ltr579",
	.init = ltr579_local_init,
	.uninit = ltr579_local_uninit,
};

static int als_gainrange;

static int final_prox_val;
static int final_lux_val;

/*----------------------------------------------------------------------------*/
static DEFINE_MUTEX(read_lock);

/*----------------------------------------------------------------------------*/
static int ltr579_als_read(int gainrange);
static int ltr579_ps_read(void);


/*----------------------------------------------------------------------------*/


typedef enum {
    CMC_BIT_ALS    = 1,
    CMC_BIT_PS     = 2,
} CMC_BIT;

/*----------------------------------------------------------------------------*/
struct ltr579_i2c_addr {    /*define a series of i2c slave address*/
    u8  write_addr;  
    u8  ps_thd;     /*PS INT threshold*/
};

/*----------------------------------------------------------------------------*/

struct ltr579_priv {
    struct alsps_hw  *hw;
    struct i2c_client *client;
    struct work_struct  eint_work;
    struct mutex lock;
	/*i2c address group*/
    struct ltr579_i2c_addr  addr;

     /*misc*/
    u16		    als_modulus;
    atomic_t    i2c_retry;
    atomic_t    als_debounce;   /*debounce time after enabling als*/
    atomic_t    als_deb_on;     /*indicates if the debounce is on*/
    atomic_t    als_deb_end;    /*the jiffies representing the end of debounce*/
    atomic_t    ps_mask;        /*mask ps: always return far away*/
    atomic_t    ps_debounce;    /*debounce time after enabling ps*/
    atomic_t    ps_deb_on;      /*indicates if the debounce is on*/
    atomic_t    ps_deb_end;     /*the jiffies representing the end of debounce*/
    atomic_t    ps_suspend;
    atomic_t    als_suspend;

    /*data*/
    u16         als;
    u16          ps;
    u8          _align;
    u16         als_level_num;
    u16         als_value_num;
    u32         als_level[C_CUST_ALS_LEVEL-1];
    u32         als_value[C_CUST_ALS_LEVEL];

    u8	ps_enable_flag;
    u8	als_enable_flag;

    atomic_t    als_cmd_val;    /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_cmd_val;     /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_thd_val;     /*the cmd value can't be read, stored in ram*/
	atomic_t    ps_thd_val_high;     /*the cmd value can't be read, stored in ram*/
	atomic_t    ps_thd_val_low;     /*the cmd value can't be read, stored in ram*/
    ulong       enable;         /*enable mask*/
    ulong       pending_intr;   /*pending interrupt*/

    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif     

	struct device_node *irq_node;
	int		irq;
};


 struct PS_CALI_DATA_STRUCT
{
    int close;
    int far_away;
    int valid;
};

static struct PS_CALI_DATA_STRUCT ps_cali={0,0,0};
static int intr_flag_value = 0;


static struct ltr579_priv *ltr579_obj = NULL;

/*----------------------------------------------------------------------------*/
static struct i2c_driver ltr579_i2c_driver = {	
	.probe      = ltr579_i2c_probe,
	.remove     = ltr579_i2c_remove,
	.detect     = ltr579_i2c_detect,
	.suspend    = ltr579_i2c_suspend,
	.resume     = ltr579_i2c_resume,
	.id_table   = ltr579_i2c_id,
	//.address_data = &ltr579_addr_data,
	.driver = {
		//.owner          = THIS_MODULE,
		.name           = LTR579_DEV_NAME,
	},
};


/* 
 * #########
 * ## I2C ##
 * #########
 */

// I2C Read
static int ltr579_i2c_read_reg(u8 regnum)
{
    u8 buffer[1],reg_value[1];
	int res = 0;
	mutex_lock(&read_lock);
	
	buffer[0]= regnum;
	res = i2c_master_send(ltr579_obj->client, buffer, 0x1);
	if(res <= 0)	{
	   	mutex_unlock(&read_lock);
	       APS_ERR("read reg send res = %d\n",res);
		return res;
	}
	res = i2c_master_recv(ltr579_obj->client, reg_value, 0x1);
	if(res <= 0)
	{
		mutex_unlock(&read_lock);
		APS_ERR("read reg recv res = %d\n",res);
		return res;
	}
	mutex_unlock(&read_lock);
	return reg_value[0];
}

// I2C Write
static int ltr579_i2c_write_reg(u8 regnum, u8 value)
{
	u8 databuf[2];    
	int res = 0;
   
	databuf[0] = regnum;   
	databuf[1] = value;
	res = i2c_master_send(ltr579_obj->client, databuf, 0x2);

	if (res < 0)
		{
			APS_ERR("wirte reg send res = %d\n",res);
		   	return res;
		}
		
	else
		return 0;
}


/*----------------------------------------------------------------------------*/
static ssize_t ltr579_show_als(struct device_driver *ddri, char *buf)
{
	int res;
	u8 dat = 0;
	
	if(!ltr579_obj)
	{
		APS_ERR("ltr579_obj is null!!\n");
		return 0;
	}
	res = ltr579_als_read(als_gainrange);
    return snprintf(buf, PAGE_SIZE, "0x%04X\n", res);    
	
}
/*----------------------------------------------------------------------------*/
static ssize_t ltr579_show_ps(struct device_driver *ddri, char *buf)
{
	int  res;
	if(!ltr579_obj)
	{
		APS_ERR("ltr579_obj is null!!\n");
		return 0;
	}
	res = ltr579_ps_read();
    return snprintf(buf, PAGE_SIZE, "0x%04X\n", res);     
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static ssize_t ltr579_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	
	if(!ltr579_obj)
	{
		APS_ERR("ltr579_obj is null!!\n");
		return 0;
	}
	
	if(ltr579_obj->hw)
	{
	
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d, (%d %d)\n", 
			ltr579_obj->hw->i2c_num, ltr579_obj->hw->power_id, ltr579_obj->hw->power_vol);
		
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}


	len += snprintf(buf+len, PAGE_SIZE-len, "MISC: %d %d\n", atomic_read(&ltr579_obj->als_suspend), atomic_read(&ltr579_obj->ps_suspend));

	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr579_store_status(struct device_driver *ddri, char *buf, size_t count)
{
	int status1,ret;
	if(!ltr579_obj)
	{
		APS_ERR("ltr579_obj is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "%d ", &status1))
	{ 
	    ret=ltr579_ps_enable();
		APS_DBG("iret= %d \n", ret);
	}
	else
	{
		APS_DBG("invalid content: '%s', length = %d\n", buf, (int)count);
	}
	return count;    
}


/*----------------------------------------------------------------------------*/
static ssize_t ltr579_show_reg(struct device_driver *ddri, char *buf, size_t count)
{
	int i,len=0;
	int reg[]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0d,0x0e,0x0f,
				0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26};
	for(i=0;i<27;i++)
		{
		len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%04X value: 0x%04X\n", reg[i],ltr579_i2c_read_reg(reg[i]));	

	    }
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t ltr579_store_reg(struct device_driver *ddri, char *buf, size_t count)
{
	int ret,value;
	int reg;
	if(!ltr579_obj)
	{
		APS_ERR("ltr579_obj is null!!\n");
		return 0;
	}
	
	if(2 == sscanf(buf, "%x %x ", &reg,&value))
	{ 
		APS_DBG("before write reg: %x, reg_value = %x  write value=%x\n", reg,ltr579_i2c_read_reg(reg),value);
	    ret=ltr579_i2c_write_reg(reg,value);
		APS_DBG("after write reg: %x, reg_value = %x\n", reg,ltr579_i2c_read_reg(reg));
	}
	else
	{
		APS_DBG("invalid content: '%s', length = %d\n", buf, (int)count);
	}
	return count;    
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(als,     S_IWUSR | S_IRUGO, ltr579_show_als,   NULL);
static DRIVER_ATTR(ps,      S_IWUSR | S_IRUGO, ltr579_show_ps,    NULL);
//static DRIVER_ATTR(config,  S_IWUSR | S_IRUGO, ltr579_show_config,ltr579_store_config);
//static DRIVER_ATTR(alslv,   S_IWUSR | S_IRUGO, ltr579_show_alslv, ltr579_store_alslv);
//static DRIVER_ATTR(alsval,  S_IWUSR | S_IRUGO, ltr579_show_alsval,ltr579_store_alsval);
//static DRIVER_ATTR(trace,   S_IWUSR | S_IRUGO,ltr579_show_trace, ltr579_store_trace);
static DRIVER_ATTR(status,  S_IWUSR | S_IRUGO, ltr579_show_status,  ltr579_store_status);
static DRIVER_ATTR(reg,     S_IWUSR | S_IRUGO, ltr579_show_reg,   ltr579_store_reg);
//static DRIVER_ATTR(i2c,     S_IWUSR | S_IRUGO, ltr579_show_i2c,   ltr579_store_i2c);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *ltr579_attr_list[] = {
    &driver_attr_als,
    &driver_attr_ps,    
   // &driver_attr_trace,        /*trace log*/
   // &driver_attr_config,
   // &driver_attr_alslv,
   // &driver_attr_alsval,
    &driver_attr_status,
   //&driver_attr_i2c,
    &driver_attr_reg,
};
/*----------------------------------------------------------------------------*/
static int ltr579_create_attr(struct driver_attribute *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(ltr579_attr_list)/sizeof(ltr579_attr_list[0]));

	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(err = driver_create_file(driver, ltr579_attr_list[idx]))
		{            
			APS_ERR("driver_create_file (%s) = %d\n", ltr579_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
	static int ltr579_delete_attr(struct device_driver *driver)
	{
	int idx ,err = 0;
	int num = (int)(sizeof(ltr579_attr_list)/sizeof(ltr579_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++) 
	{
		driver_remove_file(driver, ltr579_attr_list[idx]);
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/

/* 
 * ###############
 * ## PS CONFIG ##
 * ###############

 */
#ifdef GN_MTK_BSP_PS_DYNAMIC_CALI
static int ltr579_dynamic_calibrate(void)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	int data = 0;
	int noise = 0;
	int len = 0;
	int err = 0;
	int max = 0;
	int idx_table = 0;
	unsigned long data_total = 0;
	struct ltr579_priv *obj = ltr579_obj;

	APS_FUN(f);
	if (!obj) goto err;

	//mdelay(10);
	
	for (i = 0; i < 5; i++) {
		if (max++ > 5) {
			atomic_set(&obj->ps_thd_val_high,  2047);
			atomic_set(&obj->ps_thd_val_low, 2047);
			
			goto err;
		}
		mdelay(26);
		
		data = ltr579_ps_read();
		if(data == 0){
			j++;
		}	
		data_total += data;
	}

	if(data_total > 0){
		noise = data_total/(5 - j);
	}else{
		noise =0;
	}
	if(ps_calibrate_by_factory == 1){
		if(noise > 500){
				noise = 500;
		}
	}else if(ps_calibrate_by_factory == 0)	{
		if(noise > 50){
			noise = 30;
		}
	}	
	
	dynamic_cali = noise;
	printk("ltr579_dynamic_calibrate =%d\n",noise);
	isadjust = 1;
	if(noise < 100){
//			atomic_set(&obj->ps_thd_val_high,  noise+70); //near:6cm
//			atomic_set(&obj->ps_thd_val_low, noise+50);
//			atomic_set(&obj->ps_thd_val_high,  noise+300); //near:2cm, far:2.5cm.
//			atomic_set(&obj->ps_thd_val_low, noise+250);	
			atomic_set(&obj->ps_thd_val_high,  noise+165); 
			atomic_set(&obj->ps_thd_val_low, noise+145);	
	}else if(noise < 200){
			atomic_set(&obj->ps_thd_val_high,  noise+100);
			atomic_set(&obj->ps_thd_val_low, noise+80);
	}else if(noise < 300){
			atomic_set(&obj->ps_thd_val_high,  noise+120);
			atomic_set(&obj->ps_thd_val_low, noise+100);
	}else if(noise < 400){
			atomic_set(&obj->ps_thd_val_high,  noise+130);
			atomic_set(&obj->ps_thd_val_low, noise+100);
	}else if(noise < 600){
			atomic_set(&obj->ps_thd_val_high,  noise+150);
			atomic_set(&obj->ps_thd_val_low, noise+120);
	}else if(noise < 1000){
			atomic_set(&obj->ps_thd_val_high,  noise+160);
			atomic_set(&obj->ps_thd_val_low, noise+140);	
	}else if(noise < 1450){
			atomic_set(&obj->ps_thd_val_high,  noise+400);
			atomic_set(&obj->ps_thd_val_low, noise+300);
	}
	else{
			atomic_set(&obj->ps_thd_val_high,  1600);
			atomic_set(&obj->ps_thd_val_low, 1450);
			isadjust = 0;
		printk(KERN_ERR "ltr579 the proximity sensor structure is error\n");
	}
	
	//
	int ps_thd_val_low,	ps_thd_val_high ;
	
	ps_thd_val_low = atomic_read(&obj->ps_thd_val_low);
	ps_thd_val_high = atomic_read(&obj->ps_thd_val_high);
	printk(" ltr579_dynamic_calibrate end:noise=%d, obj->ps_thd_val_low= %d , obj->ps_thd_val_high = %d\n", noise, ps_thd_val_low, ps_thd_val_high);

	return 0;
err:
	APS_ERR("ltr579_dynamic_calibrate fail!!!\n");
	return -1;
}
#endif



static int ltr579_factory_calibrate(void)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	int data = 0;
	int noise = 0;
	int len = 0;
	int err = 0;
	int max = 0;
	int idx_table = 0;
	u8 buffer[2] = {0};
	unsigned long data_total = 0;
	struct ltr579_priv *obj = ltr579_obj;

	APS_FUN(f);
	if (!obj) goto err;

 	ltr579_i2c_write_reg(LTR579_PS_CAN_0,  0);
	ltr579_i2c_write_reg(LTR579_PS_CAN_1,  0);
	mdelay(10);
	if(test_bit(CMC_BIT_PS, &obj->enable)){
		printk("LTR579 PS already enable11\n");
	}
	else{
		err = ltr579_ps_factory_enable();
		if( err < 0){
			calibration_flag = 0;
			return -1;
			}
	}
	
	for (i = 0; i < 15; i++) {
		mdelay(50);
		data = ltr579_ps_read();
		//if(data == 0){
			//j++;
		//}	
		data_total += data;
	}

	if(data_total > 0){
		//noise = data_total/(15-j);
		noise = data_total/15;		
	}else{
		noise = 0;
	}

#if 0
	if(noise < 100){
		atomic_set(&obj->ps_thd_val_high,  noise+70);
		atomic_set(&obj->ps_thd_val_low, noise+50);
	}else if(noise < 200){
		atomic_set(&obj->ps_thd_val_high,  noise+100);
		atomic_set(&obj->ps_thd_val_low, noise+80);
	}else if(noise < 300){
		atomic_set(&obj->ps_thd_val_high,  noise+120);
		atomic_set(&obj->ps_thd_val_low, noise+100);
	}else if(noise < 400){
		atomic_set(&obj->ps_thd_val_high,  noise+130);
		atomic_set(&obj->ps_thd_val_low, noise+100);
	}else if(noise < 600){
		atomic_set(&obj->ps_thd_val_high,  noise+150);
		atomic_set(&obj->ps_thd_val_low, noise+120);
	}else if(noise < 1000){
		atomic_set(&obj->ps_thd_val_high,  noise+160);
		atomic_set(&obj->ps_thd_val_low, noise+140);	
	}else if(noise < 1450){
		atomic_set(&obj->ps_thd_val_high,  noise+400);
		atomic_set(&obj->ps_thd_val_low, noise+300);
	}
	else{
		atomic_set(&obj->ps_thd_val_high,  1600);
		atomic_set(&obj->ps_thd_val_low, 1450);
		printk(KERN_ERR "ltr579 the proximity sensor structure is error\n");
	}
#endif	
		APS_ERR( "ltr579 the proximity sensor structure NOISE is:%d\n", noise);
	
#if 0
	err = ltr579_i2c_write_reg(LTR579_PS_CAN_0,  (noise&0xff));
    	err = ltr579_i2c_write_reg(LTR579_PS_CAN_1, (noise>>8)&0x07); 
#endif

	if(test_bit(CMC_BIT_PS, &obj->enable)){
		printk("LTR579 PS already enable22\n");
	}else{
		ltr579_ps_factory_disable();
	}
	calibration_flag = 1;
	calib_value = noise;
	return noise;
err:
	APS_ERR("ltr579_dynamic_calibrate fail!!!\n");
	calibration_flag = 0;
	return -1;
}

static int ltr579_wtfactory_calibrate(void)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	int data = 0;
	int noise = 0;
	int len = 0;
	int err = 0;
	int max = 0;
	int idx_table = 0;
	u8 buffer[2] = {0};
	unsigned long data_total = 0;
	struct ltr579_priv *obj = ltr579_obj;

	APS_FUN(f);
	if (!obj) goto err;

 	ltr579_i2c_write_reg(LTR579_PS_CAN_0,  0);
	ltr579_i2c_write_reg(LTR579_PS_CAN_1,  0);
	mdelay(10);
	if(test_bit(CMC_BIT_PS, &obj->enable)){
		printk("LTR579 PS already enable\n");
	}
	else{
		err = ltr579_ps_factory_enable();
		if( err < 0){
			calibration_flag = 0;
			return -1;
		}
	}
	
	for (i = 0; i < 15; i++) {
		mdelay(50);
		data = ltr579_ps_read();
		if(data == 0){
			j++;
		}	
		data_total += data;
	}

	if(data_total > 0){
		noise = data_total/(15-j);
	}else{
		noise = 0;
	}



#if 0
	if(noise < 100){
		atomic_set(&obj->ps_thd_val_high,  noise+70);
		atomic_set(&obj->ps_thd_val_low, noise+50);
	}else if(noise < 200){
		atomic_set(&obj->ps_thd_val_high,  noise+100);
		atomic_set(&obj->ps_thd_val_low, noise+80);
	}else if(noise < 300){
		atomic_set(&obj->ps_thd_val_high,  noise+120);
		atomic_set(&obj->ps_thd_val_low, noise+100);
	}else if(noise < 400){
		atomic_set(&obj->ps_thd_val_high,  noise+130);
		atomic_set(&obj->ps_thd_val_low, noise+100);
	}else if(noise < 600){
		atomic_set(&obj->ps_thd_val_high,  noise+150);
		atomic_set(&obj->ps_thd_val_low, noise+120);
	}else if(noise < 1000){
		atomic_set(&obj->ps_thd_val_high,  noise+160);
		atomic_set(&obj->ps_thd_val_low, noise+140);	
	}else if(noise < 1450){
		atomic_set(&obj->ps_thd_val_high,  noise+400);
		atomic_set(&obj->ps_thd_val_low, noise+300);
	}
	else{
		atomic_set(&obj->ps_thd_val_high,  1600);
		atomic_set(&obj->ps_thd_val_low, 1450);
		printk(KERN_ERR "ltr579 the proximity sensor structure is error\n");
	}
#endif 	
#if 1
	err = ltr579_i2c_write_reg(LTR579_PS_CAN_0,  (noise&0xff));
    	err = ltr579_i2c_write_reg(LTR579_PS_CAN_1, (noise>>8)&0x07); 
	atomic_set(&obj->ps_thd_val_high,  70);
	atomic_set(&obj->ps_thd_val_low, 50);
#endif

	if(test_bit(CMC_BIT_PS, &obj->enable)){
		printk("LTR579 PS already enable22\n");
	}else{
		err = ltr579_ps_factory_disable();
		if ( err < 0){
			printk("LTR579 PS disable failed\n");
			calibration_flag = 0;
			return -1;
		}
	}
	calibration_flag =1 ;
	calib_value = noise;
	return noise;
	
err:
	APS_ERR("ltr579_dynamic_calibrate fail!!!\n");
	calibration_flag = 0;
	return -1;
}

static int ltr579_ps_set_thres()
{
	APS_FUN();

	int res;
	u8 databuf[2];
	
	struct i2c_client *client = ltr579_obj->client;
	struct ltr579_priv *obj = ltr579_obj;
	
	databuf[0] = LTR579_PS_THRES_LOW_0; 
	databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low)) & 0x00FF);
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return LTR579_ERR_I2C;
	}
	databuf[0] = LTR579_PS_THRES_LOW_1; 
	databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low )>> 8) & 0x00FF);
		
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return LTR579_ERR_I2C;
	}
	databuf[0] = LTR579_PS_THRES_UP_0;	
	databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_high)) & 0x00FF);
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return LTR579_ERR_I2C;
	}
	databuf[0] = LTR579_PS_THRES_UP_1;	
	databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_high) >> 8) & 0x00FF);
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return LTR579_ERR_I2C;
	}
	
	res = 0;
	return res;
	
	EXIT_ERR:
	APS_ERR("set thres: %d\n", res);
	return res;

}


static int ltr579_ps_factory_enable()
{
	struct i2c_client *client = ltr579_obj->client;
	struct ltr579_priv *obj = ltr579_obj;
	u8 databuf[2];	
	int res;
	int rawdata_ps;
	int ps_ena_flag = 0;

	int error;
	u8 setctrl;
    	APS_LOG("ltr579_ps_enable() ...start!\n");

	mdelay(WAKEUP_DELAY);

#if 0	
   	error = ltr579_i2c_write_reg(LTR579_PS_PULSES, 32); //32pulses 
	if(error<0)
    	{
        APS_LOG("ltr579_ps_enable() PS Pulses error2\n");
	    return error;
	} 
	error = ltr579_i2c_write_reg(LTR579_PS_LED, 0x36); // 60khz & 100mA 
	if(error<0)
    	{
        APS_LOG("ltr579_ps_enable() PS LED error...\n");
	    return error;
	}
	
	error = ltr579_i2c_write_reg(LTR579_PS_MEAS_RATE, 0x5C); // 11bits & 50ms time 
	if(error<0)
    	{
        APS_LOG("ltr579_ps_enable() PS time error...\n");
	    return error;
	}	
#endif	

 	setctrl = ltr579_i2c_read_reg(LTR579_MAIN_CTRL);
	setctrl = setctrl |0x01;
	error = ltr579_i2c_write_reg(LTR579_MAIN_CTRL, setctrl); 
	if(error<0)
	{
	   	 APS_LOG("ltr579_ps_enable() error1\n");
	    	return error;
	}
	mdelay(10);
	
	APS_LOG("ltr579_ps_enable ...OK!  %d\n", setctrl);

	return error;

	EXIT_ERR:
	APS_ERR("set thres: %d\n", res);
	return res;
}

static int ltr579_ps_factory_disable()
{
	int error;
	struct ltr579_priv *obj = ltr579_obj;
	u8 setctrl;
	
	setctrl = ltr579_i2c_read_reg(LTR579_MAIN_CTRL);
	setctrl = setctrl & 0xFE; 	
	error = ltr579_i2c_write_reg(LTR579_MAIN_CTRL, setctrl);
	if(error<0){
		calibration_flag = 0;
 	   	APS_LOG("ltr579_ps_factory_disable ...ERROR\n");
	}
 	else
        	APS_LOG("ltr579_ps_factory_disable ...OK\n");
	
	return error;
}


static int ltr579_ps_enable()
{
	struct i2c_client *client = ltr579_obj->client;
	struct ltr579_priv *obj = ltr579_obj;
	u8 databuf[2];	
	int res;
	int rawdata_ps;
	int ps_ena_flag = 0;
	int value1  = 1;

	int error;
	u8 setctrl;
    	APS_LOG("ltr579_ps_enable() ...start!\n");

	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		APS_DBG("ALS already enabled . At ps_enbale function.\n");
	}else{
		ltr579_als_enable(ALS_RANGE_18);
		set_bit(CMC_BIT_ALS, &obj->enable);
	}
	
	if( first_enable == 1 )
       {
		enable_irq(ltr579_obj->irq);
	}
    
	if ( obj->ps_enable_flag == 1)
	{
		APS_ERR("PS already enabled \n");
		return 0;
	}
	obj->ps_enable_flag = 1;
	ps_state_for_store = 1;
	
	if( ( first_enable_flag == 1 ) && ( ps_offset_flag == 0)){
		first_enable_flag = 0;
		res = set_crosstalk(4);
		if(res < 0 ){
			APS_ERR("set_crosstalk error!\n");
		}
	}

	mdelay(WAKEUP_DELAY);
    
   	error = ltr579_i2c_write_reg(LTR579_PS_PULSES, 48); //32pulses 
	if(error<0)
    	{
        APS_LOG("ltr579_ps_enable() PS Pulses error2\n");
	    return error;
	} 
	//error = ltr579_i2c_write_reg(LTR579_PS_LED, 0x36); // 60khz & 100mA 
	error = ltr579_i2c_write_reg(LTR579_PS_LED, 0x34); // 60khz & 100mA 
	if(error<0)
    	{
        APS_LOG("ltr579_ps_enable() PS LED error...\n");
	    return error;
	}
	
	error = ltr579_i2c_write_reg(LTR579_PS_MEAS_RATE, 0x5C); // 11bits & 50ms time 
	if(error<0)
    	{
        APS_LOG("ltr579_ps_enable() PS time error...\n");
	    return error;
	}

	if(0 == obj->hw->polling_mode_ps)
	{		

		//ltr579_ps_set_thres();
			
		databuf[0] = LTR579_INT_CFG;	
		databuf[1] = 0x03; //0x01;  0x03 for PS Logic Output Mode
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
			return LTR579_ERR_I2C;
		}
	
		databuf[0] = LTR579_INT_PST;	
		databuf[1] = 0x03;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
			return LTR579_ERR_I2C;
		}
	}

 	setctrl = ltr579_i2c_read_reg(LTR579_MAIN_CTRL);
	setctrl = setctrl |0x01;
	error = ltr579_i2c_write_reg(LTR579_MAIN_CTRL, setctrl); 
	if(error<0)
	{
	   	 APS_LOG("ltr579_ps_enable() error1\n");
	    	return error;
	}

	mdelay(10);
	
	APS_LOG("ltr579_ps_enable ...OK!  %d\n", setctrl);
	
	#ifdef GN_MTK_BSP_PS_DYNAMIC_CALI
//	if( first_enable == 1 ){
	ltr579_dynamic_calibrate();
		APS_LOG("ltr579_ps_enable one time\n");
//	}
	#endif
	
	ltr579_ps_set_thres();

	if((res = hwmsen_get_interrupt_data(ID_PROXIMITY, &value1)))
	{
		  APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", res);
	}	

	enable_irq(ltr579_obj->irq);		
	if( first_enable == 1 ){
		first_enable = 0;
//		enable_irq(ltr579_obj->irq);
	}
	return error;

	EXIT_ERR:
	APS_ERR("set thres: %d\n", res);
	return res;
}

// Put PS into Standby mode
static int ltr579_ps_disable(void)
{
	int error;
	struct ltr579_priv *obj = ltr579_obj;
	u8 setctrl;
	if ( obj->ps_enable_flag == 0)
	{
		APS_ERR("PS already disabled \n");
		return 0;
	}
	obj->ps_enable_flag = 0;
	setctrl = ltr579_i2c_read_reg(LTR579_MAIN_CTRL);
	setctrl = setctrl & 0xFE; 	
	error = ltr579_i2c_write_reg(LTR579_MAIN_CTRL, setctrl);
	if(error<0)
 	   	APS_LOG("ltr579_ps_disable ...ERROR\n");
 	else
        	APS_LOG("ltr579_ps_disable ...OK\n");
	
	disable_irq_nosync(ltr579_obj->irq);

	return error;
}


static int ltr579_ps_read(void)
{
	int psval_lo, psval_hi, psdata;

	psval_lo = ltr579_i2c_read_reg(LTR579_PS_DATA_0);
	APS_DBG("ps_rawdata_psval_lo = %d\n", psval_lo);
	if (psval_lo < 0){
	    
	    APS_DBG("psval_lo error\n");
		psdata = psval_lo;
		goto out;
	}
	psval_hi = ltr579_i2c_read_reg(LTR579_PS_DATA_1);
    APS_DBG("ps_rawdata_psval_hi = %d\n", psval_hi);

	if (psval_hi < 0){
	    APS_DBG("psval_hi error\n");
		psdata = psval_hi;
		goto out;
	}
	
	psdata = ((psval_hi & 7)* 256) + psval_lo;
    //psdata = ((psval_hi&0x7)<<8) + psval_lo;
    APS_DBG("ps_rawdata = %d\n", psdata);
    
	out:
	final_prox_val = psdata;
	
	return psdata;
}

/* 
 * ################
 * ## ALS CONFIG ##
 * ################
 */

static int ltr579_als_enable(int gainrange)
{
	int error;
	u8 setctrl;
    	int ps_ena_flag = 0;
   	int res =0;
	int data = 0;
	int value0 = 0, value1 = 1;
	gainrange = ALS_RANGE_18;	
	APS_LOG("gainrange = %d\n",gainrange);

	if( 1 == ltr579_obj->als_enable_flag ){
		APS_ERR("Als already enabled \n");
		return 0;
	}
	ltr579_obj->als_enable_flag = 1;
	
	switch (gainrange)
	{
		case ALS_RANGE_1:
			error = ltr579_i2c_write_reg(LTR579_ALS_GAIN, MODE_ALS_Range1);
			break;

		case ALS_RANGE_3:
			error = ltr579_i2c_write_reg(LTR579_ALS_GAIN, MODE_ALS_Range3);
			break;

		case ALS_RANGE_6:
			error = ltr579_i2c_write_reg(LTR579_ALS_GAIN, MODE_ALS_Range6);
			break;
			
		case ALS_RANGE_9:
			error = ltr579_i2c_write_reg(LTR579_ALS_GAIN, MODE_ALS_Range9);
			break;
			
		case ALS_RANGE_18:
			error = ltr579_i2c_write_reg(LTR579_ALS_GAIN, MODE_ALS_Range18);
			break;
		
		default:
			error = ltr579_i2c_write_reg(LTR579_ALS_GAIN, MODE_ALS_Range3);	
			if( error < 0)
				APS_ERR("ALS sensor gainrange failed!\n");
			APS_ERR("ALS sensor gainrange %d!\n", gainrange);
			break;
	}

//	error = ltr579_i2c_write_reg(LTR579_ALS_MEAS_RATE, ALS_RESO_MEAS);// 18 bit & 100ms measurement rate	
	error = ltr579_i2c_write_reg(LTR579_ALS_MEAS_RATE, ALS_RESO_MEAS_50MS);// 17 bit & 50ms measurement rate		
	APS_ERR("ALS sensor resolution & measurement rate: %d!\n", ALS_RESO_MEAS );	

	setctrl = ltr579_i2c_read_reg(LTR579_MAIN_CTRL);
	setctrl = setctrl | 0x02;// Enable ALS
	
	error = ltr579_i2c_write_reg(LTR579_MAIN_CTRL, setctrl);	
 	if(error<0)
 	    APS_LOG("ltr579_als_enable ...ERROR\n");
 	else
        APS_LOG("ltr579_als_enable ...OK\n");

	mdelay(WAKEUP_DELAY);
#if 1
    	ps_ena_flag = test_bit(CMC_BIT_PS, &ltr579_obj->enable) ? (1) : (0);
	if(ps_ena_flag == 1){
        	printk("ltr579 added by fully for test\n");
  //      	res = ltr579_ps_read(ltr579_obj->client, &ltr579_obj->ps);
  		data	= ltr579_ps_read();
       	 if(data < atomic_read(&ltr579_obj->ps_thd_val_high)){
            		printk("ltr579als added by fully for test2222 ps_data: %d, high_value:%d\n", ltr579_obj->ps, atomic_read(&ltr579_obj->ps_thd_val_high));
  //          		res = ps_report_interrupt_data(1);
			if((res = hwmsen_get_interrupt_data(ID_PROXIMITY, &value1)))
			{
		  		APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", res);
			}			
       	 }else if(data > atomic_read(&ltr579_obj->ps_thd_val_high)){
//			res = ps_report_interrupt_data(0);
			if((res = hwmsen_get_interrupt_data(ID_PROXIMITY, &value0)))
			{
		  		APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", res);
			}	
			printk("ltr579als added by fully for test 333.\n");
       	 }
    	}   	
#endif	
	return error;
}


// Put ALS into Standby mode
static int ltr579_als_disable(void)
{
	int error;
	u8 setctrl;
	struct ltr579_priv *obj = ltr579_obj;
	if ( obj->ps_enable_flag == 1){
		APS_DBG("ltr579_als_disable return. Ps is enabled\n");
		return 0;
	}	

	if ( 0 == obj->als_enable_flag){
		APS_DBG("ltr579_als_disable return. Als already disabled\n");
		return 0;
	}	
	obj->als_enable_flag = 0;

	setctrl = ltr579_i2c_read_reg(LTR579_MAIN_CTRL);
	setctrl = setctrl & 0xFD;// disable ALS	

	error = ltr579_i2c_write_reg(LTR579_MAIN_CTRL, setctrl); 
	if(error<0)
 	    APS_LOG("ltr579_als_disable ...ERROR\n");
 	else
        APS_LOG("ltr579_als_disable ...OK\n");
	
	clear_bit(CMC_BIT_ALS, &obj->enable);
	return error;
}

static int ltr579_als_read(int gainrange)
{
	int alsval_0, alsval_1, alsval_2, alsval;
	int luxdata_int;
	
	alsval_0 = ltr579_i2c_read_reg(LTR579_ALS_DATA_0);
	alsval_1 = ltr579_i2c_read_reg(LTR579_ALS_DATA_1);
	alsval_2 = ltr579_i2c_read_reg(LTR579_ALS_DATA_2);
	alsval = (alsval_2 * 256* 256) + (alsval_1 * 256) + alsval_0;
//	APS_DBG("alsval_0 = %d,alsval_1=%d,alsval_2=%d,alsval=%d\n",alsval_0,alsval_1,alsval_2,alsval);
    	if(alsval==0)
    	{
        	luxdata_int = 0;
        	goto err;
    	}

	APS_DBG("gainrange = %d\n",gainrange);
//	luxdata_int = alsval*8/gainrange/10;//formula: ALS counts * 0.8/gain/int , int=1
	if ( 1 == tp_type_flag){
		luxdata_int = alsval*80/(gainrange*22);//formula: ALS counts * 0.8/gain/int , int=1
	}else {
		luxdata_int = alsval*80/(gainrange*22);//formula: ALS counts * 0.8/gain/int , int=1
	}
	APS_DBG("als_value_lux = %d\n", luxdata_int);
	
	return luxdata_int;
err:
	final_lux_val = luxdata_int;
	APS_DBG("err als_value_lux = 0x%x\n", luxdata_int);
	return luxdata_int;
}


int ltr579_als_aal_read(void)
{
	int alsval_0, alsval_1, alsval_2, alsval;
	int luxdata_int;
	struct ltr579_priv *obj = ltr579_obj;
	int err = 0;
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		APS_DBG("ALS already enabled\n");
	}else{
	 	 err = ltr579_als_enable(als_gainrange);
	   	 if (err < 0){
			APS_ERR("enable als fail: %d\n", err);   
			return -1;
		}	
		set_bit(CMC_BIT_ALS, &obj->enable);
	}
	
	alsval_0 = ltr579_i2c_read_reg(LTR579_ALS_DATA_0);
	alsval_1 = ltr579_i2c_read_reg(LTR579_ALS_DATA_1);
	alsval_2 = ltr579_i2c_read_reg(LTR579_ALS_DATA_2);
	alsval = (alsval_2 * 256* 256) + (alsval_1 * 256) + alsval_0;
//	APS_DBG("ltr579_als_aal_read alsval_0 = %d,alsval_1=%d,alsval_2=%d,alsval=%d\n",alsval_0,alsval_1,alsval_2,alsval);

    	if(alsval==0)
    	{
        	luxdata_int = 0;
        	goto err;
    	}

//	APS_DBG("gainrange = %d\n",gainrange);	
//	luxdata_int = alsval*8/gainrange/10;//formula: ALS counts * 0.8/gain/int , int=1
	if ( 1 == tp_type_flag){
		luxdata_int = alsval*80/(als_gainrange*22);//formula: ALS counts * 0.8/gain/int , int=1
	}else {
		luxdata_int = alsval*80/(als_gainrange*22);//formula: ALS counts * 0.8/gain/int , int=1
	}
//	APS_DBG("als_value_lux = %d\n", luxdata_int);
	
	return luxdata_int;

err:
	final_lux_val = luxdata_int;
	APS_DBG("err als_value_lux = 0x%x\n", luxdata_int);
	return luxdata_int;
}
EXPORT_SYMBOL(ltr579_als_aal_read);

/*----------------------------------------------------------------------------*/
int ltr579_get_addr(struct alsps_hw *hw, struct ltr579_i2c_addr *addr)
{
	/***
	if(!hw || !addr)
	{
		return -EFAULT;
	}
	addr->write_addr= hw->i2c_addr[0];
	***/
	return 0;
}

/*-----------------------------------------------------------------------------*/
void ltr579_eint_func(void)
{
	APS_FUN();

	struct ltr579_priv *obj = ltr579_obj;
	if(!obj)
	{
		return;
	}
	
	schedule_work(&obj->eint_work);
	//schedule_delayed_work(&obj->eint_work);
}

static irqreturn_t ltr579_eint_handler(int irq, void *desc)
{
	APS_FUN();
	
	disable_irq_nosync(ltr579_obj->irq);
	
	ltr579_eint_func();
	
	return IRQ_HANDLED;
}


int ltr579_setup_eint(struct i2c_client *client)
{

	APS_FUN();

#if defined(CONFIG_OF)
	u32 ints[2] = {0, 0};
#endif

	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	//mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	//mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);	
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

#if defined(CONFIG_OF)
	APS_LOG("ltr579_setup_eint __CONFIG_OF__!\n");
	ltr579_obj->irq_node = of_find_compatible_node(NULL, NULL, "mediatek, ALS-eint");
	if (ltr579_obj->irq_node)
	{
		of_property_read_u32_array(ltr579_obj->irq_node, "debounce", ints, ARRAY_SIZE(ints));
		mt_gpio_set_debounce(ints[0], ints[1]);
		APS_LOG("ints[0] = %d, ints[1] = %d!!\n", ints[0], ints[1]);
		
		ltr579_obj->irq = irq_of_parse_and_map(ltr579_obj->irq_node, 0);
		APS_LOG("ltr579_obj->irq = %d\n", ltr579_obj->irq);
		if (!ltr579_obj->irq)
		{
			APS_ERR("irq_of_parse_and_map fail!!\n");
			return -EINVAL;
		}
		
//		if(request_irq(ltr579_obj->irq, ltr579_eint_handler, IRQF_TRIGGER_NONE, "ALS-eint", NULL)) {
		if(request_irq(ltr579_obj->irq, ltr579_eint_handler, IRQF_TRIGGER_NONE, "ALS-eint", NULL)) {
			APS_ERR("IRQ LINE NOT AVAILABLE!!\n");
			return -EINVAL;
		}

        	disable_irq_nosync(ltr579_obj->irq);
		
		APS_LOG(" after ltr579_setup_eint  register!\n");
		
	}else{
		APS_ERR("null irq node!!\n");
		return -EINVAL;
	}
#endif	

    return 0;
}



/*----------------------------------------------------------------------------*/
static void ltr579_power(struct alsps_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	//APS_LOG("power %s\n", on ? "on" : "off");

	if(hw->power_id != POWER_NONE_MACRO)
	{
		if(power_on == on)
		{
			APS_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "ltr579")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "ltr579")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}
	power_on = on;
}

/*----------------------------------------------------------------------------*/
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
static int ltr579_check_and_clear_intr(struct i2c_client *client) 
{
//***
	APS_FUN();

	int res,intp,intl;
	u8 buffer[2];	
	u8 temp;
		//if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/	
		//	  return 0;
	
		buffer[0] = LTR579_MAIN_STATUS;
		res = i2c_master_send(client, buffer, 0x1);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		res = i2c_master_recv(client, buffer, 0x1);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		temp = buffer[0];
		res = 1;
		intp = 0;
		intl = 0;
		if(0 != (buffer[0] & 0x02))
		{
			res = 0;
			intp = 1;
		}
		if(0 != (buffer[0] & 0x10))
		{
			res = 0;
			intl = 1;		
		}
	
		if(0 == res)
		{
			if((1 == intp) && (0 == intl))
			{
				buffer[1] = buffer[0] & 0xfD;
				
			}
			else if((0 == intp) && (1 == intl))
			{
				buffer[1] = buffer[0] & 0xEF;
			}
			else
			{
				buffer[1] = buffer[0] & 0xED;
			}
			buffer[0] = LTR579_MAIN_STATUS;
			res = i2c_master_send(client, buffer, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
			else
			{
				res = 0;
			}
		}
	
		return res;
	
	EXIT_ERR:
		APS_ERR("ltr579_check_and_clear_intr fail\n");
		return 1;

}
/*----------------------------------------------------------------------------*/


static int ltr579_check_intr(struct i2c_client *client) 
{
	APS_FUN();

	int res,intp,intl;
	u8 buffer[2];

	buffer[0] = LTR579_MAIN_STATUS;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = 1;
	intp = 0;
	intl = 0;
	if(0 != (buffer[0] & 0x02))
	{
		res = 0;
		intp = 1;
	}
	if(0 != (buffer[0] & 0x10))
	{
		res = 0;
		intl = 1;		
	}

	return res;

EXIT_ERR:
	APS_ERR("ltr579_check_intr fail\n");
	return -1;
}

static int ltr579_clear_intr(struct i2c_client *client) 
{
	int res;
	u8 buffer[2];

	APS_FUN();
	
	buffer[0] = LTR579_MAIN_STATUS;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	APS_DBG("buffer[0] = %d \n",buffer[0]);
	buffer[1] = buffer[0] & 0x01;
	buffer[0] = LTR579_MAIN_STATUS;

	res = i2c_master_send(client, buffer, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	else
	{
		res = 0;
	}

	return res;

EXIT_ERR:
	APS_ERR("ltr579_check_and_clear_intr fail\n");
	return 1;
}




static int ltr579_devinit(void)
{
	int res;
	int init_als_gain;
	u8 databuf[2];	

	struct i2c_client *client = ltr579_obj->client;

	struct ltr579_priv *obj = ltr579_obj;   
	char *info = l_sensor_info;
	
	mdelay(PON_DELAY);

	// Enable ALS to Full Range at startup
	init_als_gain = ALS_RANGE_3;
	als_gainrange = init_als_gain;//Set global variable

 	res = ltr579_i2c_write_reg(LTR579_PS_PULSES, 48); //32pulses 
	if(res<0)
    	{
        APS_LOG("ltr579_ps_enable() PS Pulses error2\n");
	    return res;
	} 
//	res = ltr579_i2c_write_reg(LTR579_PS_LED, 0x36); // 60khz & 100mA 
	res = ltr579_i2c_write_reg(LTR579_PS_LED, 0x34); // 60khz & 100mA 
	if(res<0)
    	{
        APS_LOG("ltr579_ps_enable() PS LED error...\n");
	    return res;
	}
	
	res = ltr579_i2c_write_reg(LTR579_PS_MEAS_RATE, 0x5c); // 11bits & 50ms time 
	if(res<0)
    	{
        APS_LOG("ltr579_ps_enable() PS time error...\n");
	    return res;
	}

	if(0 == obj->hw->polling_mode_ps)
	{					
		databuf[0] = LTR579_INT_CFG;	
		databuf[1] = 0x03;  //0x01;  0x03 for PS Logic Output Mode
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
			return LTR579_ERR_I2C;
		}
	
		databuf[0] = LTR579_INT_PST;	
		databuf[1] = 0x03;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
			return LTR579_ERR_I2C;
		}
	}

	if((res = ltr579_setup_eint(client))!=0)
	{
		APS_ERR("setup eint: %d\n", res);
		return res;
	}
    
	 info += sprintf(info,"LTR579 ");
    	 info += sprintf(info,"LITEON");

	res = 0;

	EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;

}
/*----------------------------------------------------------------------------*/


static int ltr579_get_als_value(struct ltr579_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;
	APS_DBG("als  = %d\n",als); 
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
		APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);	
		return obj->hw->als_value[idx];
	}
	else
	{
		APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);    
		return -1;
	}
}
/*----------------------------------------------------------------------------*/

static int ltr579_get_ps_value()
{
	int ps_flag;
	int val;
	int error=-1;
	int buffer=0;

	buffer = ltr579_i2c_read_reg(LTR579_MAIN_STATUS);
	APS_LOG("Main status = %d\n", buffer);
	if (buffer < 0){
	    
	    APS_DBG("MAIN status read error\n");
		return error;
	}

	ps_flag = buffer & 0x04;
	ps_flag = ps_flag >>2;
	if(ps_flag==1) //Near
	{
	 	intr_flag_value =1;
		val=0;
	}
	else if(ps_flag ==0) //Far
	{
		intr_flag_value =0;
		val=1;
	}
    APS_DBG("ps_flag = %d, val = %d\n", ps_flag,val);
    
	return val;
}

/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/*for interrup work mode support */
static void ltr579_eint_work(struct work_struct *work)
{
	struct ltr579_priv *obj = (struct ltr579_priv *)container_of(work, struct ltr579_priv, eint_work);

	APS_FUN();

	int err;
	hwm_sensor_data sensor_data;
	u8 databuf[2];
	int res = 0;
	int ps_value;
	int intr_flag=0;

	APS_LOG("ltr579als added by fully just for test1\n");
		
//	ps_value=ltr579_get_ps_value();
//	if(ps_value < 0)
//    	{
//   		err = -1;
//   		return;
//    	}

	obj->ps = ltr579_ps_read();
    	if(obj->ps < 0)
    	{
    		err = -1;
    		return;
    	}

     	APS_DBG("ltr579_eint_work rawdata ps=%d !\n",obj->ps);
        if(obj->ps > atomic_read(&obj->ps_thd_val_high)){
	 			intr_flag = 0;
				#if defined(CONFIG_OF)
					irq_set_irq_type(obj->irq, IRQ_TYPE_LEVEL_HIGH);
//					irq_set_irq_type(obj->irq, IRQ_TYPE_EDGE_RISING);
				#elif defined(CUST_EINT_ALS_TYPE)
            				mt_eint_set_polarity(CUST_EINT_ALS_NUM, 1);	
				#endif	
					//disable_irq_nosync(obj->irq);	
					//suspend_irq_satus = 1;
					/*the ended added by fully for the interrupt 20160625*/
				
				APS_LOG("--Hysteresis Type----near------ps = %d\n",obj->ps);
			}else if(obj->ps < atomic_read(&obj->ps_thd_val_low)){
				intr_flag = 1;
				#if defined(CONFIG_OF)
					irq_set_irq_type(obj->irq, IRQ_TYPE_LEVEL_LOW);
//					irq_set_irq_type(obj->irq, IRQ_TYPE_EDGE_FALLING);
				#elif defined(CUST_EINT_ALS_TYPE)
            				mt_eint_set_polarity(CUST_EINT_ALS_NUM, 0);	
				#endif
					APS_LOG("--Hysteresis Type----far------ps = %d\n",obj->ps);
				#if 0
				if(intr_flag == 1){
					if(dynamic_cali > 30 && obj->ps < (dynamic_cali - 30)){ 
			 		if(obj->ps < 100){			
						atomic_set(&obj->ps_thd_val_high,  obj->ps+70);
						atomic_set(&obj->ps_thd_val_low, obj->ps+50);
					}else if(obj->ps < 200){
						atomic_set(&obj->ps_thd_val_high,  obj->ps+100);
						atomic_set(&obj->ps_thd_val_low, obj->ps+80);
					}else if(obj->ps < 300){
						atomic_set(&obj->ps_thd_val_high,  obj->ps+120);
						atomic_set(&obj->ps_thd_val_low, obj->ps+100);
					}else if(obj->ps < 400){
						atomic_set(&obj->ps_thd_val_high,  obj->ps+130);
						atomic_set(&obj->ps_thd_val_low, obj->ps+100);
					}else if(obj->ps < 600){
						atomic_set(&obj->ps_thd_val_high,  obj->ps+150);
						atomic_set(&obj->ps_thd_val_low, obj->ps+120);
					}else if(obj->ps < 1000){
						atomic_set(&obj->ps_thd_val_high,  obj->ps+160);
						atomic_set(&obj->ps_thd_val_low, obj->ps+140);	
					}else if(obj->ps < 1250){
						atomic_set(&obj->ps_thd_val_high,  obj->ps+400);
						atomic_set(&obj->ps_thd_val_low, obj->ps+300);
					}else{
						atomic_set(&obj->ps_thd_val_high,  1600);
						atomic_set(&obj->ps_thd_val_low, 1550);
						printk(KERN_ERR "ltr559 the proximity sensor structure is error\n");
					}
					dynamic_cali = obj->ps;
					ltr579_ps_set_thres();
					}	
				}	
				#endif
			}
			/* No need to clear interrupt flag !! */			
	if((intr_flag ==0) && (ps_state_for_store == 0)){
			msleep(30);
		   	enable_irq(ltr579_obj->irq);
			return;
	}else{
		ps_state_for_store = intr_flag;
	}
				
	sensor_data.values[0] = intr_flag;
	sensor_data.value_divide = 1;
	sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;			
	APS_DBG("intr_flag_value=%d\n",intr_flag_value);
		
	//let up layer to know
	if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
	{
		  APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
	}

	APS_DBG("after report the interrupt signal\n");
	
   	enable_irq(ltr579_obj->irq);
}



/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int ltr579_open(struct inode *inode, struct file *file)
{
	file->private_data = ltr579_i2c_client;

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int ltr579_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/


static int ltr579_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)       
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct ltr579_priv *obj = i2c_get_clientdata(client);  
	int err = 0;
	void __user *ptr = (void __user*) arg;
	int dat;
	uint32_t enable;
	APS_DBG("cmd= %d\n", cmd); 
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
			    err = ltr579_ps_enable();
				if(err < 0)
				{
					APS_ERR("enable ps fail: %d\n", err); 
					goto err_out;
				}
				set_bit(CMC_BIT_PS, &obj->enable);
			}
			else
			{
			    err = ltr579_ps_disable();
				if(err < 0)
				{
					APS_ERR("disable ps fail: %d\n", err); 
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
			APS_DBG("ALSPS_GET_PS_DATA\n"); 
		    obj->ps = ltr579_ps_read();
			if(obj->ps < 0)
			{
				goto err_out;
			}
			
			dat = ltr579_get_ps_value();
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}  
			break;

		case ALSPS_GET_PS_RAW_DATA:    
			obj->ps = ltr579_ps_read();
			if(obj->ps < 0)
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
			    err = ltr579_als_enable(als_gainrange);
				if(err < 0)
				{
					APS_ERR("enable als fail: %d\n", err); 
					goto err_out;
				}
				set_bit(CMC_BIT_ALS, &obj->enable);
			}
			else
			{
			    err = ltr579_als_disable();
				if(err < 0)
				{
					APS_ERR("disable als fail: %d\n", err); 
					goto err_out;
				}
			//	clear_bit(CMC_BIT_ALS, &obj->enable);
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
		    obj->als = ltr579_als_read(als_gainrange);
			if(obj->als < 0)
			{
				goto err_out;
			}

			dat = obj->als;
				//ltr579_get_als_value(obj, obj->als);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;

		case ALSPS_GET_ALS_RAW_DATA:    
			obj->als = ltr579_als_read(als_gainrange);
			if(obj->als < 0)
			{
				goto err_out;
			}

			dat = obj->als;
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;

		default:
			APS_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
			err = -ENOIOCTLCMD;
			break;
	}

	err_out:
	return err;    
}

/*----------------------------------------------------------------------------*/
static struct file_operations ltr579_fops = {
	//.owner = THIS_MODULE,
	.open = ltr579_open,
	.release = ltr579_release,
	.unlocked_ioctl = ltr579_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice ltr579_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &ltr579_fops,
};

static int ltr579_i2c_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct ltr579_priv *obj = i2c_get_clientdata(client);    
	int err;
	APS_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		APS_LOG("check the ltr579 suspend\n");
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return -EINVAL;
		}
		
		atomic_set(&obj->als_suspend, 1);
		err = ltr579_als_disable();
		if(err < 0)
		{
			APS_ERR("disable als: %d\n", err);
			return err;
		}
		set_bit(CMC_BIT_ALS, &obj->enable);// For ltr579_i2c_resume test_bit CMC_BIT_ALS.
#if 0
		atomic_set(&obj->ps_suspend, 1);
		err = ltr579_ps_disable();
		if(err < 0)
		{
			APS_ERR("disable ps:  %d\n", err);
			return err;
		}
		
		ltr579_power(obj->hw, 0);
#endif		

	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int ltr579_i2c_resume(struct i2c_client *client)
{
	struct ltr579_priv *obj = i2c_get_clientdata(client);        
	int err;
	APS_FUN();

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	
	//ltr579_power(obj->hw, 1);

	atomic_set(&obj->als_suspend, 0);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
	    err = ltr579_als_enable(als_gainrange);
	    if (err < 0)
		{
			APS_ERR("enable als fail: %d\n", err);        
		}
		
	}

#if 0	
	atomic_set(&obj->ps_suspend, 0);
	if(test_bit(CMC_BIT_PS,  &obj->enable))
	{
		err = ltr579_ps_enable();
	    if (err < 0)
		{
			APS_ERR("enable ps fail: %d\n", err);                
		}
	}
#endif	

	return 0;
}

static void ltr579_early_suspend(struct early_suspend *h) 
{   /*early_suspend is only applied for ALS*/
	struct ltr579_priv *obj = container_of(h, struct ltr579_priv, early_drv);   
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
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		err = ltr579_als_disable();
		if(err < 0)
		{
			APS_ERR("disable als fail: %d\n", err); 
		}
		set_bit(CMC_BIT_ALS, &obj->enable);// For ltr579_late_resume.
	}
}

static void ltr579_late_resume(struct early_suspend *h)
{   /*early_suspend is only applied for ALS*/
	struct ltr579_priv *obj = container_of(h, struct ltr579_priv, early_drv);         
	int err;
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
	      err = ltr579_als_enable(als_gainrange);
	      if(err < 0)
		{
			APS_ERR("enable als fail: %d\n", err); 
		}
	}
}



int ltr579_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct ltr579_priv *obj = (struct ltr579_priv *)self;
	
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
				    err = ltr579_ps_enable();
					if(err < 0)
					{
						APS_ERR("enable ps fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_PS, &obj->enable);
				}
				else
				{
				    err = ltr579_ps_disable();
					if(err < 0)
					{
						APS_ERR("disable ps fail: %d\n", err); 
						return -1;
					}
					clear_bit(CMC_BIT_PS, &obj->enable);
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
				APS_ERR("get sensor ps data !\n");
				sensor_data = (hwm_sensor_data *)buff_out;
				value = ltr579_get_ps_value();
			//	obj->ps = ltr579_ps_read();
    				if(obj->ps < 0)
    				{
    					err = -1;
    					break;
    				}
				sensor_data->values[0] = value;
			//	sensor_data->values[1] = ltr579_ps_read();		
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;			
			}
			break;
		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

int ltr579_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct ltr579_priv *obj = (struct ltr579_priv *)self;

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
				    err = ltr579_als_enable(als_gainrange);
					if(err < 0)
					{
						APS_ERR("enable als fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
				    err = ltr579_als_disable();
					if(err < 0)
					{
						APS_ERR("disable als fail: %d\n", err); 
						return -1;
					}
					//clear_bit(CMC_BIT_ALS, &obj->enable);
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
				APS_ERR("get sensor als data !\n");
				sensor_data = (hwm_sensor_data *)buff_out;
				obj->als = ltr579_als_read(als_gainrange);
                #if defined(MTK_AAL_SUPPORT)
				sensor_data->values[0] = obj->als;
				#else
				sensor_data->values[0] =  obj->als;//ltr579_get_als_value(obj, obj->als);
				#endif
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			}
			break;
		default:
			APS_ERR("light sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}


/*----------------------------------------------------------------------------*/
static int ltr579_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, LTR579_DEV_NAME);
	return 0;
}

static int set_crosstalk(int len)
{
	int res;
	int err; 
	int *value =(int *)psrw_buffer ;
	res = read_block(len);
	if( res < 0){
			APS_DBG("Oh sorry,  read_block error\n");
			ps_calibrate_by_factory = 1;
			return -1;
	}else{
	
		if( *value == 0)
		{
			ps_calibrate_by_factory = 1;
			return -1;
		}


#if 1
	err = ltr579_i2c_write_reg(LTR579_PS_CAN_0,  ((*value)&0xff));
    	err = ltr579_i2c_write_reg(LTR579_PS_CAN_1, ((*value)>>8)&0x07); 
	if( err < 0){
			APS_DBG("Oh sorry,  set cross cancel error\n");
			ps_calibrate_by_factory = 1;
			return -1;
	} 
#endif
	}
	ps_calibrate_by_factory = 0;
	calib_value = *value;
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
    ret = f->f_op->read(f, psrw_buffer, len, &f->f_pos);
    set_fs(fs);
    filp_close(f, NULL);
    if (ret < 0 ) {
        printk("%s: read_block read file %s error!\n", __func__, STORE_FILE);
        ret = -1;
    }
	value = (int *)psrw_buffer;
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
static ssize_t pa122_store_ps_calibration(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int status1;
	int data = 0;
	if(1 == sscanf(buf, "%d", &status1))
	{ 
		if(1== status1){
			data = ltr579_factory_calibrate();
			if ( data <= 0 || data > 1500){
			    	APS_ERR("PS pa122_store_ps_calibration error\n");
				calibration_flag = 0;
				return -1;//Modify by huangzifan 2015.11.12
			}		
		}
	}
	return size;    
}
static ssize_t pa122_show_ps_calibbias(struct device *dev,struct device_attribute *attr, char *buf)
{
		if( calibration_flag == 0){//Modify by huangzifan 2015.11.12
			return sprintf(buf, "%d\n", -1); 
		}else{
			return sprintf(buf, "%d\n",calib_value); 
		}
}
static ssize_t pa122_store_ps_offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int status1 = 0;
	int error = 0;
	if(1 == sscanf(buf, "%d", &status1))
	{ 
	 	if(status1 > 0){
			error = ltr579_i2c_write_reg(LTR579_PS_CAN_0,  (status1&0xff));
   		 	if(error<0)
    			{
       	 			APS_ERR("ltr579_ps_enable() error2\n");
       			 	return size;
    			} 

    			error = ltr579_i2c_write_reg(LTR579_PS_CAN_1, (status1>>8)&0x07); 
   		 	if(error<0)
    			{
       	 			APS_ERR("ltr579_ps_enable() error2\n");
       			 	return size;
    			} 
					ps_offset_flag = 1;
       	 			APS_DBG("LTR579 ps offset finished\n");
	 	}
		
	}
	else
	{
		APS_DBG("PS huangzifan2  : '%s'\n", buf);
	}
	return size;    
}
static ssize_t pa122_store_ps_enable_irq(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int status1,error,res;
	if(1 == sscanf(buf, "%d", &status1))
	{ 
	 if(status1 == 0){
	 	}
	 if(status1 == 1) 
	 	{
	 	}
	}
	else
	{
		APS_DBG("PS huangzifan2  : '%s'\n", buf);
	}
	return size;    
}
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
		int *value =(int *)psrw_buffer ;
		res = read_block(4);
		if( res < 0){
			return sprintf(buf, "%d\n",-1);
		}else{
			return sprintf(buf, "%d\n",(*value));
		}
}
static ssize_t pa122_store_ps_factory_calibration(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
	{
	int status1;
	int res;
	int value =  0;
	int data = 0;
	if(1 == sscanf(buf, "%d", &status1))
	{ 
		if(1== status1){
			data = ltr579_wtfactory_calibrate();
			if ( data <= 0 || data > 1500){
			    	APS_ERR("PS pa122_store_ps_factory_calibration error, data = %d\n",(int)data);
				calibration_flag = 0;
				return size;
			}
			
			value = (int) data;
			res = write_block(&value,4);
			if( res < 0){
				  APS_ERR("PS pa122_store_ps_factory_calibration write_block error\n");
				  calibration_flag = 0;
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
#endif
static DEVICE_ATTR(ps_calibration, S_IWUSR | S_IRUGO,NULL, pa122_store_ps_calibration);
static DEVICE_ATTR(ps_calibbias, S_IWUSR | S_IRUGO,pa122_show_ps_calibbias, NULL);
static DEVICE_ATTR(ps_offset, S_IWUSR | S_IRUGO,NULL, pa122_store_ps_offset);
static DEVICE_ATTR(ps_enable_irq, S_IWUSR | S_IRUGO,NULL, pa122_store_ps_enable_irq);
static DEVICE_ATTR(ps_rw_data, S_IWUSR | S_IRUGO,pa122_show_ps_rw_data, pa122_store_ps_rw_data);
static DEVICE_ATTR(ps_factory_calibration, S_IWUSR | S_IRUGO,NULL, pa122_store_ps_factory_calibration);
static DEVICE_ATTR(ps_calib_flag, S_IWUSR | S_IRUGO,pa122_show_ps_calib_flag, NULL);
static struct device_attribute *calibrate_dev_attributes[] = {
	&dev_attr_ps_calibration,
	&dev_attr_ps_calibbias,
	&dev_attr_ps_offset,
	&dev_attr_ps_enable_irq,
	&dev_attr_ps_rw_data,//Add by huangzifan 2015.11.23. For proximity calibration at factory functional test.
	&dev_attr_ps_factory_calibration,
	&dev_attr_ps_calib_flag,
};
#ifdef PS_WAKEUP
static ssize_t pa122_store_ps_wakeup_enable(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int status1;
	if(1 == sscanf(buf, "%d", &status1))
	{ 
		if(0== status1){
			APS_DBG("hzf nonwakeup ps is disable\n");
			wakeup = 1;
		}
		else if(1 == status1){	
			APS_DBG("hzf nonwakup ps is enable\n");
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
/*----------------------------------------------------------------------------*/
static int ltr579_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ltr579_priv *obj;
	struct hwmsen_object obj_ps, obj_als;
	int err = 0;

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(*obj));
	ltr579_obj = obj;

	obj->hw = get_cust_alsps_hw();
	//del for bug-35417 ltr579_get_addr(obj->hw, &obj->addr);

	INIT_WORK(&obj->eint_work, ltr579_eint_work);
	obj->client = client;
	i2c_set_clientdata(client, obj);	
	atomic_set(&obj->als_debounce, 300);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 300);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->ps_thd_val_high,  obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_thd_val_low,  obj->hw->ps_threshold_low);
	//atomic_set(&obj->als_cmd_val, 0xDF);
	//atomic_set(&obj->ps_cmd_val,  0xC1);
	atomic_set(&obj->ps_thd_val,  obj->hw->ps_threshold);
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);   
	obj->als_modulus = (400*100)/(16*150);//(1/Gain)*(400/Tine), this value is fix after init ATIME and CONTROL register value
										//(400)/16*2.72 here is amplify *100
	obj->ps_enable_flag = 0;
	obj->als_enable_flag = 0;
		
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
//	set_bit(CMC_BIT_ALS, &obj->enable);
//	set_bit(CMC_BIT_PS, &obj->enable);

	APS_LOG("ltr579_devinit() start...!\n");
	ltr579_i2c_client = client;

	if(err = ltr579_devinit())
	{
		goto exit_init_failed;
	}
	APS_LOG("ltr579_devinit() ...OK!\n");

	if(err = misc_register(&ltr579_device))
	{
		APS_ERR("ltr579_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	
	/* Register sysfs attribute */
	if(err = ltr579_create_attr(&ltr579_init_info.platform_diver_addr->driver))
	{
		printk(KERN_ERR "create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}


	obj_ps.self = ltr579_obj;
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

	obj_ps.sensor_operate = ltr579_ps_operate;
	if(err = hwmsen_attach(ID_PROXIMITY, &obj_ps))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	
	obj_als.self = ltr579_obj;
	obj_als.polling = 1;
	obj_als.sensor_operate = ltr579_als_operate;
	if(err = hwmsen_attach(ID_LIGHT, &obj_als))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}


#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = ltr579_early_suspend,
	obj->early_drv.resume   = ltr579_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif

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
#ifdef PS_DYNAMIC_CALIBRATE
	int i;
	int ps_result;
	ps_device= kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!ps_device) {
        	APS_DBG("huangzifan cannot allocate hall device\n");        
		goto exit_create_attr_failed;
    	}
      ps_device->init_name = PS_DEVICE_NAME;
      ps_device->class = meizu_class;
      ps_device->release = (void (*)(struct device *))kfree;
      ps_result = device_register(ps_device);
	if (ps_result) {
         	APS_DBG("HZF cannot register ps_device\n");        
  	  	goto exit_create_attr_failed;
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
	als_isl29125 = 0;
	board_version_flag = 1;//Add by huangzifan at 20160803 for st480 orientation.
	APS_DBG("board_version_flag =%d\n",board_version_flag);
	APS_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	misc_deregister(&ltr579_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(client);
	exit_kfree:
	kfree(obj);
	exit:
	ltr579_i2c_client = NULL;           
//	MT6516_EINTIRQMask(CUST_EINT_ALS_NUM);  /*mask interrupt if fail*/
	APS_ERR("%s: err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/

static int ltr579_i2c_remove(struct i2c_client *client)
{
	int err;	
	if(err = ltr579_delete_attr(&ltr579_i2c_driver.driver))
	{
		APS_ERR("ltr579_delete_attr fail: %d\n", err);
	} 

	if(err = misc_deregister(&ltr579_device))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}
	
	ltr579_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}

static int ltr579_local_init(void) 
{
	struct alsps_hw *hw = get_cust_alsps_hw();

	ltr579_power(hw, 1);
	//ltr579_force[0] = hw->i2c_num;
	//ltr579_force[1] = hw->i2c_addr[0];
	//APS_DBG("I2C = %d, addr =0x%x\n",ltr579_force[0],ltr579_force[1]);
	if(i2c_add_driver(&ltr579_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}

static int  ltr579_local_uninit(void)
{
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_FUN();    
	ltr579_power(hw, 0);    
	i2c_del_driver(&ltr579_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init ltr579_init(void)
{
       struct alsps_hw *hw = get_cust_alsps_hw();
	APS_FUN();
	i2c_register_board_info(hw->i2c_num, &i2c_ltr579, 1);	
	alsps_driver_add(&ltr579_init_info);
    
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit ltr579_exit(void)
{
	APS_FUN();
	//platform_driver_unregister(&ltr579_alsps_driver);
}
/*----------------------------------------------------------------------------*/
module_init(ltr579_init);
module_exit(ltr579_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("XX Xx");
MODULE_DESCRIPTION("LTR-579ALS Driver");
MODULE_LICENSE("GPL");
/* Driver version v1.0 */

