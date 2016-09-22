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
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
#include <linux/xlog.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include "sm5414.h"
#include "cust_charging.h"
#include <mach/charging.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <cust_eint.h>
#include "cust_gpio_usage.h"
#include <mach/eint.h>
#if defined(CONFIG_MTK_FPGA)
#else
#include <cust_i2c.h>
#endif
#if 0//defined(GPIO_SM5414_EINT_PIN)
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif
//#ifdef CONFIG_OF_TOUCH
#include <linux/of.h>
#include <linux/of_irq.h>
//#endif
/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define sm5414_SLAVE_ADDR_WRITE   0x92
#define sm5414_SLAVE_ADDR_READ    0x93

#ifdef I2C_SWITHING_CHARGER_CHANNEL
#define sm5414_BUSNUM I2C_SWITHING_CHARGER_CHANNEL
#else
#define sm5414_BUSNUM 1
#endif

static struct i2c_client *new_client = NULL;
static struct i2c_board_info __initdata i2c_sm5414 = { I2C_BOARD_INFO("sm5414", (sm5414_SLAVE_ADDR_WRITE>>1))};
static const struct i2c_device_id sm5414_i2c_id[] = {{"sm5414",0},{}};   
kal_bool chargin_hw_init_done = KAL_FALSE;
static int sm5414_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static int is_fullcharged = 0;

static unsigned int sm5414_irq = 0;

#ifdef CONFIG_OF
static const struct of_device_id sm5414_of_match[] = {
	{ .compatible = "sm5414", },
	{},
};

MODULE_DEVICE_TABLE(of, sm5414_of_match);
#endif

static struct i2c_driver sm5414_driver = {
    .driver = {
        .name    = "sm5414",
#ifdef CONFIG_OF 
        .of_match_table = sm5414_of_match,
#endif
    },
    .probe       = sm5414_driver_probe,
    .id_table    = sm5414_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/
kal_uint8 sm5414_reg[SM5414_REG_NUM] = {0};

static DEFINE_MUTEX(sm5414_i2c_access);

int g_sm5414_hw_exist=0;

/**********************************************************
  *
  *   [I2C Function For Read/Write sm5414] 
  *
  *********************************************************/
int sm5414_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
    char     cmd_buf[1]={0x00};
    char     readData = 0;
    int      ret=0;

    mutex_lock(&sm5414_i2c_access);
    //new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

    cmd_buf[0] = cmd;
    ret = i2c_master_send(new_client, &cmd_buf[0], (1<<8 | 1));
    if (ret < 0) 
    {    
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;
		
        mutex_unlock(&sm5414_i2c_access);
        return 0;
    }
    
    readData = cmd_buf[0];
    *returnData = readData;

    //new_client->addr = new_client->addr & I2C_MASK_FLAG;
    new_client->ext_flag=0;
	
    mutex_unlock(&sm5414_i2c_access);    
    return 1;
}

int sm5414_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
    char    write_data[2] = {0};
    int     ret=0;
    
    mutex_lock(&sm5414_i2c_access);
    
    write_data[0] = cmd;
    write_data[1] = writeData;

    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
	
    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0) 
    {
        new_client->ext_flag=0;    
        mutex_unlock(&sm5414_i2c_access);
        return 0;
    }

    new_client->ext_flag=0;    
    mutex_unlock(&sm5414_i2c_access);
    return 1;
}

/**********************************************************
  *
  *   [Read / Write Function] 
  *
  *********************************************************/
kal_uint32 sm5414_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 sm5414_reg = 0;
    int ret = 0;

    pr_info("--------------------------------------------------\n");

    ret = sm5414_read_byte(RegNum, &sm5414_reg);

    pr_info("[sm5414_read_interface] Reg[%x]=0x%x\n", RegNum, sm5414_reg);
    
    sm5414_reg &= (MASK << SHIFT);
    *val = (sm5414_reg >> SHIFT);    
	
    pr_info("[sm5414_read_interface] val=0x%x\n", *val);
	
    return ret;
}

kal_uint32 sm5414_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 sm5414_reg = 0;
    int ret = 0;

    pr_info("--------------------------------------------------\n");

    ret = sm5414_read_byte(RegNum, &sm5414_reg);
    pr_info("[sm5414_config_interface] Reg[%x]=0x%x\n", RegNum, sm5414_reg);
    
    sm5414_reg &= ~(MASK << SHIFT);
    sm5414_reg |= (val << SHIFT);

    ret = sm5414_write_byte(RegNum, sm5414_reg);
    pr_info("[sm5414_config_interface] write Reg[%x]=0x%x\n", RegNum, sm5414_reg);

    // Check
    //sm5414_read_byte(RegNum, &sm5414_reg);
    //pr_info("[sm5414_config_interface] Check Reg[%x]=0x%x\n", RegNum, sm5414_reg);

    return ret;
}

