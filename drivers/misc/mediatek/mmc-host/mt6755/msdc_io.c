#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <mach/board.h>
#include <mach/mt_pm_ldo.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include "mt_sd.h"
#include "msdc_tune.h"
#include "dbg.h"

/**************************************************************/
/* Section 1: Device Tree Global Variables                    */
/**************************************************************/
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#ifndef CONFIG_MTK_LEGACY
#include <linux/regulator/consumer.h>
#endif

const struct of_device_id msdc_of_ids[] = {
	{   .compatible = "mediatek,msdc0", },
	{   .compatible = "mediatek,msdc1", },
	{   .compatible = "mediatek,msdc2", },
	{   .compatible = "mediatek,msdc3", },
	{ },
};

struct msdc_hw *p_msdc_hw[HOST_MAX_NUM] = {
#if defined(CFG_DEV_MSDC0)
	&msdc0_hw,
#else
	NULL,
#endif
#if defined(CFG_DEV_MSDC1)
	&msdc1_hw,
#else
	NULL,
#endif
#if defined(CFG_DEV_MSDC2)
	&msdc2_hw,
#else
	NULL,
#endif
#if defined(CFG_DEV_MSDC3)
	&msdc3_hw,
#else
	NULL,
#endif
};

struct device_node *gpio_node;
struct device_node *msdc0_io_cfg_node;
struct device_node *msdc1_io_cfg_node;
struct device_node *msdc2_io_cfg_node;
struct device_node *msdc3_io_cfg_node;

struct device_node *infracfg_ao_node;
struct device_node *infracfg_node;
struct device_node *pericfg_node;
struct device_node *emi_node;
struct device_node *toprgu_node;
struct device_node *apmixed_node;
struct device_node *topckgen_node;
struct device_node *eint_node;

struct device_node **msdc_io_cfg_nodes[HOST_MAX_NUM] = {
	&msdc0_io_cfg_node,
	&msdc1_io_cfg_node,
	&msdc2_io_cfg_node,
	&msdc3_io_cfg_node
};

void __iomem *gpio_base;
void __iomem *msdc0_io_cfg_base;
void __iomem *msdc1_io_cfg_base;
void __iomem *msdc2_io_cfg_base;
void __iomem *msdc3_io_cfg_base;

void __iomem *infracfg_ao_reg_base;
void __iomem *infracfg_reg_base;
void __iomem *pericfg_reg_base;
void __iomem *emi_reg_base;
void __iomem *toprgu_reg_base;
void __iomem *apmixed_reg_base;
void __iomem *topckgen_reg_base;

void __iomem **msdc_io_cfg_bases[HOST_MAX_NUM] = {
	&msdc0_io_cfg_base,
	&msdc1_io_cfg_base,
	&msdc2_io_cfg_base,
	&msdc3_io_cfg_base
};

/**************************************************************/
/* Section 2: Power                                           */
/**************************************************************/
u32 g_msdc0_io;
u32 g_msdc0_flash;
u32 g_msdc1_io;
u32 g_msdc1_flash;
u32 g_msdc2_io;
u32 g_msdc2_flash;
u32 g_msdc3_io;
u32 g_msdc3_flash;
/*
u32 g_msdc4_io     = 0;
u32 g_msdc4_flash  = 0;
*/

#if !defined(FPGA_PLATFORM)
/*#include <mach/upmu_common.h>*/
/* FIX ME: change upmu_set_rg_vmc_184() to generic API and remove extern */
/* extern void upmu_set_rg_vmc_184(kal_uint8 x); */
/*workaround for VMC 1.8v -> 1.84v*/
/* extern bool hwPowerSetVoltage(unsigned int powerId,
	int powerVOlt, char *mode_name); */

#ifndef CONFIG_MTK_LEGACY
struct regulator *reg_VEMC;
struct regulator *reg_VMC;
struct regulator *reg_VMCH;

void msdc_get_regulators(struct device *dev)
{
	if (reg_VEMC == NULL)
		reg_VEMC = regulator_get(dev, "vemc");
	if (reg_VMC == NULL)
		reg_VMC = regulator_get(dev, "vmc");
	if (reg_VMCH == NULL)
		reg_VMCH = regulator_get(dev, "vmch");
}

bool msdc_hwPowerOn(unsigned int powerId, int powerVolt, char *mode_name)
{
	struct regulator *reg = NULL;
	if (powerId == POWER_LDO_VMCH)
		reg = reg_VMCH;
	else if (powerId == POWER_LDO_VMC)
		reg = reg_VMC;
	else if (powerId == POWER_LDO_VEMC)
		reg = reg_VEMC;
	if (reg == NULL)
		return FALSE;

	/* New API voltage use micro V */
	regulator_set_voltage(reg, powerVolt, powerVolt);
	regulator_enable(reg);
	pr_err("msdc_hwPoweron:%d: name:%s", powerId, mode_name);
	return TRUE;
}

bool msdc_hwPowerDown(unsigned int powerId, char *mode_name)
{
	struct regulator *reg = NULL;

	if (powerId == POWER_LDO_VMCH)
		reg = reg_VMCH;
	else if (powerId == POWER_LDO_VMC)
		reg = reg_VMC;
	else if (powerId == POWER_LDO_VEMC)
		reg = reg_VEMC;

	if (reg == NULL)
		return FALSE;

	/* New API voltage use micro V */
	regulator_disable(reg);
	pr_err("msdc_hwPowerOff:%d: name:%s", powerId, mode_name);

	return TRUE;
}
#endif

#ifndef CONFIG_MTK_LEGACY
u32 msdc_ldo_power(u32 on, unsigned int powerId, int voltage_mv,
	u32 *status)
#else
u32 msdc_ldo_power(u32 on, MT65XX_POWER powerId, int voltage_mv,
	u32 *status)
#endif
{
	int voltage_uv = voltage_mv * 1000;

	if (on) { /* want to power on */
		if (*status == 0) {  /* can power on */
			pr_warn("msdc LDO<%d> power on<%d>\n", powerId,
				voltage_uv);
			msdc_hwPowerOn(powerId, voltage_uv, "msdc");
			*status = voltage_uv;
		} else if (*status == voltage_uv) {
			pr_err("msdc LDO<%d><%d> power on again!\n", powerId,
				voltage_uv);
		} else { /* for sd3.0 later */
			pr_warn("msdc LDO<%d> change<%d> to <%d>\n", powerId,
				*status, voltage_uv);
			msdc_hwPowerDown(powerId, "msdc");
			msdc_hwPowerOn(powerId, voltage_uv, "msdc");
			/*hwPowerSetVoltage(powerId, voltage_uv, "msdc");*/
			*status = voltage_uv;
		}
	} else {  /* want to power off */
		if (*status != 0) {  /* has been powerred on */
			pr_warn("msdc LDO<%d> power off\n", powerId);
			msdc_hwPowerDown(powerId, "msdc");
			*status = 0;
		} else {
			pr_err("LDO<%d> not power on\n", powerId);
		}
	}

	return 0;
}

