#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <linux/earlysuspend.h>
#include <linux/io.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_typedefs.h>
#include <mach/sync_write.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_dcm.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_freqhopping.h>
#include <mach/mt_gpufreq.h>
#include <mach/irqs.h>

//#include <mach/pmic_mt6325_sw.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

//#ifdef CONFIG_MD32_SUPPORT
#include <mach/md32_ipi.h>
#include <mach/md32_helper.h>
//#endif

#ifdef MTK_KERNEL_POWER_OFF_CHARGING
#include <mach/mt_boot_common.h>
#endif

#ifdef CONFIG_OF
void __iomem  *clk_apmixed_base;
void __iomem  *clk_cksys_base;
void __iomem  *clk_infracfg_ao_base;
void __iomem  *clk_audio_base;
void __iomem  *clk_mfgcfg_base;
void __iomem  *clk_mmsys_config_base;
void __iomem  *clk_imgsys_base;
void __iomem  *clk_vdec_gcon_base;
void __iomem  *clk_mjc_config_base;
void __iomem  *clk_venc_gcon_base;
void __iomem  *clk_gpio_base;
#endif

#define Bring_Up

/************************************************
 **********         log debug          **********
 ************************************************/
#define TAG     "[Power/clkmgr] "

#define clk_err(fmt, args...)       \
    pr_err(TAG fmt, ##args)
#define clk_warn(fmt, args...)      \
    pr_warn(TAG fmt, ##args)
#define clk_info(fmt, args...)      \
    pr_info(TAG fmt, ##args)
#define clk_dbg(fmt, args...)       \
    pr_info(TAG fmt, ##args)

/************************************************
 **********      register access       **********
 ************************************************/

#define clk_readl(addr) \
    DRV_Reg32(addr)

#define clk_writel(addr, val)   \
    mt_reg_sync_writel(val, addr)

#define clk_setl(addr, val) \
    mt_reg_sync_writel(clk_readl(addr) | (val), addr)

#define clk_clrl(addr, val) \
    mt_reg_sync_writel(clk_readl(addr) & ~(val), addr)

/************************************************
 **********      struct definition     **********
 ************************************************/
struct pll {
    const char *name;
    int type;
    int mode;
    int feat;
    int state;
    unsigned int cnt;
    unsigned int en_mask;
    void __iomem *base_addr;
    void __iomem *pwr_addr;
    unsigned int hp_id;
    int hp_switch;
};

struct subsys {
    const char *name;
    int type;
    int force_on;
    unsigned int cnt;
    unsigned int state;
    unsigned int default_sta;
    unsigned int sta_mask;  // mask in PWR_STATUS
    void __iomem *ctl_addr;
};

/************************************************
 **********      global variablies     **********
 ************************************************/

#define PWR_DOWN    0
#define PWR_ON      1

static int initialized = 0;
static int slp_chk_mtcmos_pll_stat = 0;

static struct pll plls[NR_PLLS];
static struct subsys syss[NR_SYSS];

/************************************************
 **********      spin lock protect     **********
 ************************************************/

static DEFINE_SPINLOCK(clock_lock);

#define clkmgr_lock(flags)  \
do {    \
    spin_lock_irqsave(&clock_lock, flags);  \
} while (0)

#define clkmgr_unlock(flags)  \
do {    \
    spin_unlock_irqrestore(&clock_lock, flags);  \
} while (0)

#define clkmgr_locked()  (spin_is_locked(&clock_lock))

int clkmgr_is_locked()
{
    return clkmgr_locked();
}
EXPORT_SYMBOL(clkmgr_is_locked);

/************************************************
 **********          pll part          **********
 ************************************************/

#define PLL_TYPE_SDM    0
#define PLL_TYPE_LC     1

#define HAVE_RST_BAR    (0x1 << 0)
#define HAVE_PLL_HP     (0x1 << 1)
#define HAVE_FIX_FRQ    (0x1 << 2) 
#define Others          (0x1 << 3)

#define RST_BAR_MASK    0x1000000

static struct pll plls[NR_PLLS] = {
    {
        .name = __stringify(ARMCA7PLL),
        .en_mask = 0x00000001,
    }, {
        .name = __stringify(MAINPLL),
        .en_mask = 0xF0000101,
    }, {
        .name = __stringify(MSDCPLL),
        .en_mask = 0x00000001,
    }, {
        .name = __stringify(UNIVPLL),
        .en_mask = 0xFE000001,
    }, {
        .name = __stringify(MMPLL),
        .en_mask = 0x00000001,
    }, {
        .name = __stringify(VENCPLL),
        .en_mask = 0x00000001,
    }, {
        .name = __stringify(TVDPLL),
        .type = PLL_TYPE_SDM,
        .en_mask = 0x00000001,
    }, {
        .name = __stringify(MPLL),
        .en_mask = 0x00000001,
    }, {
        .name = __stringify(APLL1),
        .en_mask = 0x00000001,
    }, {
        .name = __stringify(APLL2),
        .en_mask = 0x00000001,
    }
};

static struct pll *id_to_pll(unsigned int id)
{
    return id < NR_PLLS ? plls + id : NULL;
}

#define PLL_PWR_ON  (0x1 << 0)
#define PLL_ISO_EN  (0x1 << 1)

#define SDM_PLL_N_INFO_MASK 0x001FFFFF
#define UNIV_SDM_PLL_N_INFO_MASK 0x001fc000
#define SDM_PLL_N_INFO_CHG  0x80000000
#define ARMPLL_POSDIV_MASK  0x07000000

static int pll_get_state_op(struct pll *pll)
{
    return clk_readl(pll->base_addr) & 0x1;
}

int pll_is_on(int id)
{
	int state;
	unsigned long flags;
	struct pll *pll = id_to_pll(id);
	BUG_ON(!pll);

	clkmgr_lock(flags);
	state = pll_get_state_op(pll);
	clkmgr_unlock(flags);

	return state;
}
EXPORT_SYMBOL(pll_is_on);

void enable_armpll_l(void)
{
	clk_writel(ARMCA15PLL_PWR_CON0, (clk_readl(ARMCA15PLL_PWR_CON0) | 0x01));
	udelay(2);
	clk_writel(ARMCA15PLL_PWR_CON0, (clk_readl(ARMCA15PLL_PWR_CON0) & 0xfffffffd));
	clk_writel(ARMCA15PLL_CON1, (clk_readl(ARMCA15PLL_CON1) | 0x80000000));
	clk_writel(ARMCA15PLL_CON0, (clk_readl(ARMCA15PLL_CON0) | 0x01));
	udelay(100);
}
EXPORT_SYMBOL(enable_armpll_l);
void disable_armpll_l(void)
{
	clk_writel(ARMCA15PLL_CON0, (clk_readl(ARMCA15PLL_CON0) & 0xfffffffe));
	clk_writel(ARMCA15PLL_PWR_CON0, (clk_readl(ARMCA15PLL_PWR_CON0) | 0x00000002));
	clk_writel(ARMCA15PLL_PWR_CON0, (clk_readl(ARMCA15PLL_PWR_CON0) & 0xfffffffe));
}
EXPORT_SYMBOL(disable_armpll_l);
void switch_armpll_l_hwmode(int enable)
{
	/* ARM CA15*/
	if (enable == 1) {
		clk_writel(AP_PLL_CON3, (clk_readl(AP_PLL_CON3) & 0xff87ffff));
		clk_writel(AP_PLL_CON4, (clk_readl(AP_PLL_CON4) & 0xffffcfff));
	} else{
		clk_writel(AP_PLL_CON3, (clk_readl(AP_PLL_CON3) | 0x00780000));
		clk_writel(AP_PLL_CON4, (clk_readl(AP_PLL_CON4) | 0x00003000));
	}
}
EXPORT_SYMBOL(switch_armpll_l_hwmode);

void switch_armpll_ll_hwmode(int enable)
{
	/* ARM CA7*/
	if (enable == 1) {
		clk_writel(AP_PLL_CON3, (clk_readl(AP_PLL_CON3) & 0xfffeeeef));
		clk_writel(AP_PLL_CON4, (clk_readl(AP_PLL_CON4) & 0xfffffefe));
	} else {
		clk_writel(AP_PLL_CON3, (clk_readl(AP_PLL_CON3) | 0x00011110));
		clk_writel(AP_PLL_CON4, (clk_readl(AP_PLL_CON4) | 0x00000101));
	}
}
EXPORT_SYMBOL(switch_armpll_ll_hwmode);

#if 1
/*int clk_monitor(enum ckmon_sel ckmon, enum monitor_clk_sel sel, int div)*/
/* clk_monitor(100, 1, 1);*/
/*
1: 26M
2: 32K
*/
int clk_monitor(int gpio_idx, int ckmon_value, int div)
{
	unsigned long flags;
	unsigned int ckmon = 0;
	/*unsigned int gpio_offs = 0;*/
	unsigned int gpio_shift = 0;/* 3 bits */
	unsigned int gpio_value = 0;

	unsigned int ckmon_shift = 0;/* 4 bits */
	unsigned int div_shift = 0; /* 8 bits */
	int div_value = 0;
	unsigned int temp;

	if ((div > 255) || (div < 1)) {
		clk_info("CLK_Monitor error parameter\n");
		return 1;
	}
	clkmgr_lock(flags);
	div_value = div - 1;


	if (gpio_idx == 100) {
		ckmon = 3;
		gpio_shift = 0;
		gpio_value = 4;
		ckmon_shift = 24;
		ckmon_value = 1; /*26M*/
		div_shift = 24;

		temp = clk_readl(CLK_GPIO_10);
		temp = temp & (~(0x7<<gpio_shift));
		temp = temp | ((gpio_value&0x7)<<gpio_shift);
		clk_writel(CLK_GPIO_10, temp);

		temp = clk_readl(CLKMON_CLK_SEL_REG);
		temp = temp & (~(0xf<<ckmon_shift));
		temp = temp | ((ckmon_value&0xf)<<ckmon_shift);
		clk_writel(CLKMON_CLK_SEL_REG, temp);

		temp = clk_readl(CLKMON_K1_REG);
		temp = temp & (~(0xff<<div_shift));
		temp = temp | ((div_value&0xff)<<div_shift);
		clk_writel(CLKMON_K1_REG, temp);
	}
	clk_info("CLK_Monitor Reg: CLK_GPIO_10=0x%x, CLKMON_CLK_SEL_REG=0x%x, CLKMON_K1_REG=0x%x\n",
		clk_readl(CLK_GPIO_10), clk_readl(CLKMON_CLK_SEL_REG), clk_readl(CLKMON_K1_REG));
	clkmgr_unlock(flags);
	return 0;

}
EXPORT_SYMBOL(clk_monitor);
#endif
int enable_pll(int id, char *name)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(enable_pll);

int disable_pll(int id, char *name)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(disable_pll);

int pll_fsel(int id, unsigned int value)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(pll_fsel);

int pll_hp_switch_on(int id, int hp_on)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(pll_hp_switch_on);

int pll_hp_switch_off(int id, int hp_off)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(pll_hp_switch_off);

int pll_dump_regs(int id, unsigned int *ptr)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(pll_dump_regs);

void set_mipi26m(int en)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return;
}
EXPORT_SYMBOL(set_mipi26m);

void set_ada_ssusb_xtal_ck(int en)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return;
}
EXPORT_SYMBOL(set_ada_ssusb_xtal_ck);


/************************************************
 **********         subsys part        **********
 ************************************************/

#define SYS_TYPE_MODEM    0 
#define SYS_TYPE_MEDIA    1
#define SYS_TYPE_OTHER    2
#define SYS_TYPE_CONN     3

static struct subsys syss[NR_SYSS] = {
    {
        .name = __stringify(SYS_MD1),
        .sta_mask = 1U << 0,
    },  {
        .name = __stringify(SYS_MD2),
	.sta_mask = 1U << 27,
    },  {
        .name = __stringify(SYS_CONN),
        .sta_mask = 1U << 1,
    }, {
        .name = __stringify(SYS_DIS),
        .sta_mask = 1U << 3,
    }, {
        .name = __stringify(SYS_MFG),
        .sta_mask = 1U << 4,
    }, {
        .name = __stringify(SYS_ISP),
        .sta_mask = 1U << 5,
    }, {
        .name = __stringify(SYS_VDE),
        .sta_mask = 1U << 7,
    }, {
        .name = __stringify(SYS_MJC),
        .sta_mask = 1U << 20,
    }, {
    	.name = __stringify(SYS_VEN),
        .sta_mask = 1U << 21,
    }, {
        .name = __stringify(SYS_AUD),
        .sta_mask = 1U << 24,
	},
	{
		.name = __stringify(SYS_MFG2),
		.sta_mask = 1U << 23,
	},
	{
		.name = __stringify(SYS_C2K),
		.sta_mask = 1U << 28,
	}
};

static struct subsys *id_to_sys(unsigned int id)
{
    return id < NR_SYSS ? syss + id : NULL;
}

static int sys_get_state_op(struct subsys *sys)
{
    unsigned int sta = clk_readl(PWR_STATUS);
    unsigned int sta_s = clk_readl(PWR_STATUS_2ND);

    return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
}

int subsys_is_on(int id)
{
	int state;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	BUG_ON(!sys);

	clkmgr_lock(flags);
	state = sys_get_state_op(sys);
	clkmgr_unlock(flags);

	return state;
}
EXPORT_SYMBOL(subsys_is_on);

//#define STATE_CHECK_DEBUG

int enable_subsys(int id, char *name)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(enable_subsys);

int disable_subsys(int id, char *name)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(disable_subsys);

int subsys_dump_regs(int id, unsigned int *ptr)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(subsys_dump_regs);

int md_power_on(int id)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(md_power_on);

int md_power_off(int id, unsigned int timeout)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(md_power_off);

int conn_power_on(void)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(conn_power_on);

int conn_power_off(void)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(conn_power_off);
/************************************************
 **********         cg_clk part        **********
 ************************************************/


int mt_enable_clock(enum cg_clk_id id, char *name)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(mt_enable_clock);

int mt_disable_clock(enum cg_clk_id id, char *name)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 0;
}
EXPORT_SYMBOL(mt_disable_clock);

int clock_is_on(int id)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
	return 1;
}
EXPORT_SYMBOL(clock_is_on);

