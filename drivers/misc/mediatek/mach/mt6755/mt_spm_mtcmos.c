#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>	/* udelay */
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mach/mt_typedefs.h>
#include <mach/mt_spm_cpu.h>
#include <mach/mt_spm_reg.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_spm_mtcmos_internal.h>
#include <mach/hotplug.h>
#include <mach/mt_clkmgr.h>

#include <mach/md32_helper.h>
#include <mach/md32_ipi.h>
#include <mach/mt_pmic_wrap.h>

#include <mach/mt_freqhopping.h>
#define BUILDERROR
/**************************************
 * extern
 **************************************/
#ifdef CONFIG_MTK_L2C_SHARE
extern int IS_L2_BORROWED(void);
#endif				/* #ifdef CONFIG_MTK_L2C_SHARE */


/**************************************
 * for CPU MTCMOS
 **************************************/
static DEFINE_SPINLOCK(spm_cpu_lock);
#ifdef CONFIG_OF
void __iomem *spm_cpu_base;
#endif				/* #ifdef CONFIG_OF */

int spm_mtcmos_cpu_init(void)
{
#ifdef CONFIG_OF
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,SLEEP");
	if (!node) {
		pr_err("find SLEEP node failed\n");
		return -EINVAL;
	}
	spm_cpu_base = of_iomap(node, 0);
	if (!spm_cpu_base) {
		pr_err("base spm_cpu_base failed\n");
		return -EINVAL;
	}

	return 0;
#else				/* #ifdef CONFIG_OF */
	return -EINVAL;
#endif				/* #ifdef CONFIG_OF */
}

void spm_mtcmos_cpu_lock(unsigned long *flags)
{
	spin_lock_irqsave(&spm_cpu_lock, *flags);
}

void spm_mtcmos_cpu_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&spm_cpu_lock, *flags);
}

/* Define MTCMOS Bus Protect Mask */
#define MD1_PROT_MASK                    ((0x1 << 16) \
					  |(0x1 << 17) \
					  |(0x1 << 18) \
					  |(0x1 << 19) \
					  |(0x1 << 20) \
					  |(0x1 << 21))
#define CONN_PROT_MASK                   ((0x1 << 13) \
					  |(0x1 << 14))
#define DIS_PROT_MASK                    ((0x1 << 1))
#define MFG_PROT_MASK                    ((0x1 << 21))
#define MP0_CPUTOP_PROT_MASK             ((0x1 << 26) \
					  |(0x1 << 29))
#define MP1_CPUTOP_PROT_MASK             ((0x1 << 30))
#define C2K_PROT_MASK                    ((0x1 << 22) \
					  |(0x1 << 23) \
					  |(0x1 << 24))
#define MDSYS_INTF_INFRA_PROT_MASK       ((0x1 << 3) \
					  |(0x1 << 4))

 /* Define MTCMOS Power Status Mask */

#define MD1_PWR_STA_MASK                 (0x1 << 0)
#define CONN_PWR_STA_MASK                (0x1 << 1)
#define DPY_PWR_STA_MASK                 (0x1 << 2)
#define DIS_PWR_STA_MASK                 (0x1 << 3)
#define MFG_PWR_STA_MASK                 (0x1 << 4)
#define ISP_PWR_STA_MASK                 (0x1 << 5)
#define IFR_PWR_STA_MASK                 (0x1 << 6)
#define VDE_PWR_STA_MASK                 (0x1 << 7)
#define MP0_CPUTOP_PWR_STA_MASK          (0x1 << 8)
#define MP0_CPU0_PWR_STA_MASK            (0x1 << 9)
#define MP0_CPU1_PWR_STA_MASK            (0x1 << 10)
#define MP0_CPU2_PWR_STA_MASK            (0x1 << 11)
#define MP0_CPU3_PWR_STA_MASK            (0x1 << 12)
#define MCU_PWR_STA_MASK                 (0x1 << 14)
#define MP1_CPUTOP_PWR_STA_MASK          (0x1 << 15)
#define MP1_CPU0_PWR_STA_MASK            (0x1 << 16)
#define MP1_CPU1_PWR_STA_MASK            (0x1 << 17)
#define MP1_CPU2_PWR_STA_MASK            (0x1 << 18)
#define MP1_CPU3_PWR_STA_MASK            (0x1 << 19)
#define VEN_PWR_STA_MASK                 (0x1 << 21)
#define MFG_ASYNC_PWR_STA_MASK           (0x1 << 23)
#define AUDIO_PWR_STA_MASK               (0x1 << 24)
#define C2K_PWR_STA_MASK                 (0x1 << 28)
#define MDSYS_INTF_INFRA_PWR_STA_MASK    (0x1 << 29)
/* Define Non-CPU SRAM Mask */
#define MD1_SRAM_PDN                     (0x1 << 8)
#define MD1_SRAM_PDN_ACK                 (0x0 << 12)
#define DIS_SRAM_PDN                     (0x1 << 8)
#define DIS_SRAM_PDN_ACK                 (0x1 << 12)
#define MFG_SRAM_PDN                     (0x1 << 8)
#define MFG_SRAM_PDN_ACK                 (0x1 << 16)
#define ISP_SRAM_PDN                     (0x3 << 8)
#define ISP_SRAM_PDN_ACK                 (0x3 << 12)
#define IFR_SRAM_PDN                     (0xF << 8)
#define IFR_SRAM_PDN_ACK                 (0xF << 12)
#define VDE_SRAM_PDN                     (0x1 << 8)
#define VDE_SRAM_PDN_ACK                 (0x1 << 12)
#define VEN_SRAM_PDN                     (0xF << 8)
#define VEN_SRAM_PDN_ACK                 (0xF << 12)
#define AUDIO_SRAM_PDN                   (0xF << 8)
#define AUDIO_SRAM_PDN_ACK               (0xF << 12)
/* Define CPU SRAM Mask */
#define MP0_CPUTOP_SRAM_PDN              (0x1 << 0)
#define MP0_CPUTOP_SRAM_PDN_ACK          (0x1 << 8)
#define MP0_CPUTOP_SRAM_SLEEP_B          (0x1 << 0)
#define MP0_CPUTOP_SRAM_SLEEP_B_ACK      (0x1 << 8)
#define MP0_CPU0_SRAM_PDN                (0x1 << 0)
#define MP0_CPU0_SRAM_PDN_ACK            (0x1 << 8)
#define MP0_CPU1_SRAM_PDN                (0x1 << 0)
#define MP0_CPU1_SRAM_PDN_ACK            (0x1 << 8)
#define MP0_CPU2_SRAM_PDN                (0x1 << 0)
#define MP0_CPU2_SRAM_PDN_ACK            (0x1 << 8)
#define MP0_CPU3_SRAM_PDN                (0x1 << 0)
#define MP0_CPU3_SRAM_PDN_ACK            (0x1 << 8)
#define MP1_CPUTOP_SRAM_PDN              (0x1 << 0)
#define MP1_CPUTOP_SRAM_PDN_ACK          (0x1 << 8)
#define MP1_CPUTOP_SRAM_SLEEP_B          (0x1 << 0)
#define MP1_CPUTOP_SRAM_SLEEP_B_ACK      (0x1 << 8)
#define MP1_CPU0_SRAM_PDN                (0x1 << 0)
#define MP1_CPU0_SRAM_PDN_ACK            (0x1 << 8)
#define MP1_CPU1_SRAM_PDN                (0x1 << 0)
#define MP1_CPU1_SRAM_PDN_ACK            (0x1 << 8)
#define MP1_CPU2_SRAM_PDN                (0x1 << 0)
#define MP1_CPU2_SRAM_PDN_ACK            (0x1 << 8)
#define MP1_CPU3_SRAM_PDN                (0x1 << 0)
#define MP1_CPU3_SRAM_PDN_ACK            (0x1 << 8)