void msdc_dump_ldo_sts(struct msdc_host *host)
{
#ifdef MTK_MSDC_BRINGUP_DEBUG
	u32 ldo_en = 0, ldo_vol = 0, ldo_cal = 0;
	u32 id = host->id;

	switch (id) {
	case 0:
		pmic_read_interface_nolock(REG_VEMC_EN, &ldo_en, MASK_VEMC_EN,
			SHIFT_VEMC_EN);
		pmic_read_interface_nolock(REG_VEMC_VOSEL, &ldo_vol,
			MASK_VEMC_VOSEL, SHIFT_VEMC_VOSEL);
		pmic_read_interface_nolock(REG_VEMC_VOSEL_CAL, &ldo_cal,
			MASK_VEMC_VOSEL_CAL, SHIFT_VEMC_VOSEL_CAL);
		#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
		pr_err(" VEMC_EN=0x%x, VEMC_VOL=0x%x [2b'00(2.9V),2b'01(3V),2b'10(3.3V)], VEMC_CAL=0x%x\n",
			ldo_en, ldo_vol, ldo_cal);
		#else
		pr_err(" VEMC_EN=0x%x, VEMC_VOL=0x%x [1b'0(3V),1b'1(3.3V)], VEMC_CAL=0x%x\n",
			ldo_en, ldo_vol, ldo_cal);
		#endif
		break;
	case 1:
		pmic_read_interface_nolock(REG_VMC_EN, &ldo_en, MASK_VMC_EN,
			SHIFT_VMC_EN);
		pmic_read_interface_nolock(REG_VMC_VOSEL, &ldo_vol,
			MASK_VMC_VOSEL, SHIFT_VMC_VOSEL);
		pmic_read_interface_nolock(REG_VMCH_VOSEL_CAL, &ldo_cal,
			MASK_VMCH_VOSEL_CAL, SHIFT_VMCH_VOSEL_CAL);

		#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
		pr_err(" VMC_EN=0x%x, VMC_VOL=0x%x [2b'00(1.8V),2b'01(2.9V),2b'10(3V),2b'11(3.3V)], VMC_CAL=0x%x\n",
			ldo_en, ldo_vol, ldo_cal);
		#else
		pr_err(" VMC_EN=0x%x, VMC_VOL=0x%x [3b'101(2.9V),3b'011(1.8V)], VMC_CAL=0x%x\n",
			ldo_en, ldo_vol, ldo_cal);
		#endif

		pmic_read_interface_nolock(REG_VMCH_EN, &ldo_en, MASK_VMCH_EN,
			SHIFT_VMCH_EN);
		pmic_read_interface_nolock(REG_VMCH_VOSEL, &ldo_vol,
			MASK_VMCH_VOSEL, SHIFT_VMCH_VOSEL);
		pmic_read_interface_nolock(REG_VMC_VOSEL_CAL, &ldo_cal,
			MASK_VMC_VOSEL_CAL, SHIFT_VMC_VOSEL_CAL);

		#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
		pr_err(" VMCH_EN=0x%x, VMCH_VOL=0x%x [2b'00(2.9V),2b'01(3V),2b'10(3.3V)], VMC_CAL=0x%x\n",
			ldo_en, ldo_vol, ldo_cal);
		#else
		pr_err(" VMCH_EN=0x%x, VMCH_VOL=0x%x [1b'0(3V),1b'1(3.3V)], VMC_CAL=0x%x\n",
			ldo_en, ldo_vol, ldo_cal);
		#endif

		break;
	default:
		break;
	}
#endif
}

#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
void msdc_power_DL_CL_control(struct msdc_host *host, u32 sel)
{
	/* since this function may be invoked with a lock already get,
	   we use pmic_config_interface_nolock() instead of 
	   pmic_config_interface() to avoid lock dependency */
	u32 DL_on, CL_on;

	if (host->id == 0) {
		if (sel == MSDC_POWER_DL_CL_FOR_POWER_OFF) {
			CL_on = 0;
			DL_on = 0;
		} else if (sel == MSDC_POWER_DL_CL_FOR_POWER_ON) {
			CL_on = 0;
			DL_on = 1;
		/*
		} else if (sel == MSDC_POWER_DL_CL_FOR_NO_LOAD) {
			DL_on = 0;
			CL_on = 1;
		} else {
			DL_on = 1;
			CL_on = 1;
		*/
		}
		if (host->power_DL_status != DL_on)
			pmic_config_interface_nolock(REG_VEMC_DL_EN, DL_on,
				MASK_VEMC_DL_EN, SHIFT_VEMC_DL_EN);
		if (host->power_CL_status != CL_on)
			pmic_config_interface_nolock(REG_VEMC_CL_EN, CL_on,
				MASK_VEMC_CL_EN, SHIFT_VEMC_CL_EN);

		host->power_DL_status = DL_on;
		host->power_CL_status = CL_on;
		udelay(60);
	} else if (host->id == 1) {
		CL_on = 0;
		if (sel == MSDC_POWER_DL_CL_FOR_POWER_OFF) {
			DL_on = 0;
		} else if (sel == MSDC_POWER_DL_CL_FOR_POWER_ON) {
			DL_on = 1;
		/*
		} else if (sel == MSDC_POWER_DL_CL_FOR_NO_LOAD) {
			DL_on = 0;
		} else {
			DL_on = 1;*/
		}

		/* VMCH CL always off, DL alwys on */
		if (host->power_DL_status != DL_on)
			pmic_config_interface_nolock(REG_VMCH_DL_EN, DL_on,
				MASK_VMCH_DL_EN, SHIFT_VMCH_DL_EN);
		/*
		pmic_config_interface_nolock(REG_VMCH_CL_EN, on,
			MASK_VMCH_CL_EN, SHIFT_VMCH_CL_EN);
		*/

		/* VMC CL always off, DL alwys on */
		if (host->power_CL_status != DL_on)
			pmic_config_interface_nolock(REG_VMC_DL_EN, DL_on,
				MASK_VMC_DL_EN, SHIFT_VMC_DL_EN);
		/*
		pmic_config_interface_nolock(REG_VMC_CL_EN, on,
			MASK_VMC_CL_EN, SHIFT_VMC_CL_EN);
		*/

		host->power_DL_status = DL_on;
		host->power_CL_status = CL_on;
		udelay(60);
	}
	/*pr_err("msdc%d power_control DL(%d) CL(%d)\n", host->id, DL_on,
		CL_on);*/
}
#endif

void msdc_sd_power_switch(struct msdc_host *host, u32 on)
{
	unsigned int reg_val;

	switch (host->id) {
	case 1:	
		if (on) {
			/* VMC calibration +40mV. According to SA's request. */
			reg_val = host->vmc_cal_default - 2;

			pmic_config_interface(REG_VMC_VOSEL_CAL,
				reg_val,
				MASK_VMC_VOSEL_CAL,
				SHIFT_VMC_VOSEL_CAL);
		}

		msdc_ldo_power(on, POWER_LDO_VMC, VOL_1800, &g_msdc1_io);

		msdc_set_tdsel(host, 0, 1);
		msdc_set_rdsel(host, 0, 1);
		msdc_set_driving(host, host->hw, 1);
		break;
	case 2:
		msdc_ldo_power(on, POWER_LDO_VMC, VOL_1800, &g_msdc2_io);
		msdc_set_tdsel(host, 0, 1);
		msdc_set_rdsel(host, 0, 1);
		msdc_set_driving(host, host->hw, 1);
		break;
	default:
		break;
	}
}

void msdc_sdio_power(struct msdc_host *host, u32 on)
{
	switch (host->id) {
#if defined(CFG_DEV_MSDC2)
	case 2:
		g_msdc2_flash = g_msdc2_io;
		break;
#endif

#if defined(CFG_DEV_MSDC3)
	case 3:
		break;
#endif

	default:
		/* if host_id is 3, it uses default 1.8v setting,
		which always turns on */
		break;
	}

}

