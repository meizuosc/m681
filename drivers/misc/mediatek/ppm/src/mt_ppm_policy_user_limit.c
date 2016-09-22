
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "mt_ppm_internal.h"

#if defined(CONFIG_CPU_3_LEVEL_SUPPORT)
#include <linux/kobject.h>
#include <asm/uaccess.h>	/* copy_from_user */
#endif

static enum ppm_power_state ppm_userlimit_get_power_state_cb(enum ppm_power_state cur_state);
static void ppm_userlimit_update_limit_cb(enum ppm_power_state new_state);
static void ppm_userlimit_status_change_cb(bool enable);
static void ppm_userlimit_mode_change_cb(enum ppm_mode mode);

static int powermode_index = 0;
/* other members will init by ppm_main */
static struct ppm_policy_data userlimit_policy = {
	.name			= __stringify(PPM_POLICY_USER_LIMIT),
	.policy			= PPM_POLICY_USER_LIMIT,
	.priority		= PPM_POLICY_PRIO_USER_SPECIFY_BASE,
	.get_power_state_cb	= ppm_userlimit_get_power_state_cb,
	.update_limit_cb	= ppm_userlimit_update_limit_cb,
	.status_change_cb	= ppm_userlimit_status_change_cb,
	.mode_change_cb		= ppm_userlimit_mode_change_cb,
};

struct ppm_userlimit_data userlimit_data = {
	.is_freq_limited_by_user = false,
	.is_core_limited_by_user = false,
};


/* MUST in lock */
bool ppm_userlimit_is_policy_active(void)
{
	if (!userlimit_data.is_freq_limited_by_user && !userlimit_data.is_core_limited_by_user)
		return false;
	else
		return true;
}

static enum ppm_power_state ppm_userlimit_get_power_state_cb(enum ppm_power_state cur_state)
{
	if (userlimit_data.is_core_limited_by_user)
		return ppm_judge_state_by_user_limit(cur_state, userlimit_data);
	else
		return cur_state;
}

static void ppm_userlimit_update_limit_cb(enum ppm_power_state new_state)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: userlimit policy update limit for new state = %s\n",
		__func__, ppm_get_power_state_name(new_state));

	if (userlimit_data.is_freq_limited_by_user || userlimit_data.is_core_limited_by_user) {
		ppm_hica_set_default_limit_by_state(new_state, &userlimit_policy);

		for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
			userlimit_policy.req.limit[i].min_cpu_core = (userlimit_data.limit[i].min_core_num == -1)
				? userlimit_policy.req.limit[i].min_cpu_core
				: userlimit_data.limit[i].min_core_num;
			userlimit_policy.req.limit[i].max_cpu_core = (userlimit_data.limit[i].max_core_num == -1)
				? userlimit_policy.req.limit[i].max_cpu_core
				: userlimit_data.limit[i].max_core_num;
			userlimit_policy.req.limit[i].min_cpufreq_idx = (userlimit_data.limit[i].min_freq_idx == -1)
				? userlimit_policy.req.limit[i].min_cpufreq_idx
				: userlimit_data.limit[i].min_freq_idx;
			userlimit_policy.req.limit[i].max_cpufreq_idx = (userlimit_data.limit[i].max_freq_idx == -1)
				? userlimit_policy.req.limit[i].max_cpufreq_idx
				: userlimit_data.limit[i].max_freq_idx;
		}

		ppm_limit_check_for_user_limit(new_state, &userlimit_policy.req, userlimit_data);
	}

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_userlimit_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: userlimit policy status changed to %d\n", __func__, enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_userlimit_mode_change_cb(enum ppm_mode mode)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: ppm mode changed to %d\n", __func__, mode);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static int ppm_userlimit_min_cpu_core_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
		seq_printf(m, "cluster %d: min_core_num = %d, max_core_num = %d\n",
			i, userlimit_data.limit[i].min_core_num, userlimit_data.limit[i].max_core_num);
	}

	return 0;
}

