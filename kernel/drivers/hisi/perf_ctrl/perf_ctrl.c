#define pr_fmt(fmt) "perf_ctrl: " fmt

#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/hisi/hisi_drmdriver.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <global_ddr_map.h>
#include <linux/hisi/hisi_drg.h>
#include <securec.h>
#include <linux/hisi_rtg.h>
#include <linux/hisi/perf_ctrl.h>
#include <linux/hisi/render_rt.h>
#include <linux/sched/cputime.h>

#ifdef CONFIG_FRAME_RTG
#include <linux/sched/frame.h>
#endif

extern int get_ipa_status(struct ipa_stat *status);

#ifdef CONFIG_HISI_DDR_PERF_CTRL


#define PERF_CTRL_DDR_FLUX_MAX_CNT (0xFFFFFFFFUL)
#define PERF_CTRL_DDR_MAX_CH 	(4)

/* attention: there is same structure definition in atf hisi_ddr.c */
struct ddr_perfdata {
	unsigned int channel_nr;
	unsigned int io_width;
	unsigned int rd_flux[PERF_CTRL_DDR_MAX_CH];
	unsigned int wr_flux[PERF_CTRL_DDR_MAX_CH];
};
static phys_addr_t ddr_perfdata_phy_addr = 0;
static struct ddr_perfdata __iomem *freqdump_virt_addr = NULL;
struct ddr_perfdata last_data[DDR_PERFDATA_CLIENT_MAX] = {{0}, {0}};

unsigned long long get_ddrflux_data(unsigned int curr, unsigned int *last)
{
	unsigned long long delta = 0;

	if (curr >= *last)
		delta = (unsigned long long)(curr - *last);
	else
		delta = (unsigned long long)(PERF_CTRL_DDR_FLUX_MAX_CNT - *last + curr);

	*last = curr;

	return delta;
}

#endif

#ifdef CONFIG_SCHED_INFO
int get_schedstat(struct sched_stat *sched_stat_val)
{
	struct task_struct *task;

	rcu_read_lock();
	task = find_task_by_vpid(sched_stat_val->pid);
	if (!task) {
		rcu_read_unlock();
		pr_err("get task from pid(%d) fail.\n", sched_stat_val->pid);
		return -ENOENT;
	}
	get_task_struct(task);
	rcu_read_unlock();
	sched_stat_val->sum_exec_runtime = task_sched_runtime(task);
	sched_stat_val->run_delay = task->sched_info.run_delay;
	sched_stat_val->pcount = task->sched_info.pcount;
	put_task_struct(task);

	return 0;
}
#else
int get_schedstat(struct sched_stat *sched_stat_val)
{
	pr_err("CONFIG_SCHED_INFO not set.\n");
	return 0;
}
#endif

struct task_struct *get_pid_leader_task(pid_t pid)
{
	struct task_struct *cur, *leader;

	cur = find_task_by_vpid(pid);
	if (!cur) {
		pr_err("related_tid get task from pid(%d) fail.\n", pid);
		return NULL;
	}

	get_task_struct(cur);
	leader = cur->group_leader;
	put_task_struct(cur);
	if (!leader) {
		pr_err("related_tid get leader task from pid(%d) fail.\n", pid);
		return NULL;
	}

	return leader;
}

#ifdef CONFIG_HISI_IPA_THERMAL
int get_max_soc_temp(int *temp)
{
	int id = 0, ret = 0, val = 0, val_max = 0;
	int sensor_val[IPA_SENSOR_NUM] = {0};

	/*read all sensor :  c0, c1/c2 , gpu*/
	for (id = 0; id < IPA_SENSOR_NUM; id++) {
		ret = ipa_get_sensor_value((u32)id, &val);
		sensor_val[id] = val;
		if (ret) {
			pr_err("get_max_soc_temp: ipa_get_sensor_value fail. ret = %d\n", ret);
			return ret;
		}
	}

	val_max = sensor_val[0];
	for (id = 1; id < IPA_SENSOR_NUM; id++) {
		if (sensor_val[id] > val_max)
			val_max = sensor_val[id];
	}
	*temp = val_max;

	return ret;
}

