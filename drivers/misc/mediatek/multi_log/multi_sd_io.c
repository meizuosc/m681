#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/memory.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/syscalls.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/statfs.h>
#include <linux/stat.h>
#include <linux/dirent.h>
#include <linux/seq_file.h>

#include "multi_log.h"


#define TAG		"multi_log"
#define MTKLOG_FOLDER		"/mtklog"
#define MD_FOLDER			"/mdlog"
#define AP_FOLDER			"/aplog"
#define AP_LOG_TYPE			0X00
#define MD_LOG_TYPE			0x01
#define FILE_TREE			"/file_tree.txt"
#define MD_FOLDER_PRE		"/MDLog_"
#define AP_FOLDER_PRE		"/APLog_"
#define TEMP_FILE			".tmp"
#define FOLDER_FILE_COUNT	0X05


#define LOG_MAGIC	0XACDE1342
#define LOG_TYPE_MASK 0XFF000000
#define LOG_LENGTH_MASK	0XFFFFFF

#define MDLOG_EE_EXCEPTION_MUL	0x01
#define MDLOG_EE_EXCEPTION_MUL_FINISH	0X02
#define MDLOG_EE_FILE			0X04
#define MDLOG_EE_EXCEPTION		0X10
#define MDLOG_EE_EXCEPTION_1	0x20
#define MDLOG_EE_EXCEPTION_3	0x40

#define MDLOG_DB_NEW	0X01
#define	MDLOG1_HEADER		0X02

#define MIN_STORAGE_SIZE 50	/* store min 50M */
static char store_path[PATH_LEN];
static bool mdlog_db[16];



#define MAX_SIZE_DEFAULT	20	/*Default max size 20M each file */
struct logger_header {
	u32 magic;
	u32 total_size;
	u32 tex_size;
	u32 type;
	u32 attr;
	u32 index;
	u32 reserve[4];
};

struct filep_struct {
	struct file *filep[3];	/* write file, folder file tree, folder2 file tree*/
	size_t file_size;
	loff_t pos[3];
	char folder_path[FILE_NAME_LEN];	/* /storage/sdcard0/mtklog/mdlog */
	char folder_name[FILE_NAME_LEN];	/* /storage/sdcard0/mtklog/mdlog/MDLog_2015_0101_190835 */
	char file_name[FILE_NAME_LEN];		/* /ap_4.mmlts */
};

static bool log_type_status[LOG_TYPE_MAX];
static int aplog_count, mdlog_count;
static struct mutex log_lock_ap;
static struct mutex log_lock_md;
static struct mutex log_lock_io;
static u32 file_size[2][3];	/* Ap & MD log*/
static u32 MDlog_EE_status;
static struct filep_struct apfilep;
static struct filep_struct mdfilep;
static bool md1_header[2];
static char EE_type[50];

bool folder_is_exit(const char *pathname)
{
	mm_segment_t old_fs;
	int fd;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fd = sys_open(pathname, O_RDONLY, 0);
	if (fd < 0) {
		pr_err("%s: open folder %s failed.\n", TAG, pathname);
		set_fs(old_fs);
		return false;
	}
	set_fs(old_fs);
	sys_close(fd);
	return true;
}

struct file *open_create_file(const char *pathname)
{
	struct file *filp;
	int value, f_mode;
/*check folder whether exit, or create it*/
	f_mode = O_CREAT|O_RDWR;

	filp = filp_open(pathname, f_mode, 0);
	value = (int)IS_ERR(filp);
	if (value) {
		pr_err("%s: create file %s error 0x%x.\n", TAG, pathname, value);
		return NULL;
	}
	pr_err("%s: create file %s ok, count %ld.\n", TAG, pathname, file_count(filp));

	return filp;
}

int close_open_file(struct file *filp, fl_owner_t id)
{
	int ret = 0;
	int count;

	count = file_count(filp);
	while (count != 0) {
		ret = filp_close(filp, id);
		count = file_count(filp);
		pr_err("%s: close file name %s count %d.\n", TAG, filp->f_path.dentry->d_iname, count);
	}

	return ret;
}

static ssize_t multi_write_file_user(struct filep_struct *write_filep, int num, const char __user *buff, ssize_t count)
{
	ssize_t write_size;
	int retry_count = 0;

	if (write_filep->filep[num] == NULL) {
		pr_err("%s: the %d file point is NULL.\n", TAG, num);
		return 0;
	}

	if (count == 0)
		return 0;
retry1:
	write_size = vfs_write(write_filep->filep[num], buff, count, &write_filep->pos[num]);
	if (write_size == -EINTR && retry_count < 20) {
		retry_count++;
		goto retry1;
	}


	if (write_size <= 0)
		pr_err("%s: write to sd error %d, count %d.\n", TAG, (int)write_size, (int)count);
	else if (num == 0)
		write_filep->file_size += write_size;
	return write_size;

}

static ssize_t multi_write_file(struct filep_struct *write_filep, int num, const char *buff, ssize_t count)
{
	mm_segment_t old_fs;
	ssize_t write_size;
	int retry_count = 0;

	if (write_filep->filep[num] == NULL) {
		pr_err("%s: the %d file point is NULL.\n", TAG, num);
		return 0;
	}

	if (count == 0)
		return 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
retry1:
	write_size = vfs_write(write_filep->filep[num], buff, count, &write_filep->pos[num]);
	if (write_size == -EINTR && retry_count < 20) {
		retry_count++;
		goto retry1;
	}
	set_fs(old_fs);

	if (write_size < 0)
		pr_err("%s: write to sd error %d.\n", TAG, (int)write_size);
	else if (num == 0)
		write_filep->file_size += write_size;
	return write_size;
}

