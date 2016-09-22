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

#include "primary_display.h"
#include "smart_ovl.h"
#include "ddp_mmp.h"
/*************************** fps calculate ************************/


static struct task_struct *fps_monitor;
static int fps_monitor_thread(void *data);

struct fps_ctx_t*  fps_get_ctx()
{
	static int is_context_inited = 0;
	static struct fps_ctx_t primary_fps_ctx;

	if (!is_context_inited) {
		memset((void*)&primary_fps_ctx, 0, sizeof(struct fps_ctx_t));
		is_context_inited = 1;
	}

	return &primary_fps_ctx;
}


int _fps_ctx_reset(struct fps_ctx_t *fps_ctx, int reserve_num)
{
	int i;
	if (reserve_num >= FPS_ARRAY_SZ) {
		pr_err("%s error to reset, reserve=%d\n", __func__, reserve_num);
		BUG();
	}
	for (i = reserve_num; i < FPS_ARRAY_SZ; i++)
		fps_ctx->array[i] = 0;

	if (reserve_num < fps_ctx->cur_wnd_sz)
		fps_ctx->cur_wnd_sz = reserve_num;

	/* re-calc total */
	fps_ctx->total = 0;
	for (i = 0; i < fps_ctx->cur_wnd_sz; i++)
		fps_ctx->total += fps_ctx->array[i];

	return 0;
}

int _fps_ctx_update(struct fps_ctx_t *fps_ctx, unsigned int fps, unsigned long long time_ns)
{
	int i;

	fps_ctx->total -= fps_ctx->array[fps_ctx->wnd_sz-1];

	for (i = fps_ctx->wnd_sz - 1; i > 0; i--)
		fps_ctx->array[i] = fps_ctx->array[i-1];

	fps_ctx->array[0] = fps;

	fps_ctx->total += fps_ctx->array[0];
	if (fps_ctx->cur_wnd_sz < fps_ctx->wnd_sz)
		fps_ctx->cur_wnd_sz++;

	fps_ctx->last_trig = time_ns;

	return 0;
}

int fps_ctx_init(struct fps_ctx_t *fps_ctx, int wnd_sz)
{
	if (fps_ctx->is_inited)
		return 0;

	memset(fps_ctx, 0, sizeof(*fps_ctx));
	mutex_init(&fps_ctx->lock);

	if (wnd_sz > FPS_ARRAY_SZ) {
		pr_err("%s error: wnd_sz = %d\n", __func__, wnd_sz);
		wnd_sz = FPS_ARRAY_SZ;
	}
	fps_ctx->wnd_sz = wnd_sz;

	/* fps_monitor = kthread_create(fps_monitor_thread, NULL, "fps_monitor");*/
	/* wake_up_process(fps_monitor); */
	fps_ctx->is_inited = 1;

	return 0;
}

unsigned int _fps_ctx_calc_cur_fps(struct fps_ctx_t *fps_ctx, unsigned long long cur_ns)
{
	unsigned long long delta;
	unsigned long long fps = 1000000000;
	delta = cur_ns - fps_ctx->last_trig;
	do_div(fps, delta);

	if (fps > 120ULL)
		fps = 120ULL;

	return (unsigned int)fps;
}

unsigned int _fps_ctx_get_avg_fps(struct fps_ctx_t *fps_ctx)
{
	unsigned int avg_fps;
	if (fps_ctx->cur_wnd_sz == 0)
		return 0;
	avg_fps = fps_ctx->total / fps_ctx->cur_wnd_sz;
	return avg_fps;
}

unsigned int _fps_ctx_get_avg_fps_ext(struct fps_ctx_t *fps_ctx, unsigned int abs_fps)
{
	unsigned int avg_fps;
	avg_fps = (fps_ctx->total + abs_fps) / (fps_ctx->cur_wnd_sz + 1);
	return avg_fps;
}

