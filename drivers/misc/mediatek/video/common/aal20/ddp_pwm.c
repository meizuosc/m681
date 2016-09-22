#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/atomic.h>
#include <mach/mt_reg_base.h>
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#include <linux/clk.h>
#include "ddp_clkmgr.h"
#endif

#include <mach/mt_gpio.h>
#include <disp_dts_gpio.h> /* DTS GPIO */
#include <cust_leds.h>
#include <cust_leds_def.h>
#include <ddp_reg.h>
#include <ddp_pwm.h>
#include <ddp_path.h>
#include <ddp_drv.h>


#define PWM_DEFAULT_DIV_VALUE 0x0

#define PWM_ERR(fmt, arg...) pr_err("[PWM] " fmt "\n", ##arg)
#define PWM_NOTICE(fmt, arg...) pr_debug("[PWM] " fmt "\n", ##arg)
#define PWM_MSG(fmt, arg...) pr_debug("[PWM] " fmt "\n", ##arg)

#define pwm_get_reg_base(id) (DISPSYS_PWM0_BASE)

#define index_of_pwm(id) (0)
#define PWM_LOG_BUFFER_SIZE 5


static disp_pwm_id_t g_pwm_main_id = DISP_PWM0;
static atomic_t g_pwm_backlight[1] = { ATOMIC_INIT(-1) };
static volatile int g_pwm_max_backlight[1] = { 1023 };
static ddp_module_notify g_ddp_notify;

typedef struct {
	unsigned int value;
	unsigned int count;
	unsigned long tsec;
	unsigned long tusec;
} PWM_LOG;

enum PWM_LOG_TYPE {
	NOTICE_LOG = 0,
	MSG_LOG,
};

static PWM_LOG g_pwm_log_buffer[PWM_LOG_BUFFER_SIZE];
static int g_pwm_log_index;


/*****************************************************************************
 *
 * variable for get clock node fromdts
 *
*****************************************************************************/
static void __iomem *disp_pmw_mux_base;
static void __iomem *disp_pmw_osc_base;

#ifndef MUX_DISPPWM_ADDR /* disp pwm source clock select register address */
#define MUX_DISPPWM_ADDR (disp_pmw_mux_base + 0xB0)
#endif
#ifndef MUX_UPDATE_ADDR /* disp pwm source clock update register address */
#define MUX_UPDATE_ADDR (disp_pmw_mux_base + 0x4)
#endif
#ifndef OSC_ULPOSC_ADDR /* rosc control register address */
#define OSC_ULPOSC_ADDR (disp_pmw_osc_base + 0x458)
#endif
/* clock hard code access API */
#define clk_readl(addr) DRV_Reg32(addr)
#define clk_writel(addr, val) mt_reg_sync_writel(val, addr)
#define clk_setl(addr, val) mt_reg_sync_writel(clk_readl(addr) | (val), addr)
#define clk_clrl(addr, val) mt_reg_sync_writel(clk_readl(addr) & ~(val), addr)

