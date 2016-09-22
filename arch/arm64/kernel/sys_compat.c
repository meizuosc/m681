/*
 * Based on arch/arm/kernel/sys_arm.c
 *
 * Copyright (C) People who wrote linux/arch/i386/kernel/sys_i386.c
 * Copyright (C) 1995, 1996 Russell King.
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/compat.h>
#include <linux/personality.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

#include <asm/cacheflush.h>
#include <asm/unistd.h>

#include <linux/errno.h>      /* error define */
#include <linux/types.h>      /*typedef used*/
#include <linux/fs.h>         /*flip_open API*/
#include <linux/proc_fs.h>    /*proc_create API*/
#include <linux/statfs.h>     /* kstatfs struct */
#include <linux/file.h>       /*kernel write and kernel read*/
#include <asm/uaccess.h>      /*copy_to_user copy_from_user */
#define PROINFO_PARAM_PATH      "/dev/block/platform/mtk-msdc.0/by-name/proinfo"
#define PROINFO_PARAM_OFFSET	0x300000//0x100000 	/*3MB*//*1M->3M  20160105*/
#define PROINFO_PARAM_SIZE      4096//1024
enum { 
    SYSTEM_READ,
    SYSTEM_WRITE
};
static int flag = 1;//0;xiehaojian 20151231 前期把flag打开便于调试
//char *global_buf = NULL;
int emmc_partition_read(char *name,char *tmp_buf, int param_offset,int size)
{
	int result = 0;
	int ret = 0;
	loff_t pos = 0;
	struct file *read_fp;
	//char read_tmp_buf[PROINFO_PARAM_SIZE] = {0};
	//char *tmp_buf;
	
	if(flag)
		printk("shear, %d, %s\n", __LINE__, __FUNCTION__);
	
	//tmp_buf = kzalloc(size, GFP_KERNEL);
	//if(!tmp_buf)
	//	return -ENOMEM;
	
	read_fp = filp_open(name, O_RDWR, 0);
	if (IS_ERR(read_fp)) {
		result = PTR_ERR(read_fp);
		printk("shear, %d, %s, File open return fail, result=%d, file=%p\n", __LINE__, __FUNCTION__, result, read_fp);
		goto filp_open_fail;
	}
	
	pos += param_offset;
	ret = kernel_read(read_fp, pos, tmp_buf, size);
	if (ret < 0) {
		printk("shear, %d, %s, kernel_read  fail!\n", __LINE__, __FUNCTION__);
		result = -1;
		goto read_fail;
	}
	
	//memcpy(global_buf, tmp_buf, size);
	if(flag)
		printk("shear, %d, %s\n", __LINE__, __FUNCTION__);


read_fail:
	filp_close(read_fp, 0);
filp_open_fail:
	//kfree(tmp_buf);
	return result;
}
EXPORT_SYMBOL(emmc_partition_read);
int emmc_partition_write(char *name, int param_offset, char *in_buf, int size)
{
	int result = 0;
	int ret = 0;
	loff_t pos = 0;
	mm_segment_t old_fs;
	struct file *write_fp;
	char write_tmp_buf[PROINFO_PARAM_SIZE] = "0";
	
	if(flag)
		printk("shear, %d, %s\n", __LINE__, __FUNCTION__);
	ret = copy_from_user(write_tmp_buf, in_buf, size);
	if(ret)
	{
		printk("shear, %d, %s copy_from_user\n", __LINE__, __FUNCTION__);
		return -14;
	}	
	
	write_fp = filp_open(name, O_RDWR, 0);
	if (IS_ERR(write_fp)) {
			result = PTR_ERR(write_fp);
			printk("shear, %d, %s, File open return fail, result=%d file=%p\n", __LINE__, __FUNCTION__, result, write_fp);
			goto filp_open_fail;
	}
	
	pos += param_offset;
	ret = kernel_write(write_fp, &write_tmp_buf, size, pos);
	if (ret < 0) {
		printk("shear,%d, %s, kernel_write fail!\n", __LINE__, __FUNCTION__);
		result = -1;
		goto write_fail;
	}
	if(flag)
		printk("shear, %d, %s, write_tmp_buf=%s\n", __LINE__, __FUNCTION__, write_tmp_buf);
	
	old_fs = get_fs();
	set_fs(get_ds());
	ret = vfs_fsync(write_fp, 0);
	if (ret < 0)
		printk("shear,%d, %s, vfs_fsync fail!\n", __LINE__, __FUNCTION__);
	set_fs(old_fs);
	
write_fail:
	filp_close(write_fp, 0);
filp_open_fail:
	return result;
}
EXPORT_SYMBOL(emmc_partition_write);
/*
int emmc_partition_rw(char *name, int cmd, int param_offset, char *in_buf, int size)
{
	int err = -EINVAL; 
	switch(cmd){
		case SYSTEM_READ:
			err = emmc_partition_read(name, param_offset, in_buf, size);
			if(flag)
				printk("shear, %d, %s, read_tmp_buf=%s\n", __LINE__, __FUNCTION__, in_buf);
			break; 
		case SYSTEM_WRITE:
			err = emmc_partition_write(name, param_offset, in_buf, size);
			break; 
		default:
			err = -EINVAL; 
			goto out; 
		}
out:
	return err;
}
*/
int system_data_func(int cmd, __user char *in_buf, int size)
{
	int err = 0; 
	char *tmp_buf;
	
	if(size > 4096)
	{
		printk("shear, %d, %s, size is too big!\n", __LINE__, __FUNCTION__);
		return  -EINVAL;
	}

	tmp_buf = kzalloc(size, GFP_KERNEL);
	if(!tmp_buf){
		printk("shear, %d, %s\n", __LINE__, __FUNCTION__);
		return -ENOMEM;
	}

	if(flag)
		printk("shear, %d, %s\n", __LINE__, __FUNCTION__);

	if(cmd == SYSTEM_READ)
	{
		err = emmc_partition_read(PROINFO_PARAM_PATH, tmp_buf,PROINFO_PARAM_OFFSET, size);
		if(err)
		{
			printk("shear, %d, %s emmc_partition_read\n", __LINE__, __FUNCTION__);
			goto out;
		}		

		if(flag)
			printk("shear, %d, %s,tmp_buf=%s\n", __LINE__, __FUNCTION__, tmp_buf);
		err = copy_to_user(in_buf, tmp_buf, size);
		if(err)
		{
			printk("shear, %d, %s copy_to_user\n", __LINE__, __FUNCTION__);
			goto out;
		}
		if(flag)
			printk("shear, %d, %s, in_buf=%s tmp_buf=%s\n", __LINE__, __FUNCTION__, in_buf, tmp_buf);		
	}
	if(cmd == SYSTEM_WRITE)
	{
		err = emmc_partition_write(PROINFO_PARAM_PATH, PROINFO_PARAM_OFFSET, in_buf, size);
		if(err)
		{
			printk("shear, %d, %s emmc_partition_write\n", __LINE__, __FUNCTION__);
			goto out;
		}		
	}
	if(flag)
		printk("shear, %d, %s\n", __LINE__, __FUNCTION__);
	
out:
	kfree(tmp_buf);
	return err;
}

