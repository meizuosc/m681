
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/rtpm_prio.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/ion_drv.h>
#include <linux/mtk_ion.h>
#include "primary_display.h"
#include "disp_drv_log.h"
#include "ddp_dsi.h"
#include "ddp_irq.h"
#include "disp_lowpower.h"
#include "mtkfb_console.h"
#include "m4u.h"
#include "disp_drv_platform.h"


typedef enum {
	SVP_NOMAL = 0,
	SVP_IN_POINT,
	SVP_SEC,
	SVP_2_NOMAL,
	SVP_EXIT_POINT
} SVP_STATE;

#ifndef OPT_BACKUP_NUM
#define OPT_BACKUP_NUM 3
#endif

static DISP_POWER_STATE tui_power_stat_backup;
static int tui_session_mode_backup;
static SVP_STATE svp_state = SVP_NOMAL;
static DISP_HELPER_OPT opt_backup_name[OPT_BACKUP_NUM] = {
	DISP_OPT_SMART_OVL,
	DISP_OPT_IDLEMGR_SWTCH_DECOUPLE,
	DISP_OPT_BYPASS_OVL
};
static int opt_backup_value[OPT_BACKUP_NUM];
unsigned int idlemgr_flag_backup;


OPT_BACKUP tui_opt_backup[3] = {
	{DISP_OPT_IDLEMGR_SWTCH_DECOUPLE, 0},
	{DISP_OPT_SMART_OVL, 0},
	{DISP_OPT_BYPASS_OVL, 0}
};
void stop_smart_ovl_nolock(void)
{
	int i;

	for (i = 0; i < (sizeof(tui_opt_backup) / sizeof((tui_opt_backup)[0])); i++) {
		tui_opt_backup[i].value = disp_helper_get_option(tui_opt_backup[i].option);
		disp_helper_set_option(tui_opt_backup[i].option, 0);
	}
	/*primary_display_esd_check_enable(0);*/
}
void restart_smart_ovl_nolock(void)
{
	int i;

	for (i = 0; i < 3; i++)
		disp_helper_set_option(tui_opt_backup[i].option, tui_opt_backup[i].value);

}

static int disp_enter_svp(SVP_STATE state)
{
	int i;
	if (state == SVP_IN_POINT) {

		for (i = 0; i < OPT_BACKUP_NUM; i++) {
			opt_backup_value[i] = disp_helper_get_option(opt_backup_name[i]);
			disp_helper_set_option(opt_backup_name[i], 0);
		}

		if (primary_display_is_decouple_mode() && (!primary_display_is_mirror_mode())) {
			/* switch to DL */
			do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE, primary_get_sess_id(), 0, NULL, 0);
		}

		idlemgr_flag_backup = set_idlemgr(0, 0);
	}

	return 0;
}

static int disp_leave_svp(SVP_STATE state)
{
	int i;
	if (state == SVP_EXIT_POINT) {

		for (i = 0; i < OPT_BACKUP_NUM; i++)
			disp_helper_set_option(opt_backup_name[i], opt_backup_value[i]);

		set_idlemgr(idlemgr_flag_backup, 0);
	}
	return 0;
}

int setup_disp_sec(disp_ddp_path_config *data_config, cmdqRecHandle cmdq_handle,
			  int is_locked)
{
	int i, has_sec_layer = 0;

	for (i = 0; i < ARRAY_SIZE(data_config->ovl_config); i++) {
		if (data_config->ovl_config[i].layer_en && (data_config->ovl_config[i].security == DISP_SECURE_BUFFER))
			has_sec_layer = 1;
	}

	if ((has_sec_layer != primary_is_sec())) {
		MMProfileLogEx(ddp_mmp_get_events()->sec, MMProfileFlagPulse, has_sec_layer, 0);
		/* sec/nonsec switch */

		cmdqRecReset(cmdq_handle);
		if (primary_display_is_decouple_mode())
			cmdqRecWait(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);
		else
			_cmdq_insert_wait_frame_done_token_mira(primary_get_config_handle());



		MMProfileLogEx(ddp_mmp_get_events()->sec, MMProfileFlagPulse, has_sec_layer, 1);

		if (has_sec_layer) {
			/* switch nonsec --> sec */
			svp_state = SVP_IN_POINT;
			disp_enter_svp(svp_state);
			MMProfileLogEx(ddp_mmp_get_events()->sec, MMProfileFlagStart, 0, 0);
		} else {
			/*switch sec --> nonsec */
			svp_state = SVP_2_NOMAL;
			MMProfileLogEx(ddp_mmp_get_events()->sec, MMProfileFlagEnd, 0, 0);
		}
	}

	if ((has_sec_layer == primary_is_sec()) && (primary_is_sec() == 0)) {
		if (svp_state == SVP_2_NOMAL) {
			svp_state = SVP_EXIT_POINT;
			disp_leave_svp(svp_state);
		} else
			svp_state = SVP_NOMAL;

	}

	if ((has_sec_layer == primary_is_sec()) && (primary_is_sec() == 1)) {
		/* IN SVP now!*/
		svp_state = SVP_SEC;
	}

	primary_set_sec(has_sec_layer);
	return 0;
}

