#ifndef MET_DRV
#define MET_DRV

#include <linux/version.h>
#include <linux/device.h>
#include <linux/percpu.h>
#include <linux/hardirq.h>
#include <linux/clk-private.h>

extern int met_mode;

#define MET_MODE_TRACE_CMD_OFFSET	(1)
#define MET_MODE_TRACE_CMD			(1<<MET_MODE_TRACE_CMD_OFFSET)

/*#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#define MET_PRINTK(FORMAT, args...)							\
	do {\
		if (met_mode & MET_MODE_TRACE_CMD)					\
			trace_printk("%s: " FORMAT, __func__, ##args);	\
		else												\
			trace_printk(FORMAT, ##args);					\
	} while (0)
#else*/
#define MET_STRBUF_SIZE		1024
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf_nmi);
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf_irq);
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf_sirq);
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf);

#ifdef CONFIG_TRACING
#define TRACE_PUTS(p) \
	do { \
		trace_puts(p);; \
	} while (0)
#else
#define TRACE_PUTS(p) do {} while (0)
#endif

#define GET_MET_PRINTK_BUFFER_ENTER_CRITICAL() \
	({ \
		char *pmet_strbuf; \
		preempt_disable(); \
		if (in_nmi()) \
			pmet_strbuf = per_cpu(met_strbuf_nmi, smp_processor_id()); \
		else if (in_irq()) \
			pmet_strbuf = per_cpu(met_strbuf_irq, smp_processor_id()); \
		else if (in_softirq()) \
			pmet_strbuf = per_cpu(met_strbuf_sirq, smp_processor_id()); \
		else \
			pmet_strbuf = per_cpu(met_strbuf, smp_processor_id()); \
		pmet_strbuf;\
	})

#define PUT_MET_PRINTK_BUFFER_EXIT_CRITICAL(pmet_strbuf) \
	do {\
		if (pmet_strbuf)\
			TRACE_PUTS(pmet_strbuf); \
		preempt_enable_no_resched(); \
	} while (0)


