
#ifndef _PERF_UX_H_
#define _PERF_UX_H_

enum {
	UX_DEFAULT = 0,
	UX_LOW_PWR,
	UX_JUST_MK,
	UX_PERFORM,
	UX_UNDEF
};

extern int get_perf_ux_mode(void);
extern int set_perf_ux_mode(int);

#endif