int del_file(const char *pathname)
{
	mm_segment_t old_fs;
	int ret;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_unlink(pathname);
	set_fs(old_fs);
	if (ret < 0)
		pr_err("%s: del_file %s error %d.\n", TAG, pathname, ret);

	return ret;
}


int rename_folder(const char *oldpathname, const char *newpathname)
{
	mm_segment_t old_fs;
	int ret;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_renameat(AT_FDCWD, oldpathname, AT_FDCWD, newpathname);
	set_fs(old_fs);
	if (ret < 0)
		pr_err("%s: rename_folder %s --> %s error %d.\n", TAG, oldpathname, newpathname, ret);

	return ret;
}

int storage_is_full(void)
{
	struct statfs folder_statfs;
	mm_segment_t old_fs;
	int free_size;

	if (store_path == NULL) {
		pr_err("%s: store path is NULL.\n", TAG);
		return 2;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	sys_statfs(store_path, &folder_statfs);
	set_fs(old_fs);

	if (folder_statfs.f_blocks == 0) {
		pr_err("%s: sdcard unavailable, retrying.\n", TAG);
		return 2;
	}

	free_size = (folder_statfs.f_bavail/1024) * (folder_statfs.f_bsize/1024);

	if (free_size < MIN_STORAGE_SIZE) {
		pr_err("%s: storage is almost full.\n", TAG);
		return 1;
	}
	return 0;
}



void find_first_folder(const char *pathname, char *folderpath)
{
	int fd;
	void *buff_point;
	mm_segment_t old_fs;
	int num;
	long last_acctime = 0;
	struct linux_dirent64 *dirp;
	char string_tmp[FOLDER_LEN];
	char string_tmp2[FOLDER_LEN];

	strcpy(string_tmp, pathname);
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fd = sys_open(string_tmp, O_RDONLY, 0);
	if (fd < 0) {
		pr_err("%s: open folder failed.\n", TAG);
		set_fs(old_fs);
		return;
	}
	buff_point = kzalloc(1024, GFP_KERNEL);
	if (!buff_point) {
		sys_close(fd);
		set_fs(old_fs);
		return;
	}
	dirp = buff_point;
	num = sys_getdents64(fd, dirp, 1024);
	while (num > 0) {
		while (num > 0) {
			struct stat st;
			int ret;

			memset(string_tmp2, 0, FOLDER_LEN);
			strcpy(string_tmp2, string_tmp);
			strcat(string_tmp2, "/");
			strcat(string_tmp2, (char *)dirp->d_name);
			ret = sys_newlstat(string_tmp2, &st);
			if (!ret)
				if (S_ISDIR(st.st_mode) && dirp->d_name[0] != '.') {
					if (last_acctime == 0 || st.st_atime < last_acctime) {
						last_acctime = st.st_atime;
						strcpy(folderpath, string_tmp2);
					}
				}
			num -= dirp->d_reclen;
			dirp = (void *)dirp + dirp->d_reclen;
		}
		dirp = buff_point;
		memset(buff_point, 0, 1024);
		num = sys_getdents64(fd, dirp, 1024);
	}

	set_fs(old_fs);
	sys_close(fd);
	kfree(buff_point);
}

void del_folder(const char *pathname)
{
	int fd;
	void *buff_point;
	mm_segment_t old_fs;
	int num;
	struct linux_dirent64 *dirp;
	char string_tmp[FOLDER_LEN];
	char string_tmp2[FOLDER_LEN];
	int ret;

	strcpy(string_tmp, pathname);
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fd = sys_open(string_tmp, O_RDONLY, 0);
	if (fd < 0) {
		pr_err("%s: open folder failed.\n", TAG);
		return;
	}
	buff_point = kzalloc(1024, GFP_KERNEL);
	if (!buff_point) {
		sys_close(fd);
		return;
	}
	dirp = buff_point;

	num = sys_getdents64(fd, dirp, 1024);
	while (num > 0) {
		while (num > 0) {
			struct stat st;

			memset(string_tmp2, 0, FOLDER_LEN);
			strcpy(string_tmp2, string_tmp);
			strcat(string_tmp2, "/");
			strcat(string_tmp2, (char *)dirp->d_name);
			ret = sys_newlstat(string_tmp2, &st);

			if (!ret) {
				if (S_ISDIR(st.st_mode)) {
					if (dirp->d_name[0] != '.') {
						del_folder(string_tmp2);
						pr_err("%s: del folder %s.\n", TAG, string_tmp2);
						sys_rmdir(string_tmp2);
					}
				} else {
					pr_err("%s: del file %s.\n", TAG, string_tmp2);
					sys_unlink(string_tmp2);
				}
			}
			num -= dirp->d_reclen;
			dirp = (void *)dirp + dirp->d_reclen;
		}
		dirp = buff_point;
		memset(buff_point, 0, 1024);
		num = sys_getdents64(fd, dirp, 1024);
	}
	ret = sys_rmdir(string_tmp);
	set_fs(old_fs);
	if (ret < 0)
		pr_err("%s: sys_rmdir %s error %d.\n", TAG, string_tmp, ret);
	sys_close(fd);
	kfree(buff_point);
}

void rotation_folder(struct filep_struct *filep)
{
	char string_tmp[FOLDER_LEN];

	find_first_folder(filep->folder_path, string_tmp);
	pr_err("%s: first folder name %s.\n", TAG, string_tmp);
	del_folder(string_tmp);

}

long folder_total_size(const char *pathname)
{
	int fd;
	void *buff_point;
	mm_segment_t old_fs;
	int num;
	long folder_size = 0;
	struct linux_dirent64 *dirp;
	char string_tmp[FOLDER_LEN];
	char string_tmp2[FOLDER_LEN];

	strcpy(string_tmp, pathname);
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fd = sys_open(string_tmp, O_RDONLY, 0);
	if (fd < 0) {
		pr_err("%s: open folder %s failed.\n", TAG, string_tmp);
		set_fs(old_fs);
		return -1;
	}
	buff_point = kzalloc(1024, GFP_KERNEL);
	if (!buff_point) {
		sys_close(fd);
		set_fs(old_fs);
		return -1;
	}
	dirp = buff_point;
	num = sys_getdents64(fd, dirp, 1024);
	while (num > 0) {
		while (num > 0) {
			struct stat st;
			int ret;

			memset(string_tmp2, 0, FOLDER_LEN);
			strcpy(string_tmp2, string_tmp);
			strcat(string_tmp2, "/");
			strcat(string_tmp2, (char *)dirp->d_name);
			ret = sys_newlstat(string_tmp2, &st);
			if (!ret) {
				if (S_ISDIR(st.st_mode) && dirp->d_name[0] != '.')
					folder_size += folder_total_size(string_tmp2);
				else
					folder_size += st.st_size;
			}
			num -= dirp->d_reclen;
			dirp = (void *)dirp + dirp->d_reclen;
		}
		dirp = buff_point;
		memset(buff_point, 0, 1024);
		num = sys_getdents64(fd, dirp, 1024);
	}
	set_fs(old_fs);

	sys_close(fd);
	kfree(buff_point);
	return folder_size;
}




long get_file_size(const char *pathname)
{
	mm_segment_t old_fs;
	struct stat st;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_newlstat(pathname, &st);
	set_fs(old_fs);
	if (!ret)
		return st.st_size;
	else
		return 0;
}

void getnowtime(char *string)
{
	struct timeval txc;
	struct rtc_time tm;

	do_gettimeofday(&txc);
	rtc_time_to_tm(txc.tv_sec, &tm);
	sprintf(string, "%4d_%02d%02d_%02d%02d%02d",
		tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	string[16] = 0;
}

int create_folder(const char *pathname)
{
	mm_segment_t old_fs;
	unsigned int file_mode, count = 0;
	long ret;

	file_mode = S_IALLUGO;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
retry:
	ret = sys_mkdirat(AT_FDCWD, pathname, file_mode);
	if (ret < 0 && count < 5 && ret != -EEXIST) {
		pr_err("%s: mkdir %s error %ld.\n", TAG, pathname, ret);
		count++;
		goto retry;
	}
	set_fs(old_fs);
	return ret;
}

int create_file_tree(const char *pathname, int fdnum, int ftnum)
{
	int ret = 0;
	long file_size;
	char string[FOLDER_LEN] = {0};

	if (folder_is_exit(pathname) == false) {
		ret = create_folder(pathname);
		if (ret < 0 && ret != -EEXIST)
			return -1;
	}

	memset(string, 0, FOLDER_LEN);
	strcpy(string, pathname);
	strcat(string, FILE_TREE);

	if (fdnum == AP_LOG_TYPE) {
		apfilep.filep[ftnum] = open_create_file(string);
		/* file tree size */
		if (apfilep.filep[ftnum] != NULL) {
			file_size = get_file_size(string);
			if (file_size > MIN_STORAGE_SIZE * 1024 * 1024) {
				close_open_file(apfilep.filep[ftnum], NULL);
				del_file(string);
				apfilep.filep[ftnum] = open_create_file(string);
				apfilep.pos[ftnum] = 0;
			} else
				apfilep.pos[ftnum] = file_size;
		}
		if (apfilep.filep[ftnum] == NULL)
			return -1;
	} else {
		mdfilep.filep[ftnum] = open_create_file(string);
		/* file tree size */
		if (mdfilep.filep[ftnum] != NULL) {
			file_size = get_file_size(string);
			if (file_size > MIN_STORAGE_SIZE * 1024 * 1024) {
				close_open_file(mdfilep.filep[ftnum], NULL);
				del_file(string);
				mdfilep.filep[ftnum] = open_create_file(string);
				mdfilep.pos[ftnum] = 0;
			} else
				mdfilep.pos[ftnum] = file_size;
		}
		if (mdfilep.filep[ftnum] == NULL)
			return -1;
	}
	return 0;
}

void remove_tmp_file_name(const char *pathname)
{
	char string[FOLDER_LEN] = {0};
	int str_len;

	str_len = strlen(pathname);
	/* .tmp*/
	string[0] = pathname[str_len - 4];
	string[1] = pathname[str_len - 3];
	string[2] = pathname[str_len - 2];
	string[3] = pathname[str_len - 1];

	if (strncmp(string, TEMP_FILE, 4))
		return;
	strcpy(string, pathname);
	string[str_len - 4] = 0;
	rename_folder(pathname, string);
}

void remove_filp_file_tmp_name(struct filep_struct *pfilep)
{
	char string_tmp[FOLDER_LEN];

	if (pfilep->filep[0] != NULL) {
		close_open_file(pfilep->filep[0], NULL);
		/* remove  tmp file name*/
		memset(string_tmp, 0, FOLDER_LEN);
		strcpy(string_tmp, pfilep->folder_name);
		strcat(string_tmp, pfilep->file_name);
		remove_tmp_file_name(string_tmp);
		pfilep->filep[0] = NULL;
		return;
	}
	return;
}

void create_log_folder(int fdnum)
{
	int i = 0, ret = 0;
	char string[FOLDER_LEN] = {0};

	strcpy(string, store_path);
	if (folder_is_exit(string) == false)
		create_folder(string);

	if (fdnum == AP_LOG_TYPE) {
		for (i = 0; i < 2; i++)
			if (apfilep.filep[i] != NULL) {
				ret = filp_close(apfilep.filep[i], 0);
				if (ret)
					pr_err("%s: filp_close ap filep %d error %d.\n", TAG, i, ret);
				apfilep.filep[i] = NULL;
				apfilep.pos[i] = 0;
			}
		apfilep.file_size = 0;

		strcat(string, AP_FOLDER);
		create_folder(string);
		create_file_tree(string, AP_LOG_TYPE, 1);
	} else {
		for (i = 0; i < 3; i++)
			if (mdfilep.filep[i] != NULL) {
				ret = filp_close(mdfilep.filep[i], 0);
				if (ret)
					pr_err("%s: filp_close md filep %d error %d.\n", TAG, i, ret);
				mdfilep.filep[i] = NULL;
				mdfilep.pos[i] = 0;
			}
		mdfilep.file_size = 0;

		strcat(string, MD_FOLDER);
		create_folder(string);
		create_file_tree(string, MD_LOG_TYPE, 1);
	}
}

void reset_allfd(void)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < 3; i++) {
		if (apfilep.filep[i] != NULL) {
			ret = close_open_file(apfilep.filep[i], 0);
			if (ret)
				pr_err("%s: filp_close apfilep %d error %d.\n", TAG, i, ret);
			apfilep.pos[i] = 0;
		}
		if (mdfilep.filep[i] != NULL) {
			ret = close_open_file(mdfilep.filep[i], 0);
			if (ret)
				pr_err("%s: filp_close mpfilep %d error %d.\n", TAG, i, ret);
			mdfilep.pos[i] = 0;
		}
	}
	apfilep.file_size = 0;
	mdfilep.file_size = 0;
}

