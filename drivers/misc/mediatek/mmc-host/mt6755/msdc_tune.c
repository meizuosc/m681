#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <mach/board.h>
#include <core/core.h>
#include "mt_sd.h"
#include "dbg.h"
#include "autok.h"

#if (TUNE_METHOD == DATA_TUNE)
#define CMD_TUNE_HS_MAX_TIME            (2*63)
#define READ_DATA_TUNE_MAX_TIME         (2*63)
#define WRITE_DATA_TUNE_MAX_TIME        (2*63)

#elif (TUNE_METHOD == CLOCK_TUNE)
#define CMD_TUNE_UHS_MAX_TIME           (2*32*8*8)
#define CMD_TUNE_HS_MAX_TIME            (2*32)

#define READ_TUNE_UHS_CLKMOD1_MAX_TIME  (2*32*32*8)
#define READ_TUNE_UHS_MAX_TIME          (2*32*32)
#define READ_TUNE_HS_MAX_TIME           (2*32)

#define WRITE_TUNE_UHS_MAX_TIME         (2*32*8)
#define WRITE_TUNE_HS_MAX_TIME          (2*32)

#endif

#define MSDC_LOWER_FREQ
#define MSDC_MAX_FREQ_DIV               (2)
#define MSDC_MAX_TIMEOUT_RETRY          (1)
#define MSDC_MAX_TIMEOUT_RETRY_EMMC     (2)
#define MSDC_MAX_W_TIMEOUT_TUNE         (5)
#define MSDC_MAX_W_TIMEOUT_TUNE_EMMC    (64)
#define MSDC_MAX_R_TIMEOUT_TUNE         (3)
#define MSDC_MAX_POWER_CYCLE            (5)

#define MSDC_MAX_CONTINUOUS_FAIL_REQUEST_COUNT (50)

#ifdef CONFIG_EMMC_50_FEATURE
#define MAX_HS400_TUNE_COUNT            (576) /*(32*18)*/
#endif

#define CMD_SET_FOR_MMC_TUNE_CASE1      (0x00000000FB260140ULL)
#define CMD_SET_FOR_MMC_TUNE_CASE2      (0x0000000000000080ULL)
#define CMD_SET_FOR_MMC_TUNE_CASE3      (0x0000000000001000ULL)
#define CMD_SET_FOR_MMC_TUNE_CASE4      (0x0000000000000020ULL)
/*#define CMD_SET_FOR_MMC_TUNE_CASE5      (0x0000000000084000ULL)*/

#define CMD_SET_FOR_SD_TUNE_CASE1       (0x000000007B060040ULL)
#define CMD_SET_FOR_APP_TUNE_CASE1      (0x0008000000402000ULL)

#define IS_IN_CMD_SET(cmd_num, set)     ((0x1ULL << cmd_num) & (set))

#define MSDC_VERIFY_NEED_TUNE           (0)
#define MSDC_VERIFY_ERROR               (1)
#define MSDC_VERIFY_NEED_NOT_TUNE       (2)

u8 emmc_id;

int g_ett_tune = 0;       /* enable or disable the ETT tune */
int g_ett_hs400_tune = 0; /* record the number of failed HS400 ETT settings */
int g_ett_cmd_tune = 0;   /* record the number of failed command ETT settings*/
int g_ett_read_tune = 0;  /* record the number of failed read ETT settings */
int g_ett_write_tune = 0; /* record the number of failed write ETT settings */
int g_reset_tune = 0;     /* do not record the pass settigns, but try the worst
			     setting of each request. */

u32 sdio_tune_flag = 0;

int msdc_setting_parameter(struct msdc_hw *hw, unsigned int *para)
{
	struct tag_msdc_hw_para *msdc_para_hw_datap =
		(struct tag_msdc_hw_para *) para;

	if ((NULL == hw) || (msdc_para_hw_datap == NULL))
		return -1;

	if ((msdc_para_hw_datap->host_function != MSDC_SD)
	 && (msdc_para_hw_datap->host_function == MSDC_EMMC))
		return -1;

	if ((msdc_para_hw_datap->version == 0x5A01)
	 && (msdc_para_hw_datap->end_flag == 0x5a5a5a5a)) {
		memcpy(hw, msdc_para_hw_datap,
			sizeof(struct tag_msdc_hw_para));
		return 0;
	}
	return -1;
}

void msdc_reset_pwr_cycle_counter(struct msdc_host *host)
{
	host->power_cycle = 0;
	host->power_cycle_enable = 1;
}

void msdc_reset_tmo_tune_counter(struct msdc_host *host,
	TUNE_COUNTER index)
{
	switch (index) {
	case all_counter:
	case cmd_counter:
		if (host->rwcmd_time_tune != 0)
			ERR_MSG("TMO TUNE CMD Times(%d)",
				host->rwcmd_time_tune);
		host->rwcmd_time_tune = 0;
		if (index == cmd_counter)
			break;
	case read_counter:
		if (host->read_time_tune != 0)
			ERR_MSG("TMO TUNE READ Times(%d)",
				host->read_time_tune);
		host->read_time_tune = 0;
		if (index == read_counter)
			break;
	case write_counter:
		if (host->write_time_tune != 0)
			ERR_MSG("TMO TUNE WRITE Times(%d)",
				host->write_time_tune);
		host->write_time_tune = 0;
		break;
	default:
		ERR_MSG("msdc%d ==> reset tmo counter index(%d) error!\n",
			host->id, index);
		break;
	}
}

void msdc_reset_crc_tune_counter(struct msdc_host *host,
	TUNE_COUNTER index)
{
#ifdef CONFIG_EMMC_50_FEATURE
	void __iomem *base = host->base;
#endif
	struct tune_counter *t_counter = &host->t_counter;

	switch (index) {
	case all_counter:
		#ifdef CONFIG_EMMC_50_FEATURE
		if (t_counter->time_hs400 != 0) {
			if (g_reset_tune) {
				MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE,
					MSDC_EMMC50_PAD_DS_TUNE_DLY1, 0x1c);
				MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE,
					MSDC_EMMC50_PAD_DS_TUNE_DLY3, 0xe);
			}
			ERR_MSG("TUNE HS400 Times(%d)", t_counter->time_hs400);
			if (g_ett_tune)
				g_ett_hs400_tune = t_counter->time_hs400;
		}
		t_counter->time_hs400 = 0;
		#endif
	case cmd_counter:
		if (t_counter->time_cmd != 0) {
			ERR_MSG("CRC TUNE CMD Times(%d)",
				t_counter->time_cmd);
			if (g_ett_tune)
				g_ett_cmd_tune = t_counter->time_cmd;
		}
		t_counter->time_cmd = 0;
		if (index == cmd_counter)
			break;
	case read_counter:
		if (t_counter->time_read != 0) {
			ERR_MSG("CRC TUNE READ Times(%d)",
				t_counter->time_read);
			if (g_ett_tune)
				g_ett_read_tune = t_counter->time_read;
		}
		t_counter->time_read = 0;
		if (index == read_counter)
			break;
	case write_counter:
		if (t_counter->time_write != 0) {
			ERR_MSG("CRC TUNE WRITE Times(%d)",
				t_counter->time_write);
			if (g_ett_tune)
				g_ett_write_tune = t_counter->time_write;
		}
		t_counter->time_write = 0;
		break;
	default:
		ERR_MSG("msdc%d ==> reset crc counter index(%d) error!\n",
			host->id, index);
		break;
	}
}