static inline void
do_compat_cache_op(unsigned long start, unsigned long end, int flags)
{
	struct mm_struct *mm = current->active_mm;
	struct vm_area_struct *vma;

	if (end < start || flags)
		return;

	down_read(&mm->mmap_sem);
	vma = find_vma(mm, start);
	if (vma && vma->vm_start < end) {
		if (start < vma->vm_start)
			start = vma->vm_start;
		if (end > vma->vm_end)
			end = vma->vm_end;
		up_read(&mm->mmap_sem);
		__flush_cache_user_range(start & PAGE_MASK, PAGE_ALIGN(end));
		return;
	}
	up_read(&mm->mmap_sem);
}
static long custom_arm_syscall(struct pt_regs *regs, unsigned int num);

/*
 * Handle all unrecognised system calls.
 */
long compat_arm_syscall(struct pt_regs *regs)
{
	unsigned int no = regs->regs[7];

	switch (no) {
	/*
	 * Flush a region from virtual address 'r0' to virtual address 'r1'
	 * _exclusive_.  There is no alignment requirement on either address;
	 * user space does not need to know the hardware cache layout.
	 *
	 * r2 contains flags.  It should ALWAYS be passed as ZERO until it
	 * is defined to be something else.  For now we ignore it, but may
	 * the fires of hell burn in your belly if you break this rule. ;)
	 *
	 * (at a later date, we may want to allow this call to not flush
	 * various aspects of the cache.  Passing '0' will guarantee that
	 * everything necessary gets flushed to maintain consistency in
	 * the specified region).
	 */
	case __ARM_NR_compat_cacheflush:
		do_compat_cache_op(regs->regs[0], regs->regs[1], regs->regs[2]);
		return 0;

	case __ARM_NR_compat_set_tls:
		current->thread.tp_value = regs->regs[0];

		/*
		 * Protect against register corruption from context switch.
		 * See comment in tls_thread_flush.
		 */
		barrier();
		asm ("msr tpidrro_el0, %0" : : "r" (regs->regs[0]));
		return 0;
	case __ARM_NR_do_private_slot:
		return custom_arm_syscall(regs,no);
		break;
	default:
		return -ENOSYS;
	}
}

static long custom_arm_syscall(struct pt_regs *regs, unsigned int num) 
{
	return -ENOSYS;
}

long compat_arm64_syscall(struct pt_regs *regs) 
{ 
	unsigned int no = regs->regs[8];
	return custom_arm_syscall(regs, no);
}