int copy_md_file(const char *pathname)
{
	int len1, len2, len3, ret;
	char *buf;

	len1 = strlen(mdfilep.folder_path);
	len2 = strlen(pathname);
	len3 = strlen(mdfilep.folder_name);
	if (len1 == 0 || len2 == 0 || len3 == 0 || len2 <= len1)
		return -1;
	if (strncmp(pathname, mdfilep.folder_path, len1))
		return -1;
	buf = kmalloc(len2 - len1 + len3 + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;
	strcpy(buf, mdfilep.folder_name);
	strcat(buf, pathname + len1);
	ret = rename_folder(pathname, buf);
	kfree(buf);
	return ret;
}

void io_log_start(struct multi_log_start log_start)
{
	int string_len;
	char string_tmp[FOLDER_LEN] = {0};
	long folder_size;
	int num = 0;

	if (log_start.log_type > LOG_TYPE_MAX)
		return;

	if (log_start.path[0] == 0)
		return;

	string_len = strlen(log_start.path);
	if (log_start.path[string_len-1] == '/') { /* / ascii 47*/
		log_start.path[string_len-1] = 0;
	}

	if (log_start.log_type < LOG_TYPE_MD1)
		num = 0;
	else
		num = 1;

	if (store_path[0] == 0) {	/* get file_tree.txt size, and set its pos value*/
		strcpy(store_path, log_start.path);
		strcat(store_path, MTKLOG_FOLDER);
		if (folder_is_exit(store_path) == false)
			create_folder(store_path);
		pr_warn("%s: store path is null, set it to %s.\n", TAG, store_path);

	} else if (strncmp(store_path, log_start.path, strlen(log_start.path)) != 0) {
		pr_warn("%s: new store path %s.\n", TAG, log_start.path);
		strcpy(store_path, log_start.path);
		strcat(store_path, MTKLOG_FOLDER);
		/*to-do: reset log type status*/
		reset_allfd();
		if (folder_is_exit(store_path) == false)
			create_folder(store_path);
	}

	if (log_start.log_type < LOG_TYPE_MD1) {
		if (log_type_status[log_start.log_type] == false) {
			if (aplog_count == 0) {
				apfilep.file_size = 0;
				apfilep.pos[0] = 0;
				strcpy(apfilep.folder_path, store_path);
				strcat(apfilep.folder_path, AP_FOLDER);

				if (folder_is_exit(apfilep.folder_path) == false)
					create_log_folder(AP_LOG_TYPE);

				/* Rotation mtk aplog folder */
				folder_size = folder_total_size(apfilep.folder_path);
				folder_size = folder_size / 1024 / 1024;
				if (folder_size > file_size[0][2] - file_size[0][1]/2) {
					pr_err("%s: total folder size %ld.\n", TAG, folder_size);
					rotation_folder(&apfilep);
				}

				if (log_start.folder[0] != 0) { /* folder name */
					string_len = strlen(log_start.folder);
					if (log_start.folder[string_len-1] == '/') { /* / ascii 47*/
						log_start.folder[string_len-1] = 0;
					}
					strcpy(apfilep.folder_name, log_start.folder);
				} else {
					memset(string_tmp, 0, FOLDER_LEN);
					getnowtime(string_tmp);
					strcpy(apfilep.folder_name, apfilep.folder_path);
					strcat(apfilep.folder_name, AP_FOLDER_PRE);
					strcat(apfilep.folder_name, string_tmp);
				}

				if (folder_is_exit(apfilep.folder_name) == false)
					create_folder(apfilep.folder_name);
				pr_warn("%s: create new folder %s.\n", TAG, apfilep.folder_name);

				if (apfilep.filep[1] == NULL) {
					memset(string_tmp, 0, FOLDER_LEN);
					strcpy(string_tmp, apfilep.folder_path);
					create_file_tree(string_tmp, AP_LOG_TYPE, 1);
				}
				if (apfilep.pos[1] > 0)
					multi_write_file(&apfilep, 1, "\r", 1);
				multi_write_file(&apfilep, 1, apfilep.folder_name, strlen(apfilep.folder_name));
			}

			aplog_count++;
			log_type_status[log_start.log_type] = true;
		}
	} else {
		if (log_type_status[log_start.log_type] == false) {
			if (mdlog_count == 0) {
				mdfilep.file_size = 0;
				mdfilep.pos[0] = 0;
				strcpy(mdfilep.folder_path, store_path);
				strcat(mdfilep.folder_path, MD_FOLDER);

				if (folder_is_exit(mdfilep.folder_path) == false)
					create_log_folder(MD_LOG_TYPE);


				if (log_start.folder[0] != 0) { /* folder name */
					string_len = strlen(log_start.folder);
					if (log_start.folder[string_len-1] == '/') { /* / ascii 47*/
						log_start.folder[string_len-1] = 0;
					}
					strcpy(mdfilep.folder_name, log_start.folder);
				} else {
					memset(string_tmp, 0, FOLDER_LEN);
					getnowtime(string_tmp);
					strcpy(mdfilep.folder_name, mdfilep.folder_path);
					strcat(mdfilep.folder_name, MD_FOLDER_PRE);
					strcat(mdfilep.folder_name, string_tmp);
				}

				if (folder_is_exit(mdfilep.folder_name) == false)
					create_folder(mdfilep.folder_name);

				if (mdfilep.filep[1] == NULL) {
					memset(string_tmp, 0, FOLDER_LEN);
					strcpy(string_tmp, mdfilep.folder_path);
					create_file_tree(string_tmp, MD_LOG_TYPE, 1);
				}
				if (mdfilep.pos[1] > 0)
					multi_write_file(&mdfilep, 1, "\r", 1);
				multi_write_file(&mdfilep, 1, mdfilep.folder_name, strlen(mdfilep.folder_name));

				memset(string_tmp, 0, FOLDER_LEN);
				strcpy(string_tmp, mdfilep.folder_name);
				create_file_tree(string_tmp, MD_LOG_TYPE, 2);
			}

			mdlog_count++;
			log_type_status[log_start.log_type] = true;
			if (log_start.log_type == LOG_TYPE_EE_MD1 || log_start.log_type == LOG_TYPE_EE_MD3) {
				if ((MDlog_EE_status & MDLOG_EE_EXCEPTION_MUL) == MDLOG_EE_EXCEPTION_MUL
						&& (MDlog_EE_status & MDLOG_EE_EXCEPTION) == 0) {
					MDlog_EE_status |= MDLOG_EE_EXCEPTION_1;
					MDlog_EE_status |= MDLOG_EE_EXCEPTION_3;
					MDlog_EE_status |= MDLOG_EE_EXCEPTION_MUL_FINISH;
				}
				MDlog_EE_status |= MDLOG_EE_EXCEPTION;
			}
			if (log_start.log_type >= LOG_TYPE_DB_MD1 && log_start.log_type <= LOG_TYPE_DBG_MD8)
				mdlog_db[log_start.log_type - LOG_TYPE_DB_MD1] = true;
		}
	}
}

void io_log_stop(uint32_t type)
{
	char string_tmp1[FOLDER_LEN] = {0};
	char string_tmp2[FOLDER_LEN] = {0};
	int ret;

	if (type > LOG_TYPE_MAX)
			return;

	if (type < LOG_TYPE_MD1) {
		if (log_type_status[type] == true) {
			log_type_status[type] = false;
			aplog_count--;
			if (aplog_count == 0) {

				remove_filp_file_tmp_name(&apfilep);
				if (apfilep.filep[1] != NULL) {
					ret = close_open_file(apfilep.filep[1], 0);
					if (ret)
						pr_err("%s: filp_close apfilep 0 error %d.\n", TAG, ret);
					apfilep.filep[1] = NULL;
				}
				memset(apfilep.file_name, 0, FILE_NAME_LEN);
				memset(apfilep.folder_name, 0, FILE_NAME_LEN);
				apfilep.file_size = 0;
				apfilep.pos[0] = 0;
			}
		}
	} else {
		if (log_type_status[type] == true) {
			log_type_status[type] = false;
			mdlog_count--;

			if ((MDlog_EE_status & MDLOG_EE_EXCEPTION_MUL) == MDLOG_EE_EXCEPTION_MUL) {
				/*M1 & M3 happen together*/
				if (type == LOG_TYPE_EE_MD1)
					MDlog_EE_status &= ~MDLOG_EE_EXCEPTION_1;

				if (type == LOG_TYPE_EE_MD3)
					MDlog_EE_status &= ~MDLOG_EE_EXCEPTION_3;

				if ((MDlog_EE_status & MDLOG_EE_EXCEPTION_1) == 0 &&
						(MDlog_EE_status & MDLOG_EE_EXCEPTION_3) == 0)
					MDlog_EE_status &= ~MDLOG_EE_EXCEPTION_MUL_FINISH;
			}

			if (mdlog_count == 0 && (MDlog_EE_status & MDLOG_EE_EXCEPTION_MUL_FINISH) == 0) {
				remove_filp_file_tmp_name(&mdfilep);
				if ((MDlog_EE_status & MDLOG_EE_EXCEPTION) == MDLOG_EE_EXCEPTION) {
					strcpy(string_tmp1, mdfilep.folder_name);
					strcpy(string_tmp2, string_tmp1);
					strcat(string_tmp2, "_EE_");
					strcat(string_tmp2, EE_type);
					multi_write_file(&mdfilep, 1, "_EE_", 4);
					multi_write_file(&mdfilep, 1, EE_type, strlen(EE_type));
					rename_folder(string_tmp1, string_tmp2);
					MDlog_EE_status &= ~MDLOG_EE_EXCEPTION;
					MDlog_EE_status &= ~MDLOG_EE_FILE;
				}

				if (mdfilep.filep[2] != NULL) {
					ret = close_open_file(mdfilep.filep[2], 0);
					if (ret)
						pr_err("%s: filp_close mdfilep 2 error %d.\n", TAG, ret);
					mdfilep.filep[2] = NULL;
					mdfilep.pos[2] = 0;
				}
				if (mdfilep.filep[1] != NULL) {
					ret = close_open_file(mdfilep.filep[1], 0);
					if (ret)
						pr_err("%s: filp_close mdfilep 2 error %d.\n", TAG, ret);
					mdfilep.filep[1] = NULL;
				}

				memset(mdfilep.file_name, 0, FILE_NAME_LEN);
				memset(mdfilep.folder_name, 0, FILE_NAME_LEN);
				mdfilep.file_size = 0;
				mdfilep.pos[0] = 0;
			}
		}
	}
}

void io_file_size(struct multi_log_set_size log_size)
{

	if (log_size.log_type > LOG_TYPE_MAX)
			return;

	if (log_size.size > 32 * 1024)
		log_size.size = log_size.size / 1024 / 1024;

	if (log_size.log_type < LOG_TYPE_MD1) {
		switch (log_size.file_type) {
		case ONE_FILE_SIZE:
			if (log_size.size > file_size[AP_LOG_TYPE][0])
				file_size[AP_LOG_TYPE][0] = log_size.size;
			break;

		case FOLDER_SIZE:
			if (log_size.size > file_size[AP_LOG_TYPE][1])
				file_size[AP_LOG_TYPE][1] = log_size.size;
				file_size[AP_LOG_TYPE][0] = file_size[AP_LOG_TYPE][1] / FOLDER_FILE_COUNT;
			break;

		case TOTAL_SIZE:
			if (log_size.size > file_size[AP_LOG_TYPE][2])
				file_size[AP_LOG_TYPE][2] = log_size.size;
			break;

		default:
			break;
		}
	} else {
		switch (log_size.file_type) {
		case ONE_FILE_SIZE:
			if (log_size.size > file_size[MD_LOG_TYPE][0])
				file_size[MD_LOG_TYPE][0] = log_size.size;
			break;

		case FOLDER_SIZE:
			if (log_size.size > file_size[MD_LOG_TYPE][1])
				file_size[MD_LOG_TYPE][1] = log_size.size;
			break;

		case TOTAL_SIZE:
			if (log_size.size > file_size[MD_LOG_TYPE][2])
				file_size[MD_LOG_TYPE][2] = log_size.size;
			break;

		default:
			break;
		}
	}

}

static int multi_log_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

void md3_head_translate(u32 *value)
{
	struct timeval txc;
	struct rtc_time tm;

	do_gettimeofday(&txc);
	rtc_time_to_tm(txc.tv_sec, &tm);

	value[0] = txc.tv_sec;	/*create time*/
	value[1] = 0;			/* tzoffset*/
	value[2] = (((tm.tm_year+1900)&0xffff) << 16) | (((tm.tm_mon + 1)&0xff) << 8) | (tm.tm_mday & 0xff);
	value[3] = ((tm.tm_hour & 0xff) << 16) | ((tm.tm_min & 0xff) << 8) | (tm.tm_sec & 0xff);
}

static ssize_t multi_log_write(const char  __user *buf, size_t len, int log_type)
{
	int value, size;
	char *buf_point;
	struct logger_header log_index = {0};
	char string_tmp[FOLDER_LEN];

	log_index.type = log_type;

	if (log_index.type > LOG_TYPE_MAX)
		return 0;
	if (log_type_status[log_index.type] == false)
		return 0;

	log_index.magic = LOG_MAGIC;
	log_index.total_size = len + sizeof(struct logger_header);
	log_index.tex_size = len;
	log_index.index = log_index.attr = 0;
	buf_point = (char *)&log_index;
	if (log_index.type < LOG_TYPE_MD1) {
		mutex_lock(&log_lock_ap);
		if (apfilep.filep[0] == NULL) {
			strcpy(apfilep.file_name, "/aplog_0.mmlts");
			memset(string_tmp, 0, FOLDER_LEN);
			strcpy(string_tmp, apfilep.folder_name);
			strcat(string_tmp, apfilep.file_name);

			/* add file tmp name*/
			strcat(string_tmp, TEMP_FILE);
			strcat(apfilep.file_name, TEMP_FILE);

			if (storage_is_full() == 1)
				rotation_folder(&apfilep);

			apfilep.filep[0] = open_create_file(string_tmp); /*create file*/
			if (apfilep.filep[0] == NULL) {
				mutex_unlock(&log_lock_ap);
				return 0;
			}
			apfilep.file_size = 0;
			apfilep.pos[0] = 0;
		} else if (apfilep.file_size > file_size[AP_LOG_TYPE][0] * 1024 * 1024) {

			remove_filp_file_tmp_name(&apfilep);
			apfilep.file_name[14] = 0;
			value = apfilep.file_name[7] - '0';
			value = (value + 1) % FOLDER_FILE_COUNT;
			apfilep.file_name[7] = value + '0';
			memset(string_tmp, 0, FOLDER_LEN);
			strcpy(string_tmp, apfilep.folder_name);
			strcat(string_tmp, apfilep.file_name);
			del_file(string_tmp);	/* del old file */
			/* add file tmp name*/
			strcat(string_tmp, TEMP_FILE);
			strcat(apfilep.file_name, TEMP_FILE);

			if (storage_is_full() == 1)
				rotation_folder(&apfilep);

			pr_err("%s: file size 0x%x, create new file %s.\n", TAG, (int)apfilep.file_size, string_tmp);

			apfilep.filep[0] = open_create_file(string_tmp); /*create file*/
			if (apfilep.filep[0] == NULL) {
				mutex_unlock(&log_lock_ap);
				return 0;
			}
			apfilep.file_size = 0;
			apfilep.pos[0] = 0;
		}

		multi_write_file(&apfilep, 0, buf_point, sizeof(struct logger_header));
		size = multi_write_file_user(&apfilep, 0, buf, len);
		mutex_unlock(&log_lock_ap);
	} else {
		mutex_lock(&log_lock_md);
		if ((MDlog_EE_status & MDLOG_EE_EXCEPTION) == MDLOG_EE_EXCEPTION &&
				(MDlog_EE_status & MDLOG_EE_FILE) != MDLOG_EE_FILE) {
			remove_filp_file_tmp_name(&mdfilep);
			memset(string_tmp, 0, FOLDER_LEN);
			string_tmp[0] = '/';
			getnowtime(string_tmp+1);
			strcat(string_tmp, "_EE");
			strcat(string_tmp, ".mmlts");
			strcat(string_tmp, TEMP_FILE);
			strcpy(mdfilep.file_name, string_tmp);
			md1_header[0] = true;
			md1_header[1] = true;
			memset(string_tmp, 0, FOLDER_LEN);
			strcpy(string_tmp, mdfilep.folder_name);
			strcat(string_tmp, mdfilep.file_name);
			mdfilep.filep[0] = open_create_file(string_tmp); /*create file*/
			if (mdfilep.filep[0] == NULL) {
				mutex_unlock(&log_lock_md);
				return 0;
			}
			MDlog_EE_status |= MDLOG_EE_FILE;
			if (mdfilep.pos[2] > 0)
				multi_write_file(&mdfilep, 2, "\r", 1);
			multi_write_file(&mdfilep, 2, string_tmp, strlen(string_tmp));
			mdfilep.file_size = 0;
			mdfilep.pos[0] = 0;
		} else if (mdfilep.filep[0] == NULL || (mdfilep.file_size > file_size[MD_LOG_TYPE][0] * 1024 * 1024 &&
					(MDlog_EE_status & MDLOG_EE_FILE) != MDLOG_EE_FILE)) {
			remove_filp_file_tmp_name(&mdfilep);
			memset(string_tmp, 0, FOLDER_LEN);
			string_tmp[0] = '/';
			getnowtime(string_tmp+1);
			strcat(string_tmp, ".mmlts");
			strcat(string_tmp, TEMP_FILE);
			strcpy(mdfilep.file_name, string_tmp);
			md1_header[0] = true;
			md1_header[1] = true;
			memset(string_tmp, 0, FOLDER_LEN);
			strcpy(string_tmp, mdfilep.folder_name);
			strcat(string_tmp, mdfilep.file_name);

			if (storage_is_full() == 1)
				return 0;

			/* add file tmp name*/
			mdfilep.filep[0] = open_create_file(string_tmp); /*create file*/
			if (mdfilep.filep[0] == NULL) {
				mutex_unlock(&log_lock_md);
				return 0;
			}
			if (mdfilep.pos[2] > 0)
				multi_write_file(&mdfilep, 2, "\r", 1);
			multi_write_file(&mdfilep, 2, string_tmp, strlen(string_tmp));
			mdfilep.file_size = 0;
			mdfilep.pos[0] = 0;
		}

		if (log_index.type  >= LOG_TYPE_DB_MD1 && log_index.type <= LOG_TYPE_DBG_MD8 &&
			mdlog_db[log_index.type - LOG_TYPE_DB_MD1] == true) {
				mdlog_db[log_index.type - LOG_TYPE_DB_MD1] = false;
				log_index.attr |= MDLOG_DB_NEW;
		}

		if (log_index.type == LOG_TYPE_MD1 && md1_header[0] == true) {
			md1_header[0] = false;
			log_index.attr |= MDLOG1_HEADER;
			md3_head_translate(log_index.reserve);
		} else if (log_index.type == LOG_TYPE_EE_MD1 && md1_header[1] == true) {
			md1_header[1] = false;
			log_index.attr |= MDLOG1_HEADER;
			md3_head_translate(log_index.reserve);
		}

		multi_write_file(&mdfilep, 0, buf_point, sizeof(struct logger_header));
		size = multi_write_file_user(&mdfilep, 0, buf, len);
		mutex_unlock(&log_lock_md);
	}

	return size;
}

static long multi_log_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	uint32_t flag, ret = 0;
	struct multi_log_start log_start;
	struct multi_log_set_size log_size;
	struct multi_log_write_buff log_write;
	struct multi_log_EE_type md_ee_type;
	struct multi_log_copy_file md_copy_file;

	switch (cmd) {
	case MULLOG_IO_START:
		ret = copy_from_user(&log_start, argp, sizeof(log_start));
		if (ret)	{
			pr_err("%s: copy_from_user start fail ret %d!!\n", TAG, ret);
			return -EFAULT;
		}
		pr_warn("%s:get start value: type %d, store path %s, pather %s.\n",
			TAG, log_start.log_type, log_start.path, log_start.folder);
		mutex_lock(&log_lock_io);
		io_log_start(log_start);
		mutex_unlock(&log_lock_io);
		break;

	case MULLOG_IO_STOP:
		ret = copy_from_user(&flag, argp, sizeof(flag));
		if (ret) {
			pr_err("%s: copy_from_user stop fail ret %d!!\n", TAG, ret);
			return -EFAULT;
		}
		pr_warn("%s:get stop value: type %d.\n", TAG, flag);
		mutex_lock(&log_lock_io);
		io_log_stop(flag);
		mutex_unlock(&log_lock_io);
		break;

	case MULLOG_IO_FILE_SIZE:
		ret = copy_from_user(&log_size, argp, sizeof(log_size));
		if (ret) {
			pr_err("%s: copy_from_user size fail ret %d!!\n", TAG, ret);
			return -EFAULT;
		}
		mutex_lock(&log_lock_io);
		io_file_size(log_size);
		mutex_unlock(&log_lock_io);
		break;

	case MULLOG_IO_WRITE:
		ret = copy_from_user(&log_write, argp, sizeof(log_write));
		if (ret) {
			pr_err("%s: copy_from_user ee fail ret %d!!\n", TAG, ret);
			return -EFAULT;
		}
		ret = multi_log_write((char *)log_write.buf_point,
			log_write.len, log_write.log_type);
		break;

	case MULLOG_IO_MOD_EE:
		ret = copy_from_user(&flag, argp, sizeof(flag));
		if (ret) {
			pr_err("%s: copy_from_user ee fail ret %d!!\n", TAG, ret);
			return -EFAULT;
		}
		if (flag > 0)
			MDlog_EE_status |= MDLOG_EE_EXCEPTION_MUL;
		break;

	case MULLOG_IO_COPY_FILE:
		ret = copy_from_user(&md_copy_file, argp, sizeof(md_copy_file));
		if (ret) {
			pr_err("%s: copy_from_user test fail ret %d!!\n", TAG, ret);
			return -EFAULT;
		}
		if (folder_is_exit(md_copy_file.file) == false) {
			pr_err("%s: md copy file %s is not exit!\n", TAG, md_copy_file.file);
			return -ENOENT;
		}
		return copy_md_file(md_copy_file.file);

	case MULLOG_IO_EE_TYPE:
		ret = copy_from_user(&md_ee_type, argp, sizeof(md_ee_type));
		if (ret) {
			pr_err("%s: copy_from_user test fail ret %d!!\n", TAG, ret);
			return -EFAULT;
		}
		if ((MDlog_EE_status & MDLOG_EE_EXCEPTION_MUL) == MDLOG_EE_EXCEPTION_MUL) {
			if (md_ee_type.log_type == LOG_TYPE_EE_MD1)
				strcpy(EE_type, md_ee_type.ee_type);
		} else
			strcpy(EE_type, md_ee_type.ee_type);
		return 0;

	default:
		return -ENOIOCTLCMD;
	}

	return ret;
}


