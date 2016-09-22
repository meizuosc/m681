#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/interrupt.h>  /* for in_interrupt() */
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <mach/board.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/kthread.h>
#include <linux/types.h>      /*typedef used*/
#include <linux/namei.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>    /*proc_create API*/
#include <linux/statfs.h>     /* kstatfs struct */
#include <linux/file.h>       /*kernel write and kernel read*/
#include <asm/uaccess.h>      /*copy_to_user copy_from_user */
#include <asm/io.h>
#include <linux/scatterlist.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/kmod.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/mount.h>
#include <linux/device.h>
#include <linux/genhd.h>
#include <linux/shmem_fs.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/slab.h>



//-------------------------0
//RSTINFO
//-------------------------2048
//ROOT_time_record
//-------------------------1M
//AE_time_record
//-------------------------2M
//KE_time_record
//-------------------------4M
//log
//-------------------------20M



#define RSTINFO_PATH  "/dev/block/platform/mtk-msdc.0/by-name/rstinfo"
#define LOG( fmt, args...)  do{ printk("rstinfo_k:"fmt, ##args); }while(0)

#define RSTINFO_OFFSET    0
#define ROOT_TIME_RECORD_OFFSET   2048
#define KE_TIME_RECORD_OFFSET  (2*1024*1024)
#define AE_TIME_RECORD_OFFSET  (1*1024*1024)
#define LOG_RECORD_OFFSET      (4*1024*1024)             


char const * exp_str[]={"Kernel Exception Panic(KE)","Kernel Exception Panic(HWT)","Kernel Exception Panic(HW_reboot)","OTHER","OTHER","OTHER"};
static char  *CNT_STR = "------\nexception reboot cnt:\t\t%d\n------\npanic reboot sub_cnt:\t\t%d\nhw reboot sub_cnt:\t\t%d\nWDT and hw reboot sub_cnt:\t%d\nthermal reboot sub_cnt:\t\t%d\nother exception reboot sub_cnt:\t%d\n------\n\n";


uint64_t  encrypt = 0;

typedef enum {
	AE_KE = 0,	
    AE_HWT,
    AE_HW_REBOOT,
	AE_NE,
	AE_JE,
	AE_OTHER,
	AE_NORMAL,
}AE_EXP_CLASS;

typedef struct{
	uint16_t KE_cnt;
	uint16_t HWT_cnt;
	uint16_t HW_REBOOT_cnt;
	uint16_t NE_cnt;
	uint16_t JE_cnt;
	uint16_t OTHER_cnt;	
	uint16_t root_cnt;
	uint16_t android_exp_cnt;
	uint16_t KE_time_record_pointer;
	uint16_t AE_time_record_pointer;
	uint16_t log_pointer;
	uint16_t ROOT_time_record_pointer;
}RSTINFO_ALL;

typedef struct{
	time_t time_t;
	AE_EXP_CLASS  cl;
}KE_RECORD;


typedef struct{
	void * str_buf;
	void * rstinfo_data;
	uint32_t str_buf_head_len;
	RSTINFO_ALL res;
	struct file *filp_form;
	struct file *filp_to;
}op_all;


int file_read(const char *pathname,off_t offset,size_t length, void *buf )
{
	struct file *filp_info;
	ssize_t read_size = -1;
	LOG("%s IN\n",__FUNCTION__);
	filp_info = filp_open(pathname, O_RDONLY, 0);
	if (IS_ERR(filp_info)) {
			LOG("open file: %s error:%ld !\n", pathname,PTR_ERR(filp_info));
			return -1;
	}
	do{
		read_size = kernel_read(filp_info,offset,buf,length );
		if(read_size<0)
			break;
		else if(read_size==length)
			break;
		else{
			buf = (char*)buf +  read_size;
			length -= read_size;
		}

	}while(read_size);
	filp_close(filp_info, 0);
	LOG("%s OUT:%ld\n",__FUNCTION__,read_size);
	return read_size<0 ? -1 : 0;
}

int file_read_fd(struct file *file,off_t offset,size_t length, void *buf )
{
	ssize_t read_size = -1;
	LOG("%s IN\n",__FUNCTION__);
	do{
		read_size = kernel_read(file,offset,buf,length );
		if(read_size<0)
			break;
		else if(read_size==length)
			break;
		else{
			buf = (char*)buf +  read_size;
			length -= read_size;
		}

	}while(read_size);
	LOG("%s OUT\n",__FUNCTION__);
	return read_size<0 ? -1 : 0;
}