void msdc_emmc_power(struct msdc_host *host, u32 on)
{
	unsigned long tmo = 0;
	void __iomem *base = host->base;
	unsigned int sa_timeout;

	/* if MMC_CAP_WAIT_WHILE_BUSY not set, mmc core layer will wait for
	 sa_timeout */
	if (host->mmc && host->mmc->card && (on == 0) &&
	    host->mmc->caps & MMC_CAP_WAIT_WHILE_BUSY) {
		/* max timeout: 1000ms */
		sa_timeout = host->mmc->card->ext_csd.sa_timeout;
		if ((DIV_ROUND_UP(sa_timeout, 10000)) < 1000)
			tmo = DIV_ROUND_UP(sa_timeout, 10000000/HZ) + HZ/100;
		else
			tmo = HZ;
		tmo += jiffies;

		while ((MSDC_READ32(MSDC_PS) & 0x10000) != 0x10000) {
			if (time_after(jiffies, tmo)) {
				pr_err("Dat0 keep low before power off,sa_timeout = 0x%x",
					sa_timeout);
				emmc_sleep_failed = 1;
				break;
			}
		}
	}

	/* Set to 3.0V - 100mV
	 * 4'b0000: 0 mV
	 * 4'b0001: -20 mV
	 * 4'b0010: -40 mV
	 * 4'b0011: -60 mV
	 * 4'b0100: -80 mV
	 * 4'b0101: -100 mV
	*/
	#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	msdc_power_DL_CL_control(host, ((on == 0) ? 0 : 1));
	#endif
	msdc_ldo_power(on, POWER_LDO_VEMC, VOL_3000, &g_msdc0_flash);
	if (on && EMMC_VOL_ACTUAL != VOL_3000) {
		pmic_config_interface(REG_VEMC_VOSEL_CAL,
			VEMC_VOSEL_CAL_mV(EMMC_VOL_ACTUAL - VOL_3000),
			MASK_VEMC_VOSEL_CAL,
			SHIFT_VEMC_VOSEL_CAL);
	}

	msdc_dump_ldo_sts(host);
}

void msdc_sd_power(struct msdc_host *host, u32 on)
{
	u32 card_on = on;

	switch (host->id) {
	case 1:
		msdc_set_driving(host, host->hw, 0);
		msdc_set_tdsel(host, 0, 0);
		msdc_set_rdsel(host, 0, 0);
		if (host->hw->flags & MSDC_SD_NEED_POWER)
			card_on = 1;

		#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
		/*  ramp-up time: 0: 40us, 1: 240us */
		pmic_config_interface(REG_VMCH_RAMPUP_SEL, on, MASK_VMCH_RAMPUP_SEL,
			SHIFT_VMCH_RAMPUP_SEL);
		msdc_power_DL_CL_control(host, ((on == 0) ? 0 : 1));
		#endif
		msdc_ldo_power(card_on, POWER_LDO_VMCH, VOL_3000,
			&g_msdc1_flash);

		/* VMCH Golden for 6750 */
		/*          Addr  on   off  mask  shift */
		/* vosel:  0xAB2   6         0x7     8  */
		/* DL   :  0xAB4   1    0    0x1     8  */
		/* CL   :  0xAB4   1    0    0x1     9  */
		/* Ramp :  0xA80             0x1     9  */
		/* EN   :  0xA80   1    0    0x1     1  */

		msdc_ldo_power(on, POWER_LDO_VMC, VOL_3000, &g_msdc1_io);

		/* VMC Golden for 6750 */
		/*          Addr  on   off  mask  shift */
		/* vosel:  0xAC2   1         0x3     8  */
		/* DL   :  0xAC4   1    0    0x1     4  */
		/* CL   :  0xAC4   1    0    0x1     5  */
		/* EN   :  0xA7A   1    0    0x1     1  */

		break;

	default:
		break;
	}

	msdc_dump_ldo_sts(host);
	pmic_read_interface_nolock(REG_VMC_VOSEL, &card_on,
		MASK_VMC_VOSEL, SHIFT_VMC_VOSEL);
}

#endif

void msdc_sd_power_off(void)
{
	struct msdc_host *host = mtk_msdc_host[1];

	pr_err("Power Off, SD card\n");

	/* power must be on */
	g_msdc1_io = VOL_3000 * 1000;
	g_msdc1_flash = VOL_3000 * 1000;

	/* power off */
	msdc_ldo_power(0, POWER_LDO_VMC, VOL_3000, &g_msdc1_io);
	msdc_ldo_power(0, POWER_LDO_VMCH, VOL_3000, &g_msdc1_flash);

	if (host != NULL) {
		msdc_set_bad_card_and_remove(host);
	}
}
EXPORT_SYMBOL(msdc_sd_power_off);

#ifdef MSDC_HQA
/*#define HQA_VCORE  0x48*/ /* HVcore, 1.05V */
#define HQA_VCORE  0x40 /* NVcore, 1.0V */
/*#define HQA_VCORE  0x38*/ /* LVcore, 0.95V */

void msdc_HQA_set_vcore(struct msdc_host *host)
{
	if ( host->first_tune_done==0 ) {
		pmic_config_interface(REG_VCORE_VOSEL_SW, HQA_VCORE,
			VCORE_VOSEL_SW_MASK, 0);
		pmic_config_interface(REG_VCORE_VOSEL_HW, HQA_VCORE,
			VCORE_VOSEL_HW_MASK, 0);
	}
}
#endif


/**************************************************************/
/* Section 3: Clock                                           */
/**************************************************************/
#if !defined(FPGA_PLATFORM)
/* clock source for host: global */
u32 hclks_msdc50[] = {26000000 , 400000000, 200000000, 156000000,
		      182000000, 156000000, 100000000, 624000000,
		      312000000};

u32 hclks_msdc30[] = {26000000 , 208000000, 100000000, 156000000,
		      182000000, 156000000, 178000000, 200000000};

u32 hclks_msdc30_3[] = {26000000 , 50000000, 100000000, 156000000,
		      48000000, 156000000, 178000000, 54000000,
		      25000000};

u32 *hclks_msdc = hclks_msdc30;

#ifndef CONFIG_MTK_CLKMGR
#include <dt-bindings/clock/mt6755-clk.h>
#include <mach/mt_clk_id.h>
#include <mach/mt_idle.h>

int msdc_cg_clk_id[HOST_MAX_NUM] = {
	MT_CG_INFRA0_MSDC0_CG_STA,
	MT_CG_INFRA0_MSDC1_CG_STA,
	MT_CG_INFRA0_MSDC2_CG_STA,
	MT_CG_INFRA0_MSDC3_CG_STA
};

int msdc_get_ccf_clk_pointer(struct platform_device *pdev,
	struct msdc_host *host)
{
	if (pdev->id == 0) {
		host->clock_control = devm_clk_get(&pdev->dev, "MSDC0-CLOCK");
	} else if (pdev->id == 1) {
		host->clock_control = devm_clk_get(&pdev->dev, "MSDC1-CLOCK");
	} else if (pdev->id == 2) {
		host->clock_control = devm_clk_get(&pdev->dev, "MSDC2-CLOCK");
	} else if (pdev->id == 3) {
		host->clock_control = devm_clk_get(&pdev->dev, "MSDC3-CLOCK");
	}
	if (IS_ERR(host->clock_control)) {
		pr_err("can not get msdc%d clock control\n", pdev->id);
		return 1;
	} else {
		if (clk_prepare(host->clock_control)) {
			pr_err("can not prepare msdc%d clock control\n",
				pdev->id);
			return 1;
		}
	}

	return 0;
}
#endif

void msdc_select_clksrc(struct msdc_host *host, int clksrc)
{
	u32 *hclks;
#ifdef CONFIG_MTK_CLKMGR
	char name[6];
#endif

	hclks = msdc_get_hclks(host->id);

	pr_err("[%s]: msdc%d change clk_src from %dKHz to %d:%dKHz\n", __func__,
		host->id, (host->hclk/1000), clksrc, (hclks[clksrc]/1000));

#ifdef CONFIG_MTK_CLKMGR
	sprintf(name, "MSDC%d", host->id);
#endif

	host->hclk = hclks[clksrc];
	host->hw->clk_src = clksrc;
}