int get_system_temp(struct ipa_stat *status)
{
	int sensor_id = 0, temp = 0, ret = 0;

	/* soc temp */
	ret = get_max_soc_temp(&temp);
	if (ret) {
		pr_err("get_max_soc_temp failed ret = %d.\n", ret);
		return -EFAULT;
	}
	status->soc_temp = temp;

	/* system_h temp */
	sensor_id = ipa_get_periph_id(IPA_SENSOR_SYSTEM_H);
	if (sensor_id < 0) {
		pr_err("ipa_get_periph_id failed ret = %d.\n", sensor_id);
		return -1;
	}

	ret = ipa_get_periph_value(sensor_id, &temp);
	if (ret) {
		pr_err("get system_h temp failed. ret = %d.\n", ret);
		return -EFAULT;
	}
	status->board_temp = temp;

	return 0;
}

const char thermal_cdev_type_name[THERMAL_CDEV_MAX][THERMAL_NAME_LENGTH] = {
	CDEV_GPU_NAME, /* CDEV_GPU */
	CDEV_CPU_CLUSTER0_NAME, /* CDEV_CPU_CLUSTER0 */
	CDEV_CPU_CLUSTER1_NAME, /* CDEV_CPU_CLUSTER1 */
	CDEV_CPU_CLUSTER2_NAME, /* CDEV_CPU_CLUSTER2 */
};

const char thermal_zone_name[THERMAL_ZONE_MAX][THERMAL_NAME_LENGTH] = {
	SOC_THERMAL_NAME, /* SOC_THERMAL_ZONE */
	BOARD_THERMAL_NAME, /* BOARD_THERMAL_ZONE */
};

static long perf_ctrl_ioctl_get_power(void __user *uarg) {
	int i;
	unsigned int power = 0;
	struct thermal_cdev_power thp;
	errno_t sret;

	if (copy_from_user(&thp, uarg, sizeof(struct thermal_cdev_power))) {
		pr_err("get_thermal copy_from_user fail.\n");
		return -EFAULT;
	}

	if(thp.thermal_zone_type >= THERMAL_ZONE_MAX || thp.thermal_zone_type < 0) { /*lint !e574 */
		pr_err("%s %d thermal_zone_type %d Not support.\n", __func__, __LINE__, thp.thermal_zone_type);
		return -EINVAL;
	}

	sret = memset_s(thp.cdev_power, sizeof(thp.cdev_power), 0, sizeof(thp.cdev_power));
	if (EOK != sret) {
		pr_err("%s %d memset_s error sret is %d.\n", __func__, __LINE__, sret);
		return -EINVAL;
	}

	for(i = 0 ;i < THERMAL_CDEV_MAX;i++){ /*lint !e574 */
		long result = 0;
		/*Fix coverity[tainted_scalar]*/
		if(thp.thermal_zone_type >= sizeof(thermal_zone_name)/sizeof(thermal_zone_name[0]) || thp.thermal_zone_type < 0) /*lint !e574 */
			break;
		result = thermal_zone_cdev_get_power(thermal_zone_name[thp.thermal_zone_type], thermal_cdev_type_name[i], &power); /*lint !e661 !e662*/
		if (result >= 0)
			thp.cdev_power[i] = power;
	}

	if (copy_to_user(uarg, &thp, sizeof(struct thermal_cdev_power))) {
		pr_err("get_thermal copy_to_user fail.\n");
		return -EFAULT;
	}

	return 0;
}
#else
static long perf_ctrl_ioctl_get_power(void __user *uarg) { return 0; }
int get_system_temp(struct ipa_stat *status) { return 0; }
#endif