int display_enter_tui(void)
{
	msleep(500);
	DISPMSG("TDDP: %s\n", __func__);

	MMProfileLogEx(ddp_mmp_get_events()->tui, MMProfileFlagStart, 0, 0);

	primary_display_manual_lock();

	if (primary_get_state() != DISP_ALIVE) {
		DISPERR("Can't enter tui: current_stat=%d is not alive\n", primary_get_state());
		goto err0;
	}

	tui_power_stat_backup = primary_get_state();
	primary_display_set_state(DISP_BLANK);

	primary_display_idlemgr_kick((char *)__func__, 0);

	if (primary_display_is_mirror_mode()) {
		DISPERR("Can't enter tui: current_mode=%s\n", p_session_mode_spy(primary_get_sess_mode()));
		goto err1;
	}

	stop_smart_ovl_nolock();

	tui_session_mode_backup = primary_get_sess_mode();

	do_primary_display_switch_mode(DISP_SESSION_DECOUPLE_MODE, primary_get_sess_id(), 0, NULL, 0);

	MMProfileLogEx(ddp_mmp_get_events()->tui, MMProfileFlagPulse, 0, 1);

	primary_display_manual_unlock();
	return 0;

err1:
	primary_display_set_state(tui_power_stat_backup);

err0:
	MMProfileLogEx(ddp_mmp_get_events()->tui, MMProfileFlagEnd, 0, 0);
	primary_display_manual_unlock();

	return -1;
}

int display_exit_tui(void)
{
	pr_info("[TUI-HAL]  display_exit_tui() start\n");
	MMProfileLogEx(ddp_mmp_get_events()->tui, MMProfileFlagPulse, 1, 1);

	primary_display_manual_lock();
	primary_display_set_state(tui_power_stat_backup);

	/* trigger rdma to display last normal buffer */
	_decouple_update_rdma_config_nolock();
	/*workaround: wait until this frame triggered to lcm */
	msleep(32);
	do_primary_display_switch_mode(tui_session_mode_backup, primary_get_sess_id(), 0, NULL, 0);
	/*DISP_REG_SET(NULL, DISP_REG_RDMA_INT_ENABLE, 0xffffffff);*/

	restart_smart_ovl_nolock();
	primary_display_manual_unlock();

	MMProfileLogEx(ddp_mmp_get_events()->tui, MMProfileFlagEnd, 0, 0);
	DISPMSG("TDDP: %s\n", __func__);
	pr_info("[TUI-HAL]  display_exit_tui() done\n");
	return 0;

}

/*****************************************************************************
 * Below code is for Efuse test in Android Load.
 * include TE, ROI and Resolution.
 *****************************************************************************/
/* extern void DSI_ForceConfig(int forceconfig);	*/
/* extern int DSI_set_roi(int x,int y);			*/
/* extern int DSI_check_roi(void);			*/

static int width_array[] = {2560, 1440, 1920, 1280, 1200, 800, 960, 640};
static int heigh_array[] = {1440, 2560, 1200, 800, 1920, 1280, 640, 960};
static int array_id[] = {6,   2,   7,   4,   3,    0,   5,   1};
LCM_PARAMS *lcm_param2 = NULL;
disp_ddp_path_config data_config2;