/************************************************
 **********       initialization       **********
 ************************************************/

#define INFRA0_CG  0xFFFFFFFF/*0: Disable  ( with clock), 1: Enable ( without clock )*/
#define INFRA1_CG  0xFFFFFFFF/*0: Disable  ( with clock), 1: Enable ( without clock )*/
#define INFRA2_CG  0xFFFFFFFF/*0: Disable  ( with clock), 1: Enable ( without clock )*/
#define AUD_CG    0x0F0C0344/*same as denali*/
#define MFG_CG    0x00000001/*set*/
#define DISP0_CG  0xFFFFFFFF
#define DISP1_CG  0x0000003F
#define IMG_CG    0x00000FE1/*no bit10	SUFOD_CKPDN*/
#define VDEC_CG   0x00000001/*set*/
#define LARB_CG   0x00000001
#define VENC_CG   0x00001111/*set*/

static void cg_bootup_pdn(void)
{
    //AUDIO
    clk_writel(AUDIO_TOP_CON0, AUD_CG);

    //INFRA CG
		clk_writel(INFRA_PDN_SET0, 0x832ff910);
		clk_writel(INFRA_PDN_SET1, 0x6f0802);
	clk_writel(INFRA_PDN_SET2, 0xc1);
     
    //MFG
    clk_writel(MFG_CG_SET, MFG_CG);

    //DISP
		clk_writel(DISP_CG_SET0, 0xfffffffc); /*DCM enable*/
		/*clk_writel(DISP_CG_SET1, 0x0000003F);*/

    //ISP
    clk_writel(IMG_CG_SET, IMG_CG);

    //VDE
    clk_writel(VDEC_CKEN_CLR, VDEC_CG);
    clk_writel(LARB_CKEN_CLR, LARB_CG);
    
    //VENC
    clk_clrl(VENC_CG_CON, VENC_CG);
    
    //MJC
    /*clk_writel(MJC_CG_SET, MJC_CG);*/
/*msdcpll*/
		clk_clrl(MSDCPLL_CON0, 0x1);
		clk_setl(MSDCPLL_PWR_CON0, PLL_ISO_EN);
		clk_clrl(MSDCPLL_PWR_CON0, PLL_PWR_ON);

/*tvdpll*/
		clk_clrl(TVDPLL_CON0, 0x1);
		clk_setl(TVDPLL_PWR_CON0, PLL_ISO_EN);
		clk_clrl(TVDPLL_PWR_CON0, PLL_PWR_ON);

/*apll1*/
		clk_clrl(APLL1_CON0, 0x1);
		clk_setl(APLL1_PWR_CON0, PLL_ISO_EN);
		clk_clrl(APLL1_PWR_CON0, PLL_PWR_ON);
/*apll2*/
		clk_clrl(APLL2_CON0, 0x1);
		clk_setl(APLL2_PWR_CON0, PLL_ISO_EN);
		clk_clrl(APLL2_PWR_CON0, PLL_PWR_ON);
}