void msdc_save_timing_setting(struct msdc_host *host, u32 init_hw,
	u32 emmc_suspend, u32 sdio_suspend, u32 power_tuning, u32 power_off)
{
	struct msdc_hw *hw = host->hw;
	void __iomem *base = host->base;

	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_DDLSEL, hw->ddlsel);
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, hw->cmd_edge);
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, hw->rdata_edge);
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, hw->wdata_edge);

	if (emmc_suspend || sdio_suspend) {
		host->saved_para.hz = host->mclk;
		host->saved_para.sdc_cfg = MSDC_READ32(SDC_CFG);
	}

	if ((emmc_suspend || power_off) && (hw->host_function == MSDC_EMMC)) {
#ifdef CONFIG_EMMC_50_FEATURE
		MSDC_GET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
			host->saved_para.ds_dly1);
		MSDC_GET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
			host->saved_para.ds_dly3);
		host->saved_para.emmc50_pad_cmd_tune =
			MSDC_READ32(EMMC50_PAD_CMD_TUNE);
		host->saved_para.emmc50_dat01 =
			MSDC_READ32(EMMC50_PAD_DAT01_TUNE);
		host->saved_para.emmc50_dat23 =
			MSDC_READ32(EMMC50_PAD_DAT23_TUNE);
		host->saved_para.emmc50_dat45 =
			MSDC_READ32(EMMC50_PAD_DAT45_TUNE);
		host->saved_para.emmc50_dat67 =
			MSDC_READ32(EMMC50_PAD_DAT67_TUNE);
#endif
	}

	if (emmc_suspend) {
		host->saved_para.timing = host->timing;
		host->saved_para.msdc_cfg = MSDC_READ32(MSDC_CFG);
		host->saved_para.iocon = MSDC_READ32(MSDC_IOCON);
	}

	if (sdio_suspend) {
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, host->saved_para.mode);
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKDIV, host->saved_para.div);
		MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
			host->saved_para.int_dat_latch_ck_sel);
		MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL,
			host->saved_para.ckgen_msdc_dly_sel);
		MSDC_GET_FIELD(MSDC_INTEN, MSDC_INT_SDIOIRQ,
			host->saved_para.inten_sdio_irq);
		host->saved_para.msdc_cfg = MSDC_READ32(MSDC_CFG);

		host->saved_para.iocon = MSDC_READ32(MSDC_IOCON);

		host->saved_para.timing = host->timing;
	}

	host->saved_para.pad_tune0 = MSDC_READ32(MSDC_PAD_TUNE0);
	host->saved_para.pad_tune1 = MSDC_READ32(MSDC_PAD_TUNE1);

	host->saved_para.ddly0 = MSDC_READ32(MSDC_DAT_RDDLY0);
	host->saved_para.ddly1 = MSDC_READ32(MSDC_DAT_RDDLY1);

	host->saved_para.pb1 = MSDC_READ32(MSDC_PATCH_BIT1);
	host->saved_para.pb2 = MSDC_READ32(MSDC_PATCH_BIT2);
	/*msdc_dump_register(host);*/
}

void msdc_set_bad_card_and_remove(struct msdc_host *host)
{
	int got_polarity = 0;
	unsigned long flags;

	if (host == NULL) {
		ERR_MSG("WARN: host is NULL");
		return;
	}
	host->card_inserted = 0;

	if ((host->mmc == NULL) || (host->mmc->card == NULL)) {
		ERR_MSG("WARN: mmc or card is NULL");
		return;
	}
	if (host->mmc->card) {
		spin_lock_irqsave(&host->remove_bad_card, flags);
		got_polarity = host->sd_cd_polarity;
		host->block_bad_card = 1;

		mmc_card_set_removed(host->mmc->card);
		spin_unlock_irqrestore(&host->remove_bad_card, flags);

		if ((host->hw->flags & MSDC_CD_PIN_EN)
		 && (got_polarity ^ host->hw->cd_level)) {
			tasklet_hi_schedule(&host->card_tasklet);
		} else {
			mmc_remove_card(host->mmc->card);
			host->mmc->card = NULL;
			mmc_detach_bus(host->mmc);
			mmc_power_off(host->mmc);
		}

		ERR_MSG("Remove the bad card, block_bad_card=%d, "
			"card_inserted=%d",
			host->block_bad_card, host->card_inserted);
	}
}

/* 0 means pass */
u32 msdc_power_tuning(struct msdc_host *host)
{
	struct mmc_host *mmc = host->mmc;
	struct mmc_card *card;
	struct mmc_request *mrq;
	u32 power_cycle = 0;
	int read_timeout_tune = 0;
	int write_timeout_tune = 0;
	u32 rwcmd_timeout_tune = 0;
	u32 read_timeout_tune_uhs104 = 0;
	u32 write_timeout_tune_uhs104 = 0;
	u32 sw_timeout = 0;
	u32 ret = 1;
	u32 host_err = 0;
	u32 sclk = host->sclk;

	if (!mmc)
		return 1;

	card = mmc->card;
	if (card == NULL) {
		ERR_MSG("mmc->card is NULL");
		return 1;
	}
	/* eMMC first */
#ifdef CONFIG_MTK_EMMC_SUPPORT
	if (mmc_card_mmc(card) && (host->hw->host_function == MSDC_EMMC))
		return 1;
#endif

	if ((host->sd_30_busy > 0)
	 && (host->sd_30_busy <= MSDC_MAX_POWER_CYCLE)) {
		host->power_cycle_enable = 1;
	}

	if (!mmc_card_sd(card) || (host->hw->host_function != MSDC_SD))
		return ret;

	if ((host->power_cycle >= MSDC_MAX_POWER_CYCLE) ||
	    !host->power_cycle_enable) {

		if (host->continuous_fail_request_count <
		    MSDC_MAX_CONTINUOUS_FAIL_REQUEST_COUNT) {
			host->continuous_fail_request_count++;
		} else {
			ERR_MSG("%d times continuous fail request count,"
				" exceed max count %d, so remove bad card",
				host->continuous_fail_request_count,
				MSDC_MAX_CONTINUOUS_FAIL_REQUEST_COUNT);
			host->continuous_fail_request_count = 0;
			spin_unlock(&host->lock);/*release the lock in request*/
			msdc_set_bad_card_and_remove(host);
			/*card removing will define a new lock inside*/
			spin_lock(&host->lock);
			/*restore the lock in request entry*/
		}

		return ret;
	}

	/* power cycle */
	ERR_MSG("the %d time, Power cycle start", host->power_cycle);
	spin_unlock(&host->lock);

	if (host->power_control)
		host->power_control(host, 0);

	mdelay(10);

	if (host->power_control)
		host->power_control(host, 1);

	spin_lock(&host->lock);
	/*msdc_save_timing_setting(host, 0, 0, 0, 1, 0);*/
	msdc_init_tune_setting(host);
	if ((sclk > 100000000) && (host->power_cycle >= 1))
		mmc->caps &= ~MMC_CAP_UHS_SDR104;
	if (((sclk <= 100000000) &&
	     ((sclk > 50000000) || (host->timing == MMC_TIMING_UHS_DDR50)))
	 && (host->power_cycle >= 1)) {
		mmc->caps &= ~(MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104
			| MMC_CAP_UHS_DDR50);
	}

	msdc_host_mode[host->id] = mmc->caps;
	msdc_host_mode2[host->id] = mmc->caps2;

	/* clock should set to 260K*/
	mmc->ios.clock = HOST_MIN_MCLK;
	mmc->ios.bus_width = MMC_BUS_WIDTH_1;
	mmc->ios.timing = MMC_TIMING_LEGACY;
	msdc_set_mclk(host, MMC_TIMING_LEGACY, HOST_MIN_MCLK);

	/* re-init the card!*/
	mrq = host->mrq;
	host->mrq = NULL;
	power_cycle = host->power_cycle;
	host->power_cycle = MSDC_MAX_POWER_CYCLE;
	read_timeout_tune = host->read_time_tune;
	write_timeout_tune = host->write_time_tune;
	rwcmd_timeout_tune = host->rwcmd_time_tune;
	read_timeout_tune_uhs104 = host->read_timeout_uhs104;
	write_timeout_tune_uhs104 = host->write_timeout_uhs104;
	sw_timeout = host->sw_timeout;
	host_err = host->error;
	spin_unlock(&host->lock);
	ret = mmc_sd_power_cycle(mmc, mmc->ocr, card);
	spin_lock(&host->lock);
	host->mrq = mrq;
	host->power_cycle = power_cycle;
	host->read_time_tune = read_timeout_tune;
	host->write_time_tune = write_timeout_tune;
	host->rwcmd_time_tune = rwcmd_timeout_tune;
	if (sclk > 100000000) {
		host->write_timeout_uhs104 = write_timeout_tune_uhs104;
	} else {
		host->read_timeout_uhs104 = 0;
		host->write_timeout_uhs104 = 0;
	}
	host->sw_timeout = sw_timeout;
	host->error = host_err;
	if (!ret)
		host->power_cycle_enable = 0;
	ERR_MSG("the %d time, Power cycle Done, host->error(0x%x), ret(%d)",
		host->power_cycle, host->error, ret);
	(host->power_cycle)++;

	return ret;
}

