#ifndef __MT_IDLE_H__
#define __MT_IDLE_H__

extern int dpidle_enter(int cpu) __attribute__((weak));
extern int soidle3_enter(int cpu) __attribute__((weak));
extern int soidle_enter(int cpu) __attribute__((weak));
extern int mcidle_enter(int cpu) __attribute__((weak));
extern int slidle_enter(int cpu) __attribute__((weak));
extern int rgidle_enter(int cpu) __attribute__((weak));
extern int mt_idle_select(int cpu) __attribute__((weak));
extern void mt_cpuidle_framework_init(void) __attribute__((weak));

#endif /* __MT_IDLE_H__ */

