#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>

#define PATH_PRODUCT_INFO "/dev/block/platform/mtk-msdc.0/by-name/proinfo"
#define LEN_PRODUCT_INFO  1024
#define COVER_COLOR       342    // 238+104

//extern char *saved_command_line;
int tp_type_flag = 0;
static int tp_type_proc_show(struct seq_file *m, void *v)
{
/*
	char *arg_start = NULL;
	char tp_type[4] = {0};

	arg_start = strstr(saved_command_line, "meizu.tptype=");
	if(NULL != arg_start) {
		arg_start += 13;  // skip "color_type="
		strncpy(tp_type, arg_start, 1); //like "M81A", 4 char.
		seq_printf(m, "%s", tp_type);
	} else {
		seq_printf(m, "B");
	}
*/
	//Modify by huangzifan 2015.11.17
	if( tp_type_flag == 0 ){
		seq_printf(m, "B");
	}else{		
		seq_printf(m, "W");
	}
	return 0;
}
/*
static int tp_type_show(void)
{
	char *arg_start = NULL;
	char tp_type[4] = {0};

	arg_start = strstr(saved_command_line, "meizu.tptype=");
	if(NULL != arg_start) {
		arg_start += 13;  // skip "color_type="
		strncpy(tp_type, arg_start, 1); //like "M81A", 4 char
		printk("huangzifan tp_type= %s\n",tp_type);
		if( tp_type[0] == 'W'){
			tp_type_flag = 1;//W			
			}
		else 
			tp_type_flag = 0;//B
	}
	return 0;
}
*/

static int tp_type_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, tp_type_proc_show, NULL);
}

static const struct file_operations tp_type_proc_fops = {
	.open		= tp_type_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_machine_color_init(void)
{
	//proc_mkdir("lk_info", NULL);
	proc_create("lk_info/tptype", 0444, NULL, &tp_type_proc_fops);
	//	tp_type_show();
	return 0;
}
module_init(proc_machine_color_init);