static int msdc_lower_freq(struct msdc_host *host)
{
	u32 div, mode, hs400_div_dis;
	void __iomem *base = host->base;
	u32 *hclks;

	ERR_MSG("need to lower freq");
	msdc_reset_crc_tune_counter(host, all_counter);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, mode);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKDIV, div);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD_HS400, hs400_div_dis);

	hclks = msdc_get_hclks(host->id);

	if (div >= MSDC_MAX_FREQ_DIV) {
		ERR_MSG("div<%d> too large, change to power tuning", div);
		return msdc_power_tuning(host);
#ifdef CONFIG_EMMC_50_FEATURE
	} else if ((mode == 3) && (host->id == 0)) {
		mode = 1; /* change to HS200 */
		div = 0;
		msdc_clk_stable(host, mode, div, hs400_div_dis);
		host->sclk = hclks[host->hw->clk_src] / 2;
#endif
	} else if (mode == 1) {
		mode = 0;
		msdc_clk_stable(host, mode, div, hs400_div_dis);
		host->sclk = (div == 0) ?
			hclks[host->hw->clk_src]/2 :
			hclks[host->hw->clk_src]/(4*div);
	} else {
		msdc_clk_stable(host, mode, div + 1, hs400_div_dis);
		host->sclk = (mode == 2) ?
			hclks[host->hw->clk_src]/(2*4*(div+1)) :
			hclks[host->hw->clk_src]/(4*(div+1));
	}

	ERR_MSG("new div<%d>, mode<%d> new freq.<%dKHz>",
		(mode == 1) ? div : div + 1, mode, host->sclk / 1000);
	return 0;
}

#ifdef CONFIG_EMMC_50_FEATURE
/*
 *  2013-12-09
 *  HS400 error tune flow of read/write data error
 *  HS400 error tune flow of cmd error is same as eMMC4.5 backward speed mode.
 */
int emmc_hs400_tune_rw(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int cur_ds_dly1 = 0, cur_ds_dly3 = 0;
	int orig_ds_dly1 = 0, orig_ds_dly3 = 0;
	int err = 0;

	if (host->timing != MMC_TIMING_MMC_HS400)
		return err;

	MSDC_GET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
		orig_ds_dly1);
	MSDC_GET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
		orig_ds_dly3);

	if (g_ett_tune) {
		cur_ds_dly3 = orig_ds_dly3 + 1;
		cur_ds_dly1 = orig_ds_dly1;
		if (cur_ds_dly3 >= 32) {
			cur_ds_dly3 = 0;
			cur_ds_dly1 = orig_ds_dly1 + 1;
			if (cur_ds_dly1 >= 32)
				cur_ds_dly1 = 0;
		}
	} else {
		cur_ds_dly1 = orig_ds_dly1 - 1;
		cur_ds_dly3 = orig_ds_dly3;
		if (cur_ds_dly1 < 0) {
			cur_ds_dly1 = 17;
			cur_ds_dly3 = orig_ds_dly3 + 1;
			if (cur_ds_dly3 >= 32)
				cur_ds_dly3 = 0;
		}
	}

	if (++host->t_counter.time_hs400 ==
		(g_ett_tune ? (32 * 32) : MAX_HS400_TUNE_COUNT)) {
		ERR_MSG("Failed to update EMMC50_PAD_DS_TUNE_DLY, "
			"cur_ds_dly3=0x%x, cur_ds_dly1=0x%x",
			cur_ds_dly3, cur_ds_dly1);
#ifdef MSDC_LOWER_FREQ
		err = msdc_lower_freq(host);
#else
		err = 1;
#endif
		goto out;
	} else {
		MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE,
			MSDC_EMMC50_PAD_DS_TUNE_DLY1, cur_ds_dly1);
		if (cur_ds_dly3 != orig_ds_dly3) {
			MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE,
				MSDC_EMMC50_PAD_DS_TUNE_DLY3, cur_ds_dly3);
		}
		INIT_MSG("HS400_TUNE: orig_ds_dly1<0x%x>, orig_ds_dly3<0x%x>,"
			" cur_ds_dly1<0x%x>, cur_ds_dly3<0x%x>",
			orig_ds_dly1, orig_ds_dly3, cur_ds_dly1, cur_ds_dly3);
	}

 out:
	return err;
}
#endif

#ifdef CONFIG_EMMC_50_FEATURE
#define BACKUP_REG_NUM (12)
#else
#define BACKUP_REG_NUM (6)
#endif
struct msdc_reg_control backup_reg_list[BACKUP_REG_NUM] = {
	/*   addr
		mask
		value   default*/
	{(OFFSET_MSDC_PATCH_BIT0),
		(MSDC_PB0_INT_DAT_LATCH_CK_SEL),
		0x0,    0x0},
	{(OFFSET_MSDC_PATCH_BIT1),
		(MSDC_PB1_WRDAT_CRCS_TA_CNTR),
		0x0,    0x0},
		/* defalut init value is 1, but HS400 shall be 0x0*/
	{(OFFSET_MSDC_IOCON),
		(MSDC_IOCON_R_D_SMPL | MSDC_IOCON_DDLSEL |
		 MSDC_IOCON_R_D_SMPL_SEL | MSDC_IOCON_R_D0SPL |
		 MSDC_IOCON_W_D_SMPL_SEL | MSDC_IOCON_W_D_SMPL),
		0x0,    0x0},
	{(OFFSET_MSDC_PAD_TUNE0),
		(MSDC_PAD_TUNE0_DATWRDLY | MSDC_PAD_TUNE0_DATRRDLY),
		0x0,    0x0},
	{(OFFSET_MSDC_DAT_RDDLY0),
		(MSDC_DAT_RDDLY0_D3 | MSDC_DAT_RDDLY0_D2 |
		 MSDC_DAT_RDDLY0_D1 | MSDC_DAT_RDDLY0_D0),
		0x0,    0x0},
	{(OFFSET_MSDC_DAT_RDDLY1),
		(MSDC_DAT_RDDLY1_D7 | MSDC_DAT_RDDLY1_D6 |
		 MSDC_DAT_RDDLY1_D5 | MSDC_DAT_RDDLY1_D4),
		0x0,    0x0},
#ifdef CONFIG_EMMC_50_FEATURE
	{(OFFSET_EMMC50_PAD_CMD_TUNE),
		(MSDC_EMMC50_PAD_CMD_TUNE_TXDLY),
		0x0,    0x00000200},
	{(OFFSET_EMMC50_PAD_DS_TUNE),
		(MSDC_EMMC50_PAD_DS_TUNE_DLY1 | MSDC_EMMC50_PAD_DS_TUNE_DLY3),
		0x0,    0x0},
	{(OFFSET_EMMC50_PAD_DAT01_TUNE),
		(MSDC_EMMC50_PAD_DAT0_TXDLY | MSDC_EMMC50_PAD_DAT1_TXDLY),
		0x0,    0x01000100},
	{(OFFSET_EMMC50_PAD_DAT23_TUNE),
		(MSDC_EMMC50_PAD_DAT2_TXDLY | MSDC_EMMC50_PAD_DAT3_TXDLY),
		0x0,    0x01000100},
	{(OFFSET_EMMC50_PAD_DAT45_TUNE),
		(MSDC_EMMC50_PAD_DAT4_TXDLY | MSDC_EMMC50_PAD_DAT5_TXDLY),
		0x0,    0x01000100},
	{(OFFSET_EMMC50_PAD_DAT67_TUNE),
		(MSDC_EMMC50_PAD_DAT6_TXDLY | MSDC_EMMC50_PAD_DAT7_TXDLY),
		0x0,    0x01000100},
#endif
};

