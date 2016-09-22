
#include <asm/io.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/uaccess.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_pm_ldo.h>

/****************************************************************
 * GLOBAL DEFINATION
 ****************************************************************/
ROOTBUS_HW g_MT_PMIC_BusHW;

/*******************************************************************
 * Extern Variable DEFINATIONS
 *******************************************************************/

/**********************************************************************
 * Extern FUNCTION DEFINATIONS
 *******************************************************************/

/**********************************************************************
* Debug Message Settings
*****************************************************************/
#if 1
#define MSG(evt, fmt, args...) \
do {    \
	if ((DBG_PMAPI_##evt) & DBG_PMAPI_MASK) { \
		pr_notice(fmt, ##args); \
	} \
} while (0)

#define MSG_FUNC_ENTRY(f)    MSG(ENTER, "<PMAPI FUNC>: %s\n", __func__)
#else
#define MSG(evt, fmt, args...) do {} while (0)
#define MSG_FUNC_ENTRY(f)       do {} while (0)
#endif

/****************************************************************
 * FUNCTION DEFINATIONS
 *******************************************************************/
int first_power_on_flag = 1;

bool hwPowerOn(MT65XX_POWER powerId, MT_POWER_VOLTAGE powerVolt, char *mode_name)
{
	return TRUE;
}
EXPORT_SYMBOL(hwPowerOn);

bool hwPowerDown(MT65XX_POWER powerId, char *mode_name)
{
	return TRUE;
}
EXPORT_SYMBOL(hwPowerDown);