int _fps_ctx_check_drastic_change(struct fps_ctx_t *fps_ctx, unsigned int abs_fps)
{
	unsigned int avg_fps;
	avg_fps = _fps_ctx_get_avg_fps(fps_ctx);

	/* if long time no trigger, ex:500ms
	 * abs_fps=1, avg_fps may be 58
	 * we should let avg_fps decline rapidly */
	if (abs_fps < avg_fps / 4 || abs_fps > avg_fps * 4) {
		_fps_ctx_reset(fps_ctx, fps_ctx->cur_wnd_sz / 4);
	} else if (abs_fps < avg_fps / 2 || abs_fps > avg_fps * 2) {
		if (fps_ctx->cur_wnd_sz >= 2)
			_fps_ctx_reset(fps_ctx, fps_ctx->cur_wnd_sz / 2);
	}

	return 0;
}

int fps_ctx_update(struct fps_ctx_t *fps_ctx)
{
	unsigned int abs_fps, avg_fps;
	unsigned long long ns = sched_clock();

	mutex_lock(&fps_ctx->lock);
	abs_fps = _fps_ctx_calc_cur_fps(fps_ctx, ns);

	/* _fps_ctx_check_drastic_change(fps_ctx, abs_fps); */

	avg_fps = _fps_ctx_get_avg_fps(fps_ctx);

	/* if long time no trigger, ex:500ms
	 * abs_fps=1, avg_fps may be 58
	 * we should let avg_fps decline rapidly */

	if (abs_fps < avg_fps / 2 && avg_fps > 10)
		_fps_ctx_reset(fps_ctx, 0);


	_fps_ctx_update(fps_ctx, abs_fps, ns);

	MMProfileLogEx(ddp_mmp_get_events()->fps_set, MMProfileFlagPulse, abs_fps, fps_ctx->cur_wnd_sz);
	mutex_unlock(&fps_ctx->lock);
	return 0;
}

int fps_ctx_get_fps(struct fps_ctx_t *fps_ctx, unsigned int *fps, int *stable)
{
	unsigned long long ns = sched_clock();

	unsigned int abs_fps = 0, avg_fps = 0;

	*stable = 1;
	mutex_lock(&fps_ctx->lock);

	abs_fps = _fps_ctx_calc_cur_fps(fps_ctx, ns);
	avg_fps = _fps_ctx_get_avg_fps(fps_ctx);

	/* if long time no trigger, we should pull fps down */
	if (abs_fps < avg_fps/2 && avg_fps > 10) {
		_fps_ctx_reset(fps_ctx, 0);
		*fps = abs_fps;
		*stable = 0;
		goto done;
	}

	if (fps_ctx->cur_wnd_sz < fps_ctx->wnd_sz/2)
		*stable = 0;

	if (abs_fps < avg_fps)
		*fps = _fps_ctx_get_avg_fps_ext(fps_ctx, abs_fps);
	else
		*fps = avg_fps;

done:
	MMProfileLogEx(ddp_mmp_get_events()->fps_get, MMProfileFlagPulse, *fps, *stable);
	mutex_unlock(&fps_ctx->lock);
	return 0;
}

int fps_ctx_set_wnd_sz(struct fps_ctx_t *fps_ctx, unsigned int wnd_sz)
{
	int i;
	if (!fps_ctx->is_inited)
		return fps_ctx_init(fps_ctx, wnd_sz);

	if (wnd_sz > FPS_ARRAY_SZ) {
		pr_err("error: %s wnd_sz=%d\n", __func__, wnd_sz);
		return -1;
	}
	mutex_lock(&fps_ctx->lock);

	fps_ctx->total = 0;
	fps_ctx->wnd_sz = wnd_sz;

	for (i = 0; i < wnd_sz; i++)
		fps_ctx->total += fps_ctx->array[i];

	mutex_unlock(&fps_ctx->lock);
	return 0;
}