/*
   register as callback function of WIFI(combo_sdio_register_pm) .
   can called by msdc_drv_suspend/resume too.
*/
void msdc_restore_timing_setting(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int retry = 3;
	int emmc = (host->hw->host_function == MSDC_EMMC) ? 1 : 0;
	int sdio = (host->hw->host_function == MSDC_SDIO) ? 1 : 0;

	if (sdio) {
		msdc_reset_hw(host->id); /* force bit5(BV18SDT) to 0 */
		host->saved_para.msdc_cfg =
			host->saved_para.msdc_cfg & 0xFFFFFFDF;
		MSDC_WRITE32(MSDC_CFG, host->saved_para.msdc_cfg);
	}

	do {
		msdc_set_mclk(host, host->saved_para.timing,
			host->saved_para.hz);
		if ((MSDC_READ32(MSDC_CFG) & 0xFFFFFF9F) ==
		    (host->saved_para.msdc_cfg & 0xFFFFFF9F))
			break;
		ERR_MSG("msdc set_mclk is unstable (cur_cfg=%x, "
			"save_cfg=%x, cur_hz=%d, save_hz=%d).",
			MSDC_READ32(MSDC_CFG),
			host->saved_para.msdc_cfg, host->mclk,
			host->saved_para.hz);
	} while (retry--);

	MSDC_WRITE32(SDC_CFG, host->saved_para.sdc_cfg);

	MSDC_WRITE32(MSDC_IOCON, host->saved_para.iocon);

	MSDC_WRITE32(MSDC_PAD_TUNE0, host->saved_para.pad_tune0);
	MSDC_WRITE32(MSDC_PAD_TUNE1, host->saved_para.pad_tune1);

	MSDC_WRITE32(MSDC_DAT_RDDLY0, host->saved_para.ddly0);
	MSDC_WRITE32(MSDC_DAT_RDDLY1, host->saved_para.ddly1);

	MSDC_WRITE32(MSDC_PATCH_BIT1, host->saved_para.pb1);
	MSDC_WRITE32(MSDC_PATCH_BIT2, host->saved_para.pb2);

	if (sdio) {
		MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
			host->saved_para.int_dat_latch_ck_sel);
		MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL,
			host->saved_para.ckgen_msdc_dly_sel);
		MSDC_SET_FIELD(MSDC_INTEN, MSDC_INT_SDIOIRQ,
			host->saved_para.inten_sdio_irq);
	}

	if (emmc) {
#ifdef CONFIG_EMMC_50_FEATURE
		MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
			host->saved_para.ds_dly1);
		MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
			host->saved_para.ds_dly3);
		MSDC_WRITE32(EMMC50_PAD_CMD_TUNE,
			host->saved_para.emmc50_pad_cmd_tune);
		MSDC_WRITE32(EMMC50_PAD_DAT01_TUNE,
			host->saved_para.emmc50_dat01);
		MSDC_WRITE32(EMMC50_PAD_DAT23_TUNE,
			host->saved_para.emmc50_dat23);
		MSDC_WRITE32(EMMC50_PAD_DAT45_TUNE,
			host->saved_para.emmc50_dat45);
		MSDC_WRITE32(EMMC50_PAD_DAT67_TUNE,
			host->saved_para.emmc50_dat67);
#endif
	}

	/*msdc_dump_register(host);*/
}

/*
 *  2015-01-09
 *  Runtime reducing to legacy speed or slower clock, clear eMMC ett timing
 */
void emmc_clear_timing(void)
{
	int i = 0;

	if (!mtk_msdc_host[0]) {
		pr_err("[%s] msdc%d is not exist\n", __func__, 0);
		return;
	}

	pr_err("emmc_clear_timing msdc0\n");
	for (i = 0; i < BACKUP_REG_NUM; i++) {
		MSDC_SET_FIELD(
			(backup_reg_list[i].addr+mtk_msdc_host[0]->base),
			backup_reg_list[i].mask,
			backup_reg_list[i].default_value);
	}

	return;
}

void msdc_init_tune_path(struct msdc_host *host, unsigned char timing)
{
	void __iomem *base = host->base;

#if (TUNE_METHOD == DATA_TUNE)
	if (host->id == 0)
		MSDC_WRITE32(MSDC_PAD_TUNE0, 0x00000000);
	else
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, 0x7FFFFFF, 0x0000000);

	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_DDLSEL);
	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_R_D_SMPL_SEL);
	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_R_D_SMPL);
	if (timing == MMC_TIMING_MMC_HS400) {
		MSDC_CLR_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL);
		MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL);
	} else {
		MSDC_SET_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL);
		MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL);
	}

	if (timing == MMC_TIMING_MMC_HS400)
		MSDC_CLR_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS);
	else
		MSDC_SET_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS);

	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_W_D_SMPL_SEL);

	MSDC_CLR_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGRESP);
	MSDC_SET_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLYSEL);
	MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL);

	MSDC_CLR_BIT32(EMMC50_CFG0, MSDC_EMMC50_CFG_CMD_RESP_SEL);

	/* tune path and related key hw fixed setting should be wrappered follow interface */
	autok_path_sel(host);

#elif (TUNE_METHOD == CLOCK_TUNE)
	/* disable async fifo use interl delay*/
	MSDC_SET_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_RXDLYSEL);
	MSDC_CLR_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL);
	MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL);
	MSDC_CLR_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLYSEL);
	MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL);
	MSDC_CLR_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS);
	MSDC_SET_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGRESP);
	MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);

#endif
}

void msdc_init_tune_setting(struct msdc_host *host)
{
	struct msdc_hw *hw = host->hw;
	void __iomem *base = host->base;

	pr_err("msdc_init_tune_setting\n");
	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CLKTXDLY,
		MSDC_CLKTXDLY);
	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY,
		hw->datwrddly);
	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLY,
		hw->cmdrrddly);
	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY,
		hw->cmdrddly);

	MSDC_WRITE32(MSDC_IOCON, 0x00000000);

	MSDC_WRITE32(MSDC_DAT_RDDLY0, 0x00000000);
	MSDC_WRITE32(MSDC_DAT_RDDLY1, 0x00000000);

	MSDC_WRITE32(MSDC_PATCH_BIT1, 0xFFFE00C9);

	/* 64T + 48T cmd <-> resp */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPWAITCNT, 3);
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, 0);
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, 0);

	/* low speed mode should be switch data tune path too even if not covered by autok */
	autok_path_sel(host);
}

#if (TUNE_METHOD == DATA_TUNE)
#include "msdc_tune_async.c"
#elif (TUNE_METHOD == CLOCK_TUNE)
int msdc_tune_cmdrsp(struct msdc_host *host)
{
	int result = 0;
	void __iomem *base = host->base;
	u32 sel = 0;
	u32 cur_rsmpl = 0, orig_rsmpl;
	u32 cur_rrdly = 0, orig_rrdly;
	u32 cur_cntr = 0, orig_cmdrtc;
	u32 cur_dl_cksel = 0, orig_dl_cksel;
	u32 clkmode;
	int hs400 = 0;
	u32 *hclks = msdc_get_hclks(host->id);
	u32 dl_cksel_inc;

	if (host->mclk >= 20000000)
		dl_cksel_inc = hclks[host->hw->clk_src] / host->mclk;
	else
		dl_cksel_inc = 1;

	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, orig_rsmpl);
	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, orig_rrdly);
	MSDC_GET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
		orig_cmdrtc);
	MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
		orig_dl_cksel);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;

