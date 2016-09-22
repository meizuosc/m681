
#ifndef _SMART_OVL_H_
#define _SMART_OVL_H_

#define FPS_ARRAY_SZ	30

struct fps_ctx_t {
	unsigned long long last_trig;
	unsigned int array[FPS_ARRAY_SZ];
	unsigned int total;
	unsigned int wnd_sz;
	unsigned int cur_wnd_sz;
	struct mutex lock;
	int is_inited;
};


struct fps_ctx_t*  fps_get_ctx();
int fps_ctx_init(struct fps_ctx_t *fps_ctx, int wnd_sz);
int fps_ctx_update(struct fps_ctx_t *fps_ctx);
int fps_ctx_get_fps(struct fps_ctx_t *fps_ctx, unsigned int *fps, int *stable);
int fps_ctx_get_fps(struct fps_ctx_t *fps_ctx, unsigned int *fps, int *stable);
int fps_ctx_set_wnd_sz(struct fps_ctx_t *fps_ctx, unsigned int wnd_sz);
int smart_ovl_try_switch_mode_nolock(void);


#endif