static ssize_t ppm_userlimit_min_cpu_core_proc_write(struct file *file, const char __user *buffer,
					size_t count,	loff_t *pos)
{
	int id, min_core, i;
	bool is_limit = false;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	
	if (sscanf(buf, "%d %d", &id, &min_core) == 2) {
		printk("zmlin min core:[%d] id[%d]\n",min_core,id);
		if (id < 0 || id >= ppm_main_info.cluster_num) {
			ppm_err("@%s: Invalid cluster id: %d\n", __func__, id);
			goto out;
		}

		if (min_core != -1 && min_core < (int)get_cluster_min_cpu_core(id)) {
			ppm_err("@%s: Invalid input! min_core = %d\n", __func__, min_core);
			goto out;
		}

		ppm_lock(&userlimit_policy.lock);

		if (!userlimit_policy.is_enabled) {
			ppm_err("@%s: userlimit policy is not enabled!\n", __func__);
			ppm_unlock(&userlimit_policy.lock);
			goto out;
		}

		/*min core check*/
		/*if((min_core > userlimit_data.limit[id].max_core_num)&&(2 == powermode_index))
			{
				printk(" min_core>2 :%d\n",min_core);
				//min_core = 2;
			}
		/* error check */
		/*if (userlimit_data.limit[id].max_core_num != -1
			&& min_core != -1
			&& min_core > userlimit_data.limit[id].max_core_num) {
			ppm_err("@%s: min_core(%d) > max_core(%d)\n", __func__, min_core,
				userlimit_data.limit[id].max_core_num);
			ppm_unlock(&userlimit_policy.lock);
			goto out;
		}*/

		if (min_core != userlimit_data.limit[id].min_core_num) {
			userlimit_data.limit[id].min_core_num = min_core;
			ppm_info("user limit min_core_num = %d for cluster %d\n", min_core, id);
		}

		/* check is core limited or not */
		for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
			if (userlimit_data.limit[i].min_core_num != -1
				|| userlimit_data.limit[i].max_core_num != -1) {
				is_limit = true;
				break;
			}
		}
		userlimit_data.is_core_limited_by_user = is_limit;

		userlimit_policy.is_activated = ppm_userlimit_is_policy_active();
		printk("zmlin min core actived:[%d]\n",userlimit_policy.is_activated);
		ppm_unlock(&userlimit_policy.lock);
		ppm_task_wakeup();
	} else
		ppm_err("@%s: Invalid input!\n", __func__);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ppm_userlimit_max_cpu_core_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
		seq_printf(m, "cluster %d: min_core_num = %d, max_core_num = %d\n",
			i, userlimit_data.limit[i].min_core_num, userlimit_data.limit[i].max_core_num);
	}

	return 0;
}