#if 1
	if (host->mclk >= 100000000) {
		sel = 1;
	} else {
		MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR, 1);
		MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
			0);
	}

	cur_rsmpl = (orig_rsmpl + 1);
	msdc_set_smpl(host, hs400, cur_rsmpl % 2, TYPE_CMD_RESP_EDGE, NULL);
	if (host->mclk <= 400000) {
		msdc_set_smpl(host, hs400, 0, TYPE_CMD_RESP_EDGE, NULL);
		cur_rsmpl = 2;
	}
	if (cur_rsmpl >= 2) {
		cur_rrdly = (orig_rrdly + 1);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY,
			cur_rrdly & 0x1F);
	}
	if (cur_rrdly >= 32) {
		if (sel) {
			cur_cntr = (orig_cmdrtc + 1);
			MSDC_SET_FIELD(MSDC_PATCH_BIT1,
				MSDC_PB1_CMD_RSP_TA_CNTR, cur_cntr & 0x7);
		}
	}
	if (cur_cntr >= 8) {
		if (sel) {
			cur_dl_cksel = (orig_dl_cksel + dl_cksel_inc);
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
				MSDC_PB0_INT_DAT_LATCH_CK_SEL,
				cur_dl_cksel & 0x7);
		}
	}
	++(host->t_counter.time_cmd);
	if ((sel &&
	     host->t_counter.time_cmd == CMD_TUNE_UHS_MAX_TIME)
	 || (sel == 0 &&
	     host->t_counter.time_cmd == CMD_TUNE_HS_MAX_TIME)) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_cmd = 0;
	}
#else
	if (orig_rsmpl == 0) {
		cur_rsmpl = 1;
		msdc_set_smpl(host, hs400, cur_rsmpl, TYPE_CMD_RESP_EDGE, NULL);
	} else {
		cur_rsmpl = 0;
			/* need second layer */
		msdc_set_smpl(host, hs400, cur_rsmpl, TYPE_CMD_RESP_EDGE, NULL);
		cur_rrdly = (orig_rrdly + 1);
		if (cur_rrdly >= 32) {
			ERR_MSG("failed to update rrdly<%d>", cur_rrdly);
			MSDC_SET_FIELD(MSDC_PAD_TUNE0,
				MSDC_PAD_TUNE0_CMDRDLY, 0);
#ifdef MSDC_LOWER_FREQ
			return msdc_lower_freq(host);
#else
			return 1;
#endif
		}
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY,
			cur_rrdly);
	}

#endif
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, orig_rsmpl);
	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, orig_rrdly);
	MSDC_GET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR, orig_cmdrtc);
	MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
		orig_dl_cksel);
	INIT_MSG("TUNE_CMD: rsmpl<%d> rrdly<%d> cmdrtc<%d> dl_cksel<%d> "
		"sfreq.<%d>", orig_rsmpl,
		orig_rrdly, orig_cmdrtc, orig_dl_cksel, host->sclk);

	return result;
}

int msdc_tune_read(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 sel = 0;
	u32 ddr = 0, hs400 = 0;
	u32 dcrc;
	u32 clkmode = 0;
	u32 cur_rxdly0, cur_rxdly1;
	u32 cur_dsmpl = 0, orig_dsmpl;
	u32 cur_dsel = 0, orig_dsel;
	u32 cur_dl_cksel = 0, orig_dl_cksel;
	u32 cur_dat0 = 0, cur_dat1 = 0, cur_dat2 = 0, cur_dat3 = 0;
	u32 cur_dat4 = 0, cur_dat5 = 0, cur_dat6 = 0, cur_dat7 = 0;
	u32 orig_dat0, orig_dat1, orig_dat2, orig_dat3;
	u32 orig_dat4, orig_dat5, orig_dat6, orig_dat7;
	int result = 0;
	u32 *hclks = msdc_get_hclks(host->id);
	u32 dl_cksel_inc;

	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	ddr = (clkmode == 2) ? 1 : 0;
	hs400 = (clkmode == 3) ? 1 : 0;

	if (host->mclk >= 20000000)
		dl_cksel_inc = hclks[host->hw->clk_src] / host->mclk;
	else
		dl_cksel_inc = 1;

	if (host->mclk >= 100000000)
		sel = 1;
	else
		MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL, 0);

	MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL, orig_dsel);
	MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
		orig_dl_cksel);

	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, orig_dsmpl);

	MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);

	cur_dsmpl = (orig_dsmpl + 1);
	msdc_set_smpl(host, hs400, cur_dsmpl % 2, TYPE_READ_DATA_EDGE, NULL);

	if (cur_dsmpl >= 2) {

		MSDC_GET_FIELD(SDC_DCRC_STS,
			SDC_DCRC_STS_POS | SDC_DCRC_STS_NEG, dcrc);

		if (!ddr)
			dcrc &= ~SDC_DCRC_STS_NEG;

		cur_rxdly0 = MSDC_READ32(MSDC_DAT_RDDLY0);
		cur_rxdly1 = MSDC_READ32(MSDC_DAT_RDDLY1);

		orig_dat0 = (cur_rxdly0 >> 24) & 0x1F;
		orig_dat1 = (cur_rxdly0 >> 16) & 0x1F;
		orig_dat2 = (cur_rxdly0 >> 8) & 0x1F;
		orig_dat3 = (cur_rxdly0 >> 0) & 0x1F;
		orig_dat4 = (cur_rxdly1 >> 24) & 0x1F;
		orig_dat5 = (cur_rxdly1 >> 16) & 0x1F;
		orig_dat6 = (cur_rxdly1 >> 8) & 0x1F;
		orig_dat7 = (cur_rxdly1 >> 0) & 0x1F;

		if (ddr) {
			/* Combine pos edge and neg edge error into
			   bits 7~0 of dcrc */
			dcrc = dcrc | (dcrc>>8);
		}

		cur_dat0 = (dcrc & (1 << 0)) ? (orig_dat0 + 1) : orig_dat0;
		cur_dat1 = (dcrc & (1 << 1)) ? (orig_dat1 + 1) : orig_dat1;
		cur_dat2 = (dcrc & (1 << 2)) ? (orig_dat2 + 1) : orig_dat2;
		cur_dat3 = (dcrc & (1 << 3)) ? (orig_dat3 + 1) : orig_dat3;
		cur_dat4 = (dcrc & (1 << 4)) ? (orig_dat4 + 1) : orig_dat4;
		cur_dat5 = (dcrc & (1 << 5)) ? (orig_dat5 + 1) : orig_dat5;
		cur_dat6 = (dcrc & (1 << 6)) ? (orig_dat6 + 1) : orig_dat6;
		cur_dat7 = (dcrc & (1 << 7)) ? (orig_dat7 + 1) : orig_dat7;

		cur_rxdly0 = (cur_dat0 << 24) | (cur_dat1 << 16) |
			(cur_dat2 << 8) | (cur_dat3 << 0);
		cur_rxdly1 = (cur_dat4 << 24) | (cur_dat5 << 16) |
			(cur_dat6 << 8) | (cur_dat7 << 0);

		MSDC_WRITE32(MSDC_DAT_RDDLY0, cur_rxdly0 & 0x1F1F1F1F);
		MSDC_WRITE32(MSDC_DAT_RDDLY1, cur_rxdly1 & 0x1F1F1F1F);

	}

	if ((cur_dat0 >= 32) || (cur_dat1 >= 32) || (cur_dat2 >= 32) ||
	     (cur_dat3 >= 32) || (cur_dat4 >= 32) || (cur_dat5 >= 32) ||
	     (cur_dat6 >= 32) || (cur_dat7 >= 32)) {
		if (sel) {
			MSDC_WRITE32(MSDC_DAT_RDDLY0, 0);
			MSDC_WRITE32(MSDC_DAT_RDDLY1, 0);
			cur_dsel = (orig_dsel + 1);
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
				MSDC_PB0_CKGEN_MSDC_DLY_SEL, cur_dsel % 32);
		}
	}
	if (cur_dsel >= 32) {
		if (clkmode == 1 && sel) {
			cur_dl_cksel = (orig_dl_cksel + dl_cksel_inc);
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
				MSDC_PB0_INT_DAT_LATCH_CK_SEL,
				cur_dl_cksel % 8);
		}
	}
	++(host->t_counter.time_read);
	if ((sel == 1 && clkmode == 1 &&
	     host->t_counter.time_read == READ_TUNE_UHS_CLKMOD1_MAX_TIME)
	 || (sel == 1 && (clkmode == 0 || clkmode == 2) &&
	     host->t_counter.time_read == READ_TUNE_UHS_MAX_TIME)
	 || (sel == 0 && (clkmode == 0 || clkmode == 2) &&
	     host->t_counter.time_read == READ_TUNE_HS_MAX_TIME)) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_read = 0;
	}

	MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL, orig_dsel);
	MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
		orig_dl_cksel);
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, orig_dsmpl);
	cur_rxdly0 = MSDC_READ32(MSDC_DAT_RDDLY0);
	cur_rxdly1 = MSDC_READ32(MSDC_DAT_RDDLY1);
	INIT_MSG("TUNE_READ: dsmpl<%d> rxdly0<0x%x> rxdly1<0x%x> dsel<%d> "
		"dl_cksel<%d> sfreq.<%d>",
		orig_dsmpl, cur_rxdly0, cur_rxdly1, orig_dsel, orig_dl_cksel,
		host->sclk);

	return result;
}