//write one register directly
kal_uint32 sm5414_reg_config_interface (kal_uint8 RegNum, kal_uint8 val)
{   
    kal_uint32 ret = 0;
    
    ret = sm5414_write_byte(RegNum, val);
    pr_info("[sm5414_reg_config_interface] write Reg[%x]=0x%x\n", RegNum, val);//SM : log
    return ret;
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
//CTRL----------------------------------------------------
void sm5414_set_enboost(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CTRL), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CTRL_ENBOOST_MASK),
                                    (kal_uint8)(SM5414_CTRL_ENBOOST_SHIFT)
                                    );
}

void sm5414_set_chgen(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CTRL), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CTRL_CHGEN_MASK),
                                    (kal_uint8)(SM5414_CTRL_CHGEN_SHIFT)
                                    );
}

void sm5414_set_suspen(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CTRL), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CTRL_SUSPEN_MASK),
                                    (kal_uint8)(SM5414_CTRL_SUSPEN_SHIFT)
                                    );
}

void sm5414_set_reset(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CTRL), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CTRL_RESET_MASK),
                                    (kal_uint8)(SM5414_CTRL_RESET_SHIFT)
                                    );
}

void sm5414_set_encomparator(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CTRL), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CTRL_ENCOMPARATOR_MASK),
                                    (kal_uint8)(SM5414_CTRL_ENCOMPARATOR_SHIFT)
                                    );
}

//vbusctrl----------------------------------------------------
void sm5414_set_vbuslimit(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_VBUSCTRL), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_VBUSCTRL_VBUSLIMIT_MASK),
                                    (kal_uint8)(SM5414_VBUSCTRL_VBUSLIMIT_SHIFT)
                                    );
}

//chgctrl1----------------------------------------------------
void sm5414_set_prechg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL1_PRECHG_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL1_PRECHG_SHIFT)
                                    );
}
void sm5414_set_aiclen(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL1_AICLEN_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL1_AICLEN_SHIFT)
                                    );
}
void sm5414_set_autostop(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL1_AUTOSTOP_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL1_AUTOSTOP_SHIFT)
                                    );
}
void sm5414_set_aiclth(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL1_AICLTH_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL1_AICLTH_SHIFT)
                                    );
}

//chgctrl2----------------------------------------------------
void sm5414_set_fastchg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL2_FASTCHG_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL2_FASTCHG_SHIFT)
                                    );
}

//chgctrl3----------------------------------------------------
void sm5414_set_weakbat(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL3), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL3_WEAKBAT_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL3_WEAKBAT_SHIFT)
                                    );
}
void sm5414_set_batreg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL3), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL3_BATREG_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL3_BATREG_SHIFT)
                                    );
}

//chgctrl4----------------------------------------------------
void sm5414_set_dislimit(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL4_DISLIMIT_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL4_DISLIMIT_SHIFT)
                                    );
}
void sm5414_set_topoff(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL4_TOPOFF_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL4_TOPOFF_SHIFT)
                                    );
}