int primary_display_te_test(void)
{
	int ret = 0;
	int try_cnt = 3;
	int time_interval = 0;
	int time_interval_max = 0;
	long long time_te = 0;
	long long time_framedone = 0;

	DISPMSG("display_test te begin\n");
	if (primary_display_is_video_mode()) {
		DISPMSG("Video Mode No TE\n");
		return ret;
	}

	while (try_cnt >= 0) {
		try_cnt--;
		ret = dpmgr_wait_event_timeout(primary_get_dpmgr_handle(), DISP_PATH_EVENT_IF_VSYNC, HZ*1);
		time_te = sched_clock();

		dpmgr_path_trigger(primary_get_dpmgr_handle(), NULL, CMDQ_DISABLE);
		ret = dpmgr_wait_event_timeout(primary_get_dpmgr_handle(), DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		time_framedone = sched_clock();
		time_interval = (int) (time_framedone - time_te);
		time_interval = time_interval / 1000;
		if (time_interval > time_interval_max)
			time_interval_max = time_interval;
	}
	if (time_interval_max > 20000)
		ret = 0;
	else
		ret = -1;

	if (ret >= 0)
		DISPMSG("[display_test_result]==>Force On TE Open!(%d)\n", time_interval_max);
	else
		DISPMSG("[display_test_result]==>Force On TE Closed!(%d)\n", time_interval_max);

	DISPMSG("display_test te  end\n");
	return ret;
}


int primary_display_roi_test(int x, int y)
{
	int ret = 0;
	DISPMSG("display_test roi begin\n");
	DISPMSG("display_test roi set roi %d, %d\n", x, y);
	DSI_set_roi(x, y);
	msleep(50);
	dpmgr_path_trigger(primary_get_dpmgr_handle(), NULL, CMDQ_DISABLE);
	msleep(50);
	DISPMSG("display_test DSI_check_roi\n");
	ret = DSI_check_roi();
	msleep(20);
	if (ret == 0)
		DISPMSG("[display_test_result]==>DSI_ROI limit!\n");
	else
		DISPMSG("[display_test_result]==>DSI_ROI Normal!\n");

	DISPMSG("display_test set roi %d, %d\n", 0, 0);

	DSI_set_roi(0, 0);

	msleep(20);
	DISPCHECK("display_test end\n");
	return ret;
}

int primary_display_resolution_test(void)
{
	int ret = 0;
	int i = 0;
	unsigned int w_backup = 0;
	unsigned int h_backup = 0;
	int dst_width = 0;
	int dst_heigh = 0;
	LCM_DSI_MODE_CON dsi_mode_backup = primary_display_is_video_mode();
	memset((void *)&data_config2, 0, sizeof(data_config2));
	lcm_param2 = NULL;
	memcpy((void *)&data_config2,
		(void *)dpmgr_path_get_last_config(primary_get_dpmgr_handle()),
		sizeof(disp_ddp_path_config));
	w_backup = data_config2.dst_w;
	h_backup = data_config2.dst_h;
	DISPCHECK("[display_test resolution]w_backup %d h_backup %d dsi_mode_backup %d\n",
		w_backup, h_backup, dsi_mode_backup);
	/* for dsi config */
	DSI_ForceConfig(1);
	for (i = 0; i < sizeof(width_array)/sizeof(int); i++) {
		dst_width = width_array[i];
		dst_heigh = heigh_array[i];
		DISPCHECK("[display_test resolution] width %d, heigh %d\n", dst_width, dst_heigh);
		lcm_param2 = disp_lcm_get_params(primary_get_lcm());
		lcm_param2->dsi.mode = CMD_MODE;
		lcm_param2->dsi.horizontal_active_pixel = dst_width;
		lcm_param2->dsi.vertical_active_line = dst_heigh;

		data_config2.dispif_config.dsi.mode = CMD_MODE;
		data_config2.dispif_config.dsi.horizontal_active_pixel = dst_width;
		data_config2.dispif_config.dsi.vertical_active_line = dst_heigh;


		data_config2.dst_w = dst_width;
		data_config2.dst_h = dst_heigh;

		data_config2.ovl_config[0].layer    = 0;
		data_config2.ovl_config[0].layer_en = 0;
		data_config2.ovl_config[1].layer    = 1;
		data_config2.ovl_config[1].layer_en = 0;
		data_config2.ovl_config[2].layer    = 2;
		data_config2.ovl_config[2].layer_en = 0;
		data_config2.ovl_config[3].layer    = 3;
		data_config2.ovl_config[3].layer_en = 0;

		data_config2.dst_dirty = 1;
		data_config2.ovl_dirty = 1;

		dpmgr_path_set_video_mode(primary_get_dpmgr_handle(), primary_display_is_video_mode());

		dpmgr_path_config(primary_get_dpmgr_handle(), &data_config2, CMDQ_DISABLE);
		data_config2.dst_dirty = 0;
		data_config2.ovl_dirty = 0;

		dpmgr_path_start(primary_get_dpmgr_handle(), CMDQ_DISABLE);

		if (dpmgr_path_is_busy(primary_get_dpmgr_handle()))
			DISPERR("[display_test]==>Fatal error, we didn't trigger display path but it's already busy\n");

		dpmgr_path_trigger(primary_get_dpmgr_handle(), NULL, CMDQ_DISABLE);

		ret = dpmgr_wait_event_timeout(primary_get_dpmgr_handle(), DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		if (ret > 0) {
			if (!dpmgr_path_is_busy(primary_get_dpmgr_handle())) {
				if (i == 0)
					DISPCHECK("[display_test resolution] display_result 0x%x unlimited!\n",
							array_id[i]);
				else if (i == 1)
					DISPCHECK("[display_test resolution] display_result 0x%x unlimited (W<H)\n",
							array_id[i]);
				else
					DISPCHECK("[display_test resolution] display_result 0x%x(%d x %d)\n",
							array_id[i], dst_width, dst_heigh);
				break;
			}
		}
		dpmgr_path_reset(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	}
	dpmgr_path_stop(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	lcm_param2 = disp_lcm_get_params(primary_get_lcm());
	lcm_param2->dsi.mode = dsi_mode_backup;
	lcm_param2->dsi.vertical_active_line = h_backup;
	lcm_param2->dsi.horizontal_active_pixel = w_backup;
	data_config2.dispif_config.dsi.vertical_active_line = h_backup;
	data_config2.dispif_config.dsi.horizontal_active_pixel = w_backup;
	data_config2.dispif_config.dsi.mode = dsi_mode_backup;
	data_config2.dst_w = w_backup;
	data_config2.dst_h = h_backup;
	data_config2.dst_dirty = 1;
	dpmgr_path_set_video_mode(primary_get_dpmgr_handle(), primary_display_is_video_mode());
	dpmgr_path_connect(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	dpmgr_path_config(primary_get_dpmgr_handle(), &data_config2, CMDQ_DISABLE);
	data_config2.dst_dirty = 0;
	DSI_ForceConfig(0);
	return ret;
}

int primary_display_check_test(void)
{
	int ret = 0;
	int esd_backup = 0;
	DISPCHECK("[display_test]Display test[Start]\n");
	primary_display_manual_lock();
	/* disable esd check */
	if (1) {
		esd_backup = 1;
		primary_display_esd_check_enable(0);
		msleep(2000);
		DISPCHECK("[display_test]Disable esd check end\n");
	}

	/* if suspend => return */
	if (primary_get_state() == DISP_SLEPT) {
		DISPCHECK("[display_test_result]======================================\n");
		DISPCHECK("[display_test_result]==>Test Fail : primary display path is slept\n");
		DISPCHECK("[display_test_result]======================================\n");
		goto done;
	}

	/* stop trigger loop */
	DISPCHECK("[display_test]Stop trigger loop[begin]\n");
	_cmdq_stop_trigger_loop();
	if (dpmgr_path_is_busy(primary_get_dpmgr_handle())) {
		DISPCHECK("[display_test]==>primary display path is busy\n");
		ret = dpmgr_wait_event_timeout(primary_get_dpmgr_handle(), DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		if (ret <= 0)
			dpmgr_path_reset(primary_get_dpmgr_handle(), CMDQ_DISABLE);

		DISPCHECK("[display_test]==>wait frame done ret:%d\n", ret);
	}
	DISPCHECK("[display_test]Stop trigger loop[end]\n");

	/* test force te */
	/* primary_display_te_test(); */

	/* test roi */
	/* primary_display_roi_test(30, 30); */

	/* test resolution test */
	primary_display_resolution_test();

	DISPCHECK("[display_test]start dpmgr path[begin]\n");
	dpmgr_path_start(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	if (dpmgr_path_is_busy(primary_get_dpmgr_handle()))
		DISPERR("[display_test]==>Fatal error, we didn't trigger display path but it's already busy\n");

	DISPCHECK("[display_test]start dpmgr path[end]\n");

	DISPCHECK("[display_test]Start trigger loop[begin]\n");
	_cmdq_start_trigger_loop();
	DISPCHECK("[display_test]Start trigger loop[end]\n");

done:
	/* restore esd */
	if (esd_backup == 1) {
		primary_display_esd_check_enable(1);
		DISPCHECK("[display_test]Restore esd check\n");
	}
	/* unlock path */
	primary_display_manual_unlock();
	DISPCHECK("[display_test]Display test[End]\n");
	return ret;
}


static int Panel_Master_primary_display_config_dsi(const char *name, uint32_t config_value)
{
	int ret = 0;
	disp_ddp_path_config *data_config;
	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(primary_get_dpmgr_handle());
	/* modify below for config dsi */
	if (!strcmp(name, "PM_CLK")) {
		pr_debug("Pmaster_config_dsi: PM_CLK:%d\n", config_value);
		data_config->dispif_config.dsi.PLL_CLOCK = config_value;
	} else if (!strcmp(name, "PM_SSC")) {
		data_config->dispif_config.dsi.ssc_range = config_value;
	}
	pr_debug("Pmaster_config_dsi: will Run path_config()\n");
	ret = dpmgr_path_config(primary_get_dpmgr_handle(), data_config, NULL);

	return ret;
}

int fbconfig_get_esd_check_test(uint32_t dsi_id, uint32_t cmd, uint8_t *buffer, uint32_t num)
{
	int ret = 0;
	primary_display_manual_lock();
	if (primary_get_state() == DISP_SLEPT) {
		DISPCHECK("[ESD]primary display path is slept?? -- skip esd check\n");
		primary_display_manual_unlock();
		goto done;
	}
	primary_display_esd_check_enable(0);
	disp_irq_esd_cust_bycmdq(0);
	/* / 1: stop path */
	_cmdq_stop_trigger_loop();
	if (dpmgr_path_is_busy(primary_get_dpmgr_handle()))
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);

	dpmgr_path_stop(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");
	if (dpmgr_path_is_busy(primary_get_dpmgr_handle()))
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);

	dpmgr_path_reset(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	ret = fbconfig_get_esd_check(dsi_id, cmd, buffer, num);
	dpmgr_path_start(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	DISPCHECK("[ESD]start dpmgr path[end]\n");
	if (primary_display_is_video_mode()) {
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start */
		dpmgr_path_trigger(primary_get_dpmgr_handle(), NULL, CMDQ_DISABLE);
	}
	_cmdq_start_trigger_loop();
	/* when we stop trigger loop
	 * if no other thread is running, cmdq may disable its clock
	 * all cmdq event will be cleared after suspend */
	cmdqCoreSetEvent(CMDQ_EVENT_DISP_WDMA0_EOF);
	DISPCHECK("[ESD]start cmdq trigger loop[end]\n");
	disp_irq_esd_cust_bycmdq(1);
	primary_display_esd_check_enable(1);
	primary_display_manual_unlock();

done:
	return ret;
}

int Panel_Master_dsi_config_entry(const char *name, void *config_value)
{
	int ret = 0;
	int force_trigger_path = 0;
	uint32_t *config_dsi = (uint32_t *) config_value;
	LCM_PARAMS *lcm_param = NULL;
	LCM_DRIVER *pLcm_drv = DISP_GetLcmDrv();

	DISPFUNC();
	if (!strcmp(name, "DRIVER_IC_RESET") || !strcmp(name, "PM_DDIC_CONFIG")) {
		primary_display_esd_check_enable(0);
		msleep(2500);
	}
	primary_display_manual_lock();

	lcm_param = disp_lcm_get_params(primary_get_lcm());
	if (primary_get_state() == DISP_SLEPT) {
		DISPERR("[Pmaster]Panel_Master: primary display path is slept??\n");
		goto done;
	}
	/* / Esd Check : Read from lcm */
	/* / the following code is to */
	/* / 0: lock path */
	/* / 1: stop path */
	/* / 2: do esd check (!!!) */
	/* / 3: start path */
	/* / 4: unlock path */
	/* / 1: stop path */
	_cmdq_stop_trigger_loop();

	if (dpmgr_path_is_busy(primary_get_dpmgr_handle())) {
		int event_ret;

		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
		event_ret = dpmgr_wait_event_timeout(primary_get_dpmgr_handle(), DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
		if (event_ret <= 0) {
			DISPERR("wait frame done in suspend timeout\n");
			primary_display_diagnose();
			ret = -1;
		}
	}
	dpmgr_path_stop(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");

	if (dpmgr_path_is_busy(primary_get_dpmgr_handle()))
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);

	dpmgr_path_reset(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	if ((!strcmp(name, "PM_CLK")) || (!strcmp(name, "PM_SSC")))
		Panel_Master_primary_display_config_dsi(name, *config_dsi);
	else if (!strcmp(name, "PM_DDIC_CONFIG")) {
		Panel_Master_DDIC_config();
		force_trigger_path = 1;
	} else if (!strcmp(name, "DRIVER_IC_RESET")) {
		if (pLcm_drv && pLcm_drv->init_power)
			pLcm_drv->init_power();
		if (pLcm_drv)
			pLcm_drv->init();
		else
			ret = -1;
		force_trigger_path = 1;
	}
	dpmgr_path_start(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	if (primary_display_is_video_mode()) {
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start */
		dpmgr_path_trigger(primary_get_dpmgr_handle(), NULL, CMDQ_DISABLE);
		force_trigger_path = 0;
	}
	_cmdq_start_trigger_loop();

	/* when we stop trigger loop
	 * if no other thread is running, cmdq may disable its clock
	 * all cmdq event will be cleared after suspend */
	cmdqCoreSetEvent(CMDQ_EVENT_DISP_WDMA0_EOF);

	DISPCHECK("[Pmaster]start cmdq trigger loop\n");
done:
	primary_display_manual_unlock();

	if (force_trigger_path) {
		primary_display_trigger(0, NULL, 0);
		DISPCHECK("[Pmaster]force trigger display path\r\n");
	}

	return ret;
}

int primary_display_lcm_ATA(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	primary_display_esd_check_enable(0);
	primary_display_manual_lock();
	disp_irq_esd_cust_bycmdq(0);
	if (primary_get_state() == 0) {
		DISPCHECK("ATA_LCM, primary display path is already sleep, skip\n");
		goto done;
	}

	DISPCHECK("[ATA_LCM]primary display path stop[begin]\n");
	if (primary_display_is_video_mode())
		dpmgr_path_ioctl(primary_get_dpmgr_handle(), NULL, DDP_STOP_VIDEO_MODE, NULL);

	DISPCHECK("[ATA_LCM]primary display path stop[end]\n");
	ret = disp_lcm_ATA(primary_get_lcm());
	dpmgr_path_start(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	if (primary_display_is_video_mode()) {
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start */
		dpmgr_path_trigger(primary_get_dpmgr_handle(), NULL, CMDQ_DISABLE);
	}
done:
	disp_irq_esd_cust_bycmdq(1);
	primary_display_manual_unlock();
	primary_display_esd_check_enable(1);
	return ret;
}


int dynamic_debug_msg_print(unsigned int mva, int w, int h, int pitch, int bytes_per_pix)
{
	int ret = 0;
	unsigned int layer_size = pitch * h;
	unsigned int real_mva = 0;
	unsigned long kva = 0;
	unsigned int real_size = 0, mapped_size = 0;
	if (disp_helper_get_option(DISP_OPT_SHOW_VISUAL_DEBUG_INFO)) {
		static MFC_HANDLE mfc_handle;
		m4u_query_mva_info(mva, layer_size, &real_mva, &real_size);
		if (ret < 0) {
			pr_debug("m4u_query_mva_info error\n");
			return -1;
		}
		ret = m4u_mva_map_kernel(real_mva, real_size, &kva, &mapped_size);
		if (ret < 0) {
			pr_debug("m4u_mva_map_kernel fail.\n");
			return -1;
		}
		if (layer_size > mapped_size) {
			pr_debug("warning: layer size > mapped size\n");
			goto err1;
		}

		ret = MFC_Open(&mfc_handle,
			       (void *)kva,
			       pitch,
			       h,
			       bytes_per_pix,
			       DAL_COLOR_WHITE,
			       DAL_COLOR_RED);
		if (ret != MFC_STATUS_OK)
			goto err1;
		screen_logger_print(mfc_handle);
		MFC_Close(mfc_handle);
err1:
		m4u_mva_unmap_kernel(real_mva, real_size, kva);
	}
	return 0;
}

int primary_show_basic_debug_info(struct disp_frame_cfg_t *cfg)
{
	int i;
	fpsEx fps;
	char disp_tmp[20];
	int dst_layer_id = 0;

	dprec_logger_get_result_value(DPREC_LOGGER_RDMA0_TRANSFER_1SECOND, &fps);
	snprintf(disp_tmp, sizeof(disp_tmp), ",rdma_fps:%lld.%02lld,", fps.fps, fps.fps_low);
	screen_logger_add_message("rdma_fps", MESSAGE_REPLACE, disp_tmp);

	dprec_logger_get_result_value(DPREC_LOGGER_OVL_FRAME_COMPLETE_1SECOND, &fps);
	snprintf(disp_tmp, sizeof(disp_tmp), "ovl_fps:%lld.%02lld,", fps.fps, fps.fps_low);
	screen_logger_add_message("ovl_fps", MESSAGE_REPLACE, disp_tmp);

	dprec_logger_get_result_value(DPREC_LOGGER_PQ_TRIGGER_1SECOND, &fps);
	snprintf(disp_tmp, sizeof(disp_tmp), "PQ_trigger:%lld.%02lld,", fps.fps, fps.fps_low);
	screen_logger_add_message("PQ trigger", MESSAGE_REPLACE, disp_tmp);

	snprintf(disp_tmp, sizeof(disp_tmp), primary_display_is_video_mode() ? "vdo," : "cmd,");
	screen_logger_add_message("mode", MESSAGE_REPLACE, disp_tmp);

	for (i = 0; i < cfg->input_layer_num; i++) {
		if (cfg->input_cfg[i].tgt_offset_y == 0 &&
		    cfg->input_cfg[i].layer_enable) {
			dst_layer_id = dst_layer_id > cfg->input_cfg[i].layer_id ?
				dst_layer_id : cfg->input_cfg[i].layer_id;
		}
	}

	dynamic_debug_msg_print((unsigned long)cfg->input_cfg[dst_layer_id].src_phy_addr,
				cfg->input_cfg[dst_layer_id].tgt_width,
				cfg->input_cfg[dst_layer_id].tgt_height,
				cfg->input_cfg[dst_layer_id].src_pitch,
				4);
	return 0;
}

static int primary_dynamic_debug(unsigned int mva, unsigned int pitch, unsigned int w, unsigned int h,
				unsigned int x_pos, unsigned int block_sz)
{
	unsigned int real_mva, real_size, map_size;
	unsigned long map_va;
	int ret;

	if (!disp_helper_get_option(DISP_OPT_DYNAMIC_DEBUG))
		return 0;

	ret = m4u_query_mva_info(mva, 0, &real_mva, &real_size);
	if (ret) {
		pr_err("%s error to query mva = 0x%x\n", __func__, mva);
		return -1;
	}
	ret = m4u_mva_map_kernel(real_mva, real_size, &map_va, &map_size);
	if (ret) {
		pr_err("%s error to map mva = 0x%x\n", __func__, real_mva);
		return -1;
	}

	unsigned char *buf_va = map_va + (mva - real_mva);
	int x, y;
	if (x_pos + block_sz > w)
		block_sz = w/2;
	if (block_sz > h)
		block_sz = h;

	for (y = 0; y < block_sz; y++) {
		for (x = 0; x < block_sz * 4; x++)
			buf_va[x_pos * 4 + x + y * pitch] = 255;
	}

	m4u_mva_unmap_kernel(real_mva, real_size, map_va);

	return 0;

}