int msdc_tune_write(struct msdc_host *host)
{
	void __iomem *base = host->base;
	/* u32 cur_wrrdly = 0, orig_wrrdly; */
	u32 cur_dsmpl = 0, orig_dsmpl;
	u32 cur_rxdly0 = 0;
	u32 orig_dat0, orig_dat1, orig_dat2, orig_dat3;
	u32 cur_dat0 = 0, cur_dat1 = 0, cur_dat2 = 0, cur_dat3 = 0;
	u32 cur_d_cntr = 0, orig_d_cntr;
	int result = 0;

	int sel = 0;
	int clkmode = 0;
	int hs400 = 0;

	if (host->mclk >= 100000000)
		sel = 1;
	else
		MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR, 1);

	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	/*MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY,
		orig_wrrdly);*/
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, orig_dsmpl);
	MSDC_GET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
		orig_d_cntr);

	MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_DDLSEL, 0);

	cur_dsmpl = (orig_dsmpl + 1);
	hs400 = (clkmode == 3) ? 1 : 0;
	msdc_set_smpl(host, hs400, cur_dsmpl % 2, TYPE_WRITE_CRC_EDGE, NULL);
#if 0
	if (cur_dsmpl >= 2) {
		cur_wrrdly = (orig_wrrdly + 1);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY,
			cur_wrrdly & 0x1F);
	}
#endif
	if (cur_dsmpl >= 2) {
		cur_rxdly0 = MSDC_READ32(MSDC_DAT_RDDLY0);

		orig_dat0 = (cur_rxdly0 >> 24) & 0x1F;
		orig_dat1 = (cur_rxdly0 >> 16) & 0x1F;
		orig_dat2 = (cur_rxdly0 >> 8) & 0x1F;
		orig_dat3 = (cur_rxdly0 >> 0) & 0x1F;

		cur_dat0 = (orig_dat0 + 1);     /* only adjust bit-1 for crc */
		cur_dat1 = orig_dat1;
		cur_dat2 = orig_dat2;
		cur_dat3 = orig_dat3;

		cur_rxdly0 = (cur_dat0 << 24) | (cur_dat1 << 16) |
			(cur_dat2 << 8) | (cur_dat3 << 0);

		MSDC_WRITE32(MSDC_DAT_RDDLY0, cur_rxdly0 & 0x1F1F1F1F);
	}
	if (cur_dat0 >= 32) {
		if (sel) {
			cur_d_cntr = (orig_d_cntr + 1);
			MSDC_SET_FIELD(MSDC_PATCH_BIT1,
				MSDC_PB1_WRDAT_CRCS_TA_CNTR, cur_d_cntr & 0x7);
		}
	}
	++(host->t_counter.time_write);
	if ((sel == 0 &&
	     host->t_counter.time_write == WRITE_TUNE_HS_MAX_TIME)
	 || (sel &&
	     host->t_counter.time_write == WRITE_TUNE_UHS_MAX_TIME)) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_write = 0;
	}

	/*
	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY, orig_wrrdly);
	*/
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, orig_dsmpl);
	MSDC_GET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
		orig_d_cntr);
	cur_rxdly0 = MSDC_READ32(MSDC_DAT_RDDLY0);
	INIT_MSG("TUNE_WRITE: dsmpl<%d> rxdly0<0x%x> d_cntr<%d> sfreq.<%d>",
		orig_dsmpl, cur_rxdly0, orig_d_cntr, host->sclk);

	return result;
}
#endif

static void msdc_init_tx_setting(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;

	if (host->id == 0) {
		if ((ios->timing == MMC_TIMING_MMC_HS400) &&
		    (ios->clock >= 100000000)) {
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_DDR50CKD, MSDC_DDRCKD);
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_TXSKEW_SEL,
				MSDC0_HS400_TXSKEW);
			MSDC_SET_FIELD(MSDC_PAD_TUNE0,
				MSDC_PAD_TUNE0_CLKTXDLY,
				MSDC0_HS400_CLKTXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_CMD_TUNE,
				MSDC_EMMC50_PAD_CMD_TUNE_TXDLY,
				MSDC0_HS400_CMDTXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE,
				MSDC_EMMC50_PAD_DAT0_TXDLY,
				MSDC0_HS400_DAT0TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE,
				MSDC_EMMC50_PAD_DAT1_TXDLY,
				MSDC0_HS400_DAT1TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE,
				MSDC_EMMC50_PAD_DAT2_TXDLY,
				MSDC0_HS400_DAT2TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE,
				MSDC_EMMC50_PAD_DAT3_TXDLY,
				MSDC0_HS400_DAT3TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT45_TUNE,
				MSDC_EMMC50_PAD_DAT4_TXDLY,
				MSDC0_HS400_DAT4TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT45_TUNE,
				MSDC_EMMC50_PAD_DAT5_TXDLY,
				MSDC0_HS400_DAT5TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT67_TUNE,
				MSDC_EMMC50_PAD_DAT6_TXDLY,
				MSDC0_HS400_DAT6TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT67_TUNE,
				MSDC_EMMC50_PAD_DAT7_TXDLY,
				MSDC0_HS400_DAT7TXDLY);
		} else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
			if ((timing == MMC_TIMING_UHS_DDR50)
			 || (timing == MMC_TIMING_MMC_DDR52)) {
#else
			if (ios->timing == MMC_TIMING_UHS_DDR50) {
#endif
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_DDR50CKD,
					MSDC0_DDR50_DDRCKD);
			} else {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_DDR50CKD, MSDC_DDRCKD);
			}
			MSDC_SET_FIELD(MSDC_PAD_TUNE0,
				MSDC_PAD_TUNE0_CLKTXDLY,
				MSDC0_CLKTXDLY);
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_TXSKEW_SEL,
				MSDC0_TXSKEW);
			MSDC_SET_FIELD(EMMC50_PAD_CMD_TUNE,
				MSDC_EMMC50_PAD_CMD_TUNE_TXDLY,
				MSDC0_CMDTXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE,
				MSDC_EMMC50_PAD_DAT0_TXDLY,
				MSDC0_DAT0TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE,
				MSDC_EMMC50_PAD_DAT1_TXDLY,
				MSDC0_DAT1TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE,
				MSDC_EMMC50_PAD_DAT2_TXDLY,
				MSDC0_DAT2TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE,
				MSDC_EMMC50_PAD_DAT3_TXDLY,
				MSDC0_DAT3TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT45_TUNE,
				MSDC_EMMC50_PAD_DAT4_TXDLY,
				MSDC0_DAT4TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT45_TUNE,
				MSDC_EMMC50_PAD_DAT5_TXDLY,
				MSDC0_DAT5TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT67_TUNE,
				MSDC_EMMC50_PAD_DAT6_TXDLY,
				MSDC0_DAT6TXDLY);
			MSDC_SET_FIELD(EMMC50_PAD_DAT67_TUNE,
				MSDC_EMMC50_PAD_DAT7_TXDLY,
				MSDC0_DAT7TXDLY);
		}
	} else if (host->id == 1) {
		MSDC_SET_FIELD(MSDC_IOCON,
			MSDC_IOCON_DDR50CKD, MSDC_DDRCKD);
		if ((ios->timing == MMC_TIMING_UHS_SDR104) &&
		    (ios->clock >= 200000000)) {
			MSDC_SET_FIELD(MSDC_PAD_TUNE0,
				MSDC_PAD_TUNE0_CLKTXDLY,
				MSDC1_CLK_SDR104_TX_VALUE);
		} else {
			MSDC_SET_FIELD(MSDC_PAD_TUNE0,
				MSDC_PAD_TUNE0_CLKTXDLY,
				MSDC1_CLK_TX_VALUE);
		}
	} else if (host->id == 2) {
		MSDC_SET_FIELD(MSDC_IOCON,
			MSDC_IOCON_DDR50CKD, MSDC_DDRCKD);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0,
			MSDC_PAD_TUNE0_CLKTXDLY,
			MSDC2_CLK_TX_VALUE);
	}
}

