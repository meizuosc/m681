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

#define LCM_ID_NT35695 (0xf5)

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


/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
//#define LCM_DSI_CMD_MODE									1
#define LCM_DSI_CMD_MODE									0

#define FRAME_WIDTH										(1080)
#define FRAME_HEIGHT									(1920)

#ifndef MACH_FPGA
#define GPIO_65132_EN1 GPIO_LCD_BIAS_ENP_PIN
#define GPIO_65132_EN2 GPIO_LCD_BIAS_ENN_PIN
#endif

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//the same i2c-addr can't to probe two times.
#ifdef BUILD_LK 
extern int TPS65132_write_byte(kal_uint8 addr, kal_uint8 value);
#else
extern int tps65132_write_bytes(unsigned char addr, unsigned char value);
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table init_setting[] = {
//TXD       
{0xB9,3,{0xFF,0x83,0x99}},
//{0xBA,4,{0x73,0x03,0x68,0x73}},
{0xB1,15,{0x02,0x04,0x6D,0x8D,0x01,0x32,0x33,0x11,0x11,0x50,0x5D,0x56,0x73,0x02,0x02}},
{0xB2,11,{0x00,0x80,0x80,0xAE,0x05,0x07,0x5A,0x11,0x10,0x10,0x00}},
{0xB4,44,{0x00,0xFF,0x10,0x18,0x04,0x9A,0x00,0x00,0x06,0x00,0x02,0x04,0x00,0x24,0x02,0x04,0x0A,0x21,0x03,0x00,0x00,0x02,0x9F,0x88,0x10,0x18,0x04,0x9A,0x00,0x00,0x08,0x00,0x02,0x04,0x00,0x24,0x02,0x04,0x0A,0x00,0x00,0x02,0x9F,0x12}},
{0xD3,33,{0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x05,0x00,0x05,0x00,0x07,0x88,0x07,0x88,0x00,0x00,0x00,0x00,0x00,0x15,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x05,0x40}},
{0xD5,32,{0x20,0x20,0x19,0x19,0x18,0x18,0x01,0x01,0x00,0x00,0x25,0x25,0x18,0x18,0x18,0x18,0x24,0x24,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x2F,0x2F,0x30,0x30,0x31,0x31}},
{0xD6,32,{0x24,0x24,0x18,0x18,0x19,0x19,0x00,0x00,0x01,0x01,0x25,0x25,0x18,0x18,0x18,0x18,0x20,0x20,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x2F,0x2F,0x30,0x30,0x31,0x31}},
{0xD8,16,{0xAA,0x8A,0xAA,0xAA,0xAA,0x8A,0xAA,0xAA,0xAA,0x8A,0xAA,0xAA,0xAA,0x8A,0xAA,0xAA}},
{0xBD,1,{0x01}},
{0xD8,16,{0xFF,0xFC,0xC0,0x3F,0xFF,0xFC,0xC0,0x3F,0xFF,0xFC,0xC0,0x3F,0xFF,0xFC,0xC0,0x3F}},
{0xBD,1,{0x02}},
{0xD8,8,{0xFF,0xFC,0xC0,0x3F,0xFF,0xFC,0xC0,0x3F}},
{0xBD,1,{0x00}},
{0xE0,54,{0x1E,0x35,0x3F,0x39,0x79,0x81,0x87,0x81,0x89,0x91,0x96,0x9C,0x9F,0xA5,0xAB,0xAC,0xB0,0xB7,0xB8,0xBF,0xAF,0xBB,0xBC,0x60,0x59,0x62,0x7D,0x26,0x33,0x3F,0x39,0x6D,0x73,0x7B,0x75,0x79,0x83,0x8A,0x90,0x93,0x99,0xA0,0xA4,0xA4,0xAD,0xAC,0xB6,0xA8,0xB5,0xB8,0x5D,0x57,0x60,0x7B}},
{0xB6,2,{0x86,0x86}},
//{0xCC,1,{0x08}},
//{0xCC,1,{0x0C}},
{0xCC,1,{0x04}},
{0xD0,1,{0x39}},
//{0x36,1,{0x03}},//add by liuyang
//add by fae for esd 20151014
{REGFLAG_DELAY, 5, {}}, 
//add by fae for esd 20151014
{0x11,1,{0x00}},
{REGFLAG_DELAY, 120, {}},
//{0xB6,2,{0x8B,0x8B}},
{0x29,1,{0x00}},                                       
{REGFLAG_DELAY, 20, {}}, 
{REGFLAG_END_OF_TABLE, 0x00, {}}                
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


static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28,0,{}},
	{REGFLAG_DELAY, 20, {}},
	{0x10,0,{}},
	{REGFLAG_DELAY, 80, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
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

	params->dsi.vertical_sync_active = 4;//4;
	params->dsi.vertical_backporch = 3;//3;
	params->dsi.vertical_frontporch = 10;//10;

	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 8;//30; //10;
	params->dsi.horizontal_backporch = 60;//30; //20;
	params->dsi.horizontal_frontporch = 60;//140;//30; //40;
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
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9d;
	
	params->dsi.lcm_esd_check_table[1].cmd          = 0x09; //0x53;
	params->dsi.lcm_esd_check_table[1].count        = 4;
	params->dsi.lcm_esd_check_table[1].para_list[0] = 0x80;
	params->dsi.lcm_esd_check_table[1].para_list[1] = 0x73;
	params->dsi.lcm_esd_check_table[1].para_list[2] = 0x04;
	params->dsi.lcm_esd_check_table[1].para_list[3] = 0x00;
	
	params->dsi.lcm_esd_check_table[2].cmd          = 0xD0; //0x53;
	params->dsi.lcm_esd_check_table[2].count        = 1;
	params->dsi.lcm_esd_check_table[2].para_list[0] = 0x39;

    //end:modified by fae for esd 20151201

}

static void lcm_init_power(void)
{
    LCM_LOGI("liuyang:r63350_lcm_init_power is called\n");
}

static void lcm_suspend_power(void)
{
    LCM_LOGI("liuyang:r63350_lcm_suspend_power is called\n");
}

static void lcm_resume_power(void)
{
    LCM_LOGI("liuyang:r63350_lcm_suspend_power is called\n");
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

#endif

    //begin:modified by fae for save time 20160105
	SET_RESET_PIN(1);
	MDELAY(1); //10
	SET_RESET_PIN(0);
	MDELAY(2); //10

	SET_RESET_PIN(1);
	//MDELAY(120);
	MDELAY(6); //20
	 //end:modified by fae for save time 20160105
	push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
	//init_lcm_registers();
	LCM_LOGI("liuyang:hx8399c_fhd_dsi_vdo_txd is init\n");
}

static void lcm_suspend(void)
{
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
	/* SET_RESET_PIN(0); */
	mt_set_gpio_mode(GPIO_65132_EN2, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN2, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN2, GPIO_OUT_ZERO);

	mt_set_gpio_mode(GPIO_65132_EN1, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN1, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN1, GPIO_OUT_ZERO);
	MDELAY(10);
}

static void lcm_resume(void)
{
	lcm_init();	
}


static unsigned int lcm_compare_id(void)
{
  
    return 0;
}

LCM_DRIVER hx8399c_fhd_dsi_vdo_txd_nt50358_lcm_drv = {
	.name = "hx8399c_fhd_dsi_vdo_txd_nt50358",
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