void msdc_dump_clock_sts(void)
{
#ifdef MTK_MSDC_BRINGUP_DEBUG
	void __iomem *reg;

	if (apmixed_reg_base && topckgen_reg_base && infracfg_ao_reg_base) {
		reg = apmixed_reg_base + MSDCPLL_PWR_CON0_OFFSET;
		pr_err(" MSDCPLL_PWR_CON0[0x%p][bit0~1 should be 2b'01]=0x%x\n",
			reg, MSDC_READ32(reg));
		reg = apmixed_reg_base + MSDCPLL_CON0_OFFSET;
		pr_err(" MSDCPLL_CON0    [0x%p][bit0 should be 1b'1]=0x%x\n",
			reg, MSDC_READ32(reg));

		reg = topckgen_reg_base + MSDC_CLK_CFG_3_OFFSET;
		pr_err(" CLK_CFG_3       [0x%p][bit[19:16]should be 0x1]=0x%x\n",
			reg, MSDC_READ32(reg));
		pr_err(" CLK_CFG_3       [0x%p][bit[27:24]should be 0x7]=0x%x\n",
			reg, MSDC_READ32(reg));
		reg = topckgen_reg_base + MSDC_CLK_CFG_4_OFFSET;
		pr_err(" CLK_CFG_4       [0x%p][bit[3:0]should be 0x7]=0x%x\n",
			reg, MSDC_READ32(reg));

		reg = infracfg_ao_reg_base + MSDC_INFRA_PDN_STA1_OFFSET;
		pr_err(" MODULE_SW_CG_1  [0x%p][bit2=msdc0, bit4=msdc1, bit5=msdc2, 0:on,1:off]=0x%x\n",
			reg, MSDC_READ32(reg));
	} else {
		pr_err(" apmixed_reg_base = %p, topckgen_reg_base = %p, clk_infra_base = %p\n",
			apmixed_reg_base, topckgen_reg_base, infracfg_ao_reg_base);
	}
#endif
}

void msdc_clk_status(int *status)
{
	int g_clk_gate = 0;
	int i = 0;
	unsigned long flags;

	for (i = 0; i < HOST_MAX_NUM; i++) {
		if (!mtk_msdc_host[i])
			continue;

		spin_lock_irqsave(&mtk_msdc_host[i]->clk_gate_lock, flags);
		#ifndef CONFIG_MTK_CLKMGR
		if (mtk_msdc_host[i]->clk_gate_count > 0)
			g_clk_gate |= 1 << msdc_cg_clk_id[i];
		#endif
		spin_unlock_irqrestore(&mtk_msdc_host[i]->clk_gate_lock, flags);
	}
	*status = g_clk_gate;
}
#endif

/**************************************************************/
/* Section 4: GPIO and Pad                                    */
/**************************************************************/
#if !defined(FPGA_PLATFORM)
void msdc_dump_padctl_by_id(u32 id)
{
	switch (id) {
	case 0:
		pr_err("MSDC0 MODE18[0x%p] =0x%8x\tshould:0x12-- ----\n",
			MSDC0_GPIO_MODE18, MSDC_READ32(MSDC0_GPIO_MODE18));
		pr_err("MSDC0 MODE19[0x%p] =0x%8x\tshould:0x1249 1249\n",
			MSDC0_GPIO_MODE19, MSDC_READ32(MSDC0_GPIO_MODE19));
		pr_err("MSDC0 IES   [0x%p] =0x%8x\tshould:0x---- --1F\n",
			MSDC0_GPIO_IES_ADDR, MSDC_READ32(MSDC0_GPIO_IES_ADDR));
		pr_err("MSDC0 SMT   [0x%p] =0x%8x\tshould:0x---- --1F\n",
			MSDC0_GPIO_SMT_ADDR, MSDC_READ32(MSDC0_GPIO_SMT_ADDR));
		pr_err("MSDC0 TDSEL [0x%p] =0x%8x\tshould:0x---0 0000\n",
			MSDC0_GPIO_TDSEL_ADDR,
			MSDC_READ32(MSDC0_GPIO_TDSEL_ADDR));
		pr_err("MSDC0 RDSEL [0x%p] =0x%8x\tshould:0x0000 0000\n",
			MSDC0_GPIO_RDSEL_ADDR,
			MSDC_READ32(MSDC0_GPIO_RDSEL_ADDR));
		pr_err("MSDC0 SR    [0x%p] =0x%8x\n",
			MSDC0_GPIO_SR_ADDR,
			MSDC_READ32(MSDC0_GPIO_SR_ADDR));
		pr_err("MSDC0 DRV   [0x%p] =0x%8x\n",
			MSDC0_GPIO_DRV_ADDR,
			MSDC_READ32(MSDC0_GPIO_DRV_ADDR));
		pr_err("MSDC0 PULL0 [0x%p] =0x%8x\n",
			MSDC0_GPIO_PUPD0_ADDR,
			MSDC_READ32(MSDC0_GPIO_PUPD0_ADDR));
		pr_err("P-NONE: 0x4444 4444, PU:0x2111 1161, PD:0x6666 6666\n");
		pr_err("MSDC0 PULL1 [0x%p] =0x%8x\n",
			MSDC0_GPIO_PUPD1_ADDR,
			MSDC_READ32(MSDC0_GPIO_PUPD1_ADDR));
		pr_err("P-NONE: 0x---- 4444, PU:0x---- 6111, PD:0x---- 6666\n");
		break;
	case 1:
		pr_err("MSDC1 MODE4 [0x%p] =0x%8x\tshould:0x---1 1249\n",
			MSDC1_GPIO_MODE4, MSDC_READ32(MSDC1_GPIO_MODE4));
		pr_err("MSDC1 IES   [0x%p] =0x%8x\tshould:0x----   -7--\n",
			MSDC1_GPIO_IES_ADDR, MSDC_READ32(MSDC1_GPIO_IES_ADDR));
		pr_err("MSDC1 SMT   [0x%p] =0x%8x\tshould:0x----   -7--\n",
			MSDC1_GPIO_SMT_ADDR, MSDC_READ32(MSDC1_GPIO_SMT_ADDR));
		pr_err("MSDC1 TDSEL [0x%p] =0x%8x\n",
			MSDC1_GPIO_TDSEL_ADDR,
			MSDC_READ32(MSDC1_GPIO_TDSEL_ADDR));
		/* FIX ME: Check why sleep shall set as F */
		pr_err("should 1.8v: sleep:0x---- -FFF, awake:0x---- -AAA\n");
		pr_err("MSDC1 RDSEL0[0x%p] =0x%8x\n",
			MSDC1_GPIO_RDSEL0_ADDR,
			MSDC_READ32(MSDC1_GPIO_RDSEL0_ADDR));
		pr_err("1.8V: 0x-000 ----, 3.3v: 0x-30C ----\n");
		pr_err("MSDC1 RDSEL1[0x%p] =0x%8x\n",
			MSDC1_GPIO_RDSEL1_ADDR,
			MSDC_READ32(MSDC1_GPIO_RDSEL1_ADDR));
		pr_err("should 1.8V: 0x-000 ----, 3.3v: 0x---- ---C\n");
		pr_err("MSDC1 SR    [0x%p] =0x%8x\n",
			MSDC1_GPIO_SR_ADDR, MSDC_READ32(MSDC1_GPIO_SR_ADDR));
		pr_err("MSDC1 DRV   [0x%p] =0x%8x\n",
			MSDC1_GPIO_DRV_ADDR, MSDC_READ32(MSDC1_GPIO_DRV_ADDR));
		pr_err("MSDC1 PULL  [0x%p] =0x%8x\n",
			MSDC1_GPIO_PUPD_ADDR,
			MSDC_READ32(MSDC1_GPIO_PUPD_ADDR));
		pr_err("P-NONE: 0x--44 4444, PU:0x--22 2262, PD:0x--66 6666\n");
		break;
#ifdef CFG_DEV_MSDC2
	case 2:
		pr_err("MSDC2 MODE14[0x%p][21:3]  =0x%8x\tshould:0x--0x12 2490\n",
			MSDC2_GPIO_MODE14, MSDC_READ32(MSDC2_GPIO_MODE14));
		pr_err("MSDC2 IES   [0x%p][5:3]   =0x%8x\tshould:0x---- -38-\n",
			MSDC2_GPIO_IES_ADDR, MSDC_READ32(MSDC2_GPIO_IES_ADDR));
		pr_err("MSDC2 SMT   [0x%p][5:3]   =0x%8x\tshould:0x---- -38-\n",
			MSDC2_GPIO_SMT_ADDR, MSDC_READ32(MSDC2_GPIO_SMT_ADDR));
		pr_err("MSDC2 TDSEL [0x%p][23:12] =0x%8xtshould:0x--0x--00 0---\n",
			MSDC2_GPIO_TDSEL_ADDR, MSDC_READ32(MSDC2_GPIO_TDSEL_ADDR));
		pr_err("MSDC2 RDSEL0[0x%p][11:6]  =0x%8xtshould:0x--0x---- -00-\n",
			MSDC2_GPIO_RDSEL_ADDR, MSDC_READ32(MSDC2_GPIO_RDSEL_ADDR));
		pr_err("MSDC2 SR DRV[0x%p][23:12] =0x%8x\n",
			MSDC2_GPIO_SR_ADDR, MSDC_READ32(MSDC2_GPIO_SR_ADDR));
		pr_err("MSDC2 PULL  [0x%p][31:12] =0x%8x\n",
			MSDC2_GPIO_PUPD_ADDR0, MSDC_READ32(MSDC2_GPIO_PUPD_ADDR0));
		pr_err("P-NONE: 0x--44444, PU:0x--11611, PD:0x--66666\n");
		pr_err("MSDC2 PULL  [0x%p][2:0] =0x%8x\n",
			MSDC2_GPIO_PUPD_ADDR1, MSDC_READ32(MSDC2_GPIO_PUPD_ADDR1));
		pr_err("P-NONE: 0x--4, PU:0x--1, PD:0x--6\n");
		break;
#endif
	}
}