/*For msdc_ios_set_ios()*/
void msdc_ios_tune_setting(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct msdc_hw *hw = host->hw;

	void __iomem *base = host->base;
	u32 hs400 = (ios->timing == MMC_TIMING_MMC_HS400) ? 1 : 0;
	u32 mode;

	if (ios->clock >= 25000000) {
		/*INIT_MSG("SD latch rdata<%d> wdatea<%d> cmd<%d>",
			hw->rdata_edge,hw->wdata_edge, hw->cmd_edge);*/
		MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_DDLSEL, hw->ddlsel);
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		msdc_set_smpl_all(host, hs400, ((mode == 2) ? 1 : 0));

		msdc_init_tx_setting(mmc, ios);
	}

	if ((hw->host_function == MSDC_EMMC) && (host->mclk != ios->clock)
	 && (host->first_tune_done == 1)) {
		#ifdef CONFIG_EMMC_50_FEATURE
		if (hs400) {
			msdc_execute_tuning(host->mmc,
					MMC_SEND_TUNING_BLOCK_HS200);
		} else
		#endif
		if (ios->timing == MMC_TIMING_MMC_HS200) {
			msdc_execute_tuning(host->mmc,
					MMC_SEND_TUNING_BLOCK_HS200);
		} else {
			emmc_clear_timing();
			host->first_tune_done = 0;
		}
	}

#if 0
#ifdef CONFIG_EMMC_50_FEATURE
	#ifdef CONFIG_MMC_FFU
	if ((hw->host_function == MSDC_EMMC) &&
	    (ios->timing == MMC_TIMING_LEGACY) &&
	    (ios->clock <= 25000000))
		emmc_clear_timing();
	#endif
#endif
#endif
}

static u32 msdc_status_verify_case1(struct msdc_host *host,
	struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;
	unsigned long tmo = jiffies + POLLING_BUSY;
	void __iomem *base = host->base;

	while (state != 4) { /* until status to "tran" */
		msdc_reset_hw(host->id);
		while ((err = msdc_get_card_status(mmc, host, &status))) {
			ERR_MSG("CMD13 ERR<%d>", err);
			if (err != (unsigned int)-EIO) {
				return msdc_power_tuning(host);
			} else if (msdc_tune_cmdrsp(host)) {
				ERR_MSG("update cmd para failed");
				return MSDC_VERIFY_ERROR;
			}
		}

		state = R1_CURRENT_STATE(status);
		ERR_MSG("check card state<%d>", state);
		if (state == 5 || state == 6) {
			ERR_MSG("state<%d> need cmd12 to stop", state);
			msdc_send_stop(host); /* don't tuning */
		} else if (state == 7) { /* busy in programing */
			ERR_MSG("state<%d> card is busy", state);
			spin_unlock(&host->lock);
			msleep(100);
			spin_lock(&host->lock);
		} else if (state != 4) {
			ERR_MSG("state<%d> ??? ", state);
			return msdc_power_tuning(host);
		}

		if (time_after(jiffies, tmo)) {
			ERR_MSG("abort timeout. Do power cycle");
			if ((host->hw->host_function == MSDC_SD)
				&& (host->sclk >= 100000000
				|| (host->timing == MMC_TIMING_UHS_DDR50)))
				host->sd_30_busy++;
			return msdc_power_tuning(host);
		}
	}

	msdc_reset_hw(host->id);
	return MSDC_VERIFY_NEED_TUNE;
}

static u32 msdc_status_verify_case2(struct msdc_host *host,
	struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;/*0: can tune normaly; 1: err hapen; 2: tune pass;*/
	struct mmc_card *card = host->mmc->card;
	void __iomem *base = host->base;

	while (1) {
		msdc_reset_hw(host->id);
		err = msdc_get_card_status(mmc, host, &status);
		if (!err) {
			break;
		} else if (err != (unsigned int)-EIO) {
			ERR_MSG("CMD13 ERR<%d>", err);
			return msdc_power_tuning(host);
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
		ERR_MSG("CMD13 ERR<%d>", err);
	}
	state = R1_CURRENT_STATE(status);

	/*wether is right RCA*/
	if (cmd->arg == card->rca << 16) {
		return (3 == state || 8 == state) ? MSDC_VERIFY_NEED_TUNE :
			MSDC_VERIFY_NEED_NOT_TUNE;
	} else {
		return (4 == state || 5 == state || 6 == state || 7 == state)
			? MSDC_VERIFY_NEED_TUNE : MSDC_VERIFY_NEED_NOT_TUNE;
	}
}

static u32 msdc_status_verify_case3(struct msdc_host *host,
	struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;/*0: can tune normaly; 1: tune pass;*/
	void __iomem *base = host->base;

	while (1) {
		msdc_reset_hw(host->id);
		err = msdc_get_card_status(mmc, host, &status);
		if (!err) {
			break;
		} else if (err != (unsigned int)-EIO) {
			ERR_MSG("CMD13 ERR<%d>", err);
			return msdc_power_tuning(host);
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
		ERR_MSG("CMD13 ERR<%d>", err);
	}
	state = R1_CURRENT_STATE(status);
	return (5 == state || 6 == state) ? MSDC_VERIFY_NEED_TUNE :
		MSDC_VERIFY_NEED_NOT_TUNE;
}

static u32 msdc_status_verify_case4(struct msdc_host *host,
	struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;/*0: can tune normaly; 1: tune pass;*/
	void __iomem *base = host->base;

	if (cmd->arg && (0x1UL << 15))
		return MSDC_VERIFY_NEED_NOT_TUNE;
	while (1) {
		msdc_reset_hw(host->id);
		err = msdc_get_card_status(mmc, host, &status);
		if (!err) {
			break;
		} else if (err != (unsigned int)-EIO) {
			ERR_MSG("CMD13 ERR<%d>", err);
			break;
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
		ERR_MSG("CMD13 ERR<%d>", err);
	}
	state = R1_CURRENT_STATE(status);
	return 3 == state ? MSDC_VERIFY_NEED_NOT_TUNE :
		MSDC_VERIFY_NEED_TUNE;
}

#if 0
static u32 msdc_status_verify_case5(struct msdc_host *host,
	struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;/*0: can tune normaly; 1: tune pass;*/
	struct mmc_card *card = host->mmc->card;
	struct mmc_command cmd_bus_test = { 0 };
	struct mmc_request mrq_bus_sest = { 0 };

	while ((err = msdc_get_card_status(mmc, host, &status))) {
		ERR_MSG("CMD13 ERR<%d>", err);
		if (err != (unsigned int)-EIO) {
			return msdc_power_tuning(host);
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
	}
	state = R1_CURRENT_STATE(status);

	if (MMC_SEND_TUNING_BLOCK == cmd->opcode) {
		if (state == 9) {
			/*send cmd14*/
			/*u32 err = -1;*/
			cmd_bus_test.opcode = MMC_BUS_TEST_R;
			cmd_bus_test.arg = 0;
			cmd_bus_test.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

			mrq_bus_sest.cmd = &cmd_bus_test;
			cmd_bus_test.mrq = &mrq_bus_sest;
			cmd_bus_test.data = NULL;
			msdc_do_command(host, &cmd_bus_test, 0, CMD_TIMEOUT);
		}
		return MSDC_VERIFY_NEED_TUNE;
	} else {
		if (state == 4) {
			/*send cmd19*/
			/*u32 err = -1;*/
			cmd_bus_test.opcode = MMC_BUS_TEST_W;
			cmd_bus_test.arg = 0;
			cmd_bus_test.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

			mrq_bus_sest.cmd = &cmd_bus_test;
			cmd_bus_test.mrq = &mrq_bus_sest;
			cmd_bus_test.data = NULL;
			msdc_do_command(host, &cmd_bus_test, 0, CMD_TIMEOUT);
		}
		return MSDC_VERIFY_NEED_TUNE;
	}
}
#endif

static u32 msdc_status_verify(struct msdc_host *host, struct mmc_command *cmd)
{
	if (!host->mmc || !host->mmc->card || !host->mmc->card->rca) {
		/*card is not identify*/
		return MSDC_VERIFY_NEED_TUNE;
	}

	if (((host->hw->host_function == MSDC_EMMC) &&
	      IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE1))
	 || ((host->hw->host_function == MSDC_SD) &&
	      IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_SD_TUNE_CASE1))
	 || (host->app_cmd &&
	      IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_APP_TUNE_CASE1))) {
		return msdc_status_verify_case1(host, cmd);
	} else if (IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE2)) {
		return msdc_status_verify_case2(host, cmd);
	} else if (IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE3)) {
		return msdc_status_verify_case3(host, cmd);
	} else if ((host->hw->host_function == MSDC_EMMC)
		&& IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE4)) {
		return msdc_status_verify_case4(host, cmd);
#if 0
	} else if ((host->hw->host_function == MSDC_EMMC)
		&& IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE5)) {
		return msdc_status_verify_case5(host, cmd);
#endif
	} else {
		return MSDC_VERIFY_NEED_TUNE;
	}

}

