#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/types.h>

#ifdef CONFIG_MTK_HIBERNATION
#include <mach/mtk_hibernate_dpm.h>
#endif

#ifdef CONFIG_MTK_CLKMGR
/* mt_clkmgr */
#include <mach/mt_clkmgr.h>
#else
/* CCF */
#include <linux/clk.h>
#endif

#include "mach/mt_reg_base.h"
#include "mach/mt_device_apc.h"
#include "mach/mt_typedefs.h"
#include "mach/sync_write.h"
#include "mach/irqs.h"
#include "devapc.h"

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(DEVAPC_ENABLE_AEE)
#include <linux/aee.h>
#endif

/* Debug message event */
#define DEVAPC_LOG_NONE        0x00000000
#define DEVAPC_LOG_ERR         0x00000001
#define DEVAPC_LOG_WARN        0x00000002
#define DEVAPC_LOG_INFO        0x00000004
#define DEVAPC_LOG_DBG         0x00000008

#define DEVAPC_LOG_LEVEL      (DEVAPC_LOG_DBG)

#define DEVAPC_MSG(fmt, args...) \
	do {    \
		if (DEVAPC_LOG_LEVEL & DEVAPC_LOG_DBG) { \
			pr_debug(fmt, ##args); \
		} else if (DEVAPC_LOG_LEVEL & DEVAPC_LOG_INFO) { \
			pr_info(fmt, ##args); \
		} else if (DEVAPC_LOG_LEVEL & DEVAPC_LOG_WARN) { \
			pr_warn(fmt, ##args); \
		} else if (DEVAPC_LOG_LEVEL & DEVAPC_LOG_ERR) { \
			pr_err(fmt, ##args); \
		} \
	} while (0)


#define DEVAPC_VIO_LEVEL      (DEVAPC_LOG_ERR)

#define DEVAPC_VIO_MSG(fmt, args...) \
	do {    \
		if (DEVAPC_VIO_LEVEL & DEVAPC_LOG_DBG) { \
			pr_debug(fmt, ##args); \
		} else if (DEVAPC_VIO_LEVEL & DEVAPC_LOG_INFO) { \
			pr_info(fmt, ##args); \
		} else if (DEVAPC_VIO_LEVEL & DEVAPC_LOG_WARN) { \
			pr_warn(fmt, ##args); \
		} else if (DEVAPC_VIO_LEVEL & DEVAPC_LOG_ERR) { \
			pr_err(fmt, ##args); \
		} \
	} while (0)



#ifndef CONFIG_MTK_CLKMGR

#if 0
/* bypass clock! */
static struct clk *dapc_clk;
#endif

#endif

static struct cdev *g_devapc_ctrl;
static unsigned int devapc_irq;
static void __iomem *devapc_pd_base;

static unsigned int enable_dynamic_one_core_violation_debug;

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(DEVAPC_ENABLE_AEE)
static kal_uint64 devapc_vio_first_trigger_time[DEVAPC_TOTAL_SLAVES];

/* violation times */
static unsigned int devapc_vio_count[DEVAPC_TOTAL_SLAVES];

/* show the status of whether AEE warning has been populated */
static unsigned int devapc_vio_aee_shown[DEVAPC_TOTAL_SLAVES];
static unsigned int devapc_vio_current_aee_trigger_times;
#endif

static struct DEVICE_INFO devapc_devices[] = {
	/*device name                          enable_vio_irq */
	/*0 */
	{"INFRA_AO_TOP_LEVEL_CLOCK_GENERATOR", TRUE},
	{"INFRA_AO_INFRASYS_CONFIG_REGS", TRUE},
	{"INFRA_AO_IO_CFG", TRUE},
	{"INFRA_AO_PERICFG", TRUE},
	{"INFRA_AO_DRAM_CONTROLLER", TRUE},
	{"INFRA_AO_GPIO", TRUE},
	{"INFRA_AO_TOP_LEVEL_SLP_MANAGER", TRUE},
	{"INFRA_AO_TOP_RGU", TRUE},
	{"INFRA_AO_APXGPT", TRUE},
	{"INFRA_AO_RESERVE_REGION", TRUE},

	/*10 */
	{"INFRA_AO_SEJ", TRUE},
	{"INFRA_AO_AP_CIRQ_EINT", TRUE},
	{"INFRA_AO_APMIXEDSYS_ FHCTL", TRUE},
	{"INFRA_AO_PMIC_WRAP", TRUE},
	{"INFRA_AO_DEVICE_APC_AO", TRUE},
	{"INFRA_AO_DDRPHY", TRUE},
	{"INFRA_AO_KEYPAD", TRUE},
	{"INFRA_AO_TOP_MISC", TRUE},
	{"INFRA_AO_RESERVE_REGION", TRUE},
	{"INFRA_AO_RESERVE_REGION", TRUE},

	/*20 */
	{"INFRA_AO_CLDMA_AO_TOP_AP", TRUE},
	{"INFRA_AO_CLDMA_AO_TOP_MD", TRUE},
	{"INFRA_AO_AES_TOP0", TRUE},
	{"INFRA_AO_AES_TOP1", TRUE},
	{"INFRA_AO_MDEM_TEMP_SHARE", TRUE},
	{"INFRA_AO_SLEEP_CONTROL_PROCESSOR", TRUE},
	{"INFRASYS_MCUSYS_CONFIG_REG", TRUE},
	{"INFRASYS_CONTROL_REG", TRUE},
	{"INFRASYS_BOOTROM/SRAM", TRUE},
	{"INFRASYS_EMI_BUS_INTERFACE", TRUE},

	/*30 */
	{"INFRASYS_SYSTEM_CIRQ", TRUE},
	{"INFRASYS_MM_IOMMU_CONFIGURATION", TRUE},
	{"INFRASYS_EFUSEC", TRUE},
	{"INFRASYS_DEVICE_APC_MONITOR", TRUE},
	{"INFRASYS_DEBUG_TRACKER", TRUE},
	{"INFRASYS_CCI0_AP", TRUE},
	{"INFRASYS_CCI0_MD", TRUE},
	{"INFRASYS_CCI1_AP", TRUE},
	{"INFRASYS_CCI1_MD", TRUE},
	{"INFRASYS_MBIST_CONTROL_REG", TRUE},

	/*40 */
	{"INFRASYS_DRAM_CONTROLLER", TRUE},
	{"INFRASYS_TRNG", TRUE},
	{"INFRASYS_GCPU", TRUE},
	{"INFRASYS_MD_CCIF_MD1", TRUE},
	{"INFRASYS_GCE", TRUE},
	{"INFRASYS_MD_CCIF_MD2", TRUE},
	{"INFRASYS_RESERVE", TRUE},
	{"INFRASYS_ANA_MIPI_DSI0", TRUE},
	{"INFRASYS_ANA_MIPI_DSI1", TRUE},
	{"INFRASYS_ANA_MIPI_CSI0", TRUE},

	/*50 */
	{"INFRASYS_ANA_MIPI_CSI1", TRUE},
	{"INFRASYS_ANA_MIPI_CSI2", TRUE},
	{"INFRASYS_GPU_RSA", TRUE},
	{"INFRASYS_CLDMA_PDN_AP", TRUE},
	{"INFRASYS_CLDMA_PDN_MD", TRUE},
	{"INFRASYS_MDSYS_INTF", TRUE},
	{"INFRASYS_BPI_BIS_SLV0", TRUE},
	{"INFRASYS_BPI_BIS_SLV1", TRUE},
	{"INFRASYS_BPI_BIS_SLV2", TRUE},
	{"INFRAYS_EMI_MPU_REG", TRUE},

	/*60 */
	{"INFRAYS_DVFS_PROC", TRUE},
	{"INFRASYS_DEBUGTOP", TRUE},
	{"DMA", TRUE},
	{"AUXADC", TRUE},
	{"UART0", TRUE},
	{"UART1", TRUE},
	{"UART2", TRUE},
	{"UART3", TRUE},
	{"PWM", TRUE},
	{"I2C0", TRUE},

	/*70 */
	{"I2C1", TRUE},
	{"I2C2", TRUE},
	{"SPI0", TRUE},
	{"PTP_THERMAL_CTL", TRUE},
	{"BTIF", TRUE},
	{"IRTX", TRUE},
	{"DISP_PWM", TRUE},
	{"I2C3", TRUE},
	{"SPI1", TRUE},
	{"I2C4", TRUE},

	/*80 */
	{"RESERVE", TRUE},
	{"RESERVE", TRUE},
	{"USB2.0", TRUE},
	{"USB2.0 SIF", TRUE},
	{"MSDC0", TRUE},
	{"MSDC1", TRUE},
	{"MSDC2", TRUE},
	{"MSDC3", TRUE},
	{"USB3.0", TRUE},
	{"USB3.0 SIF", TRUE},

	/*90 */
	{"USB3.0 SIF2", TRUE},
	{"AUDIO", TRUE},
	{"WCN AHB SLAVE", TRUE},
	{"MD_PERIPHERALS", TRUE},
	{"MD2_PERIPHERALS", TRUE},
	{"RESERVE", TRUE},
	{"RESERVE", TRUE},
	{"RESERVE", TRUE},
	{"G3D_CONFIG_0", TRUE},
	{"G3D_CONFIG_1", TRUE},

	/*100 */
	{"G3D_CONFIG_2", TRUE},
	{"G3D_CONFIG_3", TRUE},
	{"G3D_CONFIG_4", TRUE},
	{"G3D_POWER_CONTROL", TRUE},
	{"MALI_CONFIG", TRUE},
	{"MMSYS_CONFIG", TRUE},
	{"MDP_RDMA", TRUE},
	{"MDP_RSZ0", TRUE},
	{"MDP_RSZ1", TRUE},
	{"MDP_WDMA", TRUE},

	/*110 */
	{"MDP_WROT", TRUE},
	{"MDP_TDSHP", TRUE},
	{"MDP_COLOR", TRUE},
	{"DISP_OVL0", TRUE},
	{"DISP_OVL1", TRUE},
	{"DISP_RDMA0", TRUE},
	{"DISP_RDMA1", TRUE},
	{"DISP_WDMA0", TRUE},
	{"DISP_COLOR", TRUE},
	{"DISP_CCORR", TRUE},

	/*120 */
	{"DISP_AAL", TRUE},
	{"DISP_GAMMA", TRUE},
	{"DISP_DITHER", TRUE},
	{"DSI", TRUE},
	{"DPI", TRUE},
	{"MM_MUTEX", TRUE},
	{"SMI_LARB0", TRUE},
	{"SMI_COMMON", TRUE},
	{"DISP_WDMA1", TRUE},
	{"DISP_OVL0_2L", TRUE},

	/*130 */
	{"DISP_OVL1_2L", TRUE},
	{"RESERVE", TRUE},
	{"RESERVE", TRUE},
	{"IMGSYS_CONFIG", TRUE},
	{"SMI_LARB2", TRUE},
	{"RESERVE", TRUE},
	{"RESERVE", TRUE},
	{"CAM0", TRUE},
	{"CAM1", TRUE},
	{"CAM2", TRUE},

	/*140 */
	{"CAM3", TRUE},
	{"SENINF", TRUE},
	{"CAMSV", TRUE},
	{"RESERVE", TRUE},
	{"FD", TRUE},
	{"MIPI_RX", TRUE},
	{"CAM0", TRUE},
	{"CAM2", TRUE},
	{"CAM3", TRUE},
	{"VDEC_GLOBAL_CON", TRUE},

	/*150 */
	{"SMI_LARB1", TRUE},
	{"VDEC_FULL_TOP", TRUE},
	{"VENC_GLOBAL_CON", TRUE},
	{"SMI_LARB3", TRUE},
	{"VENC", TRUE},
	{"JPEGENC", TRUE},
	{"JPEGDEC", TRUE},

};

/*****************************************************************************
*FUNCTION DEFINITION
*****************************************************************************/
static int clear_vio_status(unsigned int module);
static int devapc_ioremap(void);

/**************************************************************************
*EXTERN FUNCTION
**************************************************************************/
int mt_devapc_emi_initial(void)
{
	devapc_ioremap();

	if (NULL != devapc_pd_base) {
		mt_reg_sync_writel(readl(IOMEM(DEVAPC0_PD_APC_CON)) & (0xFFFFFFFF ^ (1 << 2)),
				   DEVAPC0_PD_APC_CON);
		mt_reg_sync_writel(ABORT_EMI, DEVAPC0_D0_VIO_STA_5);
		mt_reg_sync_writel(ABORT_EMI_MPU, DEVAPC0_D0_VIO_STA_5);

		/* Notice: You cannot unmask Type B slave (e.g. EMI, EMI_MPU) unless unregistering IRQ */

		pr_err("[DEVAPC] EMI_MPU Init done\n");
		return 0;
	}

	return -1;
}

int mt_devapc_check_emi_violation(void)
{
	if ((readl(IOMEM(DEVAPC0_D0_VIO_STA_5)) & ABORT_EMI) == 0)
		return -1;

	DEVAPC_VIO_MSG("EMI violation...\n");
	return 0;
}

int mt_devapc_check_emi_mpu_violation(void)
{
	if ((readl(IOMEM(DEVAPC0_D0_VIO_STA_5)) & ABORT_EMI_MPU) == 0)
		return -1;

	DEVAPC_VIO_MSG("EMI_MPU violation...\n");
	return 0;
}

int mt_devapc_clear_emi_violation(void)
{
	if ((readl(IOMEM(DEVAPC0_D0_VIO_STA_5)) & ABORT_EMI) != 0)
		mt_reg_sync_writel(ABORT_EMI, DEVAPC0_D0_VIO_STA_5);

	return 0;
}

int mt_devapc_clear_emi_mpu_violation(void)
{
	if ((readl(IOMEM(DEVAPC0_D0_VIO_STA_5)) & ABORT_EMI_MPU) != 0)
		mt_reg_sync_writel(ABORT_EMI_MPU, DEVAPC0_D0_VIO_STA_5);

	return 0;
}

/**************************************************************************
*STATIC FUNCTION
**************************************************************************/

static int devapc_ioremap(void)
{
	if (NULL == devapc_pd_base) {
		struct device_node *node = NULL;
		/*IO remap */

		node = of_find_compatible_node(NULL, NULL, "mediatek,devapc");
		if (node) {
			devapc_pd_base = of_iomap(node, 0);
			devapc_irq = irq_of_parse_and_map(node, 0);
			DEVAPC_MSG("[DEVAPC] PD_ADDRESS %p, IRD: %d\n", devapc_pd_base, devapc_irq);
		} else {
			pr_err("[DEVAPC] can't find DAPC_PD compatible node\n");
			return -1;
		}
	}

	return 0;
}

#ifdef CONFIG_MTK_HIBERNATION
static int devapc_pm_restore_noirq(struct device *device)
{
	if (devapc_irq != 0) {
		mt_irq_set_sens(devapc_irq, MT_LEVEL_SENSITIVE);
		mt_irq_set_polarity(devapc_irq, MT_POLARITY_LOW);
	}

	return 0;
}
#endif

static void unmask_module_irq(unsigned int module)
{
	unsigned int apc_index = 0;
	unsigned int apc_bit_index = 0;

	apc_index = module / (MOD_NO_IN_1_DEVAPC*2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC*2);

	switch (apc_index) {
	case 0:
		*DEVAPC0_D0_VIO_MASK_0 &= ~(0x1 << apc_bit_index);
		break;
	case 1:
		*DEVAPC0_D0_VIO_MASK_1 &= ~(0x1 << apc_bit_index);
		break;
	case 2:
		*DEVAPC0_D0_VIO_MASK_2 &= ~(0x1 << apc_bit_index);
		break;
	case 3:
		*DEVAPC0_D0_VIO_MASK_3 &= ~(0x1 << apc_bit_index);
		break;
	case 4:
		*DEVAPC0_D0_VIO_MASK_4 &= ~(0x1 << apc_bit_index);
		break;
	case 5:
		*DEVAPC0_D0_VIO_MASK_5 &= ~(0x1 << apc_bit_index);
		break;
	default:
		pr_err("[DEVAPC] unmask_module_irq: check if domain master setting is correct or not!\n");
		break;
	}
}

static void start_devapc(void)
{
	unsigned int i;

	mt_reg_sync_writel(readl(DEVAPC0_PD_APC_CON) & (0xFFFFFFFF ^ (1 << 2)), DEVAPC0_PD_APC_CON);

	/* SMC call is called in LK instead */

	for (i = 0; i < (ARRAY_SIZE(devapc_devices)); i++) {
		if (TRUE == devapc_devices[i].enable_vio_irq)
			unmask_module_irq(i);
	}

	pr_err("[DEVAPC] start_devapc: Current VIO_STA 0:0x%x, 1:0x%x, 2:0x%x, 3:0x%x, 4:0x%x, 5:0x%x\n",
		       readl(DEVAPC0_D0_VIO_STA_0), readl(DEVAPC0_D0_VIO_STA_1),
		       readl(DEVAPC0_D0_VIO_STA_2), readl(DEVAPC0_D0_VIO_STA_3),
		       readl(DEVAPC0_D0_VIO_STA_4), readl(DEVAPC0_D0_VIO_STA_5));

	pr_err("[DEVAPC] start_devapc: Current VIO_MASK 0:0x%x, 1:0x%x, 2:0x%x, 3:0x%x, 4:0x%x, 5:0x%x\n",
		       readl(DEVAPC0_D0_VIO_MASK_0), readl(DEVAPC0_D0_VIO_MASK_1),
		       readl(DEVAPC0_D0_VIO_MASK_2), readl(DEVAPC0_D0_VIO_MASK_3),
		       readl(DEVAPC0_D0_VIO_MASK_4), readl(DEVAPC0_D0_VIO_MASK_5));
}

static int clear_vio_status(unsigned int module)
{

	unsigned int apc_index = 0;
	unsigned int apc_bit_index = 0;

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	switch (apc_index) {
	case 0:
		*DEVAPC0_D0_VIO_STA_0 = (0x1 << apc_bit_index);
		break;
	case 1:
		*DEVAPC0_D0_VIO_STA_1 = (0x1 << apc_bit_index);
		break;
	case 2:
		*DEVAPC0_D0_VIO_STA_2 = (0x1 << apc_bit_index);
		break;
	case 3:
		*DEVAPC0_D0_VIO_STA_3 = (0x1 << apc_bit_index);
		break;
	case 4:
		*DEVAPC0_D0_VIO_STA_4 = (0x1 << apc_bit_index);
		break;
	case 5:
		*DEVAPC0_D0_VIO_STA_5 = (0x1 << apc_bit_index);
		break;
	default:
		break;
	}

	return 0;
}

static int check_vio_status(unsigned int module)
{
	unsigned int apc_index = 0;
	unsigned int apc_bit_index = 0;
	unsigned int vio_status = 0;

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	switch (apc_index) {
	case 0:
		vio_status = (*DEVAPC0_D0_VIO_STA_0 & (0x1 << apc_bit_index));
		break;
	case 1:
		vio_status = (*DEVAPC0_D0_VIO_STA_1 & (0x1 << apc_bit_index));
		break;
	case 2:
		vio_status = (*DEVAPC0_D0_VIO_STA_2 & (0x1 << apc_bit_index));
		break;
	case 3:
		vio_status = (*DEVAPC0_D0_VIO_STA_3 & (0x1 << apc_bit_index));
		break;
	case 4:
		vio_status = (*DEVAPC0_D0_VIO_STA_4 & (0x1 << apc_bit_index));
		break;
	case 5:
		vio_status = (*DEVAPC0_D0_VIO_STA_5 & (0x1 << apc_bit_index));
		break;
	default:
		break;
	}

	if (vio_status)
		return 1;

	return 0;
}

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(DEVAPC_ENABLE_AEE)
static void evaluate_aee_warning(unsigned int i, unsigned int dbg1)
{
	char aee_str[256];
	kal_uint64 current_time;

	if (devapc_vio_aee_shown[i] == 0) {
		if (devapc_vio_count[i] < DEVAPC_VIO_AEE_TRIGGER_TIMES) {
			devapc_vio_count[i]++;

			if (devapc_vio_count[i] == 1) {
				/* violation for this slave is triggered the first time */

				/* get current time from start-up in ns */
				devapc_vio_first_trigger_time[i] = sched_clock();
#if 0
				DEVAPC_VIO_MSG("[DEVAPC] devapc_vio_first_trigger_time: %llu\n",
					devapc_vio_first_trigger_time[i] / 1000000); /* ms */
#endif
			}
		}

		if (devapc_vio_count[i] >= DEVAPC_VIO_AEE_TRIGGER_TIMES) {
			current_time = sched_clock(); /* get current time from start-up in ns */
#if 0
			DEVAPC_VIO_MSG("[DEVAPC] current_time: %llu\n",
				current_time / 1000000); /* ms */
			DEVAPC_VIO_MSG("[DEVAPC] devapc_vio_count[%d]: %d\n",
				i, devapc_vio_count[i]);
#endif
			if (((current_time - devapc_vio_first_trigger_time[i]) / 1000000) <=
					(kal_uint64)DEVAPC_VIO_AEE_TRIGGER_FREQUENCY) {  /* diff time by ms */
				/* Mark the flag for showing AEE (AEE should be shown only once) */
				devapc_vio_aee_shown[i] = 1;

				DEVAPC_VIO_MSG("[DEVAPC] Populating AEE Warning...\n");

				if (devapc_vio_current_aee_trigger_times <
						DEVAPC_VIO_MAX_TOTAL_MODULE_AEE_TRIGGER_TIMES) {
					devapc_vio_current_aee_trigger_times++;

					sprintf(aee_str, "[DEVAPC] Access Violation Slave: %s (index=%d)\n",
						devapc_devices[i].device, i);

					aee_kernel_warning(aee_str,
						"%s\nAccess Violation Slave: %s\nVio Addr: 0x%x\n%s%s\n",
						"Device APC Violation",
						devapc_devices[i].device,
						dbg1,
						"CRDISPATCH_KEY:Device APC Violation Issue/",
						devapc_devices[i].device
					);
				}
			}

			devapc_vio_count[i] = 0;
		}
	}
}
#endif

static irqreturn_t devapc_violation_irq(int irq, void *dev_id)
{
	unsigned int dbg0 = 0, dbg1 = 0;
	unsigned int master_id;
	unsigned int domain_id;
	unsigned int r_w_violation;
	unsigned int device_count;
	unsigned int i;
	struct pt_regs *regs;

	dbg0 = readl(DEVAPC0_VIO_DBG0);
	dbg1 = readl(DEVAPC0_VIO_DBG1);
	master_id = dbg0 & VIO_DBG_MSTID;
	domain_id = dbg0 & VIO_DBG_DMNID;
	r_w_violation = dbg0 & VIO_DBG_RW;

	/* violation information improvement */
	if (1 == r_w_violation) {
		DEVAPC_VIO_MSG
		    ("[DEVAPC] Violation(W) - Process:%s, PID:%i, Vio Addr:0x%x, Bus ID:0x%x, Dom ID:0x%x, DBG0:0x%x\n",
		     current->comm, current->pid, dbg1, master_id, domain_id, (*DEVAPC0_VIO_DBG0));
	} else {
		DEVAPC_VIO_MSG
		    ("[DEVAPC] Violation(R) - Process:%s, PID:%i, Vio Addr:0x%x, Bus ID:0x%x, Dom ID:0x%x, DBG0:0x%x\n",
		     current->comm, current->pid, dbg1, master_id, domain_id, (*DEVAPC0_VIO_DBG0));
	}

	DEVAPC_VIO_MSG("[DEVAPC] VIO_STA 0:0x%x, 1:0x%x, 2:0x%x, 3:0x%x, 4:0x%x, 5:0x%x\n",
		       readl(DEVAPC0_D0_VIO_STA_0), readl(DEVAPC0_D0_VIO_STA_1),
		       readl(DEVAPC0_D0_VIO_STA_2), readl(DEVAPC0_D0_VIO_STA_3),
		       readl(DEVAPC0_D0_VIO_STA_4), readl(DEVAPC0_D0_VIO_STA_5));

	device_count = (sizeof(devapc_devices) / sizeof(devapc_devices[0]));

	/* checking and showing violation EMI & EMI MPU slaves */
	if (check_vio_status(INDEX_EMI))
		DEVAPC_VIO_MSG("[DEVAPC] Access Violation Slave: EMI (index=%d)\n", INDEX_EMI);

	if (check_vio_status(INDEX_EMI_MPU))
		DEVAPC_VIO_MSG("[DEVAPC] Access Violation Slave: EMI_MPU (index=%d)\n", INDEX_EMI_MPU);

	/* checking and showing violation normal slaves */
	for (i = 0; i < device_count; i++) {
		/* violation information improvement */

		if ((check_vio_status(i)) && (devapc_devices[i].enable_vio_irq == TRUE)) {
			DEVAPC_VIO_MSG("[DEVAPC] Access Violation Slave: %s (index=%d)\n",
								devapc_devices[i].device, i);

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(DEVAPC_ENABLE_AEE)
			/* Frequency-based Violation AEE Warning (Under the condition that the violation
			 * for the module is not shown, it will trigger the AEE if "x" violations in "y" ms) */
			evaluate_aee_warning(i, dbg1);
#endif
		}

		clear_vio_status(i);
	}

	if ((DEVAPC_ENABLE_ONE_CORE_VIOLATION_DEBUG) || (enable_dynamic_one_core_violation_debug)) {
		DEVAPC_VIO_MSG("[DEVAPC] ====== Start dumping Device APC violation tracing ======\n");

		DEVAPC_VIO_MSG("[DEVAPC] **************** [All IRQ Registers] ****************\n");
		regs = get_irq_regs();
		show_regs(regs);

		DEVAPC_VIO_MSG("[DEVAPC] **************** [All Current Task Stack] ****************\n");
		show_stack(current, NULL);

		DEVAPC_VIO_MSG("[DEVAPC] ====== End of dumping Device APC violation tracing ======\n");
	}

	mt_reg_sync_writel(VIO_DBG_CLR, DEVAPC0_VIO_DBG0);
	dbg0 = readl(DEVAPC0_VIO_DBG0);
	dbg1 = readl(DEVAPC0_VIO_DBG1);

	if ((dbg0 != 0) || (dbg1 != 0)) {
		DEVAPC_VIO_MSG("[DEVAPC] Multi-violation!\n");
		DEVAPC_VIO_MSG("[DEVAPC] DBG0 = %x, DBG1 = %x\n", dbg0, dbg1);
	}

	return IRQ_HANDLED;
}

static int devapc_probe(struct platform_device *dev)
{
	int ret;

	DEVAPC_MSG("[DEVAPC] module probe.\n");

	/*IO remap */
	devapc_ioremap();

	/*Temporarily disable Device APC kernel driver for bring-up */
	/*
	 * Interrupts of vilation (including SPC in SMI, or EMI MPU) are triggered by the device APC.
	 * need to share the interrupt with the SPC driver.
	 */
	ret = request_irq(devapc_irq, (irq_handler_t) devapc_violation_irq,
			  IRQF_TRIGGER_LOW | IRQF_SHARED, "devapc", &g_devapc_ctrl);
	if (ret) {
		pr_err("[DEVAPC] Failed to request irq! (%d)\n", ret);
		return ret;
	}

#ifdef CONFIG_MTK_CLKMGR
/* mt_clkmgr */
	enable_clock(MT_CG_INFRA_DEVAPC, "DEVAPC");
#else
/* CCF */
#if 0
/* bypass clock! */
	dapc_clk = devm_clk_get(&dev->dev, "devapc-main");
	if (IS_ERR(dapc_clk)) {
		pr_err("[DEVAPC] cannot get dapc clock.\n");
		return PTR_ERR(dapc_clk);
	}
	clk_prepare_enable(dapc_clk);
#endif

#endif

#ifdef CONFIG_MTK_HIBERNATION
	register_swsusp_restore_noirq_func(ID_M_DEVAPC, devapc_pm_restore_noirq, NULL);
#endif

	start_devapc();

	return 0;
}

static int devapc_remove(struct platform_device *dev)
{
	return 0;
}

static int devapc_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int devapc_resume(struct platform_device *dev)
{
	DEVAPC_MSG("[DEVAPC] module resume.\n");

	return 0;
}

static ssize_t devapc_dbg_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	char cmd[10];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%s", cmd) == 1) {
		if (!strcmp(cmd, "1")) {
			enable_dynamic_one_core_violation_debug = 1;
			pr_err("[DEVAPC] One-Core Debugging: Enabled\n");
		} else if (!strcmp(cmd, "0")) {
			enable_dynamic_one_core_violation_debug = 0;
			pr_err("[DEVAPC] One-Core Debugging: Disabled\n");
		}
	}

	return count;
}

static int devapc_dbg_open(struct inode *inode, struct file *file)
{
	return 0;
}

struct platform_device devapc_device = {
	.name = "devapc",
	.id = -1,
};

static const struct file_operations devapc_dbg_fops = {
	.owner = THIS_MODULE,
	.open  = devapc_dbg_open,
	.write = devapc_dbg_write,
	.read = NULL,
};

static struct platform_driver devapc_driver = {
	.probe = devapc_probe,
	.remove = devapc_remove,
	.suspend = devapc_suspend,
	.resume = devapc_resume,
	.driver = {
		   .name = "devapc",
		   .owner = THIS_MODULE,
		   },
};

/*
 * devapc_init: module init function.
 */
static int __init devapc_init(void)
{
	int ret;

	DEVAPC_MSG("[DEVAPC] module init.\n");

	ret = platform_device_register(&devapc_device);
	if (ret) {
		pr_err("[DEVAPC] Unable to do device register(%d)\n", ret);
		return ret;
	}

	ret = platform_driver_register(&devapc_driver);
	if (ret) {
		pr_err("[DEVAPC] Unable to register driver (%d)\n", ret);
		platform_device_unregister(&devapc_device);
		return ret;
	}

	g_devapc_ctrl = cdev_alloc();
	if (!g_devapc_ctrl) {
		pr_err("[DEVAPC] Failed to add devapc device! (%d)\n", ret);
		platform_driver_unregister(&devapc_driver);
		platform_device_unregister(&devapc_device);
		return ret;
	}
	g_devapc_ctrl->owner = THIS_MODULE;

	proc_create("devapc_dbg", (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), NULL,
		&devapc_dbg_fops);

	return 0;
}

/*
 * devapc_exit: module exit function.
 */
static void __exit devapc_exit(void)
{
	DEVAPC_MSG("[DEVAPC] DEVAPC module exit\n");
#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_DEVAPC);
#endif
}

late_initcall(devapc_init);
module_exit(devapc_exit);
MODULE_LICENSE("GPL");
