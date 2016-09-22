#ifndef __MULTILOG_H__
#define __MULTILOG_H__

#include <linux/ioctl.h>

#define LOG_TYPE_MAIN		0X01
#define LOG_TYPE_RADIO		0X02
#define LOG_TYPE_EVENTS		0X03
#define LOG_TYPE_SYSTEM		0X04
#define LOG_TYPE_CRASH		0X05
#define LOG_TYPE_KERNEL		0X06
#define LOG_TYPE_BT			0X07
#define LOG_TYPE_BTHCI		0X08
#define LOG_TYPE_BTFW		0X09
#define LOG_TYPE_MD32		0X0a
#define LOG_TYPE_ATF		0X0b

#define LOG_TYPE_NETSTATS	0X20
#define LOG_TYPE_NETINFO	0X21
#define LOG_TYPE_NETLOG		0X22
#define LOG_TYPE_NETPING	0X23

#define LOG_TYPE_MD1		0X30
#define LOG_TYPE_MD2		0X31
#define LOG_TYPE_MD3		0X32
#define LOG_TYPE_MD4		0X33
#define LOG_TYPE_MD5		0X34
#define LOG_TYPE_MD6		0X35
#define LOG_TYPE_MD7		0X36
#define LOG_TYPE_MD8		0X37
#define LOG_TYPE_EE_MD1		0X38
#define LOG_TYPE_EE_MD2		0X39
#define LOG_TYPE_EE_MD3		0X3a
#define LOG_TYPE_EE_MD4		0X3b
#define LOG_TYPE_EE_MD5		0X3c
#define LOG_TYPE_EE_MD6		0X3d
#define LOG_TYPE_EE_MD7		0X3e
#define LOG_TYPE_EE_MD8		0X3f
#define LOG_TYPE_VERSION_MD1	0X40
#define LOG_TYPE_VERSION_MD2	0X41
#define LOG_TYPE_VERSION_MD3	0X42
#define LOG_TYPE_VERSION_MD4	0X43
#define LOG_TYPE_VERSION_MD5	0X44
#define LOG_TYPE_VERSION_MD6	0X45
#define LOG_TYPE_VERSION_MD7	0X46
#define LOG_TYPE_VERSION_MD8	0X47
#define LOG_TYPE_DB_MD1		0X48
#define LOG_TYPE_DB_MD2		0X49
#define LOG_TYPE_DB_MD3		0X4a
#define LOG_TYPE_DB_MD4		0X4b
#define LOG_TYPE_DB_MD5		0X4c
#define LOG_TYPE_DB_MD6		0X4d
#define LOG_TYPE_DB_MD7		0X4e
#define LOG_TYPE_DB_MD8		0X4f
#define LOG_TYPE_DBG_MD1	0X50
#define LOG_TYPE_DBG_MD2	0X51
#define LOG_TYPE_DBG_MD3	0X52
#define LOG_TYPE_DBG_MD4	0X53
#define LOG_TYPE_DBG_MD5	0X54
#define LOG_TYPE_DBG_MD6	0X55
#define LOG_TYPE_DBG_MD7	0X56
#define LOG_TYPE_DBG_MD8	0X57

#define LOG_TYPE_MAX	0X58
#define LOG_TYPE_SHIF	(6*4)

#define PATH_LEN	255
#define FOLDER_LEN		255
#define FILE_NAME_LEN	255
struct multi_log_start {
	u32 log_type;
	char path[PATH_LEN];
	char folder[FOLDER_LEN];
};

#define ONE_FILE_SIZE	0X01
#define FOLDER_SIZE	0X02
#define TOTAL_SIZE	0X03

struct multi_log_set_size {
	u32 log_type;
	u32 file_type;
	u32 size;
};

struct multi_log_write_buff {
	u32 log_type;
	u32 len;
	u64 buf_point;
};

struct multi_log_copy_file {
	char file[FOLDER_LEN];
};

struct multi_log_EE_type {
	u32 log_type;
	char ee_type[50];
};
/*----------------------------------------------------------------------------*/
#define MULLOG_IOC_MAGIC           0x82

/* start to write*/
#define MULLOG_IO_START			_IOW(MULLOG_IOC_MAGIC, 0x01, struct multi_log_start)

/* stop to write  */
#define MULLOG_IO_STOP			_IOW(MULLOG_IOC_MAGIC, 0x02, uint32_t)

/* set file size */
#define MULLOG_IO_FILE_SIZE		_IOW(MULLOG_IOC_MAGIC, 0x03, struct multi_log_set_size)

/* function test  */
#define MULLOG_IO_TEST			_IOW(MULLOG_IOC_MAGIC, 0x04, uint32_t)


#define MULLOG_IO_MOD_EE		_IOW(MULLOG_IOC_MAGIC, 0x05, uint32_t)

/* start to write*/
#define MULLOG_IO_WRITE			_IOW(MULLOG_IOC_MAGIC, 0x06, struct multi_log_write_buff)


/* copy file*/
#define MULLOG_IO_COPY_FILE		_IOW(MULLOG_IOC_MAGIC, 0x07, struct multi_log_copy_file)

/* EE type*/
#define MULLOG_IO_EE_TYPE		_IOW(MULLOG_IOC_MAGIC, 0x08, struct multi_log_EE_type)




#endif