//chgctrl5----------------------------------------------------
void sm5414_set_topofftimer(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL5_TOPOFFTIMER_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL5_TOPOFFTIMER_SHIFT)
                                    );
}
void sm5414_set_fasttimer(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL5_FASTTIMER_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL5_FASTTIMER_SHIFT)
                                    );
}
void sm5414_set_votg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=sm5414_config_interface(   (kal_uint8)(SM5414_CHGCTRL5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(SM5414_CHGCTRL5_VOTG_MASK),
                                    (kal_uint8)(SM5414_CHGCTRL5_VOTG_SHIFT)
                                    );
}  

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
void sm5414_cnt_nshdn_pin(kal_uint32 enable)
{
    if(KAL_TRUE == enable)
    {
#if defined(GPIO_SM5414_SHDN_PIN)
        mt_set_gpio_mode(GPIO_SM5414_SHDN_PIN,GPIO_MODE_GPIO);  
        mt_set_gpio_dir(GPIO_SM5414_SHDN_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_SM5414_SHDN_PIN,GPIO_OUT_ONE);
#else
        printk("[sm5414_cnt_nshdn_pin] Not Support nSHDN PIN\n");
#endif        
    }
    else
    {   
#if defined(GPIO_SM5414_SHDN_PIN)
        mt_set_gpio_mode(GPIO_SM5414_SHDN_PIN,GPIO_MODE_GPIO);  
        mt_set_gpio_dir(GPIO_SM5414_SHDN_PIN,GPIO_DIR_OUT);        
        mt_set_gpio_out(GPIO_SM5414_SHDN_PIN,GPIO_OUT_ZERO);
#else
        printk("[sm5414_cnt_nshdn_pin] Not Support nSHDN PIN\n");
#endif 
    }
}
void sm5414_otg_enable(kal_uint32 enable)
{
   printk("zmlin otg_enable:%d\n",enable);
    if(KAL_TRUE == enable)
    {
        //Before turning on OTG, system must turn off charing function.
        mt_set_gpio_mode(GPIO_SM5414_CHGEN_PIN,GPIO_MODE_GPIO);  
        mt_set_gpio_dir(GPIO_SM5414_CHGEN_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_SM5414_CHGEN_PIN,GPIO_OUT_ONE);
        sm5414_set_enboost(ENBOOST_EN);
    }
    else
    {   
        sm5414_set_enboost(ENBOOST_DIS);
    }
}
void sm5414_hw_component_detect(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=sm5414_read_interface(0x0E, &val, 0xFF, 0x0);
    
    if(val == 0)
        g_sm5414_hw_exist=0;
    else
        g_sm5414_hw_exist=1;

    printk("[sm5414_hw_component_detect] exist=%d, Reg[0x0E]=0x%x\n", 
        g_sm5414_hw_exist, val);
}

int is_sm5414_exist(void)
{
    printk("[is_sm5414_exist] g_sm5414_hw_exist=%d\n", g_sm5414_hw_exist);
    
    return g_sm5414_hw_exist;
}

void sm5414_dump_register(void)
{
    int i=0;
    printk("[sm5414]\n ");
    for (i=0;i<SM5414_REG_NUM;i++)
    {
        sm5414_read_byte(i, &sm5414_reg[i]);
        printk("[0x%x]=0x%x ", i, sm5414_reg[i]);        
    }
    printk("\n");
}

int sm5414_get_chrg_stat(void)
{
    printk("[sm5414_get_chrg_stat] is_fullcharged = %d\n",is_fullcharged);

    return is_fullcharged;
}


//static irqreturn_t sm5414_irq_handler(int irq, void *data)
static irqreturn_t sm5414_irq_handler(unsigned irq, struct irq_desc *desc)
{
	//struct i2c_client *i2c = data;
    kal_uint8 sm5414_int1 = 0, sm5414_int2 = 0, sm5414_int3 = 0;
    kal_uint8 val = 0;
	
    printk("SM5414 enter IRQ \n");
    sm5414_read_byte(SM5414_INT1, &sm5414_int1);
    sm5414_read_byte(SM5414_INT2, &sm5414_int2);
    sm5414_read_byte(SM5414_INT3, &sm5414_int3);

    printk("SM5414 IRQ : INT1(0x%x), INT2(0x%x), INT3(0x%x)\n", sm5414_int1, sm5414_int2, sm5414_int3);
    
    if ((sm5414_int2 & SM5414_INT2_DONE) || (sm5414_int2 & SM5414_INT2_TOPOFF))
        is_fullcharged = 1; 
    else if ((sm5414_int1 & SM5414_INT1_VBUSUVLO) || (sm5414_int1 & SM5414_INT1_VBUSOVP) || (sm5414_int2 & SM5414_INT2_CHGRSTF))
        is_fullcharged = 0;

    	sm5414_read_interface(0x0E, &val, 0xFF, 0x0);

	printk("sm5414 IRQ triggered....[0x0E](0x%x)\n",val);

	return IRQ_HANDLED;
}

