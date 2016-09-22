#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>
#endif

#include <cust_gpio_usage.h>

#ifndef MACH_FPGA
#include <cust_i2c.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif


static const unsigned int BL_MIN_LEVEL = 20;
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))


/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>

/*****************************************************************************
 * Define
 *****************************************************************************/
#ifndef MACH_FPGA

#define TPS_I2C_BUSNUM  I2C_I2C_LCD_BIAS_CHANNEL	/* for I2C channel 0 */
#define I2C_ID_NAME "tps65132"
#define TPS_ADDR 0x3E
/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static struct i2c_board_info tps65132_board_info __initdata = { I2C_BOARD_INFO(I2C_ID_NAME, TPS_ADDR) };

static struct i2c_client *tps65132_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tps65132_remove(struct i2c_client *client);
/*****************************************************************************
 * Data Structure
 *****************************************************************************/

struct tps65132_dev {
	struct i2c_client *client;

};

static const struct i2c_device_id tps65132_id[] = {
	{I2C_ID_NAME, 0},
	{}
};

static struct i2c_driver tps65132_iic_driver = {
	.id_table = tps65132_id,
	.probe = tps65132_probe,
	.remove = tps65132_remove,
	/* .detect               = mt6605_detect, */
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "tps65132",
		   },
};

/*****************************************************************************
 * Function
 *****************************************************************************/
static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	LCM_LOGI("tps65132_iic_probe\n");
	LCM_LOGI("TPS: info==>name=%s addr=0x%x\n", client->name, client->addr);
	tps65132_i2c_client = client;
	return 0;
}