int file_write(const char *pathname,off_t offset,size_t length,const void *buf )
{
	struct file *filp_info;
	ssize_t write_size = -1;
	LOG("%s IN\n",__FUNCTION__);
	filp_info = filp_open(pathname, O_RDWR, 0);
	if (IS_ERR(filp_info)) {
			LOG("open file: %s error:%ld !\n", pathname,PTR_ERR(filp_info));
			return -1;
	}
	do{
		write_size = kernel_write(filp_info,buf,length,offset );
		if(write_size<0)
			break;
		else if(write_size==length)
			break;
		else{
			buf = (char*)buf +  write_size;
			length -= write_size;
		}

	}while(write_size);
	filp_close(filp_info, 0);
	LOG("%s OUT\n",__FUNCTION__);
	return write_size<0 ? -1 : 0;	
}

int file_write_fd(struct file *file,off_t offset,size_t length,const void *buf )
{
	ssize_t write_size = -1;
	LOG("%s IN\n",__FUNCTION__);
	do{
		write_size = kernel_write(file,buf,length,offset );
		if(write_size<0)
			break;
		else if(write_size==length)
			break;
		else{
			buf = (char*)buf +  write_size;
			length -= write_size;
		}

	}while(write_size);
	LOG("%s OUT\n",__FUNCTION__);
	return write_size<0 ? -1 : 0;	
}



int get_rstinfo_all(RSTINFO_ALL * rst)
{
	int i;
	LOG("%s IN\n",__FUNCTION__);
	if(-1==file_read(RSTINFO_PATH,RSTINFO_OFFSET,sizeof(RSTINFO_ALL), (void *)rst)){
		LOG("%s file_read error!\n",__FUNCTION__);
		return -1;
		}
		
	for(i=0;i<sizeof(RSTINFO_ALL)/sizeof(uint16_t);i++){
		LOG("get_rstinfo_all:%d %d\n",i,((uint16_t *)rst)[i]);	
	}
	LOG("%s OUT\n",__FUNCTION__);
	return 0;
}

int is_encrypt(void)
{
	LOG("%s:%lld  %lld",__FUNCTION__,encrypt,get_jiffies_64());
	if(!encrypt)
		return 1;
	if((get_jiffies_64()-encrypt)/HZ>100)
		return 1;
	return 0;
}

int rstinfo_partition_erase(void)
{
	uint32_t i;
	char *buf;
	struct file *filp_info;
	LOG("%s IN\n",__FUNCTION__);
	buf = kmalloc(2048,GFP_KERNEL);
	if(!buf)
		return -1;
	memset(buf,0,2048);
	filp_info = filp_open(RSTINFO_PATH, O_RDWR, 0);
	if (IS_ERR(filp_info)) {
			LOG("open file: %s error:%ld !\n", RSTINFO_PATH,PTR_ERR(filp_info));
			return -1;
	}
	for(i=0;i<10240;i++){
		file_write_fd(filp_info,i*2048,2048,buf);		
	}
	kfree(buf);
	filp_close(filp_info, 0);
	LOG("%s OUT\n",__FUNCTION__);
	return 0;
	
}


static int dev_mkdir(const char *name, umode_t mode)
{
	struct dentry *dentry;
	struct path path;
	int err;

	dentry = kern_path_create(AT_FDCWD, name, &path, LOOKUP_DIRECTORY);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	err = vfs_mkdir(path.dentry->d_inode, dentry, mode);
	//if (!err)
		/* mark as kernel-created inode */
		//dentry->d_inode->i_private = &thread;
	done_path_create(&path, dentry);
	return err;
}


static int rstinfo_root_open(struct inode *inode, struct file *file)
{
	op_all *op;
	LOG("%s IN\n",__FUNCTION__);
	
	op = kzalloc(sizeof(op_all), GFP_KERNEL);
	if (!op)
		return -ENOMEM;
	op->str_buf = kzalloc(4096,GFP_KERNEL);
	if (!op->str_buf)
		return -ENOMEM;
	op->rstinfo_data = kmalloc(8*1024,GFP_KERNEL);
	if (!op->rstinfo_data)
		return -ENOMEM;
	if(-1==get_rstinfo_all(&op->res)){
		LOG("%s error!",__FUNCTION__);
		return -EIO;
	}	
	if(-1==file_read(RSTINFO_PATH,ROOT_TIME_RECORD_OFFSET,8*1024,op->rstinfo_data)){
		LOG("%s error!",__FUNCTION__);
		return -EIO;
	}
	op->str_buf_head_len = sprintf(op->str_buf,"\n-----\nandroid root cnt: %d\n------\n",op->res.root_cnt);	
	file->private_data = op;
	LOG("%s OUT\n",__FUNCTION__);
	return 0;

}