typedef int (*spm_cpu_mtcmos_ctrl_func) (int state, int chkWfiBeforePdn);
static spm_cpu_mtcmos_ctrl_func spm_cpu_mtcmos_ctrl_funcs[] = {
	spm_mtcmos_ctrl_cpu0,
	spm_mtcmos_ctrl_cpu1,
	spm_mtcmos_ctrl_cpu2,
	spm_mtcmos_ctrl_cpu3,
	spm_mtcmos_ctrl_cpu4,
	spm_mtcmos_ctrl_cpu5,
	spm_mtcmos_ctrl_cpu6,
	spm_mtcmos_ctrl_cpu7
};

int spm_mtcmos_ctrl_cpu(unsigned int cpu, int state, int chkWfiBeforePdn)
{
	return (*spm_cpu_mtcmos_ctrl_funcs[cpu]) (state, chkWfiBeforePdn);
}

int spm_mtcmos_ctrl_cpu0(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn) {
			while ((spm_read(CPU_IDLE_STA) & MP0_CPU0_STANDBYWFI_LSB) == 0)
				;
		}
		/* TINFO="Start to turn off MP0_CPU0" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_mtcmos_cpu_lock(&flags);
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) | PWR_ISO);
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MP0_CPU0_L1_PDN, spm_read(MP0_CPU0_L1_PDN) | MP0_CPU0_SRAM_PDN);
		/* TINFO="Wait until MP0_CPU0_SRAM_PDN_ACK = 1" */
#ifndef CFG_FPGA_PLATFORM
		while (!(spm_read(MP0_CPU0_L1_PDN) & MP0_CPU0_SRAM_PDN_ACK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) & ~PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MP0_CPU0_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MP0_CPU0_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		spm_mtcmos_cpu_unlock(&flags);
		/* TINFO="Finish to turn off MP0_CPU0" */
		if (!
		    (spm_read(PWR_STATUS) &
		     (MP0_CPU1_PWR_STA_MASK | MP0_CPU2_PWR_STA_MASK | MP0_CPU3_PWR_STA_MASK))
		    && !(spm_read(PWR_STATUS_2ND) &
			 (MP0_CPU1_PWR_STA_MASK | MP0_CPU2_PWR_STA_MASK | MP0_CPU3_PWR_STA_MASK))) {
#ifdef CONFIG_MTK_L2C_SHARE
			if (!IS_L2_BORROWED())
#endif				/* #ifdef CONFIG_MTK_L2C_SHARE */
				spm_mtcmos_ctrl_cpusys0(state, chkWfiBeforePdn);
		}

		/* TINFO="Finish to turn off MP0_CPU0" */
	} else {		/* STA_POWER_ON */
		if (!(spm_read(PWR_STATUS) & MP0_CPUTOP_PWR_STA_MASK) &&
		    !(spm_read(PWR_STATUS_2ND) & MP0_CPUTOP_PWR_STA_MASK))
			spm_mtcmos_ctrl_cpusys0(state, chkWfiBeforePdn);
		spm_mtcmos_cpu_lock(&flags);
		/* TINFO="Start to turn on MP0_CPU0" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) | PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MP0_CPU0_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MP0_CPU0_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */

		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MP0_CPU0_L1_PDN, spm_read(MP0_CPU0_L1_PDN) & ~(0x1 << 0));
		/* TINFO="Wait until MP0_CPU0_SRAM_PDN_ACK = 0" */
#ifndef CFG_FPGA_PLATFORM
		while (spm_read(MP0_CPU0_L1_PDN) & MP0_CPU0_SRAM_PDN_ACK) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MP0_CPU0_PWR_CON, spm_read(MP0_CPU0_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MP0_CPU0" */
		spm_mtcmos_cpu_unlock(&flags);
	}
	return 0;
}

int spm_mtcmos_ctrl_cpu1(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(CPU_IDLE_STA) & MP0_CPU1_STANDBYWFI_LSB) == 0)
				;
		/* TINFO="Start to turn off MP0_CPU1" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_mtcmos_cpu_lock(&flags);
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) | PWR_ISO);
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MP0_CPU1_L1_PDN, spm_read(MP0_CPU1_L1_PDN) | MP0_CPU1_SRAM_PDN);
		/* TINFO="Wait until MP0_CPU1_SRAM_PDN_ACK = 1" */
#ifndef CFG_FPGA_PLATFORM
		while (!(spm_read(MP0_CPU1_L1_PDN) & MP0_CPU1_SRAM_PDN_ACK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) & ~PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MP0_CPU1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MP0_CPU1_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		spm_mtcmos_cpu_unlock(&flags);
		/* TINFO="Finish to turn off MP0_CPU0" */
		if (!
		    (spm_read(PWR_STATUS) &
		     (MP0_CPU0_PWR_STA_MASK | MP0_CPU2_PWR_STA_MASK | MP0_CPU3_PWR_STA_MASK))
		    && !(spm_read(PWR_STATUS_2ND) &
			 (MP0_CPU0_PWR_STA_MASK | MP0_CPU2_PWR_STA_MASK | MP0_CPU3_PWR_STA_MASK))) {
#ifdef CONFIG_MTK_L2C_SHARE
			if (!IS_L2_BORROWED())
#endif				/* #ifdef CONFIG_MTK_L2C_SHARE */
				spm_mtcmos_ctrl_cpusys0(state, chkWfiBeforePdn);
		}

		/* TINFO="Finish to turn off MP0_CPU1" */
	} else {		/* STA_POWER_ON */
		if (!(spm_read(PWR_STATUS) & MP0_CPUTOP_PWR_STA_MASK) &&
		    !(spm_read(PWR_STATUS_2ND) & MP0_CPUTOP_PWR_STA_MASK))
			spm_mtcmos_ctrl_cpusys0(state, chkWfiBeforePdn);
		spm_mtcmos_cpu_lock(&flags);
		/* TINFO="Start to turn on MP0_CPU1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) | PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MP0_CPU1_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MP0_CPU1_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */

		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MP0_CPU1_L1_PDN, spm_read(MP0_CPU1_L1_PDN) & ~(0x1 << 0));
		/* TINFO="Wait until MP0_CPU1_SRAM_PDN_ACK = 0" */
