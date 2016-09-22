/* BEGIN PN:DTS2013053103858 , Added by d00238048, 2013.05.31*/
#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
    #include <platform/disp_drv_platform.h>
	
#elif defined(BUILD_UBOOT)
    #include <asm/arch/mt_gpio.h>
    #include <platform/mt_pmic.h>
#else
    #include <linux/delay.h>
    #include <mach/mt_gpio.h>
    #include <mach/mt_pm_ldo.h>
#endif
#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define FRAME_WIDTH  										(800)
#define FRAME_HEIGHT 										(1280)

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#define LCD_VCC_EN        			GPIO_LCM_LVDS_PWR_EN //GPIO119
#define DISP_RSTN_CONN   		GPIO_LCM_MIPI2LVDS_EN //GPIO112

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V3(para_tbl,size,force_update)        lcm_util.dsi_set_cmdq_V3(para_tbl,size,force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

//update initial param for IC hx8394d 0.01
static LCM_setting_table_V3 lcm_initialization_setting_kd[] = {
	{0x39, 0xb9,  3,  {0xff, 0x83, 0x94}},
	{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,20,{}},
	{0x39, 0xba, 2, {0x73, 0x83}},
	{0x39, 0xb1, 15, {0x6c, 0x15, 0x15, 0x24, 0xE4, 0x11, 0xf1, 0x80, 0xe4, 0xd7, 0x23, 0x80, 0xc0, 0xd2, 0x58}},
	{0x39, 0xb2, 11, {0x00, 0x64, 0x10, 0x07, 0x20, 0x1C, 0x08, 0x08, 0x1c, 0x4d, 0x00}},
	{0x39, 0xb4, 12, {0x00, 0xff, 0x03, 0x5A, 0x03, 0x5A, 0x03, 0x5A, 0x01, 0x6a, 0x01, 0x6a}},
	{0x39, 0xb6, 2, {0x60, 0x60}},
	{0x15, 0xcc, 1, {0x09}},
	{0x39, 0xd3, 30, {0x00, 0x06, 0x00, 0x40, 0x1A, 0x08, 0x00, 0x32, 0x10, 0x07, 0x00, 0x07, 0x54, 0x15, 0x0f, 0x05, 0x04, 0x02, 0x12, 0x10, 0x05, 0x07, 0x33, 0x33, 0x0B, 0x0B, 0x37, 0x10, 0x07, 0x07}},
	{0x39, 0xd5, 44, {0x19, 0x19, 0x18, 0x18, 0x1A, 0x1A, 0x1B, 0x1B, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x20, 0x21, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x22, 0x23, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}},
	{0x39, 0xd6, 44, {0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B, 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x23, 0x22, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x21, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}},
	{0x39, 0xe0, 42, {0x00, 0x06, 0x0C, 0x31, 0x34, 0x3F, 0x1D, 0x41, 0x06, 0x0A, 0x0C, 0x17, 0x0F, 0x12, 0x15, 0x13, 0x14, 0x07, 0x12, 0x15, 0x16, 0x00, 0x06, 0x0B, 0x30, 0x34, 0x3F, 0x1D, 0x40, 0x07, 0x0A, 0x0D, 0x18, 0x0E, 0x12, 0x14, 0x12, 0x14, 0x08, 0x13, 0x14, 0x19}},
	{0x15, 0xd2, 1, {0x55}},
	{0x39, 0xc0, 2, {0x30, 0x14}},
	{0x39, 0xbf, 3, {0x41, 0x0E, 0x01}},
	{0x39, 0xc7, 4, {0x00, 0xC0, 0x40, 0xC0}},
	{0x15, 0xdf, 1, {0x8E}},
	{0x05, 0x11,0,{}},
	{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,120,{}},
	{0x05, 0x29,0,{}},
	{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,20,{}},
};

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
    memset(params, 0, sizeof(LCM_PARAMS));
    params->type   								= LCM_TYPE_DSI;
    params->width  							= FRAME_WIDTH;
    params->height 							= FRAME_HEIGHT;

    params->dsi.mode   							= BURST_VDO_MODE;

    // DSI
    /* Command mode setting */
    params->dsi.LANE_NUM						= LCM_FOUR_LANE;
    //The following defined the fomat for data coming from LCD engine.
    params->dsi.data_format.color_order 			= LCM_COLOR_ORDER_RGB;
    params->dsi.data_format.trans_seq   			= LCM_DSI_TRANS_SEQ_MSB_FIRST;
    params->dsi.data_format.padding     			= LCM_DSI_PADDING_ON_LSB;
    params->dsi.data_format.format      			= LCM_DSI_FORMAT_RGB888;

   // Highly depends on LCD driver capability.
   //video mode timing

    params->dsi.PS								= LCM_PACKED_PS_24BIT_RGB888;

    params->dsi.vertical_sync_active				= 2;
    params->dsi.vertical_backporch				= 16;
    params->dsi.vertical_frontporch				= 9;
    params->dsi.vertical_active_line				= FRAME_HEIGHT;

    params->dsi.horizontal_sync_active			= 20;
    params->dsi.horizontal_backporch			= 64;
    params->dsi.horizontal_frontporch			= 70;
    params->dsi.horizontal_active_pixel			= FRAME_WIDTH;

    //improve clk quality
    params->dsi.PLL_CLOCK = 221; //this value must be in MTK suggested table
    params->dsi.compatibility_for_nvk = 1;
    params->dsi.ssc_disable = 1;

}
static void lcm_init_kd(void)
{
    //enable power
#ifdef BUILD_LK
    upmu_set_rg_vgp3_vosel(3);//3=1.8v
    upmu_set_rg_vgp3_en(1);
#else
    hwPowerOn(MT6322_POWER_LDO_VGP3, VOL_1800, "LCM");
#endif
    lcm_util.set_gpio_out(LCD_VCC_EN, GPIO_OUT_ONE);
    msleep(50);
    //reset high to low to high
    lcm_util.set_gpio_out(DISP_RSTN_CONN, GPIO_OUT_ONE);
    mdelay(50);
    lcm_util.set_gpio_out(DISP_RSTN_CONN, GPIO_OUT_ZERO);
    mdelay(50);
    lcm_util.set_gpio_out(DISP_RSTN_CONN, GPIO_OUT_ONE);
    msleep(150);

    dsi_set_cmdq_V3(lcm_initialization_setting_kd, sizeof(lcm_initialization_setting_kd) / sizeof(lcm_initialization_setting_kd[0]), 1);  

    LCD_DEBUG("uboot:kd_hx8394d_lcm_init\n");
}