static int sm5414_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	u32 ints[2] = {0,0};
	printk("Device Tree sm5414_irq_registration!\n");
	
	node = of_find_compatible_node(NULL, NULL, "mediatek, SM5414_CHARGER-eint");
	if(node){
		of_property_read_u32_array(node , "debounce", ints, ARRAY_SIZE(ints));
		mt_gpio_set_debounce(ints[0], ints[1]);

		sm5414_irq = irq_of_parse_and_map(node, 0);
		//ret = request_irq(sm5414_irq, (irq_handler_t)sm5414_irq_handler, EINTF_TRIGGER_FALLING | IRQF_NO_SUSPEND|IRQF_ONESHOT/*CUST_EINTF_TRIGGER_FALLING*/, "SM5414_CHARGER-eint", NULL);
		ret = request_threaded_irq(sm5414_irq, NULL, sm5414_irq_handler, IRQF_TRIGGER_FALLING|IRQF_NO_SUSPEND|IRQF_ONESHOT/*CUST_EINTF_TRIGGER_FALLING*/, "SM5414_CHARGER-eint", NULL);
            //gtp_eint_trigger_type = EINTF_TRIGGER_RISING;
		if(ret < 0){
			    ret = -1;
			    printk("sm5414 request_irq IRQ LINE NOT AVAILABLE!.");
		}
	}else{
		printk("sm5414 request_irq can not find touch eint device node!.");
		ret = -1;
	}
	printk("[%s]irq:%d, debounce:%d-%d:", __FUNCTION__, sm5414_irq, ints[0], ints[1]);
	return ret;
}

void toggle_chgen(void)
{
	 sm5414_set_chgen(CHARGE_DIS);
        sm5414_set_chgen(CHARGE_EN);
}

void toggle_suspend(void)
{
	 sm5414_set_suspen(SUSPEND_EN);
        sm5414_set_suspen(SUSPEND_DIS);
}

void sm5414_reg_init(void)
{
   //INT MASK 1/2/3
    sm5414_write_byte(SM5414_INTMASK1, 0x3F);//VBUSOVP | VBUSUVLO
    sm5414_write_byte(SM5414_INTMASK2, 0xF8);//DONE | TOPOFF|CHGRSTF
    sm5414_write_byte(SM5414_INTMASK3, 0xFF);

	sm5414_set_encomparator(ENCOMPARATOR_EN);
    sm5414_set_topoff(TOPOFF_150mA);
#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	sm5414_set_batreg(BATREG_4_3_5_0_V); //VREG 4.352V
#else
	sm5414_set_batreg(BATREG_4_2_0_0_V); //VREG 4.208V
#endif 
    //sm5414_set_autostop(AUTOSTOP_DIS);
     sm5414_set_aiclth(AICL_THRESHOLD_4_4_V);
#if defined(SM5414_TOPOFF_TIMER_SUPPORT)
    sm5414_set_autostop(AUTOSTOP_EN);
    sm5414_set_topofftimer(TOPOFFTIMER_10MIN);
#else
    sm5414_set_autostop(AUTOSTOP_DIS);
#endif



}