static ssize_t rstinfo_root_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
	op_all *op = file->private_data;
	time_t *ae_time = op->rstinfo_data;
	uint32_t  buf_len = op->str_buf_head_len;
	struct rtc_time tm;
	loff_t pos = *ppos;
	size_t ret;
	uint32_t i;
	LOG("%s IN\n",__FUNCTION__);
	if(is_encrypt()) return  0;
	if (pos < 0)
		return -EINVAL;
	if (nbytes > 2048)
		nbytes = 2048;
	if (!nbytes)
		return 0;
	for(i=0;i<1024;i++){
			if(ae_time[i]>0){
				rtc_time_to_tm((unsigned long)(ae_time[i]), &tm);				
				buf_len += sprintf(op->str_buf+buf_len,"\nAndroid ROOT time = (UTC Time: %d-%d-%d %d-%d-%d)\n",
											tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				if(buf_len > (pos+nbytes))
					break;
			}
		}
	LOG("%lld %ld %d\n",pos,nbytes,buf_len);
	if(pos>=buf_len)
		return 0;
	else if(nbytes > buf_len-pos)
		nbytes = buf_len - pos;
	ret = copy_to_user(buf, (char*)(op->str_buf)+pos, nbytes);
	if (ret == nbytes)
		return -EFAULT;	
	nbytes -= ret;
	*ppos = pos + nbytes;	

	return nbytes;
}


static ssize_t rstinfo_root_write(struct file * file, const char __user * userbuf, size_t bytes, loff_t * off)
{
	char str_buf[11];
	op_all *op = file->private_data;
	struct timespec ts;
	LOG("%s IN\n",__FUNCTION__);
//	if(is_encrypt()) return  bytes;
	if(bytes>10) 
		return -EINVAL;
	if (copy_from_user(str_buf, userbuf, bytes))
		return -EFAULT;
	str_buf[4] = '\0';
	getnstimeofday(&ts);
	if(!strcmp(str_buf,"root")){
			LOG("root\n");
			if(op->res.ROOT_time_record_pointer<1024){
				file_write(RSTINFO_PATH,ROOT_TIME_RECORD_OFFSET+op->res.ROOT_time_record_pointer*8,8,(void *)(&ts.tv_sec));
				if(op->res.ROOT_time_record_pointer==1023)
				op->res.ROOT_time_record_pointer = 0;
				else
				op->res.ROOT_time_record_pointer++;
				op->res.root_cnt++;
				file_write(RSTINFO_PATH,0,sizeof(RSTINFO_ALL),(void *)(&op->res));
			}
	}
	LOG("%s OUT\n",__FUNCTION__);	
	return bytes;
}


int rstinfo_root_release(struct inode *inode, struct file *file)
{
	op_all *t = file->private_data;
	kfree(t->str_buf);
	kfree(t->rstinfo_data);
	kfree(t);
	return 0;
}




static int rstinfo_android_open(struct inode *inode, struct file *file)
{
	op_all *op;
	LOG("%s IN\n",__FUNCTION__);
	
	op = kzalloc(sizeof(op_all), GFP_KERNEL);
	if (!op)
		return -ENOMEM;
	op->str_buf = kzalloc(4096,GFP_KERNEL);
	if (!op->str_buf)
		return -ENOMEM;
	op->rstinfo_data = kmalloc(8*1024,GFP_KERNEL);
	if (!op->rstinfo_data)
		return -ENOMEM;
	if(-1==get_rstinfo_all(&op->res)){
		LOG("%s error!",__FUNCTION__);
		return -EIO;
	}	
	if(-1==file_read(RSTINFO_PATH,AE_TIME_RECORD_OFFSET,8*1024,op->rstinfo_data)){
		LOG("%s error!",__FUNCTION__);
		return -EIO;
	}
	op->str_buf_head_len = sprintf(op->str_buf,"\n-----\nandroid reboot cnt: %d\n------\n",op->res.android_exp_cnt);	
	file->private_data = op;
	LOG("%s OUT\n",__FUNCTION__);
	return 0;

}