#ifdef MTK_KERNEL_POWER_OFF_CHARGING
extern BOOTMODE g_boot_mode;
#endif

static void mt_subsys_init(void)
{
	syss[SYS_MD1].ctl_addr = MD1_PWR_CON;
	syss[SYS_MD2].ctl_addr = MD2_PWR_CON;
	syss[SYS_CONN].ctl_addr = CONN_PWR_CON;
	syss[SYS_DIS].ctl_addr = DIS_PWR_CON;
	syss[SYS_MFG].ctl_addr = MFG_PWR_CON;
	syss[SYS_ISP].ctl_addr = ISP_PWR_CON;
	syss[SYS_VDE].ctl_addr = VDE_PWR_CON;
	syss[SYS_MJC].ctl_addr = MJC_PWR_CON;
	syss[SYS_VEN].ctl_addr = VEN_PWR_CON;
	syss[SYS_AUD].ctl_addr = AUDIO_PWR_CON;
	syss[SYS_MFG2].ctl_addr = MFG_ASYNC_PWR_CON;
	syss[SYS_C2K].ctl_addr = C2K_PWR_CON;
}

static void mt_plls_init(void)
{
	plls[ARMCA7PLL].base_addr = ARMCA7PLL_CON0;
	plls[ARMCA7PLL].pwr_addr = ARMCA7PLL_PWR_CON0;
	plls[MAINPLL].base_addr = MAINPLL_CON0;
	plls[MAINPLL].pwr_addr = MAINPLL_PWR_CON0;
	plls[MSDCPLL].base_addr = MSDCPLL_CON0;
	plls[MSDCPLL].pwr_addr = MSDCPLL_PWR_CON0;
	plls[UNIVPLL].base_addr = UNIVPLL_CON0;
	plls[UNIVPLL].pwr_addr = UNIVPLL_PWR_CON0;
	plls[MMPLL].base_addr = MMPLL_CON0;
	plls[MMPLL].pwr_addr = MMPLL_PWR_CON0;
	plls[VENCPLL].base_addr = VENCPLL_CON0;
	plls[VENCPLL].pwr_addr = VENCPLL_PWR_CON0;
	plls[TVDPLL].base_addr = TVDPLL_CON0;
	plls[TVDPLL].pwr_addr = TVDPLL_PWR_CON0;
	plls[MPLL].base_addr = MPLL_CON0;
	plls[MPLL].pwr_addr = MPLL_PWR_CON0;
	plls[APLL1].base_addr = APLL1_CON0;
	plls[APLL1].pwr_addr = APLL1_PWR_CON0;
	plls[APLL2].base_addr = APLL2_CON0;
	plls[APLL2].pwr_addr = APLL2_PWR_CON0;
}