void msdc_set_pin_mode(struct msdc_host *host)
{
	switch (host->id) {
	case 0:
		MSDC_SET_FIELD(MSDC0_GPIO_MODE18, (0x3F<<25), 0x9);
		MSDC_SET_FIELD(MSDC0_GPIO_MODE19, 0xFFFFFFFF, 0x12491249);
		break;

	case 1:
		MSDC_SET_FIELD(MSDC1_GPIO_MODE4, 0x0007FFFF, 0x0011249);
		break;

#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_SET_FIELD(MSDC2_GPIO_MODE14, 0x003FFFF8, 0x24492);
		break;

#endif

	default:
		pr_err("[%s] invlalid host->id!\n", __func__);
		break;

	}
}

void msdc_set_ies_by_id(u32 id, int set_ies)
{
	switch (id) {
	case 0:
		MSDC_SET_FIELD(MSDC0_GPIO_IES_ADDR, MSDC0_IES_ALL_MASK,
			(set_ies ? 0x1F : 0));
		break;

	case 1:
		MSDC_SET_FIELD(MSDC1_GPIO_IES_ADDR, MSDC1_IES_ALL_MASK,
			(set_ies ? 0x7 : 0));
		break;

#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_SET_FIELD(MSDC2_GPIO_IES_ADDR, MSDC2_IES_ALL_MASK,
			(set_ies ? 0x7 : 0));
		break;
#endif

	default:
		pr_err("[%s] invlalid host->id!\n", __func__);
		break;
	}
}

void msdc_set_smt_by_id(u32 id, int set_smt)
{
	switch (id) {
	case 0:
		MSDC_SET_FIELD(MSDC0_GPIO_SMT_ADDR, MSDC0_SMT_ALL_MASK,
			(set_smt ? 0x1F : 0));
		break;
	case 1:
		MSDC_SET_FIELD(MSDC1_GPIO_SMT_ADDR, MSDC1_SMT_ALL_MASK,
			(set_smt ? 0x7 : 0));
		break;

#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_SET_FIELD(MSDC2_GPIO_SMT_ADDR, MSDC2_SMT_ALL_MASK,
			(set_smt ? 0x7 : 0));
		break;
#endif

	default:
		pr_err("[%s] invlalid host->id!\n", __func__);
		break;
	}
}

void msdc_set_tdsel_by_id(u32 id, bool sleep, bool sd_18)
{
	switch (id) {
	case 0:
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR, MSDC0_TDSEL_ALL_MASK , 0);
		break;
	case 1:
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL_ADDR, MSDC1_TDSEL_ALL_MASK,
			(sleep ? 0xFFF : 0xAAA));
		break;

#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_SET_FIELD(MSDC2_GPIO_TDSEL_ADDR, MSDC2_TDSEL_ALL_MASK , 0);
		break;
#endif

	default:
		pr_err("[%s] invlalid host->id!\n", __func__);
		break;
	}
}

void msdc_set_rdsel_by_id(u32 id, bool sleep, bool sd_18)
{
	switch (id) {
	case 0:
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL_ADDR, MSDC0_RDSEL_ALL_MASK, 0);
		break;

	case 1:
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_ADDR, MSDC1_RDSEL0_ALL_MASK,
			(sd_18 ? 0 : 0x30C));
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL1_ADDR, MSDC1_RDSEL1_ALL_MASK,
			(sd_18 ? 0 : 0xC));
		break;

#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_SET_FIELD(MSDC2_GPIO_RDSEL_ADDR, MSDC2_RDSEL_ALL_MASK, 0);
		break;
#endif

	default:
		pr_err("[%s] invlalid host->id!\n", __func__);
		break;
	}
}

void msdc_set_tdsel_dbg_by_id(u32 id, u32 value)
{
	switch (id) {
	case 0:
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR,
			MSDC0_TDSEL_CMD_MASK, value);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR,
			MSDC0_TDSEL_DSL_MASK, value);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR,
			MSDC0_TDSEL_CLK_MASK, value);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR,
			MSDC0_TDSEL_DAT_MASK, value);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR,
			MSDC0_TDSEL_RSTB_MASK, value);
		break;
	case 1:
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL_ADDR,
			MSDC1_TDSEL_CMD_MASK, value);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL_ADDR,
			MSDC1_TDSEL_CLK_MASK, value);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL_ADDR,
			MSDC1_TDSEL_DAT_MASK, value);
		break;
#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_SET_FIELD(MSDC2_GPIO_TDSEL_ADDR,
			MSDC2_TDSEL_CMD_MASK, value);
		MSDC_SET_FIELD(MSDC2_GPIO_TDSEL_ADDR,
			MSDC2_TDSEL_CLK_MASK, value);
		MSDC_SET_FIELD(MSDC2_GPIO_TDSEL_ADDR,
			MSDC2_TDSEL_DAT_MASK, value);
		break;
#endif
	}
}

void msdc_set_rdsel_dbg_by_id(u32 id, u32 value)
{
	switch (id) {
	case 0:
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL_ADDR,
			MSDC0_RDSEL_CMD_MASK, value);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL_ADDR,
			MSDC0_RDSEL_DSL_MASK, value);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL_ADDR,
			MSDC0_RDSEL_CLK_MASK, value);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL_ADDR,
			MSDC0_RDSEL_DAT_MASK, value);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL_ADDR,
			MSDC0_RDSEL_RSTB_MASK, value);
		break;
	case 1:
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_ADDR,
			MSDC1_RDSEL_CMD_MASK, value);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_ADDR,
			MSDC1_RDSEL_CLK_MASK, value);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL1_ADDR,
			MSDC1_RDSEL_DAT_MASK, value);
		break;
