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

#define LCM_ID_R63350 (0x3350)

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

#define PHYSICAL_WIDTH                                                          (68)
#define PHYSICAL_HEIGHT                                                         (120)

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


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    // Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 20, {}},	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

/*
static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28,0,{}},
	{REGFLAG_DELAY, 20, {}},
	{0x10,0,{}},
	{REGFLAG_DELAY, 80, {}},
	{0xB0, 0, {0x04}},    //ADD BY FAE for 0.8ma
	{0xB1, 0, {0x01}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}

};
*/
//ADD BY FAE for lcm leakage 0.8ma,
static void lcm_sleep_registers(void) 
{
    unsigned int data_array[16];

    data_array[0] = 0x00280500;                          
    dsi_set_cmdq(data_array, 1, 1);   
    MDELAY(20);  

    data_array[0] = 0x0100500;                          
    dsi_set_cmdq(data_array, 1, 1);
    MDELAY(80); 
    SET_RESET_PIN(1);
	MDELAY(1); //10
	SET_RESET_PIN(0);
	MDELAY(2); //10

	SET_RESET_PIN(1);
	//MDELAY(120);
	MDELAY(6); //20
    data_array[0] = 0x00022902;
    data_array[1] = 0x000004B0;
    dsi_set_cmdq(&data_array, 2, 1);

    data_array[0] = 0x00022902;
    data_array[1] = 0x000001B1;
    dsi_set_cmdq(&data_array, 2, 1);

     MDELAY(20); 
};