void clk_dump(void)
{
	pr_err("********** PLL register dump *********\n");
	pr_err("[ARMCA15PLL_CON0]=0x%08x\n", clk_readl(ARMCA15PLL_CON0));
	pr_err("[ARMCA15PLL_CON1]=0x%08x\n", clk_readl(ARMCA15PLL_CON1));
	pr_err("[ARMCA15PLL_PWR_CON0]=0x%08x\n", clk_readl(ARMCA15PLL_PWR_CON0));

	pr_err("[ARMCA7PLL_CON0]=0x%08x\n", clk_readl(ARMCA7PLL_CON0));
	pr_err("[ARMCA7PLL_CON1]=0x%08x\n", clk_readl(ARMCA7PLL_CON1));
	pr_err("[ARMCA7PLL_PWR_CON0]=0x%08x\n", clk_readl(ARMCA7PLL_PWR_CON0));

	pr_err("[MAINPLL_CON0]=0x%08x\n", clk_readl(MAINPLL_CON0));
	pr_err("[MAINPLL_CON1]=0x%08x\n", clk_readl(MAINPLL_CON1));
	pr_err("[MAINPLL_PWR_CON0]=0x%08x\n", clk_readl(MAINPLL_PWR_CON0));

	pr_err("[UNIVPLL_CON0]=0x%08x\n", clk_readl(UNIVPLL_CON0));
	pr_err("[UNIVPLL_CON1]=0x%08x\n", clk_readl(UNIVPLL_CON1));
	pr_err("[UNIVPLL_PWR_CON0]=0x%08x\n", clk_readl(UNIVPLL_PWR_CON0));

	pr_err("[MMPLL_CON0]=0x%08x\n", clk_readl(MMPLL_CON0));
	pr_err("[MMPLL_CON1]=0x%08x\n", clk_readl(MMPLL_CON1));
	pr_err("[MMPLL_PWR_CON0]=0x%08x\n", clk_readl(MMPLL_PWR_CON0));

	pr_err("[MSDCPLL_CON0]=0x%08x\n", clk_readl(MSDCPLL_CON0));
	pr_err("[MSDCPLL_CON1]=0x%08x\n", clk_readl(MSDCPLL_CON1));
	pr_err("[MSDCPLL_PWR_CON0]=0x%08x\n", clk_readl(MSDCPLL_PWR_CON0));

	pr_err("[VENCPLL_CON0]=0x%08x\n", clk_readl(VENCPLL_CON0));
	pr_err("[VENCPLL_CON1]=0x%08x\n", clk_readl(VENCPLL_CON1));
	pr_err("[VENCPLL_PWR_CON0]=0x%08x\n", clk_readl(VENCPLL_PWR_CON0));

	pr_err("[TVDPLL_CON0]=0x%08x\n", clk_readl(TVDPLL_CON0));
	pr_err("[TVDPLL_CON1]=0x%08x\n", clk_readl(TVDPLL_CON1));
	pr_err("[TVDPLL_PWR_CON0]=0x%08x\n", clk_readl(TVDPLL_PWR_CON0));

	pr_err("[APLL1_CON0]=0x%08x\n", clk_readl(APLL1_CON0));
	pr_err("[APLL1_CON1]=0x%08x\n", clk_readl(APLL1_CON1));
	pr_err("[APLL1_PWR_CON0]=0x%08x\n", clk_readl(APLL1_PWR_CON0));

	pr_err("[APLL2_CON0]=0x%08x\n", clk_readl(APLL2_CON0));
	pr_err("[APLL2_CON1]=0x%08x\n", clk_readl(APLL2_CON1));
	pr_err("[APLL2_PWR_CON0]=0x%08x\n", clk_readl(APLL2_PWR_CON0));

	pr_err("[AP_PLL_CON3]=0x%08x\n", clk_readl(AP_PLL_CON3));
	pr_err("[AP_PLL_CON4]=0x%08x\n", clk_readl(AP_PLL_CON4));
	pr_err("[AP_PLL_CON6]=0x%08x\n", clk_readl(AP_PLL_CON6));

	pr_err("********** subsys pwr sts register dump *********\n");
	pr_err("PWR_STATUS=0x%08x, PWR_STATUS_2ND=0x%08x\n",
		clk_readl(PWR_STATUS), clk_readl(PWR_STATUS_2ND));

	pr_err("********** mux register dump *********\n");
	pr_err("[TOP_CKMUXSEL]=0x%08x\n", clk_readl(TOP_CKMUXSEL));
	pr_err("[TOP_CKDIV1]=0x%08x\n", clk_readl(TOP_CKDIV1));
	pr_err("[INFRA_TOPCKGEN_CKDIV1_BIG]=0x%08x\n", clk_readl(INFRA_TOPCKGEN_CKDIV1_BIG));
	pr_err("[INFRA_TOPCKGEN_CKDIV1_SML]=0x%08x\n", clk_readl(INFRA_TOPCKGEN_CKDIV1_SML));
	pr_err("[INFRA_TOPCKGEN_CKDIV1_BUS]=0x%08x\n", clk_readl(INFRA_TOPCKGEN_CKDIV1_BUS));

	pr_err("[CLK_CFG_0]=0x%08x\n", clk_readl(CLK_CFG_0));
	pr_err("[CLK_CFG_1]=0x%08x\n", clk_readl(CLK_CFG_1));
	pr_err("[CLK_CFG_2]=0x%08x\n", clk_readl(CLK_CFG_2));
	pr_err("[CLK_CFG_3]=0x%08x\n", clk_readl(CLK_CFG_3));
	pr_err("[CLK_CFG_4]=0x%08x\n", clk_readl(CLK_CFG_4));
	pr_err("[CLK_CFG_5]=0x%08x\n", clk_readl(CLK_CFG_5));
	pr_err("[CLK_CFG_6]=0x%08x\n", clk_readl(CLK_CFG_6));
	pr_err("[CLK_CFG_7]=0x%08x\n", clk_readl(CLK_CFG_7));
	pr_err("[CLK_CFG_8]=0x%08x\n", clk_readl(CLK_CFG_8));

	pr_err("********** CG register dump *********\n");
	pr_err("[INFRA_SUBSYS][%p]\n", clk_infracfg_ao_base);	/* 0x10001000 */

	pr_err("[INFRA_PDN_STA0]=0x%08x\n", clk_readl(INFRA_PDN_STA0));
	pr_err("[INFRA_PDN_STA1]=0x%08x\n", clk_readl(INFRA_PDN_STA1));
	pr_err("[INFRA_PDN_STA2]=0x%08x\n", clk_readl(INFRA_PDN_STA2));

	pr_err("[INFRA_TOPAXI_PROTECTSTA1]=0x%08x\n", clk_readl(INFRA_TOPAXI_PROTECTSTA1));
	pr_err("[INFRA_TOPAXI_PROTECTSTA1_1]=0x%08x\n", clk_readl(INFRA_TOPAXI_PROTECTSTA1_1));

	pr_err("[MMSYS_SUBSYS][%p]\n", clk_mmsys_config_base);	/* 0x14000000 */
	pr_err("[DISP_CG_CON0]=0x%08x\n", clk_readl(DISP_CG_CON0));
	pr_err("[DISP_CG_CON1]=0x%08x\n", clk_readl(DISP_CG_CON1));
	pr_err("[DISP_CG_CLR0]=0x%08x\n", clk_readl(DISP_CG_CLR0));
	pr_err("[DISP_CG_CLR1]=0x%08x\n", clk_readl(DISP_CG_CLR1));

	pr_err("[MFG_SUBSYS][%p]\n", clk_mfgcfg_base);	/* 0x13000000 */
	pr_err("[MFG_CG_CON]=0x%08x\n", clk_readl(MFG_CG_CON));

	pr_err("[AUDIO_SUBSYS][%p]\n", clk_audio_base);	/* 0x1122_0000 */
	pr_err("[AUDIO_TOP_CON0]=0x%08x\n", clk_readl(AUDIO_TOP_CON0));
	pr_err("[AUDIO_TOP_CON1]=0x%08x\n", clk_readl(AUDIO_TOP_CON0+0x5a0));

	pr_err("[IMG_SUBSYS][%p]\n", clk_imgsys_base);	/* 0x15000000 */
	pr_err("[IMG_CG_CON]=0x%08x\n", clk_readl(IMG_CG_CON));

	pr_err("[VDEC_SUBSYS][%p]\n", clk_vdec_gcon_base);	/* 0x16000000 */
	pr_err("[VDEC_CKEN_SET]=0x%08x\n", clk_readl(VDEC_CKEN_SET));
	pr_err("[LARB_CKEN_SET]=0x%08x\n", clk_readl(LARB_CKEN_SET));

	pr_err("[VENC_SUBSYS][%p]\n", clk_venc_gcon_base);	/* 0x17000000 */
	pr_err("[VENC_CG_CON]=0x%08x\n", clk_readl(VENC_CG_CON));
}
void check_mfg(void)
{
	unsigned int rdata = 0;
	pwrap_read(0x200, &rdata);
	pr_err("there is no VGPU, CID = 0x%x\n", rdata);
  pwrap_read(0x618, &rdata);
	pr_err("there is no VGPU, VGPU = 0x%x\n", rdata);
}
void iomap(void);
int mt_clkmgr_init(void)
{
    iomap();
    BUG_ON(initialized);

/*NEED TO COMFIRM MTCMOS API*/
    spm_mtcmos_ctrl_vdec(STA_POWER_ON);
    spm_mtcmos_ctrl_venc(STA_POWER_ON);
    spm_mtcmos_ctrl_isp(STA_POWER_ON);
    spm_mtcmos_ctrl_aud(STA_POWER_ON);
    spm_mtcmos_ctrl_mfg_ASYNC(STA_POWER_ON);
    spm_mtcmos_ctrl_mfg(STA_POWER_ON);
	pr_alert("MFG_ASYNC_PWR_CON = %08x\r\n", clk_readl(MFG_ASYNC_PWR_CON));
	pr_alert("MFG_PWR_CON  = %08x\r\n", clk_readl(MFG_PWR_CON));
#ifdef Bring_Up
	clk_dump();
#endif
	cg_bootup_pdn();
	mt_freqhopping_init();
	mt_plls_init();
	mt_subsys_init();
	return 0;
}