static ssize_t ppm_userlimit_max_cpu_core_proc_write(struct file *file, const char __user *buffer,
					size_t count,	loff_t *pos)
{
	int id, max_core, i;
	bool is_limit = false;
	

	#if defined(CONFIG_CPU_3_LEVEL_SUPPORT)
	printk("zmlin max core.......\n");
	return count;
	#endif
	
	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &id, &max_core) == 2) {
		printk("zmlin max core:[%d] id:[%d] cluster_num:[%d] get_cpu_core:[%d] enabled:[%d] actived:[%d]\n",max_core,id, ppm_main_info.cluster_num,(int)get_cluster_max_cpu_core(id),userlimit_policy.is_enabled,ppm_userlimit_is_policy_active());
		if (id < 0 || id >= ppm_main_info.cluster_num) {
			ppm_err("@%s: Invalid cluster id: %d\n", __func__, id);
			goto out;
		}

		if (max_core != -1 && max_core > (int)get_cluster_max_cpu_core(id)) {
			ppm_err("@%s: Invalid input! max_core = %d\n", __func__, max_core);
			goto out;
		}

		ppm_lock(&userlimit_policy.lock);

		if (!userlimit_policy.is_enabled) {
			ppm_err("@%s: userlimit policy is not enabled!\n", __func__);
			ppm_unlock(&userlimit_policy.lock);
			goto out;
		}

		/* error check */
		if (userlimit_data.limit[id].min_core_num != -1
			&& max_core != -1
			&& max_core < userlimit_data.limit[id].min_core_num) {
			ppm_err("@%s: max_core(%d) < min_core(%d)\n", __func__, max_core,
				userlimit_data.limit[id].min_core_num);
			ppm_unlock(&userlimit_policy.lock);
			goto out;
		}

		if (max_core != userlimit_data.limit[id].max_core_num) {
			userlimit_data.limit[id].max_core_num = max_core;
			ppm_info("user limit max_core_num = %d for cluster %d\n", max_core, id);
		}

		/* check is core limited or not */
		for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
			if (userlimit_data.limit[i].min_core_num != -1
				|| userlimit_data.limit[i].max_core_num != -1) {
				is_limit = true;
				break;
			}
		}
		userlimit_data.is_core_limited_by_user = is_limit;

		userlimit_policy.is_activated = ppm_userlimit_is_policy_active();
		printk("zmlin actived:[%d]\n",userlimit_policy.is_activated);
		ppm_unlock(&userlimit_policy.lock);
		ppm_task_wakeup();
	} else
		printk("@%s: Invalid input!\n", __func__);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ppm_userlimit_min_cpu_freq_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
		seq_printf(m, "cluster %d: min_freq_idx = %d, max_freq_idx = %d\n",
			i, userlimit_data.limit[i].min_freq_idx, userlimit_data.limit[i].max_freq_idx);
	}

	return 0;
}

static ssize_t ppm_userlimit_min_cpu_freq_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *pos)
{
	int id, min_freq, idx, i;
	bool is_limit = false;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
       
	if (sscanf(buf, "%d %d", &id, &min_freq) == 2) {
		 printk("zmlin min freq:[%d] id[%d]\n",min_freq,id);
		if (id < 0 || id >= ppm_main_info.cluster_num) {
			ppm_err("@%s: Invalid cluster id: %d\n", __func__, id);
			goto out;
		}

		ppm_lock(&userlimit_policy.lock);

		if (!userlimit_policy.is_enabled) {
			ppm_err("@%s: userlimit policy is not enabled!\n", __func__);
			ppm_unlock(&userlimit_policy.lock);
			goto out;
		}

		idx = (min_freq == -1) ? -1 : ppm_main_freq_to_idx(id, min_freq, CPUFREQ_RELATION_L);

		/* error check, sync to max idx if min freq > max freq */
		if (userlimit_data.limit[id].max_freq_idx != -1
			&& idx != -1
			&& idx < userlimit_data.limit[id].max_freq_idx)
			idx = userlimit_data.limit[id].max_freq_idx;

		if (idx != userlimit_data.limit[id].min_freq_idx) {
			userlimit_data.limit[id].min_freq_idx = idx;
			ppm_info("user limit min_freq = %d KHz(idx = %d) for cluster %d\n", min_freq, idx, id);
		}

		/* check is freq limited or not */
		for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
			if (userlimit_data.limit[i].min_freq_idx != -1
				|| userlimit_data.limit[i].max_freq_idx != -1) {
				is_limit = true;
				break;
			}
		}
		userlimit_data.is_freq_limited_by_user = is_limit;

		userlimit_policy.is_activated = ppm_userlimit_is_policy_active();

		ppm_unlock(&userlimit_policy.lock);
		ppm_task_wakeup();
	} else
		ppm_err("@%s: Invalid input!\n", __func__);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ppm_userlimit_max_cpu_freq_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
		seq_printf(m, "cluster %d: min_freq_idx = %d, max_freq_idx = %d\n",
			i, userlimit_data.limit[i].min_freq_idx, userlimit_data.limit[i].max_freq_idx);
	}

	return 0;
}