/*****************************************************************************
 *
 * get disp pwm source mux node
 *
*****************************************************************************/
static int disp_pwm_get_muxbase(void)
{
	int ret = 0;
	struct device_node *node;

	if (disp_pmw_mux_base != NULL) {
		PWM_MSG("TOPCKGEN node exist");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,TOPCKGEN");
	if (!node) {
		PWM_ERR("DISP find TOPCKGEN node failed\n");
		return -1;
	}
	disp_pmw_mux_base = of_iomap(node, 0);
	if (!disp_pmw_mux_base) {
		PWM_ERR("DISP TOPCKGEN base failed\n");
		return -1;
	}
	PWM_MSG("find TOPCKGEN node");
	return ret;
}
/*****************************************************************************
 *
 * get disp pwm source osc
 *
*****************************************************************************/
static int disp_pwm_get_oscbase(void)
{
	int ret = 0;
	struct device_node *node;

	if (disp_pmw_osc_base != NULL) {
		PWM_MSG("SLEEP node exist");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,SLEEP");
	if (!node) {
		PWM_ERR("DISP find SLEEP node failed\n");
		return -1;
	}
	disp_pmw_osc_base = of_iomap(node, 0);
	if (!disp_pmw_osc_base) {
		PWM_ERR("DISP find SLEEP base failed\n");
		return -1;
	}
	PWM_MSG("find SLEEP node");
	return ret;
}

static int disp_pwm_check_osc_status(void)
{
	unsigned int regosc;
	regosc = clk_readl(OSC_ULPOSC_ADDR);
	if ((regosc & 0x5) != 0x5) {
		PWM_MSG("osc is off (%x)", regosc);
		return 0;
	} else {
		PWM_MSG("osc is on (%x)", regosc);
		return 1;
	}
}

/*****************************************************************************
 *
 * hardcode turn on/off ROSC api
 *
*****************************************************************************/
static int disp_pwm_osc_on(void)
{
	unsigned int regosc;

	if (disp_pwm_get_oscbase() == -1)
		return -1;

	if (disp_pwm_check_osc_status() == 1)
		return 0; /* osc already on */

	regosc = clk_readl(OSC_ULPOSC_ADDR);
	PWM_MSG("ULPOSC config : 0x%08x", regosc);

	/* OSC EN = 1 */
	regosc = regosc | 0x1;
	clk_writel(OSC_ULPOSC_ADDR, regosc);
	regosc = clk_readl(OSC_ULPOSC_ADDR);
	PWM_MSG("ULPOSC config : 0x%08x after en", regosc);
	udelay(11);

	/* OSC RST  */
	regosc = regosc | 0x2;
	clk_writel(OSC_ULPOSC_ADDR, regosc);
	regosc = clk_readl(OSC_ULPOSC_ADDR);
	PWM_MSG("ULPOSC config : 0x%08x after rst 1", regosc);
	udelay(40);
	regosc = regosc & 0xfffffffd;
	clk_writel(OSC_ULPOSC_ADDR, regosc);
	regosc = clk_readl(OSC_ULPOSC_ADDR);
	PWM_MSG("ULPOSC config : 0x%08x after rst 0", regosc);
	udelay(130);

	/* OSC CG_EN = 1 */
	regosc = regosc | 0x4;
	clk_writel(OSC_ULPOSC_ADDR, regosc);
	regosc = clk_readl(OSC_ULPOSC_ADDR);
	PWM_MSG("ULPOSC config : 0x%08x after cg_en", regosc);

	return 0;

}

static int disp_pwm_osc_off(void)
{
	unsigned int regosc;

	if (disp_pwm_get_oscbase() == -1)
		return -1;

	regosc = clk_readl(OSC_ULPOSC_ADDR);

	/* OSC CG_EN = 0 */
	regosc = regosc & (~0x4);
	clk_writel(OSC_ULPOSC_ADDR, regosc);
	regosc = clk_readl(OSC_ULPOSC_ADDR);
	PWM_MSG("ULPOSC config : 0x%08x after cg_en", regosc);

	udelay(40);

	/* OSC EN = 0 */
	regosc = regosc & (~0x1);
	clk_writel(OSC_ULPOSC_ADDR, regosc);
	regosc = clk_readl(OSC_ULPOSC_ADDR);
	PWM_MSG("ULPOSC config : 0x%08x after en", regosc);

	return 0;
}

static unsigned int disp_pwm_get_pwmmux(void)
{
	unsigned int regsrc;
	regsrc = clk_readl(MUX_DISPPWM_ADDR);
	return regsrc & 0x3;
}

/*****************************************************************************
 *
 * disp pwm source clock select mux api
 *
*****************************************************************************/
#define BRINGUP
static int disp_pwm_set_pwmmux(unsigned int clk_req)
{
	UINT32 clksrc = 0;
	unsigned int regsrc;
	unsigned int regsrc_read;


	if (disp_pwm_get_muxbase() == -1)
		return -1;
	else
		regsrc_read = disp_pwm_get_pwmmux();

	switch (clk_req) {
	case CLK26M:
		{
#if defined(CONFIG_MTK_CLKMGR) || defined(BRINGUP)
		clksrc = 0;
#else

#endif
		break;
		}
	case UNIVPLL_104M:
		{
#if defined(CONFIG_MTK_CLKMGR) || defined(BRINGUP)
		clksrc = 1;
#else

#endif
		break;
		}
	case OSC_104M:
		{
#if defined(CONFIG_MTK_CLKMGR) || defined(BRINGUP)
		clksrc = 2;
#else
		clksrc = OSC_D2;
#endif
		break;
		}
	case OSC_26M:
		{
#if defined(CONFIG_MTK_CLKMGR) || defined(BRINGUP)
		clksrc = 3;
#else
		clksrc = OSC_D8;
#endif
		break;
		}
	default:
		{
		PWM_ERR("[DPI]unknown clock frequency: %d\n", clksrc);
		break;
		}
	}

#ifdef BRINGUP

	regsrc = regsrc & 0xfffffffc;
	regsrc = regsrc | clksrc;
	clk_writel(MUX_DISPPWM_ADDR, regsrc);/* select clock source */
	regsrc = clk_readl(MUX_DISPPWM_ADDR);
	PWM_MSG("CLK_CFG(req=%d) %x -> %x", clksrc, regsrc_read, regsrc);
	regsrc = clk_readl(MUX_UPDATE_ADDR);/* set clock source update bit */
	regsrc = regsrc | (0x1 << 27);
	clk_writel(MUX_UPDATE_ADDR, regsrc);
#else
#ifdef CONFIG_MTK_CLKMGR
	PWM_MSG("normal clk api");
	clkmux_sel(MT_MUX_PWM, clksrc, "DISP_PWM");
#else
	if (clksrc > MUX_DISPPWM) {
		PWM_MSG("ccf clk(%d) api apply success", clksrc);
		ddp_clk_enable(MUX_DISPPWM);
		ddp_clk_set_parent(MUX_DISPPWM, clksrc);
		ddp_clk_disable(MUX_DISPPWM);
	} else {
		PWM_ERR("ccf clk api apply fail");
	}
#endif
#endif

	return 0;
}

static int disp_pwm_config_init(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *cmdq)
{
	struct cust_mt65xx_led *cust_led_list;
	struct cust_mt65xx_led *cust;
	struct PWM_config *config_data;
	unsigned int pwm_div;
	disp_pwm_id_t id = DISP_PWM0;
	unsigned long reg_base = pwm_get_reg_base(id);
	int index = index_of_pwm(id);
	int i;

	pwm_div = PWM_DEFAULT_DIV_VALUE;
#if 1
	cust_led_list = get_cust_led_list();
	if (cust_led_list) {
		/* WARNING: may overflow if MT65XX_LED_TYPE_LCD not configured properly */
		cust = &cust_led_list[MT65XX_LED_TYPE_LCD];
		if ((strcmp(cust->name, "lcd-backlight") == 0)
		    && (cust->mode == MT65XX_LED_MODE_CUST_BLS_PWM)) {
			config_data = &cust->config_data;
			if (config_data->clock_source >= 0 && config_data->clock_source <= 3) {
				if (disp_pwm_get_muxbase() == -1) {
					PWM_ERR("PWM get mux base fail");
				} else {
					unsigned int regVal = disp_pwm_get_pwmmux();
					/*
					* TODO: Apply CCF API, replace clkmux_sel() with
					* clk_set_parent() if this code block is enabled.
					*/
					disp_pwm_set_pwmmux(config_data->clock_source);
					PWM_MSG("disp_pwm_init : CLK_CFG_1 0x%x => 0x%x",
						regVal, disp_pwm_get_pwmmux());
				}
			}
			/* Some backlight chip/PMIC(e.g. MT6332) only accept slower clock */
			pwm_div =
				(config_data->div == 0) ? PWM_DEFAULT_DIV_VALUE : config_data->div;
			pwm_div &= 0x3FF;
			PWM_MSG("disp_pwm_init : PWM config data (%d,%d)",
				config_data->clock_source, config_data->div);
		}
	}
#endif

	atomic_set(&g_pwm_backlight[index], -1);

	/* We don't enable PWM until we really need */
	DISP_REG_MASK(cmdq, reg_base + DISP_PWM_CON_0_OFF, pwm_div << 16, (0x3ff << 16));

	DISP_REG_MASK(cmdq, reg_base + DISP_PWM_CON_1_OFF, 1023, 0x3ff);	/* 1024 levels */
	/* We don't init the backlight here until AAL/Android give */

	g_pwm_log_index = 0;
	for (i = 0; i < PWM_LOG_BUFFER_SIZE; i += 1) {
		g_pwm_log_buffer[i].count = 0;
		g_pwm_log_buffer[i].tsec = -1;
		g_pwm_log_buffer[i].tusec = -1;
		g_pwm_log_buffer[i].value = -1;
	}

	return 0;
}


static int disp_pwm_config(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *cmdq)
{
	int ret = 0;

	if (pConfig->dst_dirty)
		ret |= disp_pwm_config_init(module, pConfig, cmdq);

	return ret;
}


static void disp_pwm_trigger_refresh(disp_pwm_id_t id)
{
	if (g_ddp_notify != NULL)
		g_ddp_notify(DISP_MODULE_PWM0, DISP_PATH_EVENT_TRIGGER);
}


/* Set the PWM which acts by default (e.g. ddp_bls_set_backlight) */
void disp_pwm_set_main(disp_pwm_id_t main)
{
	g_pwm_main_id = main;
}


disp_pwm_id_t disp_pwm_get_main(void)
{
	return g_pwm_main_id;
}


int disp_pwm_is_enabled(disp_pwm_id_t id)
{
	unsigned long reg_base = pwm_get_reg_base(id);

	return DISP_REG_GET(reg_base + DISP_PWM_EN_OFF) & 0x1;
}


static void disp_pwm_set_drverIC_en(disp_pwm_id_t id, int enabled)
{
#ifdef GPIO_LCM_LED_EN
#ifndef CONFIG_MTK_FPGA
	if (id == DISP_PWM0) {
		mt_set_gpio_mode(GPIO_LCM_LED_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCM_LED_EN, GPIO_DIR_OUT);

		if (enabled)
			mt_set_gpio_out(GPIO_LCM_LED_EN, GPIO_OUT_ONE);
		else
			mt_set_gpio_out(GPIO_LCM_LED_EN, GPIO_OUT_ZERO);
	}
#endif
#endif
}


static void disp_pwm_set_enabled(cmdqRecHandle cmdq, disp_pwm_id_t id, int enabled)
{
	unsigned long reg_base = pwm_get_reg_base(id);

	if (enabled) {
		if (!disp_pwm_is_enabled(id)) {
			DISP_REG_MASK(cmdq, reg_base + DISP_PWM_EN_OFF, 0x1, 0x1);
			PWM_MSG("disp_pwm_set_enabled: PWN_EN = 0x1");

			disp_pwm_set_drverIC_en(id, enabled);
		}
	} else {
		DISP_REG_MASK(cmdq, reg_base + DISP_PWM_EN_OFF, 0x0, 0x1);
		disp_pwm_set_drverIC_en(id, enabled);
	}
}


int disp_bls_set_max_backlight(unsigned int level_1024)
{
	return disp_pwm_set_max_backlight(disp_pwm_get_main(), level_1024);
}


int disp_pwm_set_max_backlight(disp_pwm_id_t id, unsigned int level_1024)
{
	int index;

	if ((DISP_PWM_ALL & id) == 0) {
		PWM_ERR("[ERROR] disp_pwm_set_backlight: invalid PWM ID = 0x%x", id);
		return -EFAULT;
	}

	index = index_of_pwm(id);
	g_pwm_max_backlight[index] = level_1024;

	PWM_MSG("disp_pwm_set_max_backlight(id = 0x%x, level = %u)", id, level_1024);

	if (level_1024 < atomic_read(&g_pwm_backlight[index]))
		disp_pwm_set_backlight(id, level_1024);

	return 0;
}


int disp_pwm_get_max_backlight(disp_pwm_id_t id)
{
	int index = index_of_pwm(id);
	return g_pwm_max_backlight[index];
}


/* For backward compatible */
int disp_bls_set_backlight(int level_1024)
{
	return disp_pwm_set_backlight(disp_pwm_get_main(), level_1024);
}


/*
 * If you want to re-map the backlight level from user space to
 * the real level of hardware output, please modify here.
 *
 * Inputs:
 *  id          - DISP_PWM0 / DISP_PWM1
 *  level_1024  - Backlight value in [0, 1023]
 * Returns:
 *  PWM duty in [0, 1023]
 */
static int disp_pwm_level_remap(disp_pwm_id_t id, int level_1024)
{
	//printk(KERN_WARNING "liuyang:level_1024 = %d\n",level_1024);
	 if (level_1024 != 0 && level_1024 < 40)
		level_1024 = 11;
	else if (level_1024 < 512)
		level_1024 = level_1024 * 5/ 16;
	else if (level_1024 < 612)
		level_1024 = level_1024 - 352;
	else if (level_1024 < 1000)
		level_1024 = level_1024 * 9 / 5 - 841;
	else 
		level_1024 = level_1024 * 8 / 3 - 1707;

#ifdef  MTK_REDUCE_BACKLIGHT_SUPPORT
        if (level_1024 > 760)
        {
            level_1024 = 760;
        }
#endif
	//printk(KERN_WARNING "liuyang:map_level_1024 = %d\n",level_1024);
	return level_1024;
}

int disp_pwm_set_backlight(disp_pwm_id_t id, int level_1024)
{
	int ret;

#ifdef MTK_DISP_IDLE_LP
	disp_exit_idle_ex("disp_pwm_set_backlight");
#endif

	/* Always write registers by CPU */
	ret = disp_pwm_set_backlight_cmdq(id, level_1024, NULL);

	if (ret >= 0)
		disp_pwm_trigger_refresh(id);

	return 0;
}


static volatile int g_pwm_duplicate_count;
static char buffer[512] = "";
static void disp_pwm_log(int level_1024, int log_type)
{
	int i;
	struct timeval pwm_time;

	do_gettimeofday(&pwm_time);
	g_pwm_log_buffer[g_pwm_log_index].value = level_1024;
	g_pwm_log_buffer[g_pwm_log_index].tsec = (unsigned long)pwm_time.tv_sec % 1000;
	g_pwm_log_buffer[g_pwm_log_index].tusec = (unsigned long)pwm_time.tv_usec;
	g_pwm_log_index += 1;

	if (g_pwm_log_index == PWM_LOG_BUFFER_SIZE || level_1024 == 0) {
		sprintf(buffer + strlen(buffer), "(latest=%2u): ", g_pwm_log_index);
		for (i = 0; i < g_pwm_log_index; i += 1) {
			sprintf(buffer + strlen(buffer), "%5u(%4lu,%4lu)",
				g_pwm_log_buffer[i].value,
				g_pwm_log_buffer[i].tsec,
				g_pwm_log_buffer[i].tusec);
		}

		if (log_type == MSG_LOG)
			PWM_MSG("%s", buffer);
		else
			PWM_NOTICE("%s", buffer);

		g_pwm_log_index = 0;

		for (i = 0; i < PWM_LOG_BUFFER_SIZE; i += 1) {
			g_pwm_log_buffer[i].count = 0;
			g_pwm_log_buffer[i].tsec = -1;
			g_pwm_log_buffer[i].tusec = -1;
			g_pwm_log_buffer[i].value = -1;
		}

		buffer[0] = '\0';
	}
}

int disp_pwm_set_backlight_cmdq(disp_pwm_id_t id, int level_1024, void *cmdq)
{
	unsigned long reg_base;
	int old_pwm;
	int index;
	int abs_diff;

	if ((DISP_PWM_ALL & id) == 0) {
		PWM_ERR("[ERROR] disp_pwm_set_backlight_cmdq: invalid PWM ID = 0x%x", id);
		return -EFAULT;
	}
	
	index = index_of_pwm(id);
	//printk(KERN_WARNING "[liuyang] disp_pwm_set_backlight_cmdq1: level = %d\n ",level_1024); //add for debug
	old_pwm = atomic_xchg(&g_pwm_backlight[index], level_1024);
	if (old_pwm != level_1024) {
		abs_diff = level_1024 - old_pwm;
		if (abs_diff < 0)
			abs_diff = -abs_diff;

		if (old_pwm == 0 || level_1024 == 0 || abs_diff > 64) {
			/* To be printed in UART log */
			PWM_NOTICE("disp_pwm_set_backlight_cmdq(id = 0x%x, level_1024 = %d), old = %d(dup=%d)",
				id, level_1024, old_pwm, g_pwm_duplicate_count);
			disp_pwm_log(level_1024, NOTICE_LOG);
		} else {
			/*
			PWM_MSG("disp_pwm_set_backlight_cmdq(id = 0x%x, level_1024 = %d), old = %d(dup=%d)",
				id, level_1024, old_pwm, g_pwm_duplicate_count);
			*/
			disp_pwm_log(level_1024,  MSG_LOG);
		}

		if (level_1024 > g_pwm_max_backlight[index])
			level_1024 = g_pwm_max_backlight[index];
		else if (level_1024 < 0)
			level_1024 = 0;

		level_1024 = disp_pwm_level_remap(id, level_1024);
		
		reg_base = pwm_get_reg_base(id);
		DISP_REG_MASK(cmdq, reg_base + DISP_PWM_CON_1_OFF, level_1024 << 16, 0x1fff << 16);
		//printk(KERN_WARNING "[liuyang] disp_pwm_set_backlight_cmdq2: level =%d\n ",level_1024);	 //add for debug
		if (level_1024 > 0)
		{	disp_pwm_set_enabled(cmdq, id, 1);
			//printk(KERN_WARNING "[liuyang] disp_pwm_set_enabled 1");
		}
		else
		{
			disp_pwm_set_enabled(cmdq, id, 0);	/* To save power */
			//printk(KERN_WARNING "[liuyang] disp_pwm_set_enabled 0");
		}
		DISP_REG_MASK(cmdq, reg_base + DISP_PWM_COMMIT_OFF, 1, ~0);
		DISP_REG_MASK(cmdq, reg_base + DISP_PWM_COMMIT_OFF, 0, ~0);

		g_pwm_duplicate_count = 0;
	} else {
		g_pwm_duplicate_count = (g_pwm_duplicate_count + 1) & 63;
		if (g_pwm_duplicate_count == 2) {
			/*
			PWM_MSG
			    ("disp_pwm_set_backlight_cmdq(id = 0x%x, level_1024 = %d), old = %d (dup)",
			     id, level_1024, old_pwm);
			*/
		}
	}

	return 0;
}

static int ddp_pwm_power_on(DISP_MODULE_ENUM module, void *handle)
{
	PWM_MSG("ddp_pwm_power_on: %d\n", module);
#ifdef ENABLE_CLK_MGR
	if (module == DISP_MODULE_PWM0) {
#ifdef CONFIG_MTK_CLKMGR /* MTK Clock Manager */
#if defined(CONFIG_ARCH_MT6752)
		enable_clock(MT_CG_DISP1_DISP_PWM_26M, "PWM");
		enable_clock(MT_CG_DISP1_DISP_PWM_MM, "PWM");
#else
		enable_clock(MT_CG_PERI_DISP_PWM, "PWM");
#endif
#else /* Common Clock Framework */
#if defined(CONFIG_ARCH_MT6755)
		ddp_clk_enable(DISP_PWM);
#else
		disp_clk_enable(DISP_PWM);
#endif
#endif
	}
#endif

#if defined(CONFIG_ARCH_MT6755)
	disp_pwm_osc_on(); /* enable OSC 26M for default clock source */
#endif

	return 0;
}

static int ddp_pwm_power_off(DISP_MODULE_ENUM module, void *handle)
{
	PWM_MSG("ddp_pwm_power_off: %d\n", module);
#ifdef ENABLE_CLK_MGR
	if (module == DISP_MODULE_PWM0) {
		atomic_set(&g_pwm_backlight[0], 0);
#ifdef CONFIG_MTK_CLKMGR /* MTK Clock Manager */
#if defined(CONFIG_ARCH_MT6752)
		disable_clock(MT_CG_DISP1_DISP_PWM_26M, "PWM");
		disable_clock(MT_CG_DISP1_DISP_PWM_MM, "PWM");
#else
		disable_clock(MT_CG_PERI_DISP_PWM, "PWM");
#endif
#else /* Common Clock Framework */
#if defined(CONFIG_ARCH_MT6755)
		ddp_clk_disable(DISP_PWM);
#else
		disp_clk_disable(DISP_PWM);
#endif
#endif
	}
#endif

#if defined(CONFIG_ARCH_MT6755)
	disp_pwm_osc_off();
#endif
	return 0;
}

static int ddp_pwm_init(DISP_MODULE_ENUM module, void *cmq_handle)
{
	ddp_pwm_power_on(module, cmq_handle);
	return 0;
}

static int ddp_pwm_set_listener(DISP_MODULE_ENUM module, ddp_module_notify notify)
{
	g_ddp_notify = notify;
	return 0;
}



DDP_MODULE_DRIVER ddp_driver_pwm = {
	.init = ddp_pwm_init,
	.config = disp_pwm_config,
	.power_on = ddp_pwm_power_on,
	.power_off = ddp_pwm_power_off,
	.set_listener = ddp_pwm_set_listener,
};


/* ---------------------------------------------------------------------- */
/* Test code                                                              */
/* Following is only for PWM functional test, not normal code             */
/* Will not be linked into user build.                                    */
/* ---------------------------------------------------------------------- */

static void disp_pwm_test_source(const char *cmd)
{
	unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);
	int sel = (cmd[0] - '0') & 0x3;

	DISP_REG_MASK(NULL, reg_base + DISP_PWM_CON_0_OFF, (sel << 4), (0x3 << 4));
}


static void disp_pwm_test_grad(const char *cmd)
{
	const unsigned long reg_grad = pwm_get_reg_base(DISP_PWM0) + 0x18;

	switch (cmd[0]) {
	case 'H':
		DISP_REG_SET(NULL, reg_grad, (1 << 16) | (1 << 8) | 1);
		disp_pwm_set_backlight(DISP_PWM0, 1023);
		break;

	case 'L':
		DISP_REG_SET(NULL, reg_grad, (1 << 16) | (1 << 8) | 1);
		disp_pwm_set_backlight(DISP_PWM0, 40);
		break;

	default:
		DISP_REG_SET(NULL, reg_grad, 0);
		disp_pwm_set_backlight(DISP_PWM0, 512);
		break;
	}
}


static void disp_pwm_test_div(const char *cmd)
{
	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);

	int div = cmd[0] - '0';

	if (div > 5)
		div = 5;

	DISP_REG_MASK(NULL, reg_base + DISP_PWM_CON_0_OFF, div << 16, (0x3ff << 16));
	disp_pwm_set_backlight(DISP_PWM0, 256 + div);	/* to be applied */
}


