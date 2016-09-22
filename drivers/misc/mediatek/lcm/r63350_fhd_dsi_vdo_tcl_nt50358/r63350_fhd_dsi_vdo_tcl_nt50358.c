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

/*
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    // Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 20, {}},	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
*/
//ADD BY FAE for lcm leakage 0.8ma,
static void lcm_sleep_registers(void) 
{
    unsigned int data_array[16];

/**
    data_array[0] = 0x00280500;                          
    dsi_set_cmdq(data_array, 1, 1);   
    MDELAY(20);  

    data_array[0] = 0x0100500;                          
    dsi_set_cmdq(data_array, 1, 1);
    MDELAY(80); 
    SET_RESET_PIN(1);
	MDELAY(1); //10
*/

	SET_RESET_PIN(0);
	MDELAY(5); //10

	SET_RESET_PIN(120);
	//MDELAY(120);
	MDELAY(6); //20
    data_array[0] = 0x00022902;
    data_array[1] = 0x000004B0;
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00022902;
    data_array[1] = 0x000001B1;
    dsi_set_cmdq(data_array, 2, 1);

     MDELAY(20); 
};

static void init_lcm_registers(void)
{
    //begin:modified by fae for gamma 2.2 param update 20150105
    unsigned int data_array[16];
    
    data_array[0] = 0x00110500;                          
    dsi_set_cmdq(data_array, 1, 1);
    
    MDELAY(120);  

    data_array[0] = 0x00022902;
    data_array[1] = 0x000004B0;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00072902;
    data_array[1] = 0x000014B3;
    data_array[2] = 0x00000000;
    dsi_set_cmdq(data_array, 3, 1);
    
    data_array[0] = 0x00032902;
    data_array[1] = 0x00000CB4;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00042902;
	  data_array[1] = 0x06db4bb6;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00032902;
    data_array[1] = 0x001414BB;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00032902;
    data_array[1] = 0x003237BC;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00032902;
    data_array[1] = 0x003264BD;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00032902;
    data_array[1] = 0x000400BE;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00022902;
    data_array[1] = 0x000000C0;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00232902;
    data_array[1] = 0x1060C4C1;
    data_array[2] = 0xDACFFFF1;
    data_array[3] = 0x129FFFFF;
    data_array[4] = 0x4A74A4E4;
    data_array[5] = 0xFFFFCC40;
    data_array[6] = 0x8FFFF637;
    data_array[7] = 0x10101010;
    data_array[8] = 0x03016200;
    data_array[9] = 0x00010022;
    dsi_set_cmdq(data_array, 10, 1);
    
    data_array[0] = 0x00092902;
    data_array[1] = 0x80F731C2;
    data_array[2] = 0x00000808;   
    data_array[3] = 0x00000008;
    dsi_set_cmdq(data_array, 4, 1);
    
    data_array[0] = 0x00042902;
    data_array[1] = 0x000000C3;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x000C2902;
    data_array[1] = 0x000070C4;
    data_array[2] = 0x22222222;
    data_array[3] = 0x05062222;
    dsi_set_cmdq(data_array, 4, 1);
    
    data_array[0] = 0x00162902;
    data_array[1] = 0x6901C8C6;
    data_array[2] = 0x00006901;
    data_array[3] = 0x00000000;
    data_array[4] = 0x00000000;
    data_array[5] = 0x170B0000;
    data_array[6] = 0x0000C809;
    dsi_set_cmdq(data_array, 7, 1);
    

data_array[0] = 0x001f2902;//color adjust gamma by G Q 20160329
data_array[1] = 0x201606c7;
data_array[2] = 0x4e433728;
data_array[3] = 0x4f453e5b;
data_array[4] = 0x7466645c;
data_array[5] = 0x26191406;
data_array[6] = 0x5b4c4133;
data_array[7] = 0x5c4f453e;
data_array[8] = 0x00746a64;
dsi_set_cmdq(data_array, 9, 1);    
    
    
data_array[0] = 0x00382902;    //color adjust by Q
data_array[1] = 0xfe0001c8;
data_array[2] = 0xf092fdfd;
data_array[3] = 0x00fdfe00;
data_array[4] = 0xfd0000fc;
data_array[5] = 0xeee80d05;
data_array[6] = 0xfbfefe00;
data_array[7] = 0xff000043;
data_array[8] = 0x00c800fd;
data_array[9] = 0x0f06fe00;
data_array[10] = 0xff0000fc;
data_array[11] = 0x00bf00fd;
data_array[12] = 0x00fdfe00;
data_array[13] = 0xfd0000fc;
data_array[14] = 0x007b0101;
dsi_set_cmdq(data_array, 15, 1);
    
    data_array[0] = 0x00102902;
    data_array[1] = 0x3ffc31cb;
    data_array[2] = 0x0000008c;
    data_array[3] = 0x3ffc2100;
    data_array[4] = 0x0000e884;
    dsi_set_cmdq(data_array, 5, 1);
    
    data_array[0] = 0x00022902;
    data_array[1] = 0x00000BCC;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x000B2902;
    data_array[1] = 0x000011D0;
    data_array[2] = 0x1940D959;
    data_array[3] = 0x00000919;
    dsi_set_cmdq(data_array, 4, 1);
    
    data_array[0] = 0x00052902;
    data_array[1] = 0x164800D1;
    data_array[2] = 0x0000000F;
    dsi_set_cmdq(data_array, 3, 1);
    
    #if 0
    data_array[0] = 0x001B2902;
    data_array[1] = 0xBB331BD3;
    data_array[2] = 0x3333B3BB;
    data_array[3] = 0x01013333;
    data_array[4] = 0xA0D80000;
    data_array[5] = 0x333F3F0D;
    data_array[6] = 0x8A127233;
    data_array[7] = 0x00BC3D07;
    dsi_set_cmdq(data_array, 8, 1);
    #else //modify by fae 20150112
    data_array[0] = 0x001B2902;
    data_array[1] = 0xBB331BD3;
    data_array[2] = 0x3333B3BB;
    data_array[3] = 0x01013333;
    data_array[4] = 0xA0D80000;
    data_array[5] = 0x333F3F0D;
    data_array[6] = 0x8A127233;
    data_array[7] = 0x00BC3D57;
    dsi_set_cmdq(data_array, 8, 1);
    #endif
    
    data_array[0] = 0x00042902;
    data_array[1] = 0x000441D4;
    dsi_set_cmdq(data_array, 2, 1);
    
   // data_array[0] = 0x00082902;
   // data_array[1] = 0x000000D5;
   // data_array[2] = 0x3A013A01;
   // dsi_set_cmdq(data_array, 3, 1);
    
    data_array[0] = 0x00192902;
    data_array[1] = 0x7FF8BFD7;
    data_array[2] = 0xFC3ECEA8;
    data_array[3] = 0x83EFE1C1;
    data_array[4] = 0x7F103F07;
    data_array[5] = 0x40E701C0;
    data_array[6] = 0x00C0001C;
    data_array[7] = 0x00000000;
    dsi_set_cmdq(data_array, 8, 1);
    
    data_array[0] = 0x00042902;
    data_array[1] = 0x000000D8;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00042902;
    data_array[1] = 0x000000D9;
    dsi_set_cmdq(data_array, 2, 1);

/**  
data_array[0] = 0x00052902;     //sharpness by Q
data_array[1] = 0x320631dd;
data_array[2] = 0x00000060;
dsi_set_cmdq(data_array, 3, 1);
*/
    
    data_array[0] = 0x00022902;
    data_array[1] = 0x000001D6;
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00351500;                 
    dsi_set_cmdq(data_array, 1, 1);
    
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000036;//Modify, liuyang3.wt,20160514,for rotation 180,because of SHARP lcd not surport reverse scan. 
    dsi_set_cmdq(data_array, 2, 1);
    
data_array[0] = 0x002b2902;           //CE4
data_array[1] = 0xfcfc1dca;
data_array[2] = 0xcaca00fc;
data_array[3] = 0xc0960096;
data_array[4] = 0x008200c0;
data_array[5] = 0x009e0000;
data_array[6] = 0xff820000;
data_array[7] = 0x9fff839f;
data_array[8] = 0x00007d83;
data_array[9] = 0x00000000;
data_array[10] = 0x00000000;
data_array[11] = 0x00000000;
dsi_set_cmdq(data_array, 12, 1);
	
    data_array[0] = 0x00290500;                          
    dsi_set_cmdq(data_array, 1, 1);
    
    MDELAY(1);//20 //modified by fae for save resume time 20150105
	    
    //end:modified by fae for gamma 2.2 param update 20150105   
}

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
	params->dsi.vertical_backporch = 5;//3;
	params->dsi.vertical_frontporch = 16;//8;//10; //MTK suggest

	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;//30; //10;
	params->dsi.horizontal_backporch = 50;//30; //20;
	params->dsi.horizontal_frontporch = 80;//30; //40;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable                                                   = 1; */
#ifndef MACH_FPGA
	params->dsi.PLL_CLOCK = 500;	/* this value must be in MTK suggested table */
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
	//push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
	init_lcm_registers();
	//LCM_LOGI("liuyang:r63350_lcm_init\n");
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

LCM_DRIVER r63350_fhd_dsi_vdo_tcl_nt50358_lcm_drv = {
	.name = "r63350_fhd_dsi_vdo_tcl_nt50358_drv",
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
