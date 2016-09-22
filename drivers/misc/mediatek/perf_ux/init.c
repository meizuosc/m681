
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <linux/perf_ux.h>



#define PERF_UX_PROC "perf_ux"

static int _mode;

int get_perf_ux_mode(void)
{
	return _mode;
}

int set_perf_ux_mode(int m)
{
	if (m >= UX_UNDEF)
		return -EINVAL;
	_mode = m;
	return 0;
}

/*
 *
 */
static int perf_ux_mode_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "%d\n", _mode);
	return 0;
}

static int perf_ux_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, perf_ux_mode_show, inode->i_private);
}

static ssize_t perf_ux_mode_write(struct file *flip, const char *ubuf,
				  size_t cnt, loff_t *data)
{
	char buf[32];
	unsigned long val;
	int ret, i = 0;

	if (cnt > 31)
		cnt = 31;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = '\0';
	ret = kstrtoul((const char *)(&buf[i]), 10, &val);
	if (ret) {
		pr_debug("%s: invalid mode %s\n", __func__, buf);
		return -EFAULT;
	}

	if (val >= UX_UNDEF) {
		pr_debug("%s: request undefined mode:%lu\n", __func__, val);
		return -EINVAL;
	}

	_mode = (int)val;
	return cnt;
}

static const struct file_operations perf_ux_mode_fops = {
	.open    = perf_ux_mode_open,
	.read    = seq_read,
	.write   = perf_ux_mode_write,
	.llseek  = seq_lseek,
	.release = single_release
};

static int __init perf_ux_init(void)
{
	struct proc_dir_entry *ent, *p_ent;

	p_ent = proc_mkdir(PERF_UX_PROC, NULL);
	if (!p_ent)
		return -ENOENT;

	ent = proc_create("mode",
			  (S_IRUGO | S_IWUSR | S_IWGRP),
			  p_ent, &perf_ux_mode_fops);
	if (!ent)
		return -ENOENT;

	return 0;
}
device_initcall(perf_ux_init);

MODULE_LICENSE("GPL v2");
