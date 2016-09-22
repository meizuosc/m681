#if !defined(AEE_COMMON_H)
#define AEE_COMMON_H

#define LOGD(fmt, msg...)	pr_notice(fmt, ##msg)
#define LOGV(fmt, msg...)
#define LOGI	LOGD
#define LOGE(fmt, msg...)	pr_err(fmt, ##msg)
#define LOGW	LOGE

int get_memory_size(void);
int in_fiq_handler(void);
int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom, unsigned long top);

#ifdef CONFIG_SCHED_DEBUG
extern int sysrq_sched_debug_show_at_KE(void);
#endif
extern int debug_locks;
extern void mrdump_mini_per_cpu_regs(int cpu, struct pt_regs *regs);
extern void aee_rr_rec_exp_type(unsigned int type);

#endif				/* AEE_COMMON_H */