int fps_ctx_get_wnd_sz(struct fps_ctx_t *fps_ctx, unsigned int *cur_wnd, unsigned int *max_wnd)
{
	mutex_lock(&fps_ctx->lock);
	if (cur_wnd)
		*cur_wnd = fps_ctx->cur_wnd_sz;
	if (max_wnd)
		*max_wnd = fps_ctx->wnd_sz;
	mutex_unlock(&fps_ctx->lock);
	return 0;
}


static int fps_monitor_thread(void *data)
{
	msleep(16000);
	while (1) {
		int fps, stable;
		msleep(20);
		primary_display_manual_lock();
		fps_ctx_get_fps(fps_get_ctx(), &fps, &stable);
		primary_display_manual_unlock();
	}

	return 0;
}

int smart_ovl_try_switch_mode_nolock(void)
{
	unsigned int hwc_fps, lcm_fps;
	unsigned long long ovl_sz, rdma_sz;
	disp_path_handle disp_handle = NULL;
	disp_ddp_path_config *data_config = NULL;
	int i, stable;
	unsigned long long DL_bw, DC_bw, bw_th;

	if (!disp_helper_get_option(DISP_OPT_SMART_OVL))
		return 0;

	if (!primary_display_is_video_mode())
		return 0;

	if (primary_get_sess_mode() != DISP_SESSION_DIRECT_LINK_MODE &&
		primary_get_sess_mode() != DISP_SESSION_DECOUPLE_MODE)
		return 0;

	if (primary_get_state() != DISP_ALIVE)
		return 0;

	lcm_fps = primary_display_get_fps_nolock() / 100;
	fps_ctx_get_fps(fps_get_ctx(), &hwc_fps, &stable);

	/* we switch DL->DC only when fps is stable ! */
	if (primary_get_sess_mode() == DISP_SESSION_DIRECT_LINK_MODE) {
		if (!stable)
			return 0;
	}

	if (hwc_fps > lcm_fps)
		hwc_fps = lcm_fps;

	if (primary_get_sess_mode() == DISP_SESSION_DECOUPLE_MODE)
		disp_handle = primary_get_ovl2mem_handle();
	else
		disp_handle = primary_get_dpmgr_handle();

	data_config = dpmgr_path_get_last_config(disp_handle);

	/* calc wdma/rdma data size */
	rdma_sz = data_config->dst_h * data_config->dst_w * 3;

	/* calc ovl data size */
	ovl_sz = 0;
	for (i = 0; i < ARRAY_SIZE(data_config->ovl_config); i++) {
		OVL_CONFIG_STRUCT *ovl_cfg = &(data_config->ovl_config[i]);
		if (ovl_cfg->layer_en) {
			unsigned int Bpp = UFMT_GET_Bpp(ovl_cfg->fmt);
			ovl_sz += ovl_cfg->dst_w * ovl_cfg->dst_h * Bpp;
		}
	}

	/* switch criteria is:  (DL) V.S. (DC):
	 *   (ovl * lcm_fps) V.S. (ovl * hwc_fps + wdma * hwc_fps + rdma * lcm_fps)
	 */
	DL_bw = ovl_sz * lcm_fps;
	DC_bw = (ovl_sz + rdma_sz) * hwc_fps + rdma_sz * lcm_fps;

	if (primary_get_sess_mode() == DISP_SESSION_DIRECT_LINK_MODE) {
		bw_th = DL_bw*4;
		do_div(bw_th, 5);
		if (DC_bw < bw_th) {
			/* switch to DC */
			do_primary_display_switch_mode(DISP_SESSION_DECOUPLE_MODE, primary_get_sess_id(), 0, NULL, 0);
		}
	} else {
		bw_th = DC_bw*4;
		do_div(bw_th, 5);
		if (DL_bw < bw_th) {
			/* switch to DL */
			do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE, primary_get_sess_id(), 0, NULL, 0);
		}
	}

	return 0;
}


/*********************** fps calculate finish *********************************/