#ifdef CONFIG_MTK_MMC
extern void msdc_clk_status(int * status);
#else
void msdc_clk_status(int * status) { *status = 0; }
#endif

void clk_misc_cfg_ops(bool flag)
{
	pr_err("[%s]: warning! warning! warning! to use this function\n", __func__);
}
EXPORT_SYMBOL(clk_misc_cfg_ops);
/************************************************
 **********       function debug       **********
 ************************************************/

static int armbpll_fsel_read(struct seq_file *m, void *v)
{
		return 0;
}

static int armspll_fsel_read(struct seq_file *m, void *v)
{
		return 0;
}

static int armbpll_fsel_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
		char desc[32];
		int len = 0;
		unsigned int ctrl_value = 0;

		int div;
		unsigned int value;

		len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
		if (copy_from_user(desc, buffer, len))
			return 0;
		desc[len] = '\0';

		if (sscanf(desc, "%d %x", &div, &value) == 2) {
			ctrl_value = clk_readl(ARMCA15PLL_CON0 + 4);
			ctrl_value &= ~(SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
			ctrl_value |= value & (SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
			ctrl_value |= SDM_PLL_N_INFO_CHG;
			clk_writel(ARMCA15PLL_CON0 + 4, ctrl_value);
			udelay(20);
			if (div == 4)/*only do div4, others div1*/
				clk_writel(INFRA_TOPCKGEN_CKDIV1_BIG,
					(clk_readl(INFRA_TOPCKGEN_CKDIV1_BIG) & 0xFFFFFFE0) | 0xB);
			else
				clk_writel(INFRA_TOPCKGEN_CKDIV1_BIG,
					(clk_readl(INFRA_TOPCKGEN_CKDIV1_BIG) & 0xFFFFFFE0));
		}
		return count;
}

static int armspll_fsel_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
		char desc[32];
		int len = 0;
		unsigned int ctrl_value = 0;
		int div;
		unsigned int value;

		len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
		if (copy_from_user(desc, buffer, len))
			return 0;
		desc[len] = '\0';

		if (sscanf(desc, "%d %x", &div, &value) == 2) {
			ctrl_value = clk_readl(ARMCA7PLL_CON0 + 4);
			ctrl_value &= ~(SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
			ctrl_value |= value & (SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
			ctrl_value |= SDM_PLL_N_INFO_CHG;
			clk_writel(ARMCA7PLL_CON0 + 4, ctrl_value);
			udelay(20);
			if (div == 6)/*only do div6, others div1*/
				clk_writel(INFRA_TOPCKGEN_CKDIV1_SML,
					(clk_readl(INFRA_TOPCKGEN_CKDIV1_SML) & 0xFFFFFFE0) | 0x1D);
			else
				clk_writel(INFRA_TOPCKGEN_CKDIV1_SML,
					(clk_readl(INFRA_TOPCKGEN_CKDIV1_SML) & 0xFFFFFFE0));
		}
		return count;
}
/*MMPLL*/
static int sdmpll_fsel_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
		char desc[32];
		int len = 0;
		unsigned int ctrl_value = 0;
		/* mmpll = 1, vencpll = 2, syspll = 3 */
		int pll_idx;
		unsigned int con0_value, con1_value;

		len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
			if (copy_from_user(desc, buffer, len))
				return 0;
		desc[len] = '\0';
		if (sscanf(desc, "%d %x %x" , &pll_idx, &con1_value, &con0_value) == 3) {
			if (pll_idx == 1) {
				clk_writel(MMPLL_CON1, con1_value);
				clk_writel(MMPLL_CON0, con0_value);
			} else if (pll_idx == 2) {
				clk_writel(VENCPLL_CON1, con1_value);
				clk_writel(VENCPLL_CON0, con0_value);
			} else if (pll_idx == 3) {
				clk_writel(MAINPLL_CON1, con1_value);
				clk_writel(MAINPLL_CON0, con0_value);
			} else if (pll_idx == 4) {
				clk_writel(UNIVPLL_CON1, con1_value);
				clk_writel(UNIVPLL_CON0, con0_value);
			}
			udelay(20);
		}
		return count;
}
/*
 DRV_WriteReg32(VENCPLL_CON1, 0x800B0000); //286MHz
    DRV_WriteReg32(VENCPLL_CON0, 0x00000120); //POSDIV = 4

*/

void slp_check_pm_mtcmos_pll(void)
{
	int i;
	slp_chk_mtcmos_pll_stat = 1;

	for (i = 2; i < NR_PLLS; i++) {
		if (i == 7)
			continue;
		if (pll_is_on(i)) {
			slp_chk_mtcmos_pll_stat = -1;
			pr_err("%s: on\n", plls[i].name);
			pr_err("suspend warning: %s is on!!!\n", plls[i].name);
			pr_err("warning! warning! warning! it may cause resume fail\n");
		}
	}
	for (i = 0; i < NR_SYSS; i++) {
		if (subsys_is_on(i)) {
			pr_err("%s: on\n", syss[i].name);
			if (i > SYS_CONN) {
				slp_chk_mtcmos_pll_stat = -1;
				pr_err("suspend warning: %s is on!!!\n", syss[i].name);
				pr_err("warning! warning! warning! it may cause resume fail\n");
			}
		}
	}
	if (slp_chk_mtcmos_pll_stat != 1)
		clk_dump();
}
EXPORT_SYMBOL(slp_check_pm_mtcmos_pll);


void slp_check_pm_mtcmos(void)
{
	int i;

	for (i = 0; i < NR_SYSS; i++) {
		if (subsys_is_on(i))
			pr_err("%s: is on\n", syss[i].name);
		else
			pr_err("%s: is off\n", syss[i].name);
	}
	pr_err(" PWR_STATUS     = %08x\n", clk_readl(PWR_STATUS));
	pr_err(" PWR_STATUS_2ND = %08x\n", clk_readl(PWR_STATUS_2ND));
	pr_err(" MD1_PWR_CON    = %08x\n", clk_readl(MD1_PWR_CON));
	pr_err(" C2K_PWR_CON    = %08x\n", clk_readl(C2K_PWR_CON));
	pr_err(" MDSYS_INTF_INFRA_PWR_CON  = %08x\n", clk_readl(MDSYS_INTF_INFRA_PWR_CON));
	pr_err(" MDSYS_INTF_MD1_PWR_CON    = %08x\n", clk_readl(MDSYS_INTF_MD1_PWR_CON));
	pr_err(" MDSYS_INTF_C2K_PWR_CON    = %08x\n", clk_readl(MDSYS_INTF_C2K_PWR_CON));
}
EXPORT_SYMBOL(slp_check_pm_mtcmos);

static int slp_chk_mtcmos_pll_stat_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", slp_chk_mtcmos_pll_stat);
    return 0;
}