#ifndef CFG_FPGA_PLATFORM
		while (spm_read(MP0_CPU1_L1_PDN) & MP0_CPU1_SRAM_PDN_ACK) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MP0_CPU1_PWR_CON, spm_read(MP0_CPU1_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MP0_CPU1" */
		spm_mtcmos_cpu_unlock(&flags);
	}
	return 0;
}

int spm_mtcmos_ctrl_cpu2(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(CPU_IDLE_STA) & MP0_CPU2_STANDBYWFI_LSB) == 0)
				;
		/* TINFO="Start to turn off MP0_CPU2" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_mtcmos_cpu_lock(&flags);
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) | PWR_ISO);
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MP0_CPU2_L1_PDN, spm_read(MP0_CPU2_L1_PDN) | MP0_CPU2_SRAM_PDN);
		/* TINFO="Wait until MP0_CPU2_SRAM_PDN_ACK = 1" */
#ifndef CFG_FPGA_PLATFORM
		while (!(spm_read(MP0_CPU2_L1_PDN) & MP0_CPU2_SRAM_PDN_ACK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) & ~PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MP0_CPU2_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MP0_CPU2_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		spm_mtcmos_cpu_unlock(&flags);
		/* TINFO="Finish to turn off MP0_CPU0" */
		if (!
		    (spm_read(PWR_STATUS) &
		     (MP0_CPU0_PWR_STA_MASK | MP0_CPU1_PWR_STA_MASK | MP0_CPU3_PWR_STA_MASK))
		    && !(spm_read(PWR_STATUS_2ND) &
			 (MP0_CPU0_PWR_STA_MASK | MP0_CPU1_PWR_STA_MASK | MP0_CPU3_PWR_STA_MASK))) {
#ifdef CONFIG_MTK_L2C_SHARE
			if (!IS_L2_BORROWED())
#endif				/* #ifdef CONFIG_MTK_L2C_SHARE */
				spm_mtcmos_ctrl_cpusys0(state, chkWfiBeforePdn);
		}

		/* TINFO="Finish to turn off MP0_CPU2" */
	} else {		/* STA_POWER_ON */
		if (!(spm_read(PWR_STATUS) & MP0_CPUTOP_PWR_STA_MASK) &&
		    !(spm_read(PWR_STATUS_2ND) & MP0_CPUTOP_PWR_STA_MASK))
			spm_mtcmos_ctrl_cpusys0(state, chkWfiBeforePdn);
		spm_mtcmos_cpu_lock(&flags);
		/* TINFO="Start to turn on MP0_CPU2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) | PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MP0_CPU2_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MP0_CPU2_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */

		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MP0_CPU2_L1_PDN, spm_read(MP0_CPU2_L1_PDN) & ~(0x1 << 0));
		/* TINFO="Wait until MP0_CPU2_SRAM_PDN_ACK = 0" */
#ifndef CFG_FPGA_PLATFORM
		while (spm_read(MP0_CPU2_L1_PDN) & MP0_CPU2_SRAM_PDN_ACK) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MP0_CPU2_PWR_CON, spm_read(MP0_CPU2_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MP0_CPU2" */
		spm_mtcmos_cpu_unlock(&flags);
	}
	return 0;
}

int spm_mtcmos_ctrl_cpu3(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(CPU_IDLE_STA) & MP0_CPU3_STANDBYWFI_LSB) == 0)
				;
		/* TINFO="Start to turn off MP0_CPU3" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_mtcmos_cpu_lock(&flags);
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) | PWR_ISO);
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MP0_CPU3_L1_PDN, spm_read(MP0_CPU3_L1_PDN) | MP0_CPU3_SRAM_PDN);
		/* TINFO="Wait until MP0_CPU3_SRAM_PDN_ACK = 1" */
#ifndef CFG_FPGA_PLATFORM
		while (!(spm_read(MP0_CPU3_L1_PDN) & MP0_CPU3_SRAM_PDN_ACK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) & ~PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MP0_CPU3_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MP0_CPU3_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		spm_mtcmos_cpu_unlock(&flags);
		/* TINFO="Finish to turn off MP0_CPU0" */
		if (!
		    (spm_read(PWR_STATUS) &
		     (MP0_CPU0_PWR_STA_MASK | MP0_CPU1_PWR_STA_MASK | MP0_CPU2_PWR_STA_MASK))
		    && !(spm_read(PWR_STATUS_2ND) &
			 (MP0_CPU0_PWR_STA_MASK | MP0_CPU1_PWR_STA_MASK | MP0_CPU2_PWR_STA_MASK))) {
#ifdef CONFIG_MTK_L2C_SHARE
			if (!IS_L2_BORROWED())
#endif				/* #ifdef CONFIG_MTK_L2C_SHARE */
				spm_mtcmos_ctrl_cpusys0(state, chkWfiBeforePdn);
		}

		/* TINFO="Finish to turn off MP0_CPU3" */
	} else {		/* STA_POWER_ON */
		if (!(spm_read(PWR_STATUS) & MP0_CPUTOP_PWR_STA_MASK) &&
		    !(spm_read(PWR_STATUS_2ND) & MP0_CPUTOP_PWR_STA_MASK))
			spm_mtcmos_ctrl_cpusys0(state, chkWfiBeforePdn);
		spm_mtcmos_cpu_lock(&flags);
		/* TINFO="Start to turn on MP0_CPU3" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) | PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MP0_CPU3_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MP0_CPU3_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */

		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MP0_CPU3_L1_PDN, spm_read(MP0_CPU3_L1_PDN) & ~(0x1 << 0));
		/* TINFO="Wait until MP0_CPU3_SRAM_PDN_ACK = 0" */
#ifndef CFG_FPGA_PLATFORM
		while (spm_read(MP0_CPU3_L1_PDN) & MP0_CPU3_SRAM_PDN_ACK) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MP0_CPU3_PWR_CON, spm_read(MP0_CPU3_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MP0_CPU3" */
		spm_mtcmos_cpu_unlock(&flags);
	}
	return 0;
}

