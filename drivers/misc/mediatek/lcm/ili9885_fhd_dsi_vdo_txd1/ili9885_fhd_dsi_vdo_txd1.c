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

#define LCM_ID_ILI9885A	0x9885

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
	unsigned char para_list[128];
};
static struct LCM_setting_table init_setting[] =
{
    {0xB0,3, {0x98,0x85,0x0A}},               
    {0xD6,12, {0x80,0x00,0x08,0x17,0x23,0x65,0x77,0x44,0x87,0x00,0x00,0x09}},          
    {0x35,1, {0x00}},  

    {0xF5,1, {0x85}},	
    {0xB9,1, {0x02}},	

    {0xF5,1, {0x8D}},	
    {0xEF,1, {0x02}},	
    
    {0xF5,1, {0x9B}},	
    {0xD7,1, {0xA0}},	
    
    {0xF5,1, {0x9C}},	
    {0xD7,1, {0x40}},	    

    {0xF5,1, {0x8B}},	
    {0xD0,1, {0x0E}},	
    {0xF5,1, {0x00}},	
	
    {0x11,1, {0x00}},
    {REGFLAG_DELAY, 120, {}},        
    {0x29,1, {0x00}},  
    {REGFLAG_END_OF_TABLE, 0x00, {}}
	
};

static struct LCM_setting_table lcm_sleep_in_setting[] =
{
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 20, {}},
	// Sleep Mode On
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

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
	params->type   = LCM_TYPE_DSI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
    	params->physical_width = 68;
	params->physical_height = 121;
	params->dsi.mode   					= BURST_VDO_MODE;

	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size=256;

	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count=FRAME_WIDTH*3;

	params->dsi.vertical_sync_active	= 8;  //8
	params->dsi.vertical_backporch		= 16;  //16
	params->dsi.vertical_frontporch		=24 ;   //24
	params->dsi.vertical_active_line	= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active	= 8;   //24
	params->dsi.horizontal_backporch	= 70;   //80
	params->dsi.horizontal_frontporch	= 70;//80--64;   //64
	params->dsi.horizontal_active_pixel	= FRAME_WIDTH;

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
//	params->dsi.lcm_esd_check_table[0].cmd = 0xaf;
//	params->dsi.lcm_esd_check_table[0].count = 1;
//	params->dsi.lcm_esd_check_table[0].para_list[0] = 0xad;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
#if 1
	params->dsi.lcm_esd_check_table[1].cmd = 0xE5;
	params->dsi.lcm_esd_check_table[1].count =10;//10;
	params->dsi.lcm_esd_check_table[1].para_list[0] = 0x36;
	params->dsi.lcm_esd_check_table[1].para_list[1] = 0x36;
	params->dsi.lcm_esd_check_table[1].para_list[2] = 0xA1;
	params->dsi.lcm_esd_check_table[1].para_list[3] = 0xF6;

	params->dsi.lcm_esd_check_table[1].para_list[4] = 0xF6;
	params->dsi.lcm_esd_check_table[1].para_list[5] = 0x47;
	params->dsi.lcm_esd_check_table[1].para_list[6] = 0x07;
	params->dsi.lcm_esd_check_table[1].para_list[7] = 0x55;
	params->dsi.lcm_esd_check_table[1].para_list[8] = 0x15;
	params->dsi.lcm_esd_check_table[1].para_list[9] = 0x63;

	params->dsi.lcm_esd_check_table[2].cmd = 0x0B;  //0B
	params->dsi.lcm_esd_check_table[2].count = 1;
	params->dsi.lcm_esd_check_table[2].para_list[0] = 0x00; //00

	
	params->dsi.lcm_esd_check_table[3].cmd = 0xD2;
	params->dsi.lcm_esd_check_table[3].count = 4;
	params->dsi.lcm_esd_check_table[3].para_list[0] = 0x03;
	params->dsi.lcm_esd_check_table[3].para_list[1] = 0x03;
	params->dsi.lcm_esd_check_table[3].para_list[2] = 0x2A;
	params->dsi.lcm_esd_check_table[3].para_list[3] = 0x22;

#endif

    // Bit rate calculation  440params->dsi.PLL_CLOCK=500;//440;
	params->dsi.PLL_CLOCK=480;//440;
}
static void lcm_init_power(void)
{
    unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret = 0;

	cmd = 0x00;
	data = 0x0F;

	SET_RESET_PIN(0);
	MDELAY(10);
#ifndef MACH_FPGA
#ifdef BUILD_LK
	ret = TPS65132_write_byte(cmd, data);
#else
	ret = tps65132_write_bytes(cmd, data);
#endif
	if (ret < 0)
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write error----\n", cmd);

	cmd = 0x01;
	data = 0x0F;
#ifdef BUILD_LK
	ret = TPS65132_write_byte(cmd, data);
#else
	ret = tps65132_write_bytes(cmd, data);
#endif
	if (ret < 0)
		LCM_LOGI("tps6132----cmd=%0x--i2c write error----\n", cmd);

   	mt_set_gpio_mode(GPIO_65132_EN1, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN1, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN1, GPIO_OUT_ONE);
	MDELAY(3);
	mt_set_gpio_mode(GPIO_65132_EN2, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN2, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN2, GPIO_OUT_ONE);
	MDELAY(10);	
#endif
    LCM_LOGI("ili9885_fhd_dsi_vdo_txd is called\n");
}

static void lcm_suspend_power(void)
{
    LCM_LOGI("ili9885_fhd_dsi_vdo_txd is called\n");
}

static void lcm_resume_power(void)
{
    lcm_init_power();
    LCM_LOGI("ili9885_fhd_dsi_vdo_txd is called\n");
}


static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(5);
	SET_RESET_PIN(0); 
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);	

	push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
	LCM_LOGI("ili9885_fhd_dsi_vdo_txd1 is init\n");
}

static void lcm_suspend(void)
{
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
	
	SET_RESET_PIN(0); 
	MDELAY(5);
	mt_set_gpio_mode(GPIO_65132_EN2, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN2, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN2, GPIO_OUT_ZERO);
    	MDELAY(2);
	mt_set_gpio_mode(GPIO_65132_EN1, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN1, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN1, GPIO_OUT_ZERO);
	MDELAY(1);
}

static void lcm_resume(void)
{
	lcm_init();	
}


static unsigned int lcm_compare_id(void)
{
  
    return 0;
}

LCM_DRIVER ili9885_fhd_dsi_vdo_txd_lcm_drv1 = {
	.name = "ili9885_fhd_dsi_vdo_txd1",
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