static long perf_ctrl_ioctl_get_gpu_fence(void __user *uarg)
{
	struct kbase_fence_info gpu_fence;
	int ret;

	if (copy_from_user(&gpu_fence, uarg, sizeof(struct kbase_fence_info))) {
		pr_err("get_gpu_fence copy_from_user fail.\n");
		return -EFAULT;
	}

	ret = mali_kbase_report_fence_info(&gpu_fence);
	if (ret) {
		pr_err("get_gpu_fence mali fail:%d\n", ret);
		return -EFAULT;
	}

	if (copy_to_user(uarg, &gpu_fence, sizeof(struct kbase_fence_info))) {
		pr_err("get_gpu_fence copy_to_user fail.\n");
		return -EFAULT;
	}

	return 0;
}
int get_ddrc_flux_all_ch(struct ddr_flux* ddr_flux_str, DDR_PERFDATA_CLIENT client)
{
#ifdef CONFIG_HISI_DDR_PERF_CTRL
		int ddr_max_ch = 0, ch = 0;
		if(ddr_flux_str == NULL){
			pr_err("get_ddrc_flux_all_ch:invalid ddr_flux_str is NULL\n");
			return -EFAULT;
		}

		if (client >= DDR_PERFDATA_CLIENT_MAX) {
			pr_err("get_ddrc_flux_all_ch:invalid client %d!\n", client);
			return -EFAULT;
		}

		(void)atfd_hisi_service_access_register_smc(ACCESS_REGISTER_FN_MAIN_ID, ddr_perfdata_phy_addr,
			sizeof(struct ddr_perfdata), ACCESS_REGISTER_FN_SUB_ID_DDR_PERF_CTRL);

		ddr_max_ch = freqdump_virt_addr->channel_nr;

		if ((ddr_max_ch < 1) || (ddr_max_ch > PERF_CTRL_DDR_MAX_CH)) {
			pr_err("get_ddrc_flux:invalid ddr_max_ch %d!\n", ddr_max_ch);
			return -EFAULT;
		}

		for (ch = 0; ch < ddr_max_ch; ch++) {
				ddr_flux_str->rd_flux +=
					get_ddrflux_data(freqdump_virt_addr->rd_flux[ch], &last_data[client].rd_flux[ch]);
				ddr_flux_str->wr_flux +=
					get_ddrflux_data(freqdump_virt_addr->wr_flux[ch], &last_data[client].wr_flux[ch]);

		}
		return 0;
#else
		UNUSED_PARAMETER(ddr_flux_str);
		UNUSED_PARAMETER(client);
		return -EFAULT;
#endif
}

unsigned long perf_ctrl_get_cap(void)
{
	unsigned long cap = 0;

	cap |= BIT(CAP_AI_SCHED_COMM_CMD);
#ifdef CONFIG_HISI_RTG
	cap |= BIT(CAP_RTG_CMD);
#endif

	return cap;
}

static long perf_ctrl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
#ifdef CONFIG_HISI_DDR_PERF_CTRL
	struct ddr_flux ddr_flux_val = {0};
#endif
	struct sched_stat sched_stat_val;
	struct ipa_stat ipa_stat_val;
	void __user *uarg = (void __user *)(uintptr_t)arg;
	struct task_struct *leader, *pos;
	struct related_tid_info *r_t_info = NULL;
#ifdef CONFIG_HISI_DRG
	struct drg_dev_freq dev_freq;
#endif
#ifdef CONFIG_HISI_RTG
	struct rtg_group_task task;
	struct rtg_cpus cpus;
	struct rtg_freq freqs;
	struct rtg_interval interval;
	struct rtg_load_mode lt_mode;
	struct rtg_ed_params ed_params;
#endif
#ifdef CONFIG_FRAME_RTG
	int frame_rate, frame_margin;
	unsigned long frame_status;