static ssize_t ppm_userlimit_max_cpu_freq_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *pos)
{
	int id, max_freq, idx, i;
	bool is_limit = false;

	#if defined(CONFIG_CPU_3_LEVEL_SUPPORT)
	printk("zmlin max FREQ.......\n");
	return count;
	#endif

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
        
	if (sscanf(buf, "%d %d", &id, &max_freq) == 2) {
		printk("zmlin max freq:[%d] id[%d]\n",max_freq,id);
		if (id < 0 || id >= ppm_main_info.cluster_num) {
			ppm_err("@%s: Invalid cluster id: %d\n", __func__, id);
			goto out;
		}

		ppm_lock(&userlimit_policy.lock);

		if (!userlimit_policy.is_enabled) {
			ppm_err("@%s: userlimit policy is not enabled!\n", __func__);
			ppm_unlock(&userlimit_policy.lock);
			goto out;
		}

		idx = (max_freq == -1) ? -1 : ppm_main_freq_to_idx(id, max_freq, CPUFREQ_RELATION_H);

		/* error check, sync to max idx if max freq < min freq */
		if (userlimit_data.limit[id].min_freq_idx != -1
			&& idx != -1
			&& idx > userlimit_data.limit[id].min_freq_idx)
			userlimit_data.limit[id].min_freq_idx = idx;

		if (idx != userlimit_data.limit[id].max_freq_idx) {
			userlimit_data.limit[id].max_freq_idx = idx;
			ppm_info("user limit max_freq = %d KHz(idx = %d) for cluster %d\n", max_freq, idx, id);
		}

		/* check is freq limited or not */
		for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
			if (userlimit_data.limit[i].min_freq_idx != -1
				|| userlimit_data.limit[i].max_freq_idx != -1) {
				is_limit = true;
				break;
			}
		}
		userlimit_data.is_freq_limited_by_user = is_limit;

		userlimit_policy.is_activated = ppm_userlimit_is_policy_active();

		ppm_unlock(&userlimit_policy.lock);
		ppm_task_wakeup();
	} else
		ppm_err("@%s: Invalid input!\n", __func__);

out:
	free_page((unsigned long)buf);
	return count;
}
#if defined(CONFIG_CPU_3_LEVEL_SUPPORT)
// cpufreq level
struct cpufreq_list{
const char*freq_level;
};
struct cpucore_list{
const char*core_num;
};
static const struct cpufreq_list cpu_freqlist[2][8]={
{{"-1"},{"1144000"},{"1014000"},{"871000"},{"689000"},{"598000"},{"494000"},{"338000"},{"156000"}},
{{"-1"},{"1950000"},{"1755000"},{"1573000"},{"1196000"},{"1027000"},{"871000"},{"663000"},{"286000"}}
};
static const struct cpucore_list cpu_coremax[2][3]={
{{"-1"},{"4"},{"2"},{"4"}},
{{"-1"},{"0"},{"4"},{"4"}}
};
static char cpu_powermode[15] = {0};//add for show cpu power mode