static void lcm_suspend(void)
{
    unsigned int data_array[16];

    data_array[0]=0x00280500; // Display Off
    dsi_set_cmdq(data_array, 1, 1);
	
    data_array[0] = 0x00100500; // Sleep In
    dsi_set_cmdq(data_array, 1, 1);
	
    //reset low
    lcm_util.set_gpio_out(DISP_RSTN_CONN, GPIO_OUT_ZERO);
    mdelay(5);
    //disable power
    lcm_util.set_gpio_out(LCD_VCC_EN, GPIO_OUT_ZERO);
#ifndef BUILD_LK
    hwPowerDown(MT6322_POWER_LDO_VGP3, "LCM");
#endif
    mdelay(5);	
    LCD_DEBUG("uboot:kd_hx8394d_lcm_suspend\n");

}
static void lcm_resume_kd(void)
{
    //enable power
#ifdef BUILD_LK
    upmu_set_rg_vgp3_vosel(3);//3=1.8v
    upmu_set_rg_vgp3_en(1);
#else
    hwPowerOn(MT6322_POWER_LDO_VGP3, VOL_1800, "LCM");
#endif
    lcm_util.set_gpio_out(LCD_VCC_EN, GPIO_OUT_ONE);
    msleep(50);

    //reset low to high
    lcm_util.set_gpio_out(DISP_RSTN_CONN, GPIO_OUT_ONE);
    mdelay(50);
    lcm_util.set_gpio_out(DISP_RSTN_CONN, GPIO_OUT_ZERO);
    mdelay(50);
    lcm_util.set_gpio_out(DISP_RSTN_CONN, GPIO_OUT_ONE);
    msleep(150);

    dsi_set_cmdq_V3(lcm_initialization_setting_kd, sizeof(lcm_initialization_setting_kd) / sizeof(lcm_initialization_setting_kd[0]), 1);

    LCD_DEBUG("uboot:kd_hx8394d_lcm_resume\n");

}

static unsigned int lcm_compare_id_kd(void)
{
    unsigned char LCD_ID_value = 0;

    LCD_ID_value = mt_get_gpio_in(GPIO29);
    return (LCD_ID_value == 0)?1:0; 
}
LCM_DRIVER hx8394d_wxga_kd_lcm_drv =
{
    .name           = "hx8394d_wxga_dsi_video_kd",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init_kd,/*kd init fun.*/
    .suspend        = lcm_suspend,
    .resume         = lcm_resume_kd,
    .compare_id     = lcm_compare_id_kd,
   
};
