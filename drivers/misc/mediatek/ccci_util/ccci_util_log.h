#ifndef __CCCI_UTIL_LOG_H__
#define __CCCI_UTIL_LOG_H__

/* No MD id message part */
#define CCCI_UTIL_DBG_MSG(fmt, args...) \
/*	pr_debug("[ccci0/util]" fmt, ##args); */

#define CCCI_UTIL_INF_MSG(fmt, args...) \
	do { \
		ccci_dump_write(MD_SYS1, CCCI_DUMP_INIT, 0, "[util]" fmt, ##args); \
	} while (0)
/*	pr_debug("[ccci0/util]" fmt, ##args); */

#define CCCI_UTIL_ERR_MSG(fmt, args...) \
	do { \
		pr_err("[ccci0/util]" fmt, ##args); \
		ccci_dump_write(MD_SYS1, CCCI_DUMP_INIT, 0, "[util]" fmt, ##args); \
	} while (0)

/* With MD id message part */
#define CCCI_UTIL_DBG_MSG_WITH_ID(id, fmt, args...) \
/*	pr_debug("[ccci%d/util]" fmt, (id+1), ##args); */

#define CCCI_UTIL_INF_MSG_WITH_ID(id, fmt, args...) \
	do { \
		ccci_dump_write(id, CCCI_DUMP_INIT, 0, "[%d:util]" fmt, (id+1), ##args); \
	} while (0)
/*	pr_debug("[ccci%d/util]" fmt, (id+1), ##args); */

#define CCCI_UTIL_NOTICE_MSG_WITH_ID(id, fmt, args...) \
	pr_notice("[ccci%d/util]" fmt, (id+1), ##args);

#define CCCI_UTIL_ERR_MSG_WITH_ID(id, fmt, args...) \
	do { \
		pr_err("[ccci%d/util]" fmt, (id+1), ##args); \
		ccci_dump_write(id, CCCI_DUMP_INIT, 0, "[%d:util]" fmt, (id+1), ##args); \
	} while (0)

/* With MD id message and will repeat exeute part */
#define CCCI_UTIL_DBG_MSG_WITH_ID_REP(id, fmt, args...) \
/*	pr_debug("[ccci%d/util]" fmt, (id+1), ##args); */

#define CCCI_UTIL_INF_MSG_WITH_ID_REP(id, fmt, args...) \
	do { \
		ccci_dump_write(id, CCCI_DUMP_BOOTUP, CCCI_DUMP_TIME_FLAG, "[%d:util]" fmt, (id+1), ##args); \
	} while (0)
/*	pr_debug("[ccci%d/util]" fmt, (id+1), ##args); */

#define CCCI_UTIL_ERR_MSG_WITH_ID_REP(id, fmt, args...) \
	do { \
		pr_err("[ccci%d/util]" fmt, (id+1), ##args); \
		ccci_dump_write(id, CCCI_DUMP_BOOTUP, CCCI_DUMP_TIME_FLAG, "[%d:util]" fmt, (id+1), ##args); \
	} while (0)

#endif /*__CCCI_UTIL_LOG_H__ */