int spm_mtcmos_ctrl_cpu4(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(CPU_IDLE_STA) & MP1_CPU0_STANDBYWFI_LSB) == 0)
				;
		/* TINFO="Start to turn off MP1_CPU0" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_mtcmos_cpu_lock(&flags);
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) | PWR_ISO);
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MP1_CPU0_L1_PDN, spm_read(MP1_CPU0_L1_PDN) | MP1_CPU0_SRAM_PDN);
		/* TINFO="Wait until MP1_CPU0_SRAM_PDN_ACK = 1" */
#ifndef CFG_FPGA_PLATFORM
		while (!(spm_read(MP1_CPU0_L1_PDN) & MP1_CPU0_SRAM_PDN_ACK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) & ~PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MP1_CPU0_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MP1_CPU0_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		spm_mtcmos_cpu_unlock(&flags);

		/* TINFO="Finish to turn off MP1_CPU0" */
		if (!
		    (spm_read(PWR_STATUS) &
		     (MP1_CPU1_PWR_STA_MASK | MP1_CPU2_PWR_STA_MASK | MP1_CPU3_PWR_STA_MASK))
		    && !(spm_read(PWR_STATUS_2ND) &
			 (MP1_CPU1_PWR_STA_MASK | MP1_CPU2_PWR_STA_MASK | MP1_CPU3_PWR_STA_MASK))) {
#ifdef CONFIG_MTK_L2C_SHARE
			if (!IS_L2_BORROWED())
#endif				/* #ifdef CONFIG_MTK_L2C_SHARE */
				spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
		}
	} else {		/* STA_POWER_ON */
		if (!(spm_read(PWR_STATUS) & MP1_CPUTOP_PWR_STA_MASK) &&
		    !(spm_read(PWR_STATUS_2ND) & MP1_CPUTOP_PWR_STA_MASK))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
		spm_mtcmos_cpu_lock(&flags);
		/* TINFO="Start to turn on MP1_CPU0" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) | PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MP1_CPU0_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MP1_CPU0_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */

		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MP1_CPU0_L1_PDN, spm_read(MP1_CPU0_L1_PDN) & ~(0x1 << 0));
		/* TINFO="Wait until MP1_CPU0_SRAM_PDN_ACK = 0" */
#ifndef CFG_FPGA_PLATFORM
		while (spm_read(MP1_CPU0_L1_PDN) & MP1_CPU0_SRAM_PDN_ACK) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MP1_CPU0_PWR_CON, spm_read(MP1_CPU0_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MP1_CPU0" */
		spm_mtcmos_cpu_unlock(&flags);
	}
	return 0;
}

int spm_mtcmos_ctrl_cpu5(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(CPU_IDLE_STA) & MP1_CPU1_STANDBYWFI_LSB) == 0)
				;
		/* TINFO="Start to turn off MP1_CPU1" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_mtcmos_cpu_lock(&flags);
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) | PWR_ISO);
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MP1_CPU1_L1_PDN, spm_read(MP1_CPU1_L1_PDN) | MP1_CPU1_SRAM_PDN);
		/* TINFO="Wait until MP1_CPU1_SRAM_PDN_ACK = 1" */