static const struct file_operations multi_log_fops = {
	.owner		= THIS_MODULE,
	.open		= multi_log_open,
	.llseek		= no_llseek,
	.unlocked_ioctl = multi_log_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = multi_log_ioctl,
#endif
};

static struct miscdevice multi_log_miscdev = {
	MISC_DYNAMIC_MINOR,
	"multi_log",
	&multi_log_fops
};

static int multi_log_show_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "multi_log debug infor:\n");
	seq_puts(m, "log type enable:");
	for (i = 0; i < LOG_TYPE_MAX; i++)
		if (log_type_status[i] == true)
			seq_printf(m, " %d,", i);

	seq_puts(m, "\n");
	seq_printf(m, "store path %s.\n", store_path);
	seq_printf(m, "aplog_count: %d, mdlog_count: %d.\n", aplog_count, mdlog_count);
	seq_printf(m, "aplog file size: %d, folder path %s, folder name %s, file name %s.\n",
		(int)apfilep.file_size, apfilep.folder_path, apfilep.folder_name, apfilep.file_name);
	seq_printf(m, "mdlog file size: %d, folder path %s, folder name %s, file name %s.\n",
		(int)mdfilep.file_size, mdfilep.folder_path, mdfilep.folder_name, mdfilep.file_name);
	seq_printf(m, "apfilp file_0 0x%p, file_1 0x%p, mdfilp file_0 0x%p, file_1 0x%p, file_2 0x%p.\n",
		apfilep.filep[0], apfilep.filep[1], mdfilep.filep[0], mdfilep.filep[1], mdfilep.filep[2]);
	seq_printf(m, "EE type %s. EE status 0x%x.\n",	EE_type, MDlog_EE_status);
	seq_printf(m, "file setting sieze: %d, %d, %d. %d, %d, %d.\n",
		file_size[0][0], file_size[0][1], file_size[0][2], file_size[1][0], file_size[1][1], file_size[1][2]);
	return 0;


}