static ssize_t power_coremax_set(const char *buf)
{
	int id, max_core, i;
	bool is_limit = false;


	if (sscanf(buf, "%d %d", &id, &max_core) == 2) {
		printk("zmlin max core:[%d] id:[%d] cluster_num:[%d] get_cpu_core:[%d] enabled:[%d] actived:[%d]\n",max_core,id, ppm_main_info.cluster_num,(int)get_cluster_max_cpu_core(id),userlimit_policy.is_enabled,ppm_userlimit_is_policy_active());
		if (id < 0 || id >= ppm_main_info.cluster_num) {
			ppm_err("@%s: Invalid cluster id: %d\n", __func__, id);
			goto out;
		}

		if (max_core != -1 && max_core > (int)get_cluster_max_cpu_core(id)) {
			ppm_err("@%s: Invalid input! max_core = %d\n", __func__, max_core);
			goto out;
		}

		ppm_lock(&userlimit_policy.lock);

		if (!userlimit_policy.is_enabled) {
			ppm_err("@%s: userlimit policy is not enabled!\n", __func__);
			ppm_unlock(&userlimit_policy.lock);
			goto out;
		}
		#if 0
		/* error check */
		if (userlimit_data.limit[id].min_core_num != -1
			&& max_core != -1
			&& max_core < userlimit_data.limit[id].min_core_num) {
			ppm_err("@%s: max_core(%d) < min_core(%d)\n", __func__, max_core,
				userlimit_data.limit[id].min_core_num);
			ppm_unlock(&userlimit_policy.lock);
			goto out;
		}
		#endif
		if (max_core != userlimit_data.limit[id].max_core_num) {
			userlimit_data.limit[id].max_core_num = max_core;
			ppm_info("user limit max_core_num = %d for cluster %d\n", max_core, id);
		}

		/* check is core limited or not */
		for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
			if (userlimit_data.limit[i].min_core_num != -1
				|| userlimit_data.limit[i].max_core_num != -1) {
				is_limit = true;
				break;
			}
		}
		userlimit_data.is_core_limited_by_user = is_limit;

		userlimit_policy.is_activated = ppm_userlimit_is_policy_active();
		printk("zmlin actived:[%d]\n",userlimit_policy.is_activated);
		ppm_unlock(&userlimit_policy.lock);
		ppm_task_wakeup();
	} else
		printk("@%s: Invalid input!\n", __func__);
	return 0;
out:
	//free_page((unsigned long)buf);
	return -1;
}

static ssize_t power_freqmax_set(const char *buf)
{
	int id, max_freq, idx, i;
	bool is_limit = false;

	if (sscanf(buf, "%d %d", &id, &max_freq) == 2) {
		printk("zmlin max freq:[%d] id[%d]\n",max_freq,id);
		if (id < 0 || id >= ppm_main_info.cluster_num) {
			ppm_err("@%s: Invalid cluster id: %d\n", __func__, id);
			goto out;
		}

		ppm_lock(&userlimit_policy.lock);

		if (!userlimit_policy.is_enabled) {
			ppm_err("@%s: userlimit policy is not enabled!\n", __func__);
			ppm_unlock(&userlimit_policy.lock);
			goto out;
		}

		idx = (max_freq == -1) ? -1 : ppm_main_freq_to_idx(id, max_freq, CPUFREQ_RELATION_H);

		/* error check, sync to max idx if max freq < min freq */
		if (userlimit_data.limit[id].min_freq_idx != -1
			&& idx != -1
			&& idx > userlimit_data.limit[id].min_freq_idx)
			userlimit_data.limit[id].min_freq_idx = idx;

		if (idx != userlimit_data.limit[id].max_freq_idx) {
			userlimit_data.limit[id].max_freq_idx = idx;
			ppm_info("user limit max_freq = %d KHz(idx = %d) for cluster %d\n", max_freq, idx, id);
		}

		/* check is freq limited or not */
		for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
			if (userlimit_data.limit[i].min_freq_idx != -1
				|| userlimit_data.limit[i].max_freq_idx != -1) {
				is_limit = true;
				break;
			}
		}
		userlimit_data.is_freq_limited_by_user = is_limit;

		userlimit_policy.is_activated = ppm_userlimit_is_policy_active();

		ppm_unlock(&userlimit_policy.lock);
		ppm_task_wakeup();
	} else
		ppm_err("@%s: Invalid input!\n", __func__);

	return 0;
out:
	//free_page((unsigned long)buf);
	return -1;
}