static ssize_t rstinfo_android_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
	op_all *op = file->private_data;
	time_t *ae_time = op->rstinfo_data;
	uint32_t  buf_len = op->str_buf_head_len;
	struct rtc_time tm;
	loff_t pos = *ppos;
	size_t ret;
	uint32_t i;
	LOG("%s IN\n",__FUNCTION__);
	if(is_encrypt()) return  0;
	if (pos < 0)
		return -EINVAL;
	if (nbytes > 2048)
		nbytes = 2048;
	if (!nbytes)
		return 0;
	for(i=0;i<1024;i++){
			if(ae_time[i]>0){
				rtc_time_to_tm((unsigned long)(ae_time[i]&0x0000ffffffffffff), &tm);				
				buf_len += sprintf(op->str_buf+buf_len,"\nAndroid reboot type%d time = (UTC Time: %d-%d-%d %d-%d-%d)\n",
											(int)(ae_time[i]>>48),tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				if(buf_len > (pos+nbytes))
					break;
			}
		}
	LOG("%lld %ld %d\n",pos,nbytes,buf_len);
	if(pos>=buf_len)
		return 0;
	else if(nbytes > buf_len-pos)
		nbytes = buf_len - pos;
	ret = copy_to_user(buf, (char*)(op->str_buf)+pos, nbytes);
	if (ret == nbytes)
		return -EFAULT;	
	nbytes -= ret;
	*ppos = pos + nbytes;	
	LOG("%s OUT\n",__FUNCTION__);
	return nbytes;
}


static int isspace(int x)
{
 if(x==' '||x=='\t'||x=='\n'||x=='\f'||x=='\b'||x=='\r')
  return 1;
 else  
  return 0;
}
static int isdigit(int x)
{
 if(x<='9'&&x>='0')         
  return 1;
 else 
  return 0;
}


static int atoi(const char *nptr)
{
        int c;              /* current char */
        int total;         /* current total */
        int sign;           /* if '-', then negative, otherwise positive */
        /* skip whitespace */
        while ( isspace((int)(unsigned char)*nptr) )
            ++nptr;
        c = (int)(unsigned char)*nptr++;
        sign = c;           /* save sign indication */
        if (c == '-' || c == '+')
            c = (int)(unsigned char)*nptr++;    /* skip sign */
        total = 0;
        while (isdigit(c)) {
            total = 10 * total + (c - '0');     /* accumulate digit */
            c = (int)(unsigned char)*nptr++;    /* get next char */
        }
        if (sign == '-')
            return -total;
        else
            return total;   /* return result, negated if necessary */
} 



static ssize_t rstinfo_android_write(struct file * file, const char __user * userbuf, size_t bytes, loff_t * off)
{
	char str_buf[15]={0};
	op_all *op = file->private_data;
	struct timespec ts;
	uint64_t type;

	LOG("%s IN\n",__FUNCTION__);
//	if(is_encrypt()) return  bytes;
	if(bytes>14) 
		return -EINVAL;
	if (copy_from_user(str_buf, userbuf, bytes))
		return -EFAULT;
	str_buf[6] = '\0';
	str_buf[10] = '\0';
	getnstimeofday(&ts);
	if(!strcmp(str_buf,"reboot")){
			LOG("AE reboot\n");
			type = (uint64_t)atoi(&str_buf[7])<<48;
			ts.tv_sec |= type;
			if(op->res.AE_time_record_pointer<1024){
				file_write(RSTINFO_PATH,AE_TIME_RECORD_OFFSET+op->res.AE_time_record_pointer*8,8,(void *)(&ts.tv_sec));
				if(op->res.AE_time_record_pointer==1023)
				op->res.AE_time_record_pointer = 0;
				else
				op->res.AE_time_record_pointer++;
				op->res.android_exp_cnt++;
				file_write(RSTINFO_PATH,0,sizeof(RSTINFO_ALL),(void *)(&op->res));
			}
	}
	LOG("%s OUT\n",__FUNCTION__);	
	return bytes;
}