#ifndef CFG_FPGA_PLATFORM
		while (!(spm_read(MP1_CPU1_L1_PDN) & MP1_CPU1_SRAM_PDN_ACK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) & ~PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MP1_CPU1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MP1_CPU1_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		spm_mtcmos_cpu_unlock(&flags);
		/* TINFO="Finish to turn off MP1_CPU1" */
		if (!
		    (spm_read(PWR_STATUS) &
		     (MP1_CPU0_PWR_STA_MASK | MP1_CPU2_PWR_STA_MASK | MP1_CPU3_PWR_STA_MASK))
		    && !(spm_read(PWR_STATUS_2ND) &
			 (MP1_CPU0_PWR_STA_MASK | MP1_CPU2_PWR_STA_MASK | MP1_CPU3_PWR_STA_MASK))) {
#ifdef CONFIG_MTK_L2C_SHARE
			if (!IS_L2_BORROWED())
#endif				/* #ifdef CONFIG_MTK_L2C_SHARE */
				spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
		}
	} else {		/* STA_POWER_ON */
		if (!(spm_read(PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
		spm_mtcmos_cpu_lock(&flags);
		/* TINFO="Start to turn on MP1_CPU1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) | PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MP1_CPU1_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MP1_CPU1_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */

		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MP1_CPU1_L1_PDN, spm_read(MP1_CPU1_L1_PDN) & ~(0x1 << 0));
		/* TINFO="Wait until MP1_CPU1_SRAM_PDN_ACK = 0" */
#ifndef CFG_FPGA_PLATFORM
		while (spm_read(MP1_CPU1_L1_PDN) & MP1_CPU1_SRAM_PDN_ACK) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MP1_CPU1_PWR_CON, spm_read(MP1_CPU1_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MP1_CPU1" */
		spm_mtcmos_cpu_unlock(&flags);
	}
	return 0;
}

int spm_mtcmos_ctrl_cpu6(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(CPU_IDLE_STA) & MP1_CPU2_STANDBYWFI_LSB) == 0)
				;
		/* TINFO="Start to turn off MP1_CPU2" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_mtcmos_cpu_lock(&flags);
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) | PWR_ISO);
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MP1_CPU2_L1_PDN, spm_read(MP1_CPU2_L1_PDN) | MP1_CPU2_SRAM_PDN);
		/* TINFO="Wait until MP1_CPU2_SRAM_PDN_ACK = 1" */
#ifndef CFG_FPGA_PLATFORM
		while (!(spm_read(MP1_CPU2_L1_PDN) & MP1_CPU2_SRAM_PDN_ACK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) & ~PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MP1_CPU2_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MP1_CPU2_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		spm_mtcmos_cpu_unlock(&flags);
		/* TINFO="Finish to turn off MP1_CPU2" */
		if (!
		    (spm_read(PWR_STATUS) &
		     (MP1_CPU0_PWR_STA_MASK | MP1_CPU1_PWR_STA_MASK | MP1_CPU3_PWR_STA_MASK))
		    && !(spm_read(PWR_STATUS_2ND) &
			 (MP1_CPU0_PWR_STA_MASK | MP1_CPU1_PWR_STA_MASK | MP1_CPU3_PWR_STA_MASK))) {
#ifdef CONFIG_MTK_L2C_SHARE
			if (!IS_L2_BORROWED())
#endif				/* #ifdef CONFIG_MTK_L2C_SHARE */
				spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
		}
	} else {		/* STA_POWER_ON */
		if (!(spm_read(PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
		spm_mtcmos_cpu_lock(&flags);
		/* TINFO="Start to turn on MP1_CPU2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) | PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MP1_CPU2_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MP1_CPU2_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */

		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MP1_CPU2_L1_PDN, spm_read(MP1_CPU2_L1_PDN) & ~(0x1 << 0));
		/* TINFO="Wait until MP1_CPU2_SRAM_PDN_ACK = 0" */
#ifndef CFG_FPGA_PLATFORM
		while (spm_read(MP1_CPU2_L1_PDN) & MP1_CPU2_SRAM_PDN_ACK) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MP1_CPU2_PWR_CON, spm_read(MP1_CPU2_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MP1_CPU2" */
		spm_mtcmos_cpu_unlock(&flags);
	}
	return 0;
}

int spm_mtcmos_ctrl_cpu7(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(CPU_IDLE_STA) & MP1_CPU3_STANDBYWFI_LSB) == 0)
				;
		/* TINFO="Start to turn off MP1_CPU3" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_mtcmos_cpu_lock(&flags);
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) | PWR_ISO);
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MP1_CPU3_L1_PDN, spm_read(MP1_CPU3_L1_PDN) | MP1_CPU3_SRAM_PDN);
		/* TINFO="Wait until MP1_CPU3_SRAM_PDN_ACK = 1" */
#ifndef CFG_FPGA_PLATFORM
		while (!(spm_read(MP1_CPU3_L1_PDN) & MP1_CPU3_SRAM_PDN_ACK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) & ~PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MP1_CPU3_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MP1_CPU3_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		spm_mtcmos_cpu_unlock(&flags);
		/* TINFO="Finish to turn off MP1_CPU3" */
		if (!
		    (spm_read(PWR_STATUS) &
		     (MP1_CPU0_PWR_STA_MASK | MP1_CPU1_PWR_STA_MASK | MP1_CPU2_PWR_STA_MASK))
		    && !(spm_read(PWR_STATUS_2ND) &
			 (MP1_CPU0_PWR_STA_MASK | MP1_CPU1_PWR_STA_MASK | MP1_CPU2_PWR_STA_MASK))) {
#ifdef CONFIG_MTK_L2C_SHARE
			if (!IS_L2_BORROWED())
#endif				/* #ifdef CONFIG_MTK_L2C_SHARE */
				spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
		}
	} else {		/* STA_POWER_ON */
		if (!(spm_read(PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);

		spm_mtcmos_cpu_lock(&flags);
		/* TINFO="Start to turn on MP1_CPU3" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) | PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MP1_CPU3_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MP1_CPU3_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */

		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MP1_CPU3_L1_PDN, spm_read(MP1_CPU3_L1_PDN) & ~(0x1 << 0));
		/* TINFO="Wait until MP1_CPU3_SRAM_PDN_ACK = 0" */
#ifndef CFG_FPGA_PLATFORM
		while (spm_read(MP1_CPU3_L1_PDN) & MP1_CPU3_SRAM_PDN_ACK) {
		}
#endif
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MP1_CPU3_PWR_CON, spm_read(MP1_CPU3_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MP1_CPU3" */
		spm_mtcmos_cpu_unlock(&flags);
	}
	return 0;
}


int spm_mtcmos_ctrl_cpusys0(int state, int chkWfiBeforePdn)
{
	int err = 0;
	unsigned long flags;
	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MP1_CPUTOP" */
		/* TINFO="Set bus protect" */
		if (chkWfiBeforePdn)
			while ((spm_read(CPU_IDLE_STA) & MP0_CPUTOP_IDLE_LSB) == 0)
				;
		spm_mtcmos_cpu_lock(&flags);
#if 0
		spm_write(INFRA_TOPAXI_PROTECTEN,
			  spm_read(INFRA_TOPAXI_PROTECTEN) | MP0_CPUTOP_PROT_MASK);
		while ((spm_read(INFRA_TOPAXI_PROTECTSTA1) & MP0_CPUTOP_PROT_MASK) !=
		       MP0_CPUTOP_PROT_MASK)
			;
#else
		spm_topaxi_protect(MP0_CPUTOP_PROT_MASK, 1);
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) | PWR_ISO);
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MP0_CPUTOP_L2_PDN, spm_read(MP0_CPUTOP_L2_PDN) | MP0_CPUTOP_SRAM_PDN);
		/* TINFO="Wait until MP1_CPUTOP_SRAM_PDN_ACK = 1" */
		while (!(spm_read(MP0_CPUTOP_L2_PDN) & MP0_CPUTOP_SRAM_PDN_ACK))
			;
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) & ~PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MP0_CPUTOP_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MP0_CPUTOP_PWR_STA_MASK))
			;
#endif

		spm_mtcmos_cpu_unlock(&flags);
		/* TINFO="Finish to turn off MP1_CPUTOP" */
	} else {
		spm_mtcmos_cpu_lock(&flags);
		/* STA_POWER_ON */
		/* TINFO="Start to turn on MP1_CPUTOP" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MP0_CPUTOP_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MP0_CPUTOP_PWR_STA_MASK))
			;
#endif

		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MP0_CPUTOP_L2_PDN, spm_read(MP0_CPUTOP_L2_PDN) & ~(0x1 << 0));
		/* TINFO="Wait until MP1_CPUTOP_SRAM_PDN_ACK = 0" */
		while (spm_read(MP0_CPUTOP_L2_PDN) & MP0_CPUTOP_SRAM_PDN_ACK)
			;
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MP0_CPUTOP_PWR_CON, spm_read(MP0_CPUTOP_PWR_CON) | PWR_RST_B);
		/* TINFO="Release bus protect" */
#if 0
		spm_write(INFRA_TOPAXI_PROTECTEN,
			  spm_read(INFRA_TOPAXI_PROTECTEN) & ~MP0_CPUTOP_PROT_MASK);
		while (spm_read(INFRA_TOPAXI_PROTECTSTA1) & MP0_CPUTOP_PROT_MASK)
			;
#else
		spm_topaxi_protect(MP0_CPUTOP_PROT_MASK, 0);
#endif
		spm_mtcmos_cpu_unlock(&flags);

		/* TINFO="Finish to turn on MP1_CPUTOP" */
	}
	return err;
}