static ssize_t power_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buffer)
{
	return  sprintf(buffer,"%s\n", cpu_powermode);
}
static ssize_t power_mode_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buffer, size_t count)
{
	int i;
    	char buffer_coremax[2][10] = {0};
    	char buffer_freqmax[2][15] = {0};
	char buffer_id_core[20] = {0};
	char buffer_id_freq[20] = {0};
	int freq_index[2] = {0};
    	const struct cpufreq_list *cpu_f = cpu_freqlist;
	const struct cpucore_list *cpu_c = cpu_coremax;	
    	sscanf(buffer, "%s", (char *)cpu_powermode);

    	printk("zmlin power_mode_store\n");
	//init
	//for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
		sprintf(buffer_coremax[0],"%s","-1"/*cpu_c[i][0]*/);
		sprintf(buffer_freqmax[0],"%s","-1"/*cpu_f[i][0]*/);
		sprintf(buffer_coremax[1],"%s","-1"/*cpu_c[i][0]*/);
		sprintf(buffer_freqmax[1],"%s","-1"/*cpu_f[i][0]*/);
	//}
	
    	if(strcmp(cpu_powermode, "low") == 0){//cluster:0 core:4 cluster:1 core:0
		powermode_index = 1;
		freq_index[0] = 1;
		freq_index[1] = 4;
		//for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
		sprintf(buffer_coremax[0],"%s","4"/*cpu_c[i][0]*/);
		sprintf(buffer_freqmax[0],"%s","1144000"/*cpu_f[i][0]*/);
		sprintf(buffer_coremax[1],"%s","0"/*cpu_c[i][0]*/);
		sprintf(buffer_freqmax[1],"%s","1196000"/*cpu_f[i][0]*/);
		//}
		//sscanf("-1", "%s", (char *)(buffer_coremax[0]));
		//sscanf("0", "%s", (char *)(buffer_coremax[1]));
		//buffer_corenum[0][strlen("4")] = '\0';
		//buffer_corenum[1][strlen("0")] = '\0';
		//sscanf((cpu_f+5)->freq_level, "%s", (char *)&buffer_freq);
		//buffer_freq[strlen((cpu_f+4)->freq_level)] = '\0';
    	}
    	else 
    		if(strcmp(cpu_powermode, "normal") == 0){//cluster:0 core:4 cluster:1 core:2
				powermode_index = 2;
				freq_index[0] = 1;
				freq_index[1] = 1;
				//for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
				sprintf(buffer_coremax[0],"%s","2"/*cpu_c[i][0]*/);
				sprintf(buffer_freqmax[0],"%s","1144000"/*cpu_f[i][0]*/);
				sprintf(buffer_coremax[1],"%s","4"/*cpu_c[i][0]*/);
				sprintf(buffer_freqmax[1],"%s","1950000"/*cpu_f[i][0]*/);
				//}
				//sscanf("-1", "%s", (char *)(buffer_coremax[0]));
				//sscanf("2", "%s", (char *)(buffer_coremax[1]));
				//buffer_corenum[0][strlen("4")] = '\0';
				//buffer_corenum[1][strlen("2")] = '\0';
				//sscanf(cpu_f->freq_level, "%s", (char *)&buffer_freq);
				//buffer_freq[strlen(cpu_f->freq_level)] = '\0';
    			}
		else
			if(strcmp(cpu_powermode, "high") == 0){//cluster:0 core:4 cluster:1 core:4
					powermode_index = 3;
					freq_index[0] = 1;
					freq_index[1] = 1;
					//for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
					sprintf(buffer_coremax[0],"%s","4"/*cpu_c[i][0]*/);
					sprintf(buffer_freqmax[0],"%s","1144000"/*cpu_f[i][0]*/);
					sprintf(buffer_coremax[1],"%s","4"/*cpu_c[i][0]*/);
					sprintf(buffer_freqmax[1],"%s","1950000"/*cpu_f[i][0]*/);
					//}
					//sscanf("-1", "%s", (char *)(buffer_coremax[0]));
					//sscanf("-1", "%s", (char *)(buffer_coremax[1]));
					//buffer_corenum[strlen("8")] = '\0';
					//sscanf(cpu_f->freq_level, "%s", (char *)&buffer_freq);
					//buffer_freq[strlen(cpu_f->freq_level)] = '\0';
				}
			else{//cluster:0 core:4 cluster:1 core:4
				powermode_index = 3;
				freq_index[0] = 1;
				freq_index[1] = 1;
				//for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
				sprintf(buffer_coremax[0],"%s","-1"/*cpu_c[i][0]*/);
				sprintf(buffer_freqmax[0],"%s","-1"/*cpu_f[i][0]*/);
				sprintf(buffer_coremax[1],"%s","-1"/*cpu_c[i][0]*/);
				sprintf(buffer_freqmax[1],"%s","-1"/*cpu_f[i][0]*/);
				//}
				//sscanf("-1", "%s", (char *)(buffer_coremax[0]));
				//sscanf("-1", "%s", (char *)(buffer_coremax[1]));
				//buffer_corenum[strlen("8")] = '\0';
				//sscanf(cpu_f->freq_level, "%s", (char *)&buffer_freq);
				//buffer_freq[strlen(cpu_f->freq_level)] = '\0';
				}
			
   	printk("power_mode_store:%d,core0:%s,core1:%s,freq0:%s,freq1:%s\n",powermode_index,buffer_coremax[0],buffer_coremax[1],buffer_freqmax[0],buffer_freqmax[1]);

	for (i = 0; i < userlimit_policy.req.cluster_num; i++) {
		sprintf(buffer_id_core,"%d %s", i, buffer_coremax[i]);
		power_coremax_set(buffer_id_core);
		sprintf(buffer_id_freq,"%d %s", i, buffer_freqmax[i]);
		power_freqmax_set(buffer_id_freq);
	}
   return count;
}