int rstinfo_android_release(struct inode *inode, struct file *file)
{
	op_all *t = file->private_data;
	kfree(t->str_buf);
	kfree(t->rstinfo_data);
	kfree(t);
	return 0;
}



static int rstinfo_cnt_log_open(struct inode *inode, struct file *file)
{
	op_all *op;
	uint16_t exp_sum,exp_other;
	LOG("%s IN\n",__FUNCTION__);
	
	op = kzalloc(sizeof(op_all), GFP_KERNEL);
	if (!op)
		return -ENOMEM;
	op->str_buf = kzalloc(4096,GFP_KERNEL);
	if (!op->str_buf)
		return -ENOMEM;
	op->rstinfo_data = kmalloc(16*1024,GFP_KERNEL);
	if (!op->rstinfo_data)
		return -ENOMEM;
	if(-1==get_rstinfo_all(&op->res)){
		LOG("%s error!",__FUNCTION__);
		return -EIO;
	}	
	if(-1==file_read(RSTINFO_PATH,KE_TIME_RECORD_OFFSET,16*1024,op->rstinfo_data)){
		LOG("%s error!",__FUNCTION__);
		return -EIO;
	}
	exp_sum = op->res.OTHER_cnt+op->res.KE_cnt+op->res.JE_cnt+op->res.HW_REBOOT_cnt+op->res.HWT_cnt+op->res.NE_cnt;
	exp_other = op->res.OTHER_cnt+op->res.JE_cnt+op->res.NE_cnt;
	op->str_buf_head_len = sprintf(op->str_buf,CNT_STR,exp_sum,op->res.KE_cnt,op->res.HW_REBOOT_cnt,op->res.HWT_cnt,0,exp_other);
	
	dev_mkdir("/sdcard/kelog", 0777);
	op->filp_to = filp_open("/sdcard/kelog/rst_log.txt", O_RDWR | O_CREAT | O_TRUNC, 660);
	if (IS_ERR(op->filp_to)) {
			LOG("open /sdcard/rst_log.txt error [%ld]!\n", PTR_ERR(op->filp_to));
			return -EIO;
		}
	file->private_data = op;
	LOG("%s OUT\n",__FUNCTION__);
	return 0;

}

static ssize_t rstinfo_cnt_log_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
	op_all *op = file->private_data;
	KE_RECORD *ke_time = op->rstinfo_data;
	uint32_t  buf_len = op->str_buf_head_len;
	struct rtc_time tm;
	loff_t pos = *ppos;
	size_t ret;
	uint32_t i;

	LOG("%s IN\n",__FUNCTION__);
	if(is_encrypt()) return  0;
	if (pos < 0)
		return -EINVAL;
	if (nbytes > 2048)
		nbytes = 2048;
	if (!nbytes)
		return 0;
	for(i=0;i<1024;i++){
			if(ke_time[i].time_t>0){
				rtc_time_to_tm((unsigned long)(ke_time[i].time_t), &tm);				
				buf_len += sprintf(op->str_buf+buf_len,"\n valid=1, exception(%s), time=(local time :%d-%d-%d %d:%d:%d) \n",
							exp_str[ke_time[i].cl],tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				if(buf_len > (pos+nbytes))
					break;
			}
		}
	buf_len += sprintf(op->str_buf+buf_len,"\n\n\nrst log create succeed! please get log in path: /sdcard/kelog/rst_log.txt\n\n");
	LOG("%lld %ld %d\n",pos,nbytes,buf_len);
	if(pos>=buf_len)
		return 0;
	else if(nbytes > buf_len-pos)
		nbytes = buf_len - pos;
	ret = copy_to_user(buf, (char*)(op->str_buf)+pos, nbytes);
	if (ret == nbytes)
		return -EFAULT;	
	nbytes -= ret;
	*ppos = pos + nbytes;	
	if(-1==file_write_fd(op->filp_to,pos,nbytes, op->str_buf+pos)){
		LOG("%s error!",__FUNCTION__);
		return -EIO;
	}
	LOG("%s OUT\n",__FUNCTION__);
	return nbytes;
}


int rstinfo_cnt_log_release(struct inode *inode, struct file *file)
{
	op_all *t = file->private_data;
	filp_close(t->filp_to, 0);
	kfree(t->str_buf);
	kfree(t->rstinfo_data);
	kfree(t);
	return 0;
}