int spm_mtcmos_ctrl_cpusys1(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MP1_CPUTOP" */
		/* TINFO="Set bus protect" */
		if (chkWfiBeforePdn) {
			while ((spm_read(CPU_IDLE_STA) & MP1_CPUTOP_IDLE_LSB) == 0)
				;
		}
		spm_mtcmos_cpu_lock(&flags);
#if 0
		spm_write(INFRA_TOPAXI_PROTECTEN,
			  spm_read(INFRA_TOPAXI_PROTECTEN) | MP1_CPUTOP_PROT_MASK);
		while ((spm_read(INFRA_TOPAXI_PROTECTSTA1) & MP1_CPUTOP_PROT_MASK) !=
		       MP1_CPUTOP_PROT_MASK)
			;
#else
		spm_topaxi_protect(MP1_CPUTOP_PROT_MASK, 1);
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) | PWR_ISO);
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MP1_CPUTOP_L2_PDN, spm_read(MP1_CPUTOP_L2_PDN) | MP1_CPUTOP_SRAM_PDN);
		/* TINFO="Wait until MP1_CPUTOP_SRAM_PDN_ACK = 1" */
#ifndef CFG_FPGA_PLATFORM
		while (!(spm_read(MP1_CPUTOP_L2_PDN) & MP1_CPUTOP_SRAM_PDN_ACK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) & ~PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MP1_CPUTOP_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MP1_CPUTOP_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */

		switch_armpll_l_hwmode(0);	/*Switch to SW mode */
		mt_pause_armpll(FH_ARMCA15_PLLID, 1);	/*Pause FQHP function */
		disable_armpll_l();	/*Turn off arm pll */
		spm_mtcmos_cpu_unlock(&flags);

		/* TINFO="Finish to turn off MP1_CPUTOP" */
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);
		enable_armpll_l();	/*Turn on arm pll */
		mt_pause_armpll(FH_ARMCA15_PLLID, 0);	/*Non-pause FQHP function */
		switch_armpll_l_hwmode(1);	/*Switch to HW mode */

		/* TINFO="Start to turn on MP1_CPUTOP" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) | PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MP1_CPUTOP_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MP1_CPUTOP_PWR_STA_MASK)) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */

		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MP1_CPUTOP_L2_PDN, spm_read(MP1_CPUTOP_L2_PDN) & ~(0x1 << 0));
		/* TINFO="Wait until MP1_CPUTOP_SRAM_PDN_ACK = 0" */
#ifndef CFG_FPGA_PLATFORM
		while (spm_read(MP1_CPUTOP_L2_PDN) & MP1_CPUTOP_SRAM_PDN_ACK) {
		}
#endif				/* #ifndef CFG_FPGA_PLATFORM */
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MP1_CPUTOP_PWR_CON, spm_read(MP1_CPUTOP_PWR_CON) | PWR_RST_B);
		/* TINFO="Release bus protect" */
#if 0
		spm_write(INFRA_TOPAXI_PROTECTEN,
			  spm_read(INFRA_TOPAXI_PROTECTEN) & ~MP1_CPUTOP_PROT_MASK);
#ifndef CFG_FPGA_PLATFORM
		while (spm_read(INFRA_TOPAXI_PROTECTSTA1) & MP1_CPUTOP_PROT_MASK) {
		}
#endif
#else				/* #ifndef CFG_FPGA_PLATFORM */

		spm_topaxi_protect(MP1_CPUTOP_PROT_MASK, 0);
#endif
		spm_mtcmos_cpu_unlock(&flags);
		/* TINFO="Finish to turn on MP1_CPUTOP" */
	}
	return 0;
}

void spm_mtcmos_ctrl_cpusys1_init_1st_bring_up(int state)
{

	if (state == STA_POWER_DOWN) {
		spm_mtcmos_ctrl_cpu7(STA_POWER_DOWN, 0);
		spm_mtcmos_ctrl_cpu6(STA_POWER_DOWN, 0);
		spm_mtcmos_ctrl_cpu5(STA_POWER_DOWN, 0);
		spm_mtcmos_ctrl_cpu4(STA_POWER_DOWN, 0);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_ctrl_cpu4(STA_POWER_ON, 1);
		spm_mtcmos_ctrl_cpu5(STA_POWER_ON, 1);
		spm_mtcmos_ctrl_cpu6(STA_POWER_ON, 1);
		spm_mtcmos_ctrl_cpu7(STA_POWER_ON, 1);
	}
}

bool spm_cpusys0_can_power_down(void)
{
	return !(spm_read(PWR_STATUS) &
		 (MP1_CPU0_PWR_STA_MASK | MP1_CPU1_PWR_STA_MASK | MP1_CPU2_PWR_STA_MASK |
		  MP1_CPU3_PWR_STA_MASK | MP1_CPUTOP_PWR_STA_MASK | MP0_CPU1_PWR_STA_MASK |
		  MP0_CPU2_PWR_STA_MASK | MP0_CPU3_PWR_STA_MASK))
	    && !(spm_read(PWR_STATUS_2ND) &
		 (MP1_CPU0_PWR_STA_MASK | MP1_CPU1_PWR_STA_MASK | MP1_CPU2_PWR_STA_MASK |
		  MP1_CPU3_PWR_STA_MASK | MP1_CPUTOP_PWR_STA_MASK | MP0_CPU1_PWR_STA_MASK |
		  MP0_CPU2_PWR_STA_MASK | MP0_CPU3_PWR_STA_MASK));
}

bool spm_cpusys1_can_power_down(void)
{
	return !(spm_read(PWR_STATUS) &
		 (MP0_CPU0_PWR_STA_MASK | MP0_CPU1_PWR_STA_MASK | MP0_CPU2_PWR_STA_MASK |
		  MP0_CPU3_PWR_STA_MASK | MP0_CPUTOP_PWR_STA_MASK | MP1_CPU1_PWR_STA_MASK |
		  MP1_CPU2_PWR_STA_MASK | MP1_CPU3_PWR_STA_MASK))
	    && !(spm_read(PWR_STATUS_2ND) &
		 (MP0_CPU0_PWR_STA_MASK | MP0_CPU1_PWR_STA_MASK | MP0_CPU2_PWR_STA_MASK |
		  MP0_CPU3_PWR_STA_MASK | MP0_CPUTOP_PWR_STA_MASK | MP1_CPU1_PWR_STA_MASK |
		  MP1_CPU2_PWR_STA_MASK | MP1_CPU3_PWR_STA_MASK));
}


/**************************************
 * for non-CPU MTCMOS
 **************************************/
/*static DEFINE_SPINLOCK(spm_noncpu_lock);*/


#define spm_mtcmos_noncpu_lock(flags)   \
do {    \
    spin_lock_irqsave(&spm_noncpu_lock, flags);  \
} while (0)

#define spm_mtcmos_noncpu_unlock(flags) \
do {    \
    spin_unlock_irqrestore(&spm_noncpu_lock, flags);    \
} while (0)