#define MET_PRINTK(FORMAT, args...) \
	do { \
		char *pmet_strbuf; \
		preempt_disable(); \
		if (in_nmi()) \
			pmet_strbuf = per_cpu(met_strbuf_nmi, smp_processor_id()); \
		else if (in_irq()) \
			pmet_strbuf = per_cpu(met_strbuf_irq, smp_processor_id()); \
		else if (in_softirq()) \
			pmet_strbuf = per_cpu(met_strbuf_sirq, smp_processor_id()); \
		else \
			pmet_strbuf = per_cpu(met_strbuf, smp_processor_id()); \
		if (met_mode & MET_MODE_TRACE_CMD) \
			snprintf(pmet_strbuf, MET_STRBUF_SIZE, "%s: " FORMAT, __func__, ##args); \
		else \
			snprintf(pmet_strbuf, MET_STRBUF_SIZE, FORMAT, ##args); \
		TRACE_PUTS(pmet_strbuf); \
		preempt_enable_no_resched(); \
	} while (0)
/*#endif*/

#define MET_FTRACE_PRINTK(TRACE_NAME, args...)			\
	do {							\
		trace_##TRACE_NAME(args);;			\
	} while (0)


#define MET_TYPE_PMU	1
#define MET_TYPE_BUS	2
#define MET_TYPE_MISC	3

struct metdevice {

	struct list_head list;
	int type;
	const char *name;
	struct module *owner;
	struct kobject *kobj;

	int (*create_subfs)(struct kobject *parent);
	void (*delete_subfs)(void);
	int mode;
	int cpu_related;
	int polling_interval;
	int polling_count_reload;
	int __percpu *polling_count;
	int header_read_again; /*for header size > 1 page*/
	void (*start)(void);
	void (*stop)(void);
	int (*reset)(void);
	void (*timed_polling)(unsigned long long stamp, int cpu);
	void (*tagged_polling)(unsigned long long stamp, int cpu);
	int (*print_help)(char *buf, int len);
	int (*print_header)(char *buf, int len);
	int (*process_argument)(const char *arg, int len);

	struct list_head exlist;	/* for linked list before register */
	void *reversed1;
};

int met_register(struct metdevice *met);
int met_deregister(struct metdevice *met);
int met_set_platform(const char *plf_name, int flag);
int met_set_topology(const char *topology_name, int flag);
int met_devlink_add(struct metdevice *met);
int met_devlink_del(struct metdevice *met);
int met_devlink_register_all(void);
int met_devlink_deregister_all(void);

int fs_reg(void);
void fs_unreg(void);


/******************************************************************************
 * Tracepoints
 ******************************************************************************/
/*#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
#	error Kernels prior to 2.6.32 not supported
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
#	define MET_DEFINE_PROBE(probe_name, proto) \
		static void probe_##probe_name(PARAMS(proto))
#	define MET_REGISTER_TRACE(probe_name) \
		register_trace_##probe_name(probe_##probe_name)
#	define MET_UNREGISTER_TRACE(probe_name) \
		unregister_trace_##probe_name(probe_##probe_name)
#else*/
#	define MET_DEFINE_PROBE(probe_name, proto) \
		static void probe_##probe_name(void *data, PARAMS(proto))
#	define MET_REGISTER_TRACE(probe_name) \
		register_trace_##probe_name(probe_##probe_name, NULL)
#	define MET_UNREGISTER_TRACE(probe_name) \
		unregister_trace_##probe_name(probe_##probe_name, NULL)
/*#endif*/



/* ====================== Tagging API ================================ */

#define MAX_EVENT_CLASS	31
#define MAX_TAGNAME_LEN	128
#define MET_CLASS_ALL	0x80000000

/* IOCTL commands of MET tagging */
/*typedef*/ struct mtag_cmd_t {
	unsigned int class_id;
	unsigned int value;
	unsigned int slen;
	char tname[MAX_TAGNAME_LEN];
	void *data;
	unsigned int size;
} /*mtag_cmd_t*/;

#define TYPE_START		1
#define TYPE_END		2
#define TYPE_ONESHOT	3
#define TYPE_ENABLE		4
#define TYPE_DISABLE	5
#define TYPE_REC_SET	6
#define TYPE_DUMP		7
#define TYPE_DUMP_SIZE	8
#define TYPE_DUMP_SAVE	9
#define TYPE_USRDATA	10
#define TYPE_DUMP_AGAIN		11
#define TYPE_ASYNC_START	12
#define TYPE_ASYNC_END	13

/* Use 'm' as magic number */
#define MTAG_IOC_MAGIC  'm'
/* Please use a different 8-bit number in your code */
#define MTAG_CMD_START		_IOW(MTAG_IOC_MAGIC, TYPE_START, struct mtag_cmd_t)
#define MTAG_CMD_END		_IOW(MTAG_IOC_MAGIC, TYPE_END, struct mtag_cmd_t)
#define MTAG_CMD_ONESHOT	_IOW(MTAG_IOC_MAGIC, TYPE_ONESHOT, struct mtag_cmd_t)
#define MTAG_CMD_ENABLE		_IOW(MTAG_IOC_MAGIC, TYPE_ENABLE, int)
#define MTAG_CMD_DISABLE	_IOW(MTAG_IOC_MAGIC, TYPE_DISABLE, int)
#define MTAG_CMD_REC_SET	_IOW(MTAG_IOC_MAGIC, TYPE_REC_SET, int)
#define MTAG_CMD_DUMP		_IOW(MTAG_IOC_MAGIC, TYPE_DUMP, struct mtag_cmd_t)
#define MTAG_CMD_DUMP_SIZE	_IOWR(MTAG_IOC_MAGIC, TYPE_DUMP_SIZE, int)
#define MTAG_CMD_DUMP_SAVE	_IOW(MTAG_IOC_MAGIC, TYPE_DUMP_SAVE, struct mtag_cmd_t)
#define MTAG_CMD_USRDATA	_IOW(MTAG_IOC_MAGIC, TYPE_USRDATA, struct mtag_cmd_t)
#define MTAG_CMD_DUMP_AGAIN	_IOW(MTAG_IOC_MAGIC, TYPE_DUMP_AGAIN, void *)
#define MTAG_CMD_ASYNC_START		_IOW(MTAG_IOC_MAGIC, TYPE_ASYNC_START, struct mtag_cmd_t)
#define MTAG_CMD_ASYNC_END		_IOW(MTAG_IOC_MAGIC, TYPE_ASYNC_END, struct mtag_cmd_t)

/* include file */
#ifndef MET_USER_EVENT_SUPPORT
#define met_tag_init() (0)

#define met_tag_uninit() (0)

#define met_tag_start(id, name) (0)

#define met_tag_end(id, name) (0)

#define met_tag_async_start(id, name , cookie) (0)

#define met_tag_async_end(id, name , cookie) (0)

#define met_tag_oneshot(id, name, value) (0)

#define met_tag_userdata(pData) (0)

#define met_tag_dump(id, name, data, length) (0)

#define met_tag_disable(id) (0)

#define met_tag_enable(id) (0)

#define met_set_dump_buffer(size) (0)

#define met_save_dump_buffer(pathname) (0)

#define met_save_log(pathname) (0)

#define met_record_on() (0)

#define met_record_off() (0)

#define met_show_bw_limiter() (0)
#define met_reg_bw_limiter() (0)
#define met_show_clk_tree() (0)
#define met_reg_clk_tree() (0)
#define met_ccf_clk_enable(clk) (0)
#define met_ccf_clk_disable(clk) (0)
#define met_ccf_clk_set_rate(clk, top) (0)
#define met_ccf_clk_set_parent(clk, parent) (0)
#define met_fh_print_dds(pll_id, dds_value) (0)

#else
#include <linux/kernel.h>
int __attribute__((weak)) met_tag_init(void);

int __attribute__((weak)) met_tag_uninit(void);

int __attribute__((weak)) met_tag_start(unsigned int class_id,
					const char *name);

int __attribute__((weak)) met_tag_end(unsigned int class_id,
					const char *name);

int __attribute__((weak)) met_tag_async_start(unsigned int class_id,
					const char *name,
					unsigned int cookie);

int __attribute__((weak)) met_tag_async_end(unsigned int class_id,
					const char *name,
					unsigned int cookie);

int __attribute__((weak)) met_tag_oneshot(unsigned int class_id,
					const char *name,
					unsigned int value);

int met_tag_userdata(char *pData);

int __attribute__((weak)) met_tag_dump(unsigned int class_id,
					const char *name,
					void *data,
					unsigned int length);

int __attribute__((weak)) met_tag_disable(unsigned int class_id);

int __attribute__((weak)) met_tag_enable(unsigned int class_id);

int __attribute__((weak)) met_set_dump_buffer(int size);

int __attribute__((weak)) met_save_dump_buffer(const char *pathname);

int __attribute__((weak)) met_save_log(const char *pathname);

int __attribute__((weak)) met_show_bw_limiter(void);
int __attribute__((weak)) met_reg_bw_limiter(void *fp);
int __attribute__((weak)) met_show_clk_tree(const char *name,
			unsigned int addr,
			unsigned int status);
int __attribute__((weak)) met_reg_clk_tree(void *fp);

int __attribute__((weak)) met_ccf_clk_enable(struct clk *clk);
int __attribute__((weak)) met_ccf_clk_disable(struct clk *clk);
int __attribute__((weak)) met_ccf_clk_set_rate(struct clk *clk, struct clk *top);
int __attribute__((weak)) met_ccf_clk_set_parent(struct clk *clk, struct clk *parent);

extern unsigned int __attribute__((weak)) met_fh_dds[];
int __attribute__((weak)) met_fh_print_dds(int pll_id, unsigned int dds_value);

#define met_record_on()		tracing_on()

#define met_record_off()	tracing_off()

#endif				/* MET_USER_EVENT_SUPPORT */



/******************************************************************************
 * User-Defined Trace Line
 ******************************************************************************/
#include "met_drv_cust_udtl_props.h"
#define MET_UDTL_PROP_TYPE	ulong


/*
 * Declare PROPS Data Structure
 */
#undef		_PROP_X_
#define		_PROP_X_(f)	ulong f;
#undef	MET_UDTL_TRACELINE
#define MET_UDTL_TRACELINE(_PAGE_NAME_, _TRACELINE_NAME_ , _PROPS_NAME_)\
	struct _PROPS_NAME_ ## _t {\
	_PROPS_NAME_\
	};\
	extern struct _PROPS_NAME_ ## _t    _g_ ## _PROPS_NAME_;

#include "met_drv_cust_udtl_traceline.h"


/*
 * Instance a PROPS Structure
 */
#define MET_UDTL_DEFINE_PROP(_PROPS_NAME_)\
		struct _PROPS_NAME_ ## _t    _g_ ## _PROPS_NAME_;


/*
 * Access PROPS Structure
 */
#define MET_UDTL_GET_PROP(_PROPS_NAME_)	_g_ ## _PROPS_NAME_

/*
 * PROPS Structure related
 */
#define MET_UDTL_PROP_DATATYPE(_PROPS_NAME_) struct _PROPS_NAME_ ## _t
#define MET_UDTL_PROP_SIZE(_PROPS_NAME_) sizeof(struct _PROPS_NAME_ ## _t)



/*
 * Functions Call
 */
#define MET_UDTL_TRACELINE_BEGIN(_PAGE_NAME_, _TRACELINE_NAME_, _FMTSTR_, ...) \
{\
	extern __attribute__((weak))\
	void ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _BEGIN(const char *fmtstr, ...);\
	do {\
		if (ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _BEGIN)\
			ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _BEGIN(_FMTSTR_, ## __VA_ARGS__);\
	} while (0);\
}

#define MET_UDTL_TRACELINE_END(_PAGE_NAME_, _TRACELINE_NAME_, _FMTSTR_, ...) \
{\
	extern __attribute__((weak))\
	void ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _END(const char *fmtstr, ...);\
	do {\
		if (ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _END)\
			ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _END(_FMTSTR_, ## __VA_ARGS__);\
	} while (0);\
}

#define MET_UDTL_TRACELINE_PROP(_PAGE_NAME_, _TRACELINE_NAME_ , _PROPS_NAME_) \
{\
	extern __attribute__((weak))\
	void ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _PROP(struct _PROPS_NAME_ ## _t *info);\
	do {\
		if (ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _PROP)\
			ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _PROP(&_g_ ## _PROPS_NAME_);\
	} while (0);\
}

#define MET_UDTL_TRACELINE_DEBUG(_PAGE_NAME_, _TRACELINE_NAME_ , _DEBUG_STR_) \
{\
	extern __attribute__((weak))\
	void ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _DEBUG(char *debug_str);\
	do {\
		if (ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _DEBUG)\
			ms_udtl_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _DEBUG(_DEBUG_STR_);\
	} while (0);\
}

#define MET_UDTL_IS_TRACELINE_ON(_PAGE_NAME_, _TRACELINE_NAME_) \
({\
	extern __attribute__((weak))\
	int ms_udtl_is_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _on(void);\
	int on = 0;\
	do {\
		if (ms_udtl_is_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _on)\
			on = ms_udtl_is_ ## _PAGE_NAME_ ## _ ## _TRACELINE_NAME_ ## _on();\
	} while (0);\
	on;\
})

/*
 * Wrapper for DISP/MDP/GCE mmsys profiling
 */

void __attribute__((weak)) met_mmsys_event_gce_thread_begin(ulong thread_no, ulong task_handle, ulong engineFlag,
								void *pCmd, ulong size);
void __attribute__((weak)) met_mmsys_event_gce_thread_end(ulong thread_no, ulong task_handle, ulong engineFlag);

void __attribute__((weak)) met_mmsys_event_disp_sof(int mutex_id);
void __attribute__((weak)) met_mmsys_event_disp_mutex_eof(int mutex_id);
void __attribute__((weak)) met_mmsys_event_disp_ovl_eof(int ovl_id);

void __attribute__((weak)) met_mmsys_config_isp_base_addr(unsigned long *isp_reg_list);
void __attribute__((weak)) met_mmsys_event_isp_pass1_begin(int sensor_id);
void __attribute__((weak)) met_mmsys_event_isp_pass1_end(int sensor_id);





#endif				/* MET_DRV */