static int proc_armbpll_fsel_open(struct inode *inode, struct file *file)
{
	return single_open(file, armbpll_fsel_read, NULL);
}
static const struct file_operations armbpll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open  = proc_armbpll_fsel_open,
	.read  = seq_read,
	.write = armbpll_fsel_write,
};

static int proc_armspll_fsel_open(struct inode *inode, struct file *file)
{
	return single_open(file, armspll_fsel_read, NULL);
}
static const struct file_operations armspll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open  = proc_armspll_fsel_open,
	.read  = seq_read,
	.write = armspll_fsel_write,
};
/*MMPLL*/
static const struct file_operations mmpll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open  = proc_armspll_fsel_open,
	.read  = seq_read,
	.write = sdmpll_fsel_write,
};
/*VENCPLL*/
static const struct file_operations vencpll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open  = proc_armspll_fsel_open,
	.read  = seq_read,
	.write = sdmpll_fsel_write,
};
/*SYSPLL*/
static const struct file_operations syspll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open  = proc_armspll_fsel_open,
	.read  = seq_read,
	.write = sdmpll_fsel_write,
};
/*UNIVPLL*/
static const struct file_operations univpll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open  = proc_armspll_fsel_open,
	.read  = seq_read,
	.write = sdmpll_fsel_write,
};