static void disp_pwm_enable_debug(const char *cmd)
{
	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);

	if (cmd[0] == '1')
		DISP_REG_SET(NULL, reg_base + 0x20, 3);
	else
		DISP_REG_SET(NULL, reg_base + 0x20, 0);
}

static void disp_pwm_test_pin_mux(void)
{
	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM1);
	/* set gpio function for dvt test */
	mt_set_gpio_mode(GPIO157, GPIO_MODE_01);  /* For DVT PIN MUX verification only, not normal path */
	mt_set_gpio_dir(GPIO157, GPIO_DIR_OUT);   /* For DVT PIN MUX verification only, not normal path */
#if 0
	mt_set_gpio_mode(GPIO4, GPIO_MODE_01);
	mt_set_gpio_dir(GPIO4, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO14, GPIO_MODE_03);
	mt_set_gpio_dir(GPIO14, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO127, GPIO_MODE_02);
	mt_set_gpio_dir(GPIO127, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO134, GPIO_MODE_02);
	mt_set_gpio_dir(GPIO134, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO153, GPIO_MODE_05);
	mt_set_gpio_dir(GPIO153, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO186, GPIO_MODE_02);
	mt_set_gpio_dir(GPIO186, GPIO_DIR_OUT);
#endif
	DISP_REG_MASK(NULL, reg_base + DISP_PWM_CON_1_OFF, 512 << 16, 0x1fff << 16);
	DISP_REG_MASK(NULL, reg_base + DISP_PWM_EN_OFF, 0x1, 0x1);
	DISP_REG_SET(NULL, reg_base + 0x20, 3);
	DISP_REG_MASK(NULL, reg_base + DISP_PWM_COMMIT_OFF, 1, ~0);
	DISP_REG_MASK(NULL, reg_base + DISP_PWM_COMMIT_OFF, 0, ~0);
}