#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_SET_FIELD(MSDC2_GPIO_RDSEL_ADDR,
			MSDC2_RDSEL_CMD_MASK, value);
		MSDC_SET_FIELD(MSDC2_GPIO_RDSEL_ADDR,
			MSDC2_RDSEL_CLK_MASK, value);
		MSDC_SET_FIELD(MSDC2_GPIO_RDSEL_ADDR,
			MSDC2_RDSEL_DAT_MASK, value);
		break;
#endif
	}
}

void msdc_get_tdsel_dbg_by_id(u32 id, u32 *value)
{
	switch (id) {
	case 0:
		MSDC_GET_FIELD(MSDC0_GPIO_TDSEL_ADDR,
			MSDC0_TDSEL_CMD_MASK, *value);
		break;
	case 1:
		MSDC_GET_FIELD(MSDC1_GPIO_TDSEL_ADDR,
			MSDC1_TDSEL_CMD_MASK, *value);
		break;
#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_GET_FIELD(MSDC2_GPIO_TDSEL_ADDR,
			MSDC2_TDSEL_CMD_MASK, *value);
		break;
#endif
	}
}

void msdc_get_rdsel_dbg_by_id(u32 id, u32 *value)
{
	switch (id) {
	case 0:
		MSDC_GET_FIELD(MSDC0_GPIO_RDSEL_ADDR,
			MSDC0_RDSEL_CMD_MASK, *value);
		break;
	case 1:
		MSDC_GET_FIELD(MSDC1_GPIO_RDSEL0_ADDR,
			MSDC1_RDSEL_CMD_MASK, *value);
		break;
#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_GET_FIELD(MSDC2_GPIO_RDSEL_ADDR,
			MSDC2_RDSEL_CMD_MASK, *value);
		break;
#endif
	}
}

void msdc_set_sr_by_id(u32 id, int clk, int cmd, int dat, int rst, int ds)
{
	switch (id) {
	case 0:
		MSDC_SET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_CMD_MASK,
			(cmd != 0));
		MSDC_SET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_DSL_MASK,
			(ds != 0));
		MSDC_SET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_CLK_MASK,
			(clk != 0));
		MSDC_SET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_DAT_MASK,
			(dat != 0));
		MSDC_SET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_RSTB_MASK,
			(rst != 0));
		break;
	case 1:
		MSDC_SET_FIELD(MSDC1_GPIO_SR_ADDR, MSDC1_SR_CMD_MASK,
			(cmd != 0));
		MSDC_SET_FIELD(MSDC1_GPIO_SR_ADDR, MSDC1_SR_CLK_MASK,
			(clk != 0));
		MSDC_SET_FIELD(MSDC1_GPIO_SR_ADDR, MSDC1_SR_DAT_MASK,
			(dat != 0));
		break;

#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_SET_FIELD(MSDC2_GPIO_SR_ADDR, MSDC2_SR_CMD_MASK,
			(cmd != 0));
		MSDC_SET_FIELD(MSDC2_GPIO_SR_ADDR, MSDC2_SR_CLK_MASK,
			(clk != 0));
		MSDC_SET_FIELD(MSDC2_GPIO_SR_ADDR, MSDC2_SR_DAT_MASK,
			(dat != 0));
		break;
#endif

	default:
		pr_err("[%s] invlalid host->id!\n", __func__);
		break;
	}

}

void msdc_set_driving_by_id(u32 id, struct msdc_hw *hw, bool sd_18)
{
	switch (id) {
	case 0:
		MSDC_SET_FIELD(MSDC0_GPIO_DRV_ADDR, MSDC0_DRV_CMD_MASK,
			hw->cmd_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV_ADDR, MSDC0_DRV_DSL_MASK,
			hw->ds_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV_ADDR, MSDC0_DRV_CLK_MASK,
			hw->clk_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV_ADDR, MSDC0_DRV_DAT_MASK,
			hw->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV_ADDR, MSDC0_DRV_RSTB_MASK,
			hw->rst_drv);
		break;
	case 1:
		if (sd_18) {
			MSDC_SET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_CMD_MASK,
				hw->cmd_drv_sd_18);
			MSDC_SET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_CLK_MASK,
				hw->clk_drv_sd_18);
			MSDC_SET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_DAT_MASK,
				hw->dat_drv_sd_18);
		} else {
			MSDC_SET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_CMD_MASK,
				hw->cmd_drv);
			MSDC_SET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_CLK_MASK,
				hw->clk_drv);
			MSDC_SET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_DAT_MASK,
				hw->dat_drv);
		}
		break;

#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_SET_FIELD(MSDC2_GPIO_DRV_ADDR, MSDC2_DRV_CMD_MASK,
			hw->cmd_drv);
		MSDC_SET_FIELD(MSDC2_GPIO_DRV_ADDR, MSDC2_DRV_CLK_MASK,
			hw->clk_drv);
		MSDC_SET_FIELD(MSDC2_GPIO_DRV_ADDR, MSDC2_DRV_DAT_MASK,
			hw->dat_drv);
		break;
#endif

	default:
		pr_err("[%s] invlalid host->id!\n", __func__);
		break;
	}
}

void msdc_get_driving_by_id(u32 id, struct msdc_ioctl *msdc_ctl)
{
	switch (id) {
	case 0:
		MSDC_GET_FIELD(MSDC0_GPIO_DRV_ADDR, MSDC0_DRV_CMD_MASK,
			msdc_ctl->cmd_pu_driving);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV_ADDR, MSDC0_DRV_DSL_MASK,
			msdc_ctl->ds_pu_driving);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV_ADDR, MSDC0_DRV_CLK_MASK,
			msdc_ctl->clk_pu_driving);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV_ADDR, MSDC0_DRV_DAT_MASK,
			msdc_ctl->dat_pu_driving);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV_ADDR, MSDC0_DRV_RSTB_MASK,
			msdc_ctl->rst_pu_driving);
		break;
	case 1:
		MSDC_GET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_CMD_MASK,
			msdc_ctl->cmd_pu_driving);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_CLK_MASK,
			msdc_ctl->clk_pu_driving);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_DAT_MASK,
			msdc_ctl->dat_pu_driving);
		msdc_ctl->rst_pu_driving = 0;
		msdc_ctl->ds_pu_driving = 0;
		break;

#ifdef CFG_DEV_MSDC2
	case 2:
		MSDC_GET_FIELD(MSDC2_GPIO_DRV_ADDR, MSDC2_DRV_CMD_MASK,
			msdc_ctl->cmd_pu_driving);
		MSDC_GET_FIELD(MSDC2_GPIO_DRV_ADDR, MSDC2_DRV_CLK_MASK,
			msdc_ctl->clk_pu_driving);
		MSDC_GET_FIELD(MSDC2_GPIO_DRV_ADDR, MSDC2_DRV_DAT_MASK,
			msdc_ctl->dat_pu_driving);
		msdc_ctl->rst_pu_driving = 0;
		msdc_ctl->ds_pu_driving = 0;
		break;
#endif

	default:
		pr_err("[%s] invlalid host->id!\n", __func__);
		break;
	}
}