#endif
	unsigned long cap;

	if (!uarg) {
		pr_err("perf_ctrl: invalid user uarg!\n");
		return -EINVAL;
	}

	if (_IOC_TYPE(cmd) != PERF_CTRL_MAGIC) {
		pr_err("PERF_CTRL_MAGIC fail. TYPD = %d\n", _IOC_TYPE(cmd));
		return -EINVAL;
	}

	if (_IOC_NR(cmd) >= PERF_CTRL_MAX_NR) {
		pr_err("PERF_CTRL_MAX_NR fail. _IOC_NR(cmd) = %d, MAX_NR = %d.\n", _IOC_NR(cmd), PERF_CTRL_MAX_NR);
		return -EINVAL;
	}

	switch (cmd) {
	case PERF_CTRL_GET_SCHED_STAT: //return sched_stat of pid.
		if (copy_from_user(&sched_stat_val, uarg, sizeof(struct sched_stat))) {
			pr_err("copy_from_user fail.\n");
			ret = -EFAULT;
			break;
		}

		ret = get_schedstat(&sched_stat_val);

		if (ret) {
			pr_err("get_schedstat fail. ret = %ld\n", ret);
		} else {
			if (copy_to_user(uarg, &sched_stat_val, sizeof(struct sched_stat))) {
				pr_err("copy_to_user fail.\n");
				ret = -EFAULT;
			}
		}
		break;

#ifdef CONFIG_HISI_IPA_THERMAL
	case PERF_CTRL_GET_IPA_STAT: /* get ipa status */
		if (copy_from_user(&ipa_stat_val, uarg, sizeof(struct ipa_stat))) {
			pr_err("copy_from_user fail.\n");
			ret = -EFAULT;
			break;
		}

		ret = get_ipa_status(&ipa_stat_val);
		ret += get_system_temp(&ipa_stat_val);
		if (ret) {
			pr_err("get_ipa_stat fail. ret = %ld\n", ret);
			ret = -EFAULT;
		} else {
			if (copy_to_user(uarg, &ipa_stat_val, sizeof(struct ipa_stat))) {
				pr_err("copy_to_user fail.\n");
				ret = -EFAULT;
			}
		}
		break;
#endif

#ifdef CONFIG_HISI_DDR_PERF_CTRL
	case PERF_CTRL_GET_DDR_FLUX:
		if(get_ddrc_flux_all_ch(&ddr_flux_val, DDR_PERFDATA_PERFCTRL) == -EFAULT){
			ret = -EFAULT;
			break;
		}

		if (copy_to_user(uarg, &ddr_flux_val, sizeof(ddr_flux_val))) {
			pr_err("ddr_fluxdata copy_to_user fail!\n");
			ret = -EFAULT;
		}
		break;
#endif

	case PERF_CTRL_GET_RELATED_TID:
		r_t_info = kzalloc(sizeof(struct related_tid_info), GFP_KERNEL);

		if (!r_t_info) {
			pr_err("related_tid kzalloc fail out of memory.\n");
			ret = -ENOMEM;
			goto err;
		}

		if (copy_from_user(r_t_info, uarg, sizeof(struct related_tid_info))) {
			pr_err("related_tid copy_from_user fail.\n");
			ret = -EFAULT;
			goto free;
		}

		rcu_read_lock();
		leader = get_pid_leader_task(r_t_info->pid);
		pos = leader;
		if (pos == NULL || leader == NULL) {
			rcu_read_unlock();
			pr_err("related_tid get task from pid(%d) fail.\n", r_t_info->pid);
			ret = -ENODEV;
			goto free;
		}

		// cppcheck-suppress *
		do {
			if (r_t_info->rel_count >= MAX_TID_COUNT) {
				pr_err("related_tid count > MAX_TID_COUNT(%d).\n", MAX_TID_COUNT);
				break;
			}
			r_t_info->rel_tid[r_t_info->rel_count++] = pos->pid;
		} while_each_thread(leader, pos);
		rcu_read_unlock();

		if (copy_to_user(uarg, r_t_info, sizeof(struct related_tid_info))) {
			pr_err("related_tid copy_to_user fail!\n");
			ret = -EFAULT;
		}
free:
		kfree(r_t_info);
err:
		break;
	case PERF_CTRL_DRG_GET_DEV_FREQ:
#ifdef CONFIG_HISI_DRG
		if (copy_from_user(&dev_freq, uarg, sizeof(struct drg_dev_freq))) {
			pr_err("get_drg_dev_freq copy_from_user fail.\n");
			return -EFAULT;
		}

		if (dev_freq.type == DRG_NONE_DEV) {
			pr_err("get_drg_dev_freq type is DRG_NONE_DEV.\n");
			return -ENODEV;
		}

		if (dev_freq.type != drg_get_freq_range(&dev_freq)) {
			pr_err("get_drg_dev_freq get fail.\n");
			return -ENODEV;
		}

		if (copy_to_user(uarg, &dev_freq, sizeof(struct drg_dev_freq))) {
			pr_err("get_drg_dev_freq copy_from_user fail.\n");
			return -EFAULT;
		}
#else
		ret = -EFAULT;
#endif
		break;
	case PERF_CTRL_GET_THERMAL_CDEV_POWER:
		ret = perf_ctrl_ioctl_get_power(uarg);

		if (ret < 0) {
			pr_err("get_thermal_cdev_power thermal_zone_cdev_get_power failed ret %ld.\n", ret);
			return ret;
		}
		break;
	case PERF_CTRL_GET_GPU_FENCE:
		ret = perf_ctrl_ioctl_get_gpu_fence(uarg);
		if (ret < 0) {
			pr_err("get_gpu_fence failed ret %ld.\n", ret);
			return ret;
		}
		break;

	case PERF_CTRL_GET_DEV_CAP:
		cap = perf_ctrl_get_cap();

		if (copy_to_user(uarg, &cap, sizeof(unsigned long))) {
			pr_err("perf_ctrl_get_cap copy_from_user fail.\n");
			return -EFAULT;
		}
		break;

#ifdef CONFIG_HISI_RTG
	case PERF_CTRL_SET_FRAME_RATE:
#ifdef CONFIG_FRAME_RTG
		if (copy_from_user(&frame_rate, uarg, sizeof(int))) {
			pr_err("frame_qos copy_from_user fail.\n");
			return -EFAULT;
		}
		set_frame_rate(frame_rate);

		ret = sched_set_group_window_size(DEFAULT_RT_FRAME_ID, frame_rate);
#else
		ret = -EFAULT;
#endif
		break;

	case PERF_CTRL_SET_FRAME_MARGIN:
#ifdef CONFIG_FRAME_RTG
		if (copy_from_user(&frame_margin, uarg, sizeof(int))) {
			pr_err("frame_margin copy_from_user fail.\n");
			return -EFAULT;
		}

		ret = set_frame_margin(frame_margin);
#else
		ret = -EFAULT;
#endif
		break;

	case PERF_CTRL_SET_FRAME_STATUS:
#ifdef CONFIG_FRAME_RTG
		if (copy_from_user(&frame_status, uarg, sizeof(unsigned long))) {
			pr_err("frame_status copy_from_user fail.\n");
			return -EFAULT;
		}

		ret = sched_set_group_window_rollover(DEFAULT_RT_FRAME_ID);

		if (!ret)
			ret = set_frame_status(frame_status);
#else
		ret = -EFAULT;
#endif
		break;

	case PERF_CTRL_SET_TASK_RTG:
		if (copy_from_user(&task, uarg, sizeof(struct rtg_group_task))) {
			pr_err("set_rtg_task copy_from_user fail.\n");
			return -EFAULT;
		}

		ret = sched_set_group_id(task.pid, task.grp_id);
		break;

	case PERF_CTRL_SET_RTG_CPUS:
		if (copy_from_user(&cpus, uarg, sizeof(struct rtg_cpus))) {
			pr_err("set_rtg_cpus copy_from_user fail.\n");
			return -EFAULT;
		}

		ret = sched_set_preferred_cluster(cpus.grp_id, cpus.cluster_id);
		break;

	case PERF_CTRL_SET_RTG_FREQ:
		if (copy_from_user(&freqs, uarg, sizeof(struct rtg_freq))) {
			pr_err("set_rtg_freq copy_from_user fail.\n");
			return -EFAULT;
		}

		ret = sched_set_group_freq(freqs.grp_id, freqs.freq);
		break;

	case PERF_CTRL_SET_RTG_FREQ_UPDATE_INTERVAL:
		if (copy_from_user(&interval, uarg, sizeof(struct rtg_interval))) {
			pr_err("set_rtg_interval copy_from_user fail.\n");
			return -EFAULT;
		}

		ret = sched_set_group_freq_update_interval(interval.grp_id, interval.interval);
		break;

	case PERF_CTRL_SET_RTG_UTIL_INVALID_INTERVAL:
		if (copy_from_user(&interval, uarg, sizeof(struct rtg_interval))) {
			pr_err("set_rtg_interval copy_from_user fail.\n");
			return -EFAULT;
		}

		ret = sched_set_group_util_invalid_interval(interval.grp_id, interval.interval);
		break;

	case PERF_CTRL_SET_RTG_LOAD_MODE:
		if (copy_from_user(&lt_mode, uarg, sizeof(struct rtg_load_mode))) {
			pr_err("rtg load mode copy_from_user fail.\n");
			return -EFAULT;
		}

		ret = sched_set_group_load_mode(&lt_mode);
		break;

	case PERF_CTRL_SET_RTG_ED_PARAMS:
		if (copy_from_user(&ed_params, uarg, sizeof(struct rtg_ed_params))) {
			pr_err("rtg_ed_params copy_from_user fail.\n");
			return -EFAULT;
		}
		ret = sched_set_group_ed_params(&ed_params);
		break;
#endif /* CONFIG_HISI_RTG */

	default:
		pr_err("cmd error, here is default, cmd = %d\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}

static int perf_ctrl_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int perf_ctrl_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations perf_ctrl_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = perf_ctrl_ioctl,
	.open = perf_ctrl_open,
	.release = perf_ctrl_release,
};

static struct miscdevice perf_ctrl_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hisi_perf_ctrl",
	.fops = &perf_ctrl_fops,
};