static int pwm_simple_strtoul(char *ptr, unsigned long *res)
{
	int i;
	char buffer[20];
	int end = 0;
	int ret = 0;

	for (i = 0; i < 20; i += 1) {
		end = i;
		PWM_MSG("%c\n", ptr[i]);
		if (ptr[i] < '0' || ptr[i] > '9')
			break;
	}

	if (end > 0) {
		strncpy(buffer, ptr, end);
		buffer[end] = '\0';
		ret = kstrtoul(buffer, 0, res);

	}
	return end;
}

static int pwm_parse_triple(const char *cmd, unsigned long *offset, unsigned long *value, unsigned long *mask)
{
	int count = 0;
	char *next = (char *)cmd;
	int end;

	*value = 0;
	*mask = 0;
	end = pwm_simple_strtoul(next, offset);
	next += end;
	if (*offset > 0x1000UL || (*offset & 0x3UL) != 0)  {
		*offset = 0UL;
		return 0;
	}

	count++;

	if (*next == ',')
		next++;

	end = pwm_simple_strtoul(next, value);
	next += end;
	count++;

	if (*next == ',')
		next++;

	end = pwm_simple_strtoul(next, mask);
	next += end;
	count++;

	return count;
}

static void disp_pwm_dump(void)
{
	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);
	int offset;

	PWM_NOTICE("[DUMP] Base = 0x%lx", reg_base);
	for (offset = 0; offset <= 0x28; offset += 4) {
		unsigned int val = DISP_REG_GET(reg_base + offset);

		PWM_NOTICE("[DUMP] [+0x%02x] = 0x%08x", offset, val);
	}
}