int msdc_crc_tune(struct msdc_host *host, struct mmc_command *cmd,
	struct mmc_data *data, struct mmc_command *stop,
	struct mmc_command *sbc)
{
	u32 status_verify = 0;

#ifdef MTK_MSDC_USE_CMD23
	/* cmd->error also set when autocmd23 crc error */
	if ((cmd->error == (unsigned int)-EIO)
	 || (stop && (stop->error == (unsigned int)-EIO))
	 || (sbc && (sbc->error == (unsigned int)-EIO))) {
#else
	if ((cmd->error == (unsigned int)-EIO)
	 || (stop && (stop->error == (unsigned int)-EIO))) {
#endif
		if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("failed to updata cmd para");
			return 1;
		}
	}

	if (data && (data->error == (unsigned int)-EIO)) {
#ifdef CONFIG_EMMC_50_FEATURE
		if ((host->id == 0) &&
		    (host->timing == MMC_TIMING_MMC_HS400)) {
			if (emmc_hs400_tune_rw(host)) {
				ERR_MSG("failed to updata write para");
				return 1;
			}
		} else
#endif
		if (data->flags & MMC_DATA_READ) {      /* read */
			if (msdc_tune_read(host)) {
				ERR_MSG("failed to updata read para");
				return 1;
			}
		} else {
			if (msdc_tune_write(host)) {
				ERR_MSG("failed to updata write para");
				return 1;
			}
		}
	}

	status_verify = msdc_status_verify(host, cmd);
	if (MSDC_VERIFY_ERROR == status_verify) {
		ERR_MSG("status verify failed");
		/*data_abort = 1;*/
		if (host->hw->host_function == MSDC_SD)
			return 1;
	} else if (MSDC_VERIFY_NEED_NOT_TUNE == status_verify) {
		/* clear the error condition. */
		ERR_MSG("need not error tune");
		cmd->error = 0;
		return 1;
	}

	return 0;
}

int msdc_cmd_timeout_tune(struct msdc_host *host, struct mmc_command *cmd)
{
	/* CMD TO -> not tuning.
	   cmd->error also set when autocmd23 TO error */
	if (cmd->error != (unsigned int)-ETIMEDOUT)
		return 0;

	if (check_mmc_cmd1718(cmd->opcode) ||
	    check_mmc_cmd2425(cmd->opcode)) {
		if ((host->sw_timeout) ||
		    (++(host->rwcmd_time_tune) > MSDC_MAX_TIMEOUT_RETRY)) {
			ERR_MSG("msdc%d exceed max r/w cmd timeout "
				"tune times(%d) or SW timeout(%d),Power cycle",
				host->id, host->rwcmd_time_tune,
				host->sw_timeout);
			if (!(host->sd_30_busy) && msdc_power_tuning(host))
				return 1;
		}
	} else {
		return 1;
	}

	return 0;
}

int msdc_data_timeout_tune(struct msdc_host *host, struct mmc_data *data)
{
	if (!data || (data->error != (unsigned int)-ETIMEDOUT))
		return 0;

	/* [ALPS114710] Patch for data timeout issue. */
	if (data->flags & MMC_DATA_READ) {
		if (!(host->sw_timeout) &&
		    (host->hw->host_function == MSDC_SD) &&
		    (host->sclk > 100000000) &&
		    (host->read_timeout_uhs104 < MSDC_MAX_R_TIMEOUT_TUNE)) {
			if (host->t_counter.time_read)
				host->t_counter.time_read--;
			host->read_timeout_uhs104++;
			msdc_tune_read(host);
		} else if ((host->sw_timeout) ||
			   (host->read_timeout_uhs104
				>= MSDC_MAX_R_TIMEOUT_TUNE) ||
			   (++(host->read_time_tune)
				> MSDC_MAX_TIMEOUT_RETRY)) {
			ERR_MSG("msdc%d exceed max read timeout retry times(%d)"
				" or SW timeout(%d) or read timeout "
				"tuning times(%d), Power cycle",
				host->id, host->read_time_tune,
				host->sw_timeout, host->read_timeout_uhs104);
			if (msdc_power_tuning(host))
				return 1;
		}
	} else if (data->flags & MMC_DATA_WRITE) {
		if (!(host->sw_timeout)
		 && (host->hw->host_function == MSDC_SD)
		 && (host->sclk > 100000000)
		 && (host->write_timeout_uhs104 < MSDC_MAX_W_TIMEOUT_TUNE)) {
			if (host->t_counter.time_write)
				host->t_counter.time_write--;
			host->write_timeout_uhs104++;
			msdc_tune_write(host);
		} else if (!(host->sw_timeout) &&
			   (host->hw->host_function == MSDC_EMMC) &&
			   (host->write_timeout_emmc
				< MSDC_MAX_W_TIMEOUT_TUNE_EMMC)) {
			if (host->t_counter.time_write)
				host->t_counter.time_write--;
			host->write_timeout_emmc++;

#ifdef CONFIG_EMMC_50_FEATURE
			if ((host->id == 0) &&
			    (host->timing == MMC_TIMING_MMC_HS400))
				emmc_hs400_tune_rw(host);
			else
				msdc_tune_write(host);
#else
			msdc_tune_write(host);
#endif
		} else if ((host->hw->host_function == MSDC_SD)
			&& ((host->sw_timeout) ||
			    (host->write_timeout_uhs104
				>= MSDC_MAX_W_TIMEOUT_TUNE) ||
			    (++(host->write_time_tune)
				> MSDC_MAX_TIMEOUT_RETRY))) {
			ERR_MSG("msdc%d exceed max write timeout retry "
				"times(%d) or SW timeout(%d) or write timeout "
				"tuning time(%d), Power cycle",
				host->id, host->write_time_tune,
				host->sw_timeout, host->write_timeout_uhs104);
			if (!(host->sd_30_busy) && msdc_power_tuning(host))
				return 1;
		} else if ((host->hw->host_function == MSDC_EMMC)
			&& ((host->sw_timeout) ||
			    (++(host->write_time_tune)
				> MSDC_MAX_TIMEOUT_RETRY_EMMC))) {
			ERR_MSG("msdc%d exceed max write timeout retry "
				"times(%d) or SW timeout(%d) or write timeout "
				"tuning time(%d), Power cycle",
				host->id, host->write_time_tune,
				host->sw_timeout, host->write_timeout_emmc);
			host->write_timeout_emmc = 0;
			return 1;
		}
	}

	return 0;
}
