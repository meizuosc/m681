#ifndef _MT_PMIC_COMMON_H_
#define _MT_PMIC_COMMON_H_

#include <mach/mt_pm_ldo.h>

#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>

/*
 * PMIC Exported Function for power service
 */
extern signed int g_I_SENSE_offset;

/*
 * PMIC extern functions
 */
extern unsigned int pmic_read_interface(unsigned int RegNum, unsigned int *val, unsigned int MASK, unsigned int SHIFT);
extern unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val, unsigned int MASK, unsigned int SHIFT);
extern unsigned int pmic_read_interface_nolock(unsigned int RegNum,
	unsigned int *val,
	unsigned int MASK,
	unsigned int SHIFT);
extern unsigned int pmic_config_interface_nolock(unsigned int RegNum,
	unsigned int val,
	unsigned int MASK,
	unsigned int SHIFT);
extern unsigned short pmic_set_register_value(PMU_FLAGS_LIST_ENUM flagname, unsigned int val);
extern unsigned short pmic_get_register_value(PMU_FLAGS_LIST_ENUM flagname);
extern unsigned short bc11_set_register_value(PMU_FLAGS_LIST_ENUM flagname, unsigned int val);
extern unsigned short bc11_get_register_value(PMU_FLAGS_LIST_ENUM flagname);
extern void upmu_set_reg_value(unsigned int reg, unsigned int reg_val);
extern unsigned int upmu_get_reg_value(unsigned int reg);
extern void pmic_lock(void);
extern void pmic_unlock(void);

extern void pmic_enable_interrupt(unsigned int intNo, unsigned int en, char *str);
extern void pmic_register_interrupt_callback(unsigned int intNo, void (EINT_FUNC_PTR) (void));
extern unsigned short is_battery_remove_pmic(void);

extern signed int PMIC_IMM_GetCurrent(void);
extern unsigned int PMIC_IMM_GetOneChannelValue(pmic_adc_ch_list_enum dwChannel, int deCount,
					      int trimd);
extern void pmic_auxadc_init(void);
extern void lockadcch3(void);
extern void unlockadcch3(void);

extern unsigned int pmic_Read_Efuse_HPOffset(int i);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);

extern int get_dlpt_imix_spm(void);
extern int get_dlpt_imix(void);
extern int dlpt_check_power_off(void);
extern unsigned int pmic_read_vbif28_volt(unsigned int *val);
extern unsigned int pmic_get_vbif28_volt(void);
extern void pmic_auxadc_debug(int index);
extern int do_ptim_ex(bool isSuspend, unsigned int *bat, signed int *cur);
extern void get_ptim_value(bool isSuspend, unsigned int *bat, signed int *cur);

#ifdef CONFIG_MTK_PMIC_CHIP_MT6353
extern void md1_pmic_setting_on(void);
#endif
/*
extern bool hwPowerOn(MT65XX_POWER powerId, int voltage_uv, char *mode_name);
extern bool hwPowerDown(MT65XX_POWER powerId, char *mode_name);
*/
extern int pmic_force_vcore_pwm(bool enable);

#endif				/* _MT_PMIC_COMMON_H_ */
