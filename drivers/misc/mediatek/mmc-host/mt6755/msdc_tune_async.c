int msdc_tune_cmdrsp(struct msdc_host *host)
{
	int result = 0;
	void __iomem *base = host->base;
	u32 rsmpl;
	u32 dly, dly1, dly2, dly1_sel, dly2_sel;
	u32 clkmode, hs400;
	/*
	u32 *hclks = msdc_get_hclks(host->id);
	u32 dl_cksel_inc;

	if (host->mclk >= 20000000)
		dl_cksel_inc = hclks[host->hw->clk_src] / host->mclk;
	else
		dl_cksel_inc = 1;
	*/

	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, rsmpl);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;

	rsmpl++;
	msdc_set_smpl(host, hs400, rsmpl % 2, TYPE_CMD_RESP_EDGE, NULL);

	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLYSEL, dly1_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, dly1);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL, dly2_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRDLY2, dly2);

	if (rsmpl >= 2) {
		dly = ((dly1_sel ? dly : 0) + (dly2_sel ? dly2 : 0) + 1) % 63;

		dly1_sel = 1;
		if (dly < 32) {
			dly1 = dly;
			dly2_sel = 0;
			dly2 = 0;
		} else {
			dly1 = 31;
			dly2_sel = 1;
			dly2 = dly - 31;
		}
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLYSEL,
			dly1_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL,
			dly2_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, dly1);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRDLY2, dly2);
	}

	++(host->t_counter.time_cmd);
	if (host->t_counter.time_cmd == CMD_TUNE_HS_MAX_TIME) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_cmd = 0;
	}

	INIT_MSG("TUNE_CMD: rsmpl<%d> dly1<%d> dly2<%d> sfreq.<%d>",
		rsmpl & 0x1, dly1, dly2, host->sclk);

	return result;
}

int msdc_tune_read(struct msdc_host *host)
{
	int result = 0;
	void __iomem *base = host->base;
	u32 clkmode, hs400, ddr;
	u32 dsmpl;
	u32 dly, dly1, dly2, dly1_sel, dly2_sel;
	int tune_times_max;
	/*
	u32 *hclks = msdc_get_hclks(host->id);
	u32 dl_cksel_inc;

	if (host->mclk >= 20000000)
		dl_cksel_inc = hclks[host->hw->clk_src] / host->mclk;
	else
		dl_cksel_inc = 1;
	*/

	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;
	if (clkmode == 2 || clkmode == 3)
		ddr = 1;
	else
		ddr = 0;

	MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_DDLSEL, 0);
	MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_RD_DAT_SEL, dsmpl);

	if (host->id != 0 && ddr == 0) {
		dsmpl++;
		msdc_set_smpl(host, hs400, dsmpl % 2, TYPE_READ_DATA_EDGE,
			NULL);
		tune_times_max = READ_DATA_TUNE_MAX_TIME;
	} else {
		dsmpl = 2;
		tune_times_max = READ_DATA_TUNE_MAX_TIME/2;
	}

	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL, dly1_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLY, dly1);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL, dly2_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2, dly2);

	if (dsmpl >= 2) {
		dly = ((dly1_sel ? dly : 0) + (dly2_sel ? dly2 : 0) + 1) % 63;

		dly1_sel = 1;
		if (dly < 32) {
			dly1 = dly;
			dly2_sel = 0;
			dly2 = 0;
		} else {
			dly1 = 31;
			dly2_sel = 1;
			dly2 = dly - 31;
		}
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL,
			dly1_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL,
			dly2_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLY, dly1);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2, dly2);
	}

	++(host->t_counter.time_read);
	if (host->t_counter.time_read == tune_times_max) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_read = 0;
	}

	INIT_MSG("TUNE_READ: dsmpl<%d> dly1<0x%x> dly2<0x%x> sfreq.<%d>",
		dsmpl & 0x1, dly1, dly2, host->sclk);

	return result;
}

int msdc_tune_write(struct msdc_host *host)
{
	int result = 0;
	void __iomem *base = host->base;
	u32 dsmpl;
	u32 dly, dly1, dly2, dly1_sel, dly2_sel;
	int clkmode, hs400, ddr;
	int tune_times_max;

	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;
	if (clkmode == 2 || clkmode == 3)
		ddr = 1;
	else
		ddr = 0;

	if (host->id == 0 && hs400) {
		MSDC_GET_FIELD(EMMC50_CFG0, MSDC_EMMC50_CFG_CRC_STS_EDGE,
			dsmpl);
	} else {
		MSDC_GET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE,
			dsmpl);
	}

	if (host->id != 0 && ddr == 0) {
		dsmpl++;
		msdc_set_smpl(host, hs400, dsmpl % 2, TYPE_WRITE_CRC_EDGE,
			NULL);
		tune_times_max = WRITE_DATA_TUNE_MAX_TIME;
	} else {
		dsmpl = 2;
		tune_times_max = WRITE_DATA_TUNE_MAX_TIME/2;
	}

	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL, dly1_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLY, dly1);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL, dly2_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2, dly2);

	if (dsmpl >= 2) {
		dly = ((dly1_sel ? dly : 0) + (dly2_sel ? dly2 : 0) + 1) % 63;

		dly1_sel = 1;
		if (dly < 32) {
			dly1 = dly;
			dly2_sel = 0;
			dly2 = 0;
		} else {
			dly1 = 31;
			dly2_sel = 1;
			dly2 = dly - 31;
		}
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL,
			dly1_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL,
			dly2_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLY, dly1);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2, dly2);
	}

	++(host->t_counter.time_write);
	if (host->t_counter.time_write == tune_times_max) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_write = 0;
	}

	INIT_MSG("TUNE_WRITE: dsmpl<%d> dly1<0x%x> dly2<0x%x> sfreq.<%d>",
		dsmpl & 0x1, dly1, dly2, host->sclk);

	return result;
}

