#ifndef __AAL_CONTROL_H__
#define __AAL_CONTROL_H__

#define AAL_TAG                  "[ALS/AAL]"
#define AAL_LOG(fmt, args...)	 pr_debug(AAL_TAG fmt, ##args)
#define AAL_FUN(f)               pr_debug(AAL_TAG"%s\n", __FUNCTION__)
#define AAL_ERR(fmt, args...)    pr_err(AAL_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#endif
extern int aal_use;