void msdc_pin_config_by_id(u32 id, u32 mode)
{
	switch (id) {
	case 0:
		/*Attention: don't pull CLK high; Don't toggle RST to prevent
		  from entering boot mode */
		if (MSDC_PIN_PULL_NONE == mode) {
			/* high-Z */
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD0_ADDR,
				MSDC0_PUPD0_MASK, 0x4444444);
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD1_ADDR,
				MSDC0_PUPD1_MASK, 0x4444);
		} else if (MSDC_PIN_PULL_DOWN == mode) {
			/* cmd/clk/dat/(rstb)/dsl:pd-50k*/
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD0_ADDR,
				MSDC0_PUPD0_MASK, 0x6666666);
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD1_ADDR,
				MSDC0_PUPD1_MASK, 0x6666);
		} else if (MSDC_PIN_PULL_UP == mode) {
			/* clk/dsl:pd-50k, cmd/dat:pu-10k, (rstb:pu-50k)*/
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD0_ADDR,
				MSDC0_PUPD0_MASK, 0x1111161);
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD1_ADDR,
				MSDC0_PUPD1_MASK, 0x6111);
		}
		break;
	case 1:
		if (MSDC_PIN_PULL_NONE == mode) {
			/* high-Z */
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD_ADDR,
				MSDC1_PUPD_MASK, 0x444444);
		} else if (MSDC_PIN_PULL_DOWN == mode) {
			/* cmd/clk/dat:pd-50k */
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD_ADDR,
				MSDC1_PUPD_MASK, 0x666666);
		} else if (MSDC_PIN_PULL_UP == mode) {
			/* cmd/dat:pu-50k, clk:pd-50k */
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD_ADDR,
				MSDC1_PUPD_MASK, 0x222262);
		}
		break;
#ifdef CFG_DEV_MSDC2
	case 2:
		if (MSDC_PIN_PULL_NONE == mode) {
			/* high-Z */
			MSDC_SET_FIELD(MSDC2_GPIO_PUPD_ADDR0,
				MSDC2_PUPD_MASK0, 0x44444);
			MSDC_SET_FIELD(MSDC2_GPIO_PUPD_ADDR1,
				MSDC2_PUPD_MASK1, 0x4);
		} else if (MSDC_PIN_PULL_DOWN == mode) {
			/* cmd/clk/dat:pd-50k */
			MSDC_SET_FIELD(MSDC2_GPIO_PUPD_ADDR0,
				MSDC2_PUPD_MASK0, 0x66666);
			MSDC_SET_FIELD(MSDC2_GPIO_PUPD_ADDR1,
				MSDC2_PUPD_MASK1, 0x6);
		} else if (MSDC_PIN_PULL_UP == mode) {
			/* cmd/dat:pu-10k, clk:pd-50k, */
			MSDC_SET_FIELD(MSDC2_GPIO_PUPD_ADDR0,
				MSDC2_PUPD_MASK0, 0x11611);
			MSDC_SET_FIELD(MSDC2_GPIO_PUPD_ADDR1,
				MSDC2_PUPD_MASK1, 0x1);
		}
		break;
#endif

	default:
		pr_err("[%s] invlalid host->id!\n", __func__);
		break;
	}

}
#endif /*if !defined(FPGA_PLATFORM)*/

/**************************************************************/
/* Section 5: Device Tree Init function                       */
/*            This function is placed here so that all        */
/*            functions and variables used by it has already  */
/*            been declared                                   */
/**************************************************************/
int msdc_dt_init(struct platform_device *pdev, unsigned int *cd_irq,
	void __iomem **base, unsigned int *irq, struct msdc_hw **hw)
{
	unsigned int msdc_hw_parameter[sizeof(struct tag_msdc_hw_para) / 4];
	unsigned int msdc_custom[1];
	struct device_node *msdc_cust_node = NULL;
#if !defined(FPGA_PLATFORM) && !defined(CONFIG_MTK_LEGACY)
	struct device_node *msdc_backup_node;
#endif
	unsigned int host_id = 0;
	int i, ret;
	char *msdc_names[] = {"msdc0", "msdc1", "msdc2", "msdc3"};
	char *msdc_cust_names[] = {
		"mediatek,MSDC0_custom", "mediatek,MSDC1_custom", NULL, NULL};
	char *ioconfig_names[] = {
		"mediatek,IOCFG_5", "mediatek,IOCFG_0",
		"mediatek,IOCFG_4", "NOT EXIST"
	};

	for (i = 0; i < HOST_MAX_NUM; i++) {
		if (0 == strcmp(pdev->dev.of_node->name, msdc_names[i])) {
			if (p_msdc_hw[i] == NULL)
				return 1;
			host_id = i;
			break;
		}
	}

	if (i == HOST_MAX_NUM)
		return 1;

	pr_err("of msdc DT probe %s!\n", pdev->dev.of_node->name);

	/* iomap register */
	*base = of_iomap(pdev->dev.of_node, 0);
	if (!(*base)) {
		pr_err("can't of_iomap for msdc!!\n");
		return -ENOMEM;
	} else {
		pr_err("of_iomap for msdc @ 0x%p\n", *base);
	}

	/* get irq #  */
	*irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	pr_debug("msdc get irq # %d\n", *irq);
	BUG_ON(irq < 0);
	if (gpio_node == NULL) {
		gpio_node = of_find_compatible_node(NULL, NULL,
			"mediatek,GPIO");
		gpio_base = of_iomap(gpio_node, 0);
		pr_debug("of_iomap for gpio base @ 0x%p\n", gpio_base);
	}

	if (*msdc_io_cfg_nodes[host_id] == NULL) {
		*msdc_io_cfg_nodes[host_id] = of_find_compatible_node(
			NULL, NULL, ioconfig_names[host_id]);
		*msdc_io_cfg_bases[host_id] =
			of_iomap(*msdc_io_cfg_nodes[host_id], 0);
		pr_debug("of_iomap for MSDC%d IOCFG base @ 0x%p\n",
			host_id, *msdc_io_cfg_bases[host_id]);
	}

#ifndef FPGA_PLATFORM
	/*FIX ME, correct node match string */
	if (infracfg_ao_node == NULL) {
		infracfg_ao_node = of_find_compatible_node(NULL, NULL,
			"mediatek,infracfg_ao");
		infracfg_ao_reg_base = of_iomap(infracfg_ao_node, 0);
		pr_debug("of_iomap for infracfg_ao base @ 0x%p\n",
			infracfg_ao_reg_base);
	}

	if (infracfg_node == NULL) {
		infracfg_node = of_find_compatible_node(NULL, NULL,
			"mediatek,infracfg");
		infracfg_reg_base = of_iomap(infracfg_node, 0);
		pr_debug("of_iomap for infracfg base @ 0x%p\n",
			infracfg_reg_base);
	}

	if (pericfg_node == NULL) {
		pericfg_node = of_find_compatible_node(NULL, NULL,
			"mediatek,pericfg");
		pericfg_reg_base = of_iomap(pericfg_node, 0);
		pr_debug("of_iomap for pericfg base @ 0x%p\n",
			pericfg_reg_base);
	}

	if (emi_node == NULL) {
		emi_node = of_find_compatible_node(NULL, NULL,
			"mediatek,emi");
		emi_reg_base = of_iomap(emi_node, 0);
		pr_debug("of_iomap for emi base @ 0x%p\n",
			emi_reg_base);
	}

	if (toprgu_node == NULL) {
		toprgu_node = of_find_compatible_node(NULL, NULL,
			"mediatek,toprgu");
		toprgu_reg_base = of_iomap(toprgu_node, 0);
		pr_debug("of_iomap for toprgu base @ 0x%p\n",
			toprgu_reg_base);
	}

	if (apmixed_node == NULL) {
		apmixed_node = of_find_compatible_node(NULL, NULL,
			"mediatek,apmixed");
		apmixed_reg_base = of_iomap(apmixed_node, 0);
		pr_err("of_iomap for APMIXED base @ 0x%p\n",
			apmixed_reg_base);
	}

	if (topckgen_node == NULL) {
		topckgen_node = of_find_compatible_node(NULL, NULL,
			"mediatek,mt6755-topckgen");
		topckgen_reg_base = of_iomap(topckgen_node, 0);
		pr_err("of_iomap for TOPCKGEN base @ 0x%p\n",
			topckgen_reg_base);
	}

#ifndef CONFIG_MTK_LEGACY
	/* backup original dev.of_node */
	msdc_backup_node = pdev->dev.of_node;
	/* get regulator supply node */
	pdev->dev.of_node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt_pmic_regulator_supply");
	msdc_get_regulators(&(pdev->dev));
	/* restore original dev.of_node */
	pdev->dev.of_node = msdc_backup_node;
#endif

	if (host_id == 1)
		pmic_ldo_oc_int_register();

#endif

#ifdef FPGA_PLATFORM
	msdc_fpga_pwr_init(&(pdev->dev));
#endif

	/* Get custom node in Device tree, if not set, use default */
	pdev->id = host_id;
	*hw = p_msdc_hw[host_id];
	if (host_id == 0) {
		(*hw)->ett_settings = msdc0_ett_settings;
		(*hw)->ett_count = MSDC0_ETT_COUNTS;
	}

	if (msdc_cust_names[host_id]) {
		msdc_cust_node = of_find_compatible_node(NULL, NULL,
			msdc_cust_names[host_id]);
	}

	if (!msdc_cust_node) {
		pr_err("MSDC%d: No custom DTS node, use default parm\n",
			host_id);
		goto skip_cust_node;
	}

	/* If use custom parameter,
		custom = <1> should be setted in device tree */
	ret = of_property_read_u32_array(msdc_cust_node, "custom",
		msdc_custom, ARRAY_SIZE(msdc_custom));
	if (ret == 0 && msdc_custom[0] == 1) {
		/* Get HW parameter from Device tree */
		if (!of_property_read_u32_array(msdc_cust_node, "hw",
			msdc_hw_parameter,
			ARRAY_SIZE(msdc_hw_parameter))) {
			if (host_id == 0 || host_id == 1) {
				if (msdc_setting_parameter(*hw,
					msdc_hw_parameter)) {
					pr_err("MSDC%d hw setting custom parameter error\n",
						host_id);
				} else {
					pr_err("MSDC%d hw setting custom parameter OK\n",
						host_id);
				}
			}
		}
	}
	/* Get ett seeting from device tree, only for msdc0 */
	if (host_id == 0) {
		/* If use custom parameter, custom_ett = <1>
		   should be setted in device tree */
		ret = of_property_read_u32_array(msdc_cust_node,
			"custom_ett", msdc_custom,
			ARRAY_SIZE(msdc_custom));
		if (ret == 0 && msdc_custom[0] == 1) {
			if (of_property_read_u32_array(msdc_cust_node,
				"ett_settings",
				(unsigned int *)msdc0_ett_settings,
				MSDC0_ETT_COUNTS * 4)) {
				pr_err("MSDC0 set custom ett settings error\n");
			} else {
				pr_err("MSDC0 set custom ett settings OK\n");
			}
		}
	}

skip_cust_node:
	if (*hw == NULL)
		return -ENODEV;

	if ((pdev->id == 1) &&
	    ((*hw)->host_function == MSDC_SD) &&
	    (eint_node == NULL)) {
		eint_node = of_find_compatible_node(NULL, NULL,
			"mediatek, MSDC1_INS-eint");
		if (eint_node) {
			pr_debug("find MSDC1_INS-eint node!!\n");
			(*hw)->flags |= (MSDC_CD_PIN_EN | MSDC_REMOVABLE);

			/* get irq #  */
			if (!(*cd_irq)) {
				*cd_irq = irq_of_parse_and_map(eint_node, 0);
				pr_err("can't irq_of_parse_and_map for card detect eint!!\n");
			} else {
				pr_debug("msdc1 EINT get irq # %d\n", *cd_irq);
			}

		} else {
			pr_err("can't find MSDC1_INS-eint compatible node\n");
		}
	}

	return 0;
}