//SHARP Glass Panel init 
static struct LCM_setting_table init_setting[] = {
{0x11,1,{0x00}},
{REGFLAG_DELAY, 120, {}}, 
{0xB0,1,{0x04}}, 
{0xB3,6,{0x14,0x00,0x00,0x00,0x00,0x00}}, 
{0xB4,2,{0x0C,0x00}}, 
{0xB6,3,{0x4B,0xDB,0x16}}, 
{0xB8,7,{0x57,0x3D,0x19,0x1E,0x0A,0x50,0x50}}, 
{0xB9,7,{0x6F,0x3D,0x28,0x3C,0x14,0xC8,0xC8}}, 
{0xBA,7,{0xB5,0x33,0x41,0x64,0x23,0xA0,0xA0}}, 
{0xBB,2,{0x14,0x14}}, 
{0xBC,2,{0x37,0x32}}, 
{0xBD,2,{0x64,0x32}}, 
{0xC0,1,{0x00}}, 
{0xC1,31,{0x04,0x60,0x00,0x40,0x10,0x00,0x58,0x03,0x00,0x00,0x00,0x64,0x84,0x30,0x4A,0x00,0x00,0x9D,0xC1,0x01,0x00,0xCA,0x00,0x00,0x00,0x00,0x00,0x00,0x42,0x00,0x02,0x20,0x03,0x98}}, 
{0xC2,8,{0x31,0xF7,0x80,0x06,0x08,0x00,0x00,0x08}}, 
{0xC3,3,{0x00,0x00,0x00}}, 
{0xC4,11,{0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x06}}, 
{0xC6,21,{0x77,0x01,0x6E,0x02,0x67,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x19,0x0A,0x77}}, 
{0xC7,30,{0x00,0x12,0x1A,0x22,0x31,0x3D,0x47,0x56,0x3A,0x41,0x4B,0x57,0x65,0x71,0x77,0x00,0x12,0x1A,0x22,0x31,0x3D,0x47,0x56,0x3A,0x41,0x4B,0x57,0x65,0x71,0x77}},
{0xC8,55,{0x01,0x00,0x04,0xFF,0xFC,0xE2,0xFF,0x00,0x03,0xFF,0xFE,0xFC,0xF0,0x00,0x00,0x04,0x07,0xF7,0xE3,0x00,0x04,0xFF,0xFA,0xFC,0x00,0x00,0x03,0xFF,0xFD,0xE6,0x00,0x00,0x00,0x03,0x03,0xBF,0x00,0x00,0x04,0xFF,0xFB,0xAD,0x00,0x00,0x03,0xFF,0xFC,0xDA,0x00,0x00,0x00,0x04,0x09,0xFC,0x00}},
{0xC9,19,{0x00,0x00,0x00,0x00,0x00,0xFC,0x00,0x00,0x00,0x00,0x00,0xFC,0x00,0x00,0x00,0x00,0x00,0xFC,0x00}}, 
{0xCA,43,{0x1D,0xFC,0xFC,0xFC,0x00,0xCA,0xCA,0x96,0x00,0x96,0xC0,0xC0,0x00,0x82,0x00,0x00,0x00,0x9E,0x00,0x00,0x00,0x82,0xFF,0x9F,0x83,0xFF,0x9F,0x83,0x7D,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0xCB,15,{0xE6,0xE0,0xC7,0x63,0x00,0x00,0x00,0x00,0x20,0xE0,0x87,0x00,0xE8,0x00,0x00}}, 
{0xCC,1,{0x06}}, 
{0xCE,25,{0x55,0x40,0x49,0x53,0x59,0x5E,0x63,0x68,0x6E,0x74,0x7E,0x8A,0x98,0xA8,0xBB,0xD0,0xFF,0x04,0x00,0x04,0x04,0x42,0x00,0x69,0x5A}}, 
{0xD0,10,{0x11,0x00,0x00,0x56,0xCB,0x40,0x19,0x19,0x09,0x00}}, 
{0xD1,4,{0x00,0x48,0x16,0x0F}}, 
{0xD3,26,{0x1B,0x33,0x99,0xBB,0xB3,0x33,0x33,0x33,0x11,0x00,0x01,0x00,0x00,0xE8,0xA0,0x02,0x2F,0x27,0x33,0x33,0x72,0x12,0x8A,0x57,0x3D,0xBC}}, 
{0xD4,3,{0x41,0x04,0x00,0xD5}}, 
{0xD5,7,{0x00,0x00,0x00,0x01,0x31,0x01,0x31}}, 
{0xD6,1,{0x01}}, 
{0xD7,24,{0xBF,0xF8,0x7F,0xA8,0xCE,0x3E,0xFC,0xC1,0xE1,0xEF,0x83,0x07,0x3F,0x10,0x7F,0xC0,0x01,0xE7,0x40,0x3C,0x00,0xC0,0x00,0x00}}, 
{0xD9,3,{0x20,0x00,0x14}}, 
{0xDD,4,{0x30,0x06,0x23,0x65}}, 
{0xDE,4,{0x00,0x3F,0xFF,0x10}}, 
{0x36,1,{0x00}}, //Modify, liuyang3.wt,20160514,for rotation 180,because of SHARP lcd not surport reverse scan. 
{0x29,1,{0x00}},
{REGFLAG_DELAY, 20, {}},
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

       params->physical_width = PHYSICAL_WIDTH;
       params->physical_height = PHYSICAL_HEIGHT;

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
	params->dsi.vertical_backporch = 5;//3;
	params->dsi.vertical_frontporch = 16;//10;//modify by MTK

	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;//30; //10;
	params->dsi.horizontal_backporch = 50;//30; //20;
	params->dsi.horizontal_frontporch = 80;//30; //40;
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
	params->dsi.clk_lp_per_line_enable = 0;
	//params->dsi.esd_check_enable = 1;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1C;

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
    LCM_LOGI("liuyang:r63350_lcm_resume_power is called\n");
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
	//lcm_compare_id();
	LCM_LOGI("liuyang:r63350_TCL_SHARP_PANEL_init\n");
}

static void lcm_suspend(void)
{
	//push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
	lcm_sleep_registers();
	/* SET_RESET_PIN(0); */
	MDELAY(10);
	mt_set_gpio_mode(GPIO_65132_EN2, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN2, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN2, GPIO_OUT_ZERO);
    MDELAY(5);
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

LCM_DRIVER r63350_fhd_dsi_vdo_tcl_sharp_nt50358_lcm_drv = {
	.name = "r63350_fhd_dsi_vdo_tcl_sharp_nt50358_drv",
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