static int sm5414_driver_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{             
    int err=0, irq = 0,ret = 0; 

    pr_notice("[sm5414_driver_probe] \n");

	 #if defined(GPIO_SM5414_SHDN_PIN)
        mt_set_gpio_mode(GPIO_SM5414_SHDN_PIN,GPIO_MODE_GPIO);  
        mt_set_gpio_dir(GPIO_SM5414_SHDN_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_SM5414_SHDN_PIN,GPIO_OUT_ONE);
	#endif
	#if defined(GPIO_SM5414_EINT_PIN)
	mt_set_gpio_mode(GPIO_SM5414_EINT_PIN, GPIO_SM5414_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_SM5414_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_SM5414_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_SM5414_EINT_PIN, GPIO_PULL_UP);
	#endif
	//chargin_hw_init_done = KAL_TRUE;  //debug
	
    if (!(new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
        err = -ENOMEM;
        goto exit;
    }    
    memset(new_client, 0, sizeof(struct i2c_client));

    new_client = client;    
    //---------------------
    sm5414_hw_component_detect();

   /* ret = sm5414_irq_registration();
    if (ret < 0) {
		printk("Error : can't initialize SM5414 irq\n");
		goto err_init_irq;
	}*/
    #if 0//defined(GPIO_SM5414_EINT_PIN)
    mt_set_gpio_mode(GPIO_SM5414_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_SM5414_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_SM5414_EINT_PIN, GPIO_PULL_DISABLE);
    //mt65xx_eint_set_sens(CUST_EINT_SM5414_CHARGER_NUM, CUST_EINT_SM5414_CHARGER_SENSITIVE);
    mt65xx_eint_set_hw_debounce(CUST_EINT_SM5414_CHARGER_NUM, CUST_EINT_SM5414_CHARGER_DEBOUNCE_CN);
    mt65xx_eint_registration(CUST_EINT_SM5414_CHARGER_NUM, CUST_EINT_SM5414_CHARGER_DEBOUNCE_EN, CUST_EINTF_TRIGGER_LOW, sm5414_irq_handler, 1);	
    #endif
    //new_client->irq= gpio_to_irq(GPIO_SM5414_EINT_PIN);
    //mt_eint_registration(CUST_EINT_SM5414_CHARGER_NUM, IRQF_TRIGGER_FALLING, sm5414_irq_handler, 1);
	//ret = request_threaded_irq(new_client->irq, NULL, sm5414_irq_handler,IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND, "sm5414", new_client);
   // if (ret < 0) {
//		printk("Error : can't initialize SM5414 irq\n");
//		goto err_init_irq;
//	}
    //mt_eint_unmask(CUST_EINT_SM5414_CHARGER_NUM);
	sm5414_reg_init();
	chargin_hw_init_done = KAL_TRUE;
    ret = sm5414_irq_registration();
    if (ret < 0) {
		printk("Error : can't initialize SM5414 irq\n");
		goto err_init_irq;
	}
#if 0 //SM
	toggle_chgen();
	toggle_suspend();
#endif
	//chargin_hw_init_done = KAL_TRUE;

	sm5414_dump_register();

	pr_notice("[sm5414_driver_probe] DONE\n");

    return 0;
    
err_init_irq:
exit:
    return err;

}

/**********************************************************
  *
  *   [platform_driver API] 
  *
  *********************************************************/
kal_uint8 g_reg_value_sm5414=0;
static ssize_t show_sm5414_access(struct device *dev,struct device_attribute *attr, char *buf)
{
    pr_notice("[show_sm5414_access] 0x%x\n", g_reg_value_sm5414);
    return sprintf(buf, "%u\n", g_reg_value_sm5414);
}
static ssize_t store_sm5414_access(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    int ret=0;
    char *pvalue = NULL;
    unsigned int reg_value = 0;
    unsigned int reg_address = 0;
    
    pr_notice("[store_sm5414_access] \n");
    
    if(buf != NULL && size != 0)
    {
        pr_notice("[store_sm5414_access] buf is %s and size is %zu \n",buf,size);
        reg_address = simple_strtoul(buf,&pvalue,16);
        
        if(size > 3)
        {        
            reg_value = simple_strtoul((pvalue+1),NULL,16);        
            pr_notice("[store_sm5414_access] write sm5414 reg 0x%x with value 0x%x !\n",reg_address,reg_value);
            ret=sm5414_config_interface(reg_address, reg_value, 0xFF, 0x0);
        }
        else
        {    
            ret=sm5414_read_interface(reg_address, &g_reg_value_sm5414, 0xFF, 0x0);
            pr_notice("[store_sm5414_access] read sm5414 reg 0x%x with value 0x%x !\n",reg_address,g_reg_value_sm5414);
            pr_notice("[store_sm5414_access] Please use \"cat sm5414_access\" to get value\r\n");
        }        
    }    
    return size;
}
static DEVICE_ATTR(sm5414_access, 0664, show_sm5414_access, store_sm5414_access); //664

static int sm5414_user_space_probe(struct platform_device *dev)    
{    
    int ret_device_file = 0;

    pr_notice("******** sm5414_user_space_probe!! ********\n" );
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_sm5414_access);
    
    return 0;
}

struct platform_device sm5414_user_space_device = {
    .name   = "sm5414-user",
    .id     = -1,
};

static struct platform_driver sm5414_user_space_driver = {
    .probe      = sm5414_user_space_probe,
    .driver     = {
        .name = "sm5414-user",
    },
};

static int __init sm5414_subsys_init(void)
{    
    int ret=0;
    
    pr_notice("[sm5414_init] init start. ch=%d\n", sm5414_BUSNUM);
    i2c_register_board_info(sm5414_BUSNUM, &i2c_sm5414, 1);
    if(i2c_add_driver(&sm5414_driver)!=0)
    {
        pr_notice("[sm5414_init] failed to register sm5414 i2c driver.\n");
    }
    else
    {
        pr_notice("[sm5414_init] Success to register sm5414 i2c driver.\n");
    }

    // sm5414 user space access interface
    ret = platform_device_register(&sm5414_user_space_device);
    if (ret) {
        pr_notice("****[sm5414_init] Unable to device register(%d)\n", ret);
        return ret;
    }
    ret = platform_driver_register(&sm5414_user_space_driver);
    if (ret) {
        pr_notice("****[sm5414_init] Unable to register driver (%d)\n", ret);
        return ret;
    }
    
    return 0;        
}

static void __exit sm5414_exit(void)
{
    i2c_del_driver(&sm5414_driver);
}

//module_init(sm5414_init);
//module_exit(sm5414_exit);
subsys_initcall(sm5414_subsys_init);
   
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C sm5414 Driver");