/**************************************************************/
/* Section 6: MISC                                            */
/**************************************************************/
#if !defined(FPGA_PLATFORM)
void dump_axi_bus_info(void)
{
	pr_err("=============== AXI BUS INFO =============");
	return; /*weiping check*/

	if (infracfg_ao_reg_base && infracfg_reg_base && pericfg_reg_base) {
		pr_err("reg[0x10001224]=0x%x",
			MSDC_READ32(infracfg_ao_reg_base + 0x224));
		pr_err("reg[0x10201000]=0x%x",
			MSDC_READ32(infracfg_reg_base + 0x000));
		pr_err("reg[0x10201018]=0x%x",
			MSDC_READ32(infracfg_reg_base + 0x018));
		pr_err("reg[0x1000320c]=0x%x",
			MSDC_READ32(pericfg_reg_base + 0x20c));
		pr_err("reg[0x10003210]=0x%x",
			MSDC_READ32(pericfg_reg_base + 0x210));
		pr_err("reg[0x10003214]=0x%x",
			MSDC_READ32(pericfg_reg_base + 0x214));
	} else {
		pr_err("infracfg_ao_reg_base = %p, infracfg_reg_base = %p, pericfg_reg_base = %p\n",
			infracfg_ao_reg_base,
			infracfg_reg_base,
			pericfg_reg_base);
	}
}

void dump_emi_info(void)
{
	unsigned int i = 0;
	unsigned int addr = 0;
	return; /*weiping check */

	pr_err("=============== EMI INFO =============");
	if (!emi_reg_base) {
		pr_err("emi_reg_base = %p\n", emi_reg_base);
		return;
	}

	pr_err("before, reg[0x102034e8]=0x%x",
		MSDC_READ32(emi_reg_base + 0x4e8));
	pr_err("before, reg[0x10203400]=0x%x",
		MSDC_READ32(emi_reg_base + 0x400));
	MSDC_WRITE32(emi_reg_base + 0x4e8, 0x2000000);
	MSDC_WRITE32(emi_reg_base + 0x400, 0xff0001);
	pr_err("after, reg[0x102034e8]=0x%x",
		MSDC_READ32(emi_reg_base + 0x4e8));
	pr_err("after, reg[0x10203400]=0x%x",
		MSDC_READ32(emi_reg_base + 0x400));

	for (i = 0; i < 5; i++) {
		for (addr = 0; addr < 0x78; addr += 4) {
			pr_err("reg[0x%x]=0x%x", (0x10203500 + addr),
				MSDC_READ32((emi_reg_base + 0x500 + addr)));
			if (addr % 0x10 == 0)
				mdelay(1);
		}
	}
}

void msdc_polling_axi_status(int line, int dead)
{
	int i = 0;
	if (!pericfg_reg_base) {
		pr_err("pericfg_reg_base = %p\n", pericfg_reg_base);
		return;
	}

	while (MSDC_READ32(pericfg_reg_base + 0x214) & 0xc) {
		if (++i < 300) {
			mdelay(10);
			continue;
		}
		pr_err("[%s]: check peri-bus: 0x%x at %d\n", __func__,
			MSDC_READ32(pericfg_reg_base + 0x214), line);

		pr_err("############## AXI bus hang! start ##################");
		dump_emi_info();
		mdelay(10);

		dump_axi_bus_info();
		mdelay(10);

		/*dump_audio_info();
		mdelay(10);*/

		msdc_dump_info(0);
		mdelay(10);

		msdc_dump_gpd_bd(0);
		pr_err("############## AXI bus hang! end ####################");
		if (dead == 0)
			break;
		else
			i = 0;
	}
}
#endif /*if !defined(FPGA_PLATFORM)*/