int spm_mtcmos_ctrl_vdec(int state)
{
	int err = 0;

	spm_mtcmos_cpu_init();
	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VDE" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | VDE_SRAM_PDN);
		/* TINFO="Wait until VDE_SRAM_PDN_ACK = 1" */
		while (!(spm_read(VDE_PWR_CON) & VDE_SRAM_PDN_ACK)) {
		}
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~PWR_ON_2ND);

#ifndef CFG_FPGA_PLATFORM
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VDE_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Finish to turn off VDE" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on VDE" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & VDE_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~(0x1 << 8));
		/* TINFO="Wait until VDE_SRAM_PDN_ACK = 0" */
		while (spm_read(VDE_PWR_CON) & VDE_SRAM_PDN_ACK) {
		}
		/* TINFO="Finish to turn on VDE" */
	}
	return err;
}

int spm_mtcmos_ctrl_venc(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VEN" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | VEN_SRAM_PDN);
		/* TINFO="Wait until VEN_SRAM_PDN_ACK = 1" */
		while (!(spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK)) {
		}
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~PWR_ON_2ND);

		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VEN_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
		}

		/* TINFO="Finish to turn off VEN" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on VEN" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | PWR_ON_2ND);

		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & VEN_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
		}

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 8));
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 9));
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 10));
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 11));
		/* TINFO="Wait until VEN_SRAM_PDN_ACK = 0" */
		while (spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK) {
		}
		/* TINFO="Finish to turn on VEN" */
	}
	return err;
}

int spm_mtcmos_ctrl_isp(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ISP" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | ISP_SRAM_PDN);
		/* TINFO="Wait until ISP_SRAM_PDN_ACK = 1" */
		while (!(spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK)) {
		}
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & ISP_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Finish to turn off ISP" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on ISP" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & ISP_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~(0x1 << 8));
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~(0x1 << 9));
		/* TINFO="Wait until ISP_SRAM_PDN_ACK = 0" */
		while (spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK) {
		}
		/* TINFO="Finish to turn on ISP" */
	}
	return err;
}


int spm_mtcmos_ctrl_disp(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DIS" */
		/* TINFO="Set bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN, spm_read(INFRA_TOPAXI_PROTECTEN) | DIS_PROT_MASK);
		while ((spm_read(INFRA_TOPAXI_PROTECTSTA1) & DIS_PROT_MASK) != DIS_PROT_MASK) {
		}
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | DIS_SRAM_PDN);
		/* TINFO="Wait until DIS_SRAM_PDN_ACK = 1" */
		while (!(spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK)) {
		}
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & DIS_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Finish to turn off DIS" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on DIS" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & DIS_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~(0x1 << 8));
		/* TINFO="Wait until DIS_SRAM_PDN_ACK = 0" */
		while (spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK) {
		}
		/* TINFO="Release bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN,
			  spm_read(INFRA_TOPAXI_PROTECTEN) & ~DIS_PROT_MASK);
		while (spm_read(INFRA_TOPAXI_PROTECTSTA1) & DIS_PROT_MASK) {
		}
		/* TINFO="Finish to turn on DIS" */
	}
	return err;
}

int spm_mtcmos_ctrl_mfg(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG" */
		/* TINFO="Set bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN, spm_read(INFRA_TOPAXI_PROTECTEN) | MFG_PROT_MASK);
		while ((spm_read(INFRA_TOPAXI_PROTECTSTA1) & MFG_PROT_MASK) != MFG_PROT_MASK) {
		}
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | MFG_SRAM_PDN);
		/* TINFO="Wait until MFG_SRAM_PDN_ACK = 1" */
		while (!(spm_read(MFG_PWR_CON) & MFG_SRAM_PDN_ACK)) {
		}
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Finish to turn off MFG" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on MFG" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MFG_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~(0x1 << 8));
		/* TINFO="Wait until MFG_SRAM_PDN_ACK = 0" */
		while (spm_read(MFG_PWR_CON) & MFG_SRAM_PDN_ACK) {
		}
		/* TINFO="Release bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN,
			  spm_read(INFRA_TOPAXI_PROTECTEN) & ~MFG_PROT_MASK);
		while (spm_read(INFRA_TOPAXI_PROTECTSTA1) & MFG_PROT_MASK) {
		}
		/* TINFO="Finish to turn on MFG" */
	}
	return err;
}

int spm_mtcmos_ctrl_mfg_ASYNC(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG_ASYNC" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG_ASYNC_PWR_CON, spm_read(MFG_ASYNC_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG_ASYNC_PWR_CON, spm_read(MFG_ASYNC_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG_ASYNC_PWR_CON, spm_read(MFG_ASYNC_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG_ASYNC_PWR_CON, spm_read(MFG_ASYNC_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG_ASYNC_PWR_CON, spm_read(MFG_ASYNC_PWR_CON) & ~PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Finish to turn off MFG_ASYNC" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on MFG_ASYNC" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG_ASYNC_PWR_CON, spm_read(MFG_ASYNC_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG_ASYNC_PWR_CON, spm_read(MFG_ASYNC_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_ASYNC_PWR_CON, spm_read(MFG_ASYNC_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_ASYNC_PWR_CON, spm_read(MFG_ASYNC_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_ASYNC_PWR_CON, spm_read(MFG_ASYNC_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MFG_ASYNC" */
	}
	return err;
}

void DFS4MD_IPI_handler(int id, void *data, unsigned int len)
{
	return;
}

int spm_mtcmos_ctrl_mdsys1(int state)
{
	int err = 0;
	int count = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MD1" */
		/* TINFO="Set bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1,
			  spm_read(INFRA_TOPAXI_PROTECTEN_1) | MD1_PROT_MASK);
		while ((spm_read(INFRA_TOPAXI_PROTECTSTA1_1) & MD1_PROT_MASK) != MD1_PROT_MASK) {
			count++;
			if (count > 1000)
				break;
		}
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | MD1_SRAM_PDN);
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MD1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MD1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif

		/* TINFO="MD_EXT_BUCK_ISO[0]=1" */
		spm_write(MD_EXT_BUCK_ISO, spm_read(MD_EXT_BUCK_ISO) | (0x1 << 0));
		/* TINFO="Finish to turn off MD1" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on MD1" */
		/* TINFO="MD_EXT_BUCK_ISO[0]=0" */
		spm_write(MD_EXT_BUCK_ISO, spm_read(MD_EXT_BUCK_ISO) & ~(0x1 << 0));
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MD1_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MD1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~(0x1 << 8));
		/* TINFO="Release bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1,
			  spm_read(INFRA_TOPAXI_PROTECTEN_1) & ~MD1_PROT_MASK);
		while (spm_read(INFRA_TOPAXI_PROTECTSTA1_1) & MD1_PROT_MASK) {
		}
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MD1" */
	}
	return err;
}