#ifdef CONFIG_HISI_DDR_PERF_CTRL

static int perf_ctrl_ddr_init(void)
{
	int ret = 0;
	struct device_node *np = NULL;
	uint32_t data[2] = {0};

	np = of_find_compatible_node(NULL, NULL, "hisi,hisi-ddr-perfdata");
	if (!np) {
		pr_err("%s: no compatible node found!\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32_array(np, "hisi,ddr-perdata", &data[0], 2UL);
	if (ret) {
		pr_err("%s: find ddr-perfdata node failed!\n", __func__);
		return ret;
	}

	ddr_perfdata_phy_addr = (phys_addr_t)(HISI_SUB_RESERVED_BL31_SHARE_MEM_PHYMEM_BASE + data[0]);
	freqdump_virt_addr = (struct ddr_perfdata *)ioremap(ddr_perfdata_phy_addr, (u64)data[1]);

	if (freqdump_virt_addr == NULL) {
		pr_err("%s: allocate memory for ddr-perfdata failed!\n", __func__);
		return -ENOMEM;
	}
	pr_info("perf_ctrl: perfdata phys:%llx virt:%llx data:%x %x\n", ddr_perfdata_phy_addr, (unsigned long long)(uintptr_t)freqdump_virt_addr, data[0], data[1]); /*lint !e626*/

	return ret;
}

static void perf_ctrl_ddr_exit(void)
{
	if (!freqdump_virt_addr) {
		iounmap(freqdump_virt_addr);
		freqdump_virt_addr = NULL;
	}
}

#else
static int perf_ctrl_ddr_init(void) { return 0; }
static void perf_ctrl_ddr_exit(void) { }
#endif

static int __init perf_ctrl_dev_init(void)
{
	int err;

	err = misc_register(&perf_ctrl_device);
	if (err)
		return err;

	err = perf_ctrl_ddr_init();
	if (err)
		return err;

	return 0;
}

static void  __exit perf_ctrl_dev_exit(void)
{
	perf_ctrl_ddr_exit();
	misc_deregister(&perf_ctrl_device);
}

module_init(perf_ctrl_dev_init);
module_exit(perf_ctrl_dev_exit);