void disp_pwm_test(const char *cmd, char *debug_output)
{
	unsigned long offset, value, mask;
	unsigned int regsrc;
	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);

	debug_output[0] = '\0';
	PWM_NOTICE("disp_pwm_test(%s)", cmd);

	if (strncmp(cmd, "src:", 4) == 0) {
		disp_pwm_test_source(cmd + 4);
	} else if (strncmp(cmd, "div:", 4) == 0) {
		disp_pwm_test_div(cmd + 4);
	} else if (strncmp(cmd, "grad:", 5) == 0) {
		disp_pwm_test_grad(cmd + 5);
	} else if (strncmp(cmd, "dbg:", 4) == 0) {
		disp_pwm_enable_debug(cmd + 4);
	} else if (strncmp(cmd, "set:", 4) == 0) {
		int count = pwm_parse_triple(cmd + 4, &offset, &value, &mask);
		if (count == 3) {
			DISP_REG_MASK(NULL, reg_base + offset, value, mask);
		} else if (count == 2) {
			DISP_REG_SET(NULL, reg_base + offset, value);
			mask = 0xffffffff;
		}
		if (count >= 2)
			PWM_MSG("[+0x%03lx] = 0x%08lx(%lu) & 0x%08lx", offset, value, value, mask);

	} else if (strncmp(cmd, "dump", 4) == 0) {
		disp_pwm_dump();

	} else if (strncmp(cmd, "pinmux", 6) == 0) {
		disp_pwm_test_pin_mux();

	} else if (strncmp(cmd, "pwmmux:", 7) == 0) {
		unsigned int clksrc = cmd[7] - '0';
		disp_pwm_osc_on();
		disp_pwm_set_pwmmux(clksrc);

	} else if (strncmp(cmd, "clkinfo", 7) == 0) {
		disp_pwm_get_muxbase();
		disp_pwm_get_oscbase();
		regsrc = disp_pwm_get_pwmmux();
		PWM_MSG("clock source=%d, osc status=%d", regsrc, disp_pwm_check_osc_status());
	}
}