#define power_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

power_attr(power_mode);

static struct attribute *g[] = {
	&power_mode_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};
#endif
PROC_FOPS_RW(userlimit_min_cpu_core);
PROC_FOPS_RW(userlimit_max_cpu_core);
PROC_FOPS_RW(userlimit_min_cpu_freq);
PROC_FOPS_RW(userlimit_max_cpu_freq);
static int __init ppm_userlimit_policy_init(void)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(userlimit_min_cpu_core),
		PROC_ENTRY(userlimit_max_cpu_core),
		PROC_ENTRY(userlimit_min_cpu_freq),
		PROC_ENTRY(userlimit_max_cpu_freq),
	};

	FUNC_ENTER(FUNC_LV_POLICY);
	
#if defined(CONFIG_CPU_3_LEVEL_SUPPORT)
/*kernel/power/power_mode*/
    ret = sysfs_create_group(power_kobj, &attr_group);
	if (ret) {
		pr_err("android_power_init: sysfs_create_group failed\n");
		return ret;
	}
    //end
#endif

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, policy_dir, entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/policy/%s failed\n", __func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	userlimit_data.limit = kzalloc(ppm_main_info.cluster_num * sizeof(*userlimit_data.limit), GFP_KERNEL);
	if (!userlimit_data.limit) {
		ppm_err("@%s: fail to allocate memory for ut_data!\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	/* init userlimit_data */
	for (i = 0; i < ppm_main_info.cluster_num; i++) {
		userlimit_data.limit[i].min_freq_idx = -1;
		userlimit_data.limit[i].max_freq_idx = -1;
		userlimit_data.limit[i].min_core_num = -1;
		userlimit_data.limit[i].max_core_num = -1;
		//debug
		/*userlimit_data.limit[i].min_freq_idx = -1;
		userlimit_data.limit[i].min_core_num = -1;
		if(0 == i){
				userlimit_data.limit[i].max_freq_idx = 0;
				userlimit_data.limit[i].max_core_num = 2;
			}
		if(1 == i){
				userlimit_data.limit[i].max_freq_idx = 0;
				userlimit_data.limit[i].max_core_num = 4;
		}*/
	}

	if (ppm_main_register_policy(&userlimit_policy)) {
		ppm_err("@%s: userlimit policy register failed\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ppm_info("@%s: register %s done!\n", __func__, userlimit_policy.name);

out:
	FUNC_EXIT(FUNC_LV_POLICY);

	return ret;
}

static void __exit ppm_userlimit_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	kfree(userlimit_data.limit);

	ppm_main_unregister_policy(&userlimit_policy);

	FUNC_EXIT(FUNC_LV_POLICY);
}

module_init(ppm_userlimit_policy_init);
module_exit(ppm_userlimit_policy_exit);