static ssize_t multi_log_show_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	return cnt;
}

static int multi_log_show_open(struct inode *inode, struct file *file)
{
	return single_open(file, multi_log_show_show, inode->i_private);
}

static const struct file_operations multi_log_show_fops = {
	.open = multi_log_show_open,
	.write = multi_log_show_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init multi_log_init(void)
{
	int ret, i;
	struct proc_dir_entry *pe;

	ret = misc_register(&multi_log_miscdev);
	if (ret < 0)
		return ret;

	mutex_init(&log_lock_ap);
	mutex_init(&log_lock_md);
	mutex_init(&log_lock_io);
	for (i = 0; i < LOG_TYPE_MAX; i++)
		log_type_status[i] = false;
	file_size[AP_LOG_TYPE][0] = MAX_SIZE_DEFAULT;
	file_size[AP_LOG_TYPE][1] = file_size[AP_LOG_TYPE][0] * FOLDER_FILE_COUNT;
	file_size[AP_LOG_TYPE][2] = file_size[AP_LOG_TYPE][1] * FOLDER_FILE_COUNT;
	file_size[MD_LOG_TYPE][0] = MAX_SIZE_DEFAULT;
	file_size[MD_LOG_TYPE][1] = file_size[MD_LOG_TYPE][0] * FOLDER_FILE_COUNT;
	file_size[MD_LOG_TYPE][2] = file_size[MD_LOG_TYPE][1] * FOLDER_FILE_COUNT;

	pe = proc_create("mtprof/multi_log", 0664, NULL, &multi_log_show_fops);
	if (!pe)
		return -ENOMEM;

	return 0;

}
static void __exit multi_log_exit(void)
{
	misc_deregister(&multi_log_miscdev);
}

module_init(multi_log_init);
module_exit(multi_log_exit);
MODULE_LICENSE("GPL");