//for slp_check_pm_mtcmos_pll
static int proc_slp_chk_mtcmos_pll_stat_open(struct inode *inode, struct file *file)
{
    return single_open(file, slp_chk_mtcmos_pll_stat_read, NULL);
}
static const struct file_operations slp_chk_mtcmos_pll_stat_proc_fops = { 
    .owner = THIS_MODULE,
    .open  = proc_slp_chk_mtcmos_pll_stat_open,
    .read  = seq_read,
};

void mt_clkmgr_debug_init(void)
{
//use proc_create
	struct proc_dir_entry *entry;
	struct proc_dir_entry *clkmgr_dir;

	clkmgr_dir = proc_mkdir("clkmgr", NULL);
	if (!clkmgr_dir) {
		clk_err("[%s]: fail to mkdir /proc/clkmgr\n", __func__);
		return;
	}
	entry = proc_create("armbpll_fsel", S_IRUGO | S_IWUSR, clkmgr_dir, &armbpll_fsel_proc_fops);
	entry = proc_create("armspll_fsel", S_IRUGO | S_IWUSR, clkmgr_dir, &armspll_fsel_proc_fops);
	entry = proc_create("mmpll_fsel", S_IRUGO | S_IWUSR, clkmgr_dir, &mmpll_fsel_proc_fops);
	entry = proc_create("vencpll_fsel", S_IRUGO | S_IWUSR, clkmgr_dir, &vencpll_fsel_proc_fops);
	entry = proc_create("syspll_fsel", S_IRUGO | S_IWUSR, clkmgr_dir, &syspll_fsel_proc_fops);
	entry = proc_create("univpll_fsel", S_IRUGO | S_IWUSR, clkmgr_dir, &univpll_fsel_proc_fops);
	entry = proc_create("slp_chk_mtcmos_pll_stat", S_IRUGO, clkmgr_dir, &slp_chk_mtcmos_pll_stat_proc_fops);
}