static int rstinfo_rst_log_open(struct inode *inode, struct file *file)
{
	op_all *op;
	LOG("%s IN\n",__FUNCTION__);
	
	op = kzalloc(sizeof(op_all), GFP_KERNEL);
	if (!op)
		return -ENOMEM;
	op->str_buf = kzalloc(2048,GFP_KERNEL);
	if (!op->str_buf)
		return -ENOMEM;
	if(-1==get_rstinfo_all(&op->res)){
		LOG("%s error!",__FUNCTION__);
		return -EIO;
	}	
	op->filp_form= filp_open(RSTINFO_PATH, O_RDONLY,0);
	if (IS_ERR(op->filp_to)) {
			LOG("open  error [%ld]!\n", PTR_ERR(op->filp_to));
			return -EIO;
		}
	file->private_data = op;
	dev_mkdir("/sdcard/kelog", 0777);
	LOG("%s OUT\n",__FUNCTION__);
	return 0;

}

static ssize_t rstinfo_rst_log_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
	op_all *op = file->private_data;
	char *str_buff = op->str_buf;
	uint32_t log_file_size,len;
	char *file_name = kmalloc(100,GFP_KERNEL);
	int i,j,k=0;

	LOG("%s IN\n",__FUNCTION__);
	if(is_encrypt()) return  0;
	for(i=0;i<16;i++){
		file_read_fd(op->filp_form,LOG_RECORD_OFFSET + i*1024*1024,sizeof(log_file_size),(void *)(&log_file_size));
		if(log_file_size>0){
			sprintf(file_name,"/sdcard/kelog/rst_ke_%02d.txt",k++);
			op->filp_to= filp_open(file_name, O_RDWR | O_CREAT | O_TRUNC, 666);
			if (IS_ERR(op->filp_to)) {
				LOG("rstinfo: open rstinfo error [%ld]!\n", PTR_ERR(op->filp_to));
				return -EIO;
			}
			for(j=0;j<log_file_size/2048;j++){
				file_read_fd(op->filp_form,LOG_RECORD_OFFSET+i*1024*1024+j*2048+100,2048,str_buff);
				file_write_fd(op->filp_to,j*2048,2048,str_buff);
			}
			file_read_fd(op->filp_form,LOG_RECORD_OFFSET+i*1024*1024+j*2048+100,log_file_size%2048,str_buff);
			file_write_fd(op->filp_to,j*2048,log_file_size%2048,str_buff);
			filp_close(op->filp_to, 0);
			
		}
	}
	kfree(file_name);
	len = sprintf(str_buff,"\n\n%d ke log create succeed! please get log in path: /sdcard/kelog/ \n\n",k);
	LOG("%s OUT\n",__FUNCTION__);
	return  simple_read_from_buffer(buf, nbytes, ppos,str_buff, len);
	
}


int rstinfo_rst_log_release(struct inode *inode, struct file *file)
{
	op_all *t = file->private_data;
	filp_close(t->filp_form, 0);
	kfree(t->str_buf);
	kfree(t);
	return 0;
}


static int rstinfo_cnt_open(struct inode *inode, struct file *file)
{
	char * buf;
	LOG("%s IN\n",__FUNCTION__);
	buf = kzalloc(1024, GFP_KERNEL);
	if (!buf)
	return -ENOMEM;
	file->private_data = buf;
	LOG("%s OUT\n",__FUNCTION__);
	return 0;
}


static ssize_t rstinfo_cnt_write(struct file * file, const char __user * userbuf, size_t bytes, loff_t * off)
{
	char str_buf[11];
	char str_time[10];
	struct timespec ts;
	struct rtc_time tm;
	LOG("%s IN\n",__FUNCTION__);
	if(bytes>10) 
		return -EINVAL;
	if (copy_from_user(str_buf, userbuf, bytes))
		return -EFAULT;
	str_buf[bytes] = '\0';
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	sprintf(str_time,"%d\n",tm.tm_min+tm.tm_sec/30);
	LOG("buf:%s time: %s %d \n",str_buf,str_time,tm.tm_min+tm.tm_sec/30);
	if(!strcmp(str_buf,str_time))
		encrypt=get_jiffies_64();
	if(!is_encrypt()){
		if(!strcmp(str_buf,"erase\n"))
			rstinfo_partition_erase();
		if(!strcmp(str_buf,"incrypt\n"))
			encrypt = 0;		
	}

	LOG("%s OUT\n",__FUNCTION__);
	return bytes;
}