int spm_mtcmos_ctrl_mdsys2(int state)
{
	int err = 0;
	int count = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off C2K" */
		/* TINFO="Set bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1,
			  spm_read(INFRA_TOPAXI_PROTECTEN_1) | C2K_PROT_MASK);
		while ((spm_read(INFRA_TOPAXI_PROTECTSTA1_1) & C2K_PROT_MASK) != C2K_PROT_MASK) {
			count++;
			if (count > 1000)
				break;
		}
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(C2K_PWR_CON, spm_read(C2K_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(C2K_PWR_CON, spm_read(C2K_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(C2K_PWR_CON, spm_read(C2K_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(C2K_PWR_CON, spm_read(C2K_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(C2K_PWR_CON, spm_read(C2K_PWR_CON) & ~PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & C2K_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & C2K_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif

		/* TINFO="Finish to turn off C2K" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on C2K" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(C2K_PWR_CON, spm_read(C2K_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(C2K_PWR_CON, spm_read(C2K_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & C2K_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & C2K_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(C2K_PWR_CON, spm_read(C2K_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(C2K_PWR_CON, spm_read(C2K_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(C2K_PWR_CON, spm_read(C2K_PWR_CON) | PWR_RST_B);
		/* TINFO="Release bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1,
			  spm_read(INFRA_TOPAXI_PROTECTEN_1) & ~C2K_PROT_MASK);
		while (spm_read(INFRA_TOPAXI_PROTECTSTA1_1) & C2K_PROT_MASK) {
		}
		/* TINFO="Finish to turn on C2K" */
	}
	return err;
}

int spm_mtcmos_ctrl_mdsys_intf_infra(int state)
{
	int err = 0;
	int count = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MDSYS_INTF_INFRA" */
		/* TINFO="Set bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN,
			  spm_read(INFRA_TOPAXI_PROTECTEN) | MDSYS_INTF_INFRA_PROT_MASK);
		while ((spm_read(INFRA_TOPAXI_PROTECTSTA1) & MDSYS_INTF_INFRA_PROT_MASK) !=
		       MDSYS_INTF_INFRA_PROT_MASK) {
			count++;
			if (count > 1000)
				break;
		}
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MDSYS_INTF_INFRA_PWR_CON, spm_read(MDSYS_INTF_INFRA_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MDSYS_INTF_INFRA_PWR_CON,
			  spm_read(MDSYS_INTF_INFRA_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MDSYS_INTF_INFRA_PWR_CON,
			  spm_read(MDSYS_INTF_INFRA_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MDSYS_INTF_INFRA_PWR_CON, spm_read(MDSYS_INTF_INFRA_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MDSYS_INTF_INFRA_PWR_CON,
			  spm_read(MDSYS_INTF_INFRA_PWR_CON) & ~PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MDSYS_INTF_INFRA_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MDSYS_INTF_INFRA_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif

		/* TINFO="Finish to turn off MDSYS_INTF_INFRA" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on MDSYS_INTF_INFRA" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MDSYS_INTF_INFRA_PWR_CON, spm_read(MDSYS_INTF_INFRA_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MDSYS_INTF_INFRA_PWR_CON,
			  spm_read(MDSYS_INTF_INFRA_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & MDSYS_INTF_INFRA_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & MDSYS_INTF_INFRA_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MDSYS_INTF_INFRA_PWR_CON,
			  spm_read(MDSYS_INTF_INFRA_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MDSYS_INTF_INFRA_PWR_CON, spm_read(MDSYS_INTF_INFRA_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MDSYS_INTF_INFRA_PWR_CON, spm_read(MDSYS_INTF_INFRA_PWR_CON) | PWR_RST_B);
		/* TINFO="Release bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN,
			  spm_read(INFRA_TOPAXI_PROTECTEN) & ~MDSYS_INTF_INFRA_PROT_MASK);
		while (spm_read(INFRA_TOPAXI_PROTECTSTA1) & MDSYS_INTF_INFRA_PROT_MASK) {
		}
		/* TINFO="Finish to turn on MDSYS_INTF_INFRA" */
	}
	return err;
}

int spm_mtcmos_ctrl_connsys(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CONN" */
		/* TINFO="Set bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN,
			  spm_read(INFRA_TOPAXI_PROTECTEN) | CONN_PROT_MASK);
		while ((spm_read(INFRA_TOPAXI_PROTECTSTA1) & CONN_PROT_MASK) != CONN_PROT_MASK) {
		}
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CONN_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Finish to turn off CONN" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on CONN" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & CONN_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_RST_B);
		/* TINFO="Release bus protect" */
		spm_write(INFRA_TOPAXI_PROTECTEN,
			  spm_read(INFRA_TOPAXI_PROTECTEN) & ~CONN_PROT_MASK);
		while (spm_read(INFRA_TOPAXI_PROTECTSTA1) & CONN_PROT_MASK) {
		}
		/* TINFO="Finish to turn on CONN" */
	}
	return err;
}

int spm_mtcmos_ctrl_mjc(int state)
{
	return 0;
}

int spm_mtcmos_ctrl_aud(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off AUDIO" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | AUDIO_SRAM_PDN);
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK = 1" */
		while (!(spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK)) {
		}
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & AUDIO_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & AUDIO_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Finish to turn off AUDIO" */
	} else {		/* STA_POWER_ON */
		/* TINFO="Start to turn on AUDIO" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_ON_2ND);

#ifndef IGNORE_PWR_ACK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (!(spm_read(PWR_STATUS) & AUDIO_PWR_STA_MASK)
		       || !(spm_read(PWR_STATUS_2ND) & AUDIO_PWR_STA_MASK)) {
		}
#endif

		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 8));
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 9));
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 10));
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 11));
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK) {
		}
		/* TINFO="Finish to turn on AUDIO" */
	}
	return err;
}

int spm_topaxi_prot(int bit, int en)
{
	/*unsigned long flags;*/
	/* XXX: only in kernel */
	/* spm_mtcmos_noncpu_lock(flags); */

	if (en == 1) {
		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | (1 << bit));
		while ((spm_read(TOPAXI_PROT_STA1) & (1 << bit)) != (1 << bit))
			;
	} else {
		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~(1 << bit));
		while (spm_read(TOPAXI_PROT_STA1) & (1 << bit))
			;
	}

	/* XXX: only in kernel */
	/* spm_mtcmos_noncpu_unlock(flags); */

	return 0;
}

static int mt_spm_mtcmos_init(void)
{
	return 0;
}
module_init(mt_spm_mtcmos_init);