#ifdef CONFIG_OF
void iomap(void)
{
    struct device_node *node;

//apmixed
    node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-apmixedsys");
    if (!node) {
			clk_dbg("[CLK_APMIXED] find node failed\n");
    }
    clk_apmixed_base = of_iomap(node, 0);
    if (!clk_apmixed_base)
			clk_dbg("[CLK_APMIXED] base failed\n");
//cksys_base
    node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-topckgen");
    if (!node) {
			clk_dbg("[CLK_CKSYS] find node failed\n");
    }
    clk_cksys_base = of_iomap(node, 0);
    if (!clk_cksys_base)
			clk_dbg("[CLK_CKSYS] base failed\n");
//infracfg_ao        
    node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-infrasys");
    if (!node) {
			clk_dbg("[CLK_INFRACFG_AO] find node failed\n");
    }
    clk_infracfg_ao_base = of_iomap(node, 0);
    if (!clk_infracfg_ao_base)
			clk_dbg("[CLK_INFRACFG_AO] base failed\n");
//audio        
    node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-audiosys");
    if (!node) {
			clk_dbg("[CLK_AUDIO] find node failed\n");
    }
    clk_audio_base = of_iomap(node, 0);
    if (!clk_audio_base)
			clk_dbg("[CLK_AUDIO] base failed\n");
//mfgcfg        
		node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-mfgsys");
    if (!node) {
				clk_dbg("[CLK_MFGCFG node failed\n");
    }
    clk_mfgcfg_base = of_iomap(node, 0);
    if (!clk_mfgcfg_base)
			clk_dbg("[CLK_G3D_CONFIG] base failed\n");
//mmsys_config
    node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-mmsys");
    if (!node) {
			clk_dbg("[CLK_MMSYS_CONFIG] find node failed\n");
    }
    clk_mmsys_config_base = of_iomap(node, 0);
    if (!clk_mmsys_config_base)
			clk_dbg("[CLK_MMSYS_CONFIG] base failed\n");
//imgsys        
    node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-imgsys");
    if (!node) {
			clk_dbg("[CLK_IMGSYS_CONFIG] find node failed\n");
    }
    clk_imgsys_base = of_iomap(node, 0);
    if (!clk_imgsys_base)
			clk_dbg("[CLK_IMGSYS_CONFIG] base failed\n");
//vdec_gcon
    node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-vdec_gcon");
    if (!node) {
			clk_dbg("[CLK_VDEC_GCON] find node failed\n");
    }
    clk_vdec_gcon_base = of_iomap(node, 0);
    if (!clk_vdec_gcon_base)
			clk_dbg("[CLK_VDEC_GCON] base failed\n");
//venc_gcon
    node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-venc_gcon");
    if (!node) {
			clk_dbg("[CLK_VENC_GCON] find node failed\n");
    }
    clk_venc_gcon_base = of_iomap(node, 0);
    if (!clk_venc_gcon_base)
			clk_dbg("[CLK_VENC_GCON] base failed\n");
/*gpio*/
		node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-pinctrl");
		if (!node)
			clk_dbg("[CLK_GPIO] find node failed\n");

		clk_gpio_base = of_iomap(node, 0);
		if (!clk_gpio_base)
			clk_dbg("[CLK_GPIO] base failed\n");
}
#endif

static int mt_clkmgr_debug_module_init(void)
{
    mt_clkmgr_debug_init();
    return 0;
}

module_init(mt_clkmgr_debug_module_init);