static ssize_t rstinfo_cnt_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
	char * str_buf = file->private_data;
	size_t str_len=0;
	uint16_t exp_sum,exp_other;
	RSTINFO_ALL res;	
	LOG("%s IN\n",__FUNCTION__);
	if(is_encrypt()) return 0;
	if(-1==get_rstinfo_all(&res)){
		LOG("%s error!",__FUNCTION__);
		return -EIO;
	}
	exp_sum = res.OTHER_cnt+res.KE_cnt+res.JE_cnt+res.HW_REBOOT_cnt+res.HWT_cnt+res.NE_cnt;
	exp_other = res.OTHER_cnt+res.JE_cnt+res.NE_cnt;
	str_len = sprintf(str_buf,CNT_STR,exp_sum,res.KE_cnt,res.HW_REBOOT_cnt,res.HWT_cnt,0,exp_other);
	LOG("%s OUT\n",__FUNCTION__);
	return simple_read_from_buffer(buf, nbytes, ppos, str_buf, str_len);
	

}

int rstinfo_cnt_release(struct inode *inode, struct file *file)
{
	
	kfree(file->private_data);
	return 0;
}





static ssize_t rstinfo_test_write(struct file * file, const char __user * userbuf, size_t bytes, loff_t * off)
{
	char buf[10];
	if(is_encrypt()) return  bytes;
	if(copy_from_user(buf, userbuf,bytes))
		return -EFAULT;		
	LOG("%s %c",__FUNCTION__,buf[0]);
	if(buf[0]=='p'){
		*((int *) 0) = 123; 
	}else if(buf[0]=='w'){
		preempt_disable();
		while(1);
	}
	return bytes;
}

static ssize_t rstinfo_test_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
		
		return 0;
	

}

const struct file_operations rstinfo_cnt_log_fops = {
	.open    =  rstinfo_cnt_log_open,
	.read    =	rstinfo_cnt_log_read,
	.release =  rstinfo_cnt_log_release,
};

const struct file_operations rstinfo_android_fops = {
	.open    =  rstinfo_android_open,
	.read    =	rstinfo_android_read,
	.write   =  rstinfo_android_write,
	.release =  rstinfo_android_release,
};


const struct file_operations rstinfo_cnt_fops = {
	.open    =  rstinfo_cnt_open,
	.read    =	rstinfo_cnt_read,
	.write   =  rstinfo_cnt_write,
	.release =  rstinfo_cnt_release,
};


const struct file_operations rstinfo_rst_log_fops = {
	.open    =  rstinfo_rst_log_open,
	.read    =	rstinfo_rst_log_read,
	.release =  rstinfo_rst_log_release,
};


const struct file_operations rstinfo_root_fops = {
	.open    =  rstinfo_root_open,
	.read    =	rstinfo_root_read,
	.write   =  rstinfo_root_write,
	.release =  rstinfo_root_release,
};


const struct file_operations rstinfo_test_fops = {
	.write   =  rstinfo_test_write,	
	.read	 =  rstinfo_test_read,
};






static int __init rstinfo_init(void)
{

	LOG("%s\n",__FUNCTION__);
	

	if(!debugfs_create_file("rstinfo_cnt", 0666, NULL, NULL, &rstinfo_cnt_fops))
		goto err;
	if(!debugfs_create_file("rstinfo_cnt_log", 0444, NULL, NULL, &rstinfo_cnt_log_fops))
		goto err;	
	if(!debugfs_create_file("rstinfo_rst_log", 0444, NULL, NULL, &rstinfo_rst_log_fops))
		goto err;	
	if(!debugfs_create_file("rstinfo_android_cnt", 0666, NULL, NULL, &rstinfo_android_fops))
		goto err;	
	if(!debugfs_create_file("rootinfo_cnt",0666, NULL, NULL, &rstinfo_root_fops))
		goto err;
	if(!debugfs_create_file("rstinfo_test",0666, NULL, NULL, &rstinfo_test_fops))
		goto err;
	LOG("%s OK\n",__FUNCTION__);
	return 0;
err:
	return -ENOENT;
}



late_initcall(rstinfo_init);
//module_exit(rstinfo__exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("heyifan@wingtech.com");
MODULE_DESCRIPTION("rstinfo driver");