static int tps65132_remove(struct i2c_client *client)
{
	LCM_LOGI("tps65132_remove\n");
	tps65132_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

//static int tps65132_write_bytes(unsigned char addr, unsigned char value)
int tps65132_write_bytes(unsigned char addr, unsigned char value) //chenhaibing 20150921 modified

{
	int ret = 0;
	struct i2c_client *client = tps65132_i2c_client;
	char write_data[2] = { 0 };
	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		LCM_LOGI("tps65132 write data fail !!\n");
	return ret;
}

static int __init tps65132_iic_init(void)
{
	LCM_LOGI("tps65132_iic_init\n");
	i2c_register_board_info(TPS_I2C_BUSNUM, &tps65132_board_info, 1);
	LCM_LOGI("tps65132_iic_init2\n");
	i2c_add_driver(&tps65132_iic_driver);
	LCM_LOGI("tps65132_iic_init success\n");
	return 0;
}

static void __exit tps65132_iic_exit(void)
{
	LCM_LOGI("tps65132_iic_exit\n");
	i2c_del_driver(&tps65132_iic_driver);
}


module_init(tps65132_iic_init);
module_exit(tps65132_iic_exit);

MODULE_AUTHOR("Xiaokuan Shi");
MODULE_DESCRIPTION("MTK TPS65132 I2C Driver");
MODULE_LICENSE("GPL");
#endif
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
//#define LCM_DSI_CMD_MODE									1
#define LCM_DSI_CMD_MODE									0

#define FRAME_WIDTH										(1080)
#define FRAME_HEIGHT									(1920)

#ifndef MACH_FPGA
//#define GPIO_65132_EN GPIO_LCD_BIAS_ENP_PIN
#define GPIO_65132_EN1 GPIO_LCD_BIAS_ENP_PIN
#define GPIO_65132_EN2 GPIO_LCD_BIAS_ENN_PIN
#endif

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF


#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28,1,{0x00}},
	{REGFLAG_DELAY, 20, {}},
	{0x10,1,{0x00}},
	{REGFLAG_DELAY, 120, {}},
};
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    // Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 20, {}},	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting[] = {
{0xB9,3,{0xFF,0x83,0x98}},
// Set Power 
{0xB1,11,{0x28,0x10,0x90,0x01,0x02,0x33,0x33,0x33,0x24,0xC5,0x0D}},
// Set Display 
{0xB2,9, {0x40,0x80,0xAE,0x0C,0x12,0x45,0x01,0x11,0x00}},
// Set CYC 
{0xB4,25,{0x00,0xFF,0x63,0x7E,0x63,0x7E,0x00,0x00,0x09,0x07,0x93,0x00,0x07,0x3A,0x03,0x95,0x03,0x9C,0x00,0x00,0x05,0x03,0x93,0x3A,0x00}},
{0xBD,1,{0x02}},
{0xD3,3,{0x00,0x04,0x10}},
{0xBD,1,{0x00}},
{0xD3,47,{0x20,0x00,0x08,0x04,0x08,0x04,0x00,0x00,0x08,0x00,0x32,0x10,0x09,0x00,0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0x00,0x0B,0x0B,0x37,0x00,0x00,0x00,0x10,0x40,0x00,0x07,0x13,0x00,0x00,0x00,0x62,0x86,0x07,0x13,0x62,0x86}},
{0xD5,40,{0x39,0x39,0x22,0x23,0x20,0x21,0x1E,0x1E,0x38,0x38,0x02,0x03,0x02,0x03,0x00,0x01,0x00,0x01,0x06,0x07,0x06,0x07,0x04,0x05,0x04,0x05,0x1F,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
{0xD6,40,{0x39,0x39,0x21,0x20,0x23,0x22,0x1E,0x1E,0x38,0x38,0x05,0x04,0x05,0x04,0x07,0x06,0x07,0x06,0x01,0x00,0x01,0x00,0x03,0x02,0x03,0x02,0x1F,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
// Set Panel Scan 
{0xCC,1,{0x02}},
// Set Gamma (Reserved) 
{0xE0,59,{0x3F,0x41,0x44,0x46,0x46,0x47,0x48,0x45,0x88,0x94,0xA0,0x9A,0x9E,0xAD,0xAF,0xB1,0xBB,0xBC,0xB5,0xBF,0xCF,0x62,0x61,0x60,0x61,0x60,0x6F,0x6F,0x60,0x3D,0x41,0x44,0x46,0x46,0x47,0x48,0x45,0x88,0x94,0xA0,0x9A,0x9E,0xAD,0xAF,0xB1,0xBB,0xBC,0xB5,0xBF,0xCF,0x62,0x61,0x60,0x61,0x60,0x6F,0x6F,0x60}},
//{0x35,1,0x00}},
{0x51,1,{0x80}},
{0x53,1,{0x24}},
{0x55,1,{0x02}},
{0x11,0,{0x00}},
{REGFLAG_DELAY,120,{}},
{0x29,0,{0x00}},
{REGFLAG_DELAY,10,{}},
{REGFLAG_END_OF_TABLE, 0x00,{}}  
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
#endif
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 3; //2;
	params->dsi.vertical_backporch = 13; //8;
	params->dsi.vertical_frontporch = 19; //10;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 15; //10;
	params->dsi.horizontal_backporch = 47; //20;
	params->dsi.horizontal_frontporch = 15; //40;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable                                                   = 1; */
#ifndef MACH_FPGA
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 500;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 450;	/* this value must be in MTK suggested table */
#endif
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
    //start:modified by fae for esd 20151201
	params->dsi.clk_lp_per_line_enable = 1;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9d;

	params->dsi.lcm_esd_check_table[1].cmd          = 0x09; //0x53;
	params->dsi.lcm_esd_check_table[1].count        = 4;
	params->dsi.lcm_esd_check_table[1].para_list[0] = 0x80;
	params->dsi.lcm_esd_check_table[1].para_list[1] = 0xf3;
	params->dsi.lcm_esd_check_table[1].para_list[2] = 0x04;
	params->dsi.lcm_esd_check_table[1].para_list[3] = 0x00;
	
//	params->dsi.lcm_esd_check_table[2].cmd          = 0xD0; //0x53;
//	params->dsi.lcm_esd_check_table[2].count        = 1;
//	params->dsi.lcm_esd_check_table[2].para_list[0] = 0x39;

    //end:modified by fae for esd 20151201
}

#ifdef BUILD_LK

#define TPS65132_SLAVE_ADDR_WRITE  0x7C
static struct mt_i2c_t TPS65132_i2c;

int TPS65132_write_byte(kal_uint8 addr, kal_uint8 value)//chenhaibing 20150921
{
	kal_uint32 ret_code = I2C_OK;
	kal_uint8 write_data[2];
	kal_uint16 len;

	write_data[0] = addr;
	write_data[1] = value;

	TPS65132_i2c.id = I2C_I2C_LCD_BIAS_CHANNEL;	/* I2C2; */
	/* Since i2c will left shift 1 bit, we need to set FAN5405 I2C address to >>1 */
	TPS65132_i2c.addr = (TPS65132_SLAVE_ADDR_WRITE >> 1);
	TPS65132_i2c.mode = ST_MODE;
	TPS65132_i2c.speed = 100;
	len = 2;

	ret_code = i2c_write(&TPS65132_i2c, write_data, len);
	/* printf("%s: i2c_write: ret_code: %d\n", __func__, ret_code); */

	return ret_code;
}
#endif


static void lcm_init_power(void)
{
    unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret = 0;
	cmd = 0x00;
	data = 0x0F;
	
    mt_set_gpio_mode(GPIO_65132_EN1, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN1, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN1, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_65132_EN2, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN2, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN2, GPIO_OUT_ONE);
	
	MDELAY(10);

#ifdef BUILD_LK
	ret = TPS65132_write_byte(cmd, data);
#else
	ret = tps65132_write_bytes(cmd, data);
#endif

	if (ret < 0)
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write error----\n", cmd);
	else
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write success----\n", cmd);

	cmd = 0x01;
	data = 0x0F;

#ifdef BUILD_LK
	ret = TPS65132_write_byte(cmd, data);
#else
	ret = tps65132_write_bytes(cmd, data);
#endif

	if (ret < 0)
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write error----\n", cmd);
	else
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write success----\n", cmd);

    LCM_LOGI("liuyang:hx8399 lcm_init_power is called\n");
}

static void lcm_suspend_power(void)
{
    LCM_LOGI("liuyang:hx8399 lcm_suspend_power is called\n");
}

static void lcm_resume_power(void)
{
    LCM_LOGI("liuyang:hx8399 lcm_resume_power is called\n");
}


static void lcm_init(void)
{
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret = 0;
	cmd = 0x00;
	data = 0x0F;
	
#ifndef MACH_FPGA
    mt_set_gpio_mode(GPIO_65132_EN1, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN1, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN1, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_65132_EN2, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN2, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN2, GPIO_OUT_ONE);
    MDELAY(2);
	
#ifdef BUILD_LK
	ret = TPS65132_write_byte(cmd, data);
#else
	ret = tps65132_write_bytes(cmd, data);
#endif
    MDELAY(2);
	if (ret < 0)
		LCM_LOGI("hx8399c----tps6132----cmd=%0x--i2c write error----\n", cmd);
	else
		LCM_LOGI("hx8399c----tps6132----cmd=%0x--i2c write success----\n", cmd);

	cmd = 0x01;
	data = 0x0F;

#ifdef BUILD_LK
	ret = TPS65132_write_byte(cmd, data);
#else
	ret = tps65132_write_bytes(cmd, data);
#endif
    MDELAY(2);
	if (ret < 0)
		LCM_LOGI("hx8399c----tps6132----cmd=%0x--i2c write error----\n", cmd);
	else
		LCM_LOGI("hx8399c----tps6132----cmd=%0x--i2c write success----\n", cmd);

#endif

	SET_RESET_PIN(1);
	MDELAY(2);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	//MDELAY(120);
	MDELAY(50);// modified by fae for save resume time 70ms 20151015

	push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
	//init_lcm_registers();
	LCM_LOGI("liuyang:hx8399c_lcm_init!\n");
	
}

static void lcm_suspend(void)
{
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
    SET_RESET_PIN(0);
	mt_set_gpio_mode(GPIO_65132_EN2, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN2, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN2, GPIO_OUT_ZERO);
	MDELAY(1);
	mt_set_gpio_mode(GPIO_65132_EN1, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN1, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN1, GPIO_OUT_ZERO);

}

static void lcm_resume(void)
{
	lcm_init();
}

static unsigned int lcm_compare_id(void)
{

    return 0;
}

LCM_DRIVER hx8398a_fhd_dsi_vdo_yassy_nt50358_lcm_drv = {
	.name = "hx8398a_fhd_dsi_vdo_yassy_nt50358_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
};
