#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include <cust_alsps.h>

static struct alsps_hw cust_alsps_hw_ltr579 = {
    .i2c_num    = 1,
    .polling_mode_ps = 0,   //    1, // 0,
    .polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    .i2c_addr   = {0x72, 0x48, 0x78, 0x00},
     .als_level  =   {3, 6, 9, 15, 20, 35, 55, 80, 100, 360, 609, 913, 1490,  2019,  3500,  4605,  7043, 11000,  208040},	/* als_code */
     .als_value  =  {0, 6, 9, 15, 20, 35, 55, 80, 100, 360, 600, 900, 1350,  2000,  3500,  4500,  8000,  8000,   10000, 10000}, 
    .ps_threshold_high = 500,  //1500,   // 1800,  // 1345,	 //838, //700,	//430,
    .ps_threshold_low = 200,  //1500,  // 900, //392, //500,	//250,
    .ps_threshold = 900,
};
#if 1
struct alsps_hw *get_cust_alsps_hw_ltr579(void) {
    return &cust_alsps_hw_ltr579;
}
#else
struct alsps_hw *ltr579_get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}
#endif
int TMD2771_CMM_PPCOUNT_VALUE = 0x0B;
int ZOOM_TIME = 4;
int TMD2771_CMM_CONTROL_VALUE = 0x20; //0xE0:12.5mA/0xA0:25mA/0x60:50mA/0x20:100mA

