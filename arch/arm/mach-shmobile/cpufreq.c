/*
 * arch/arm/mach-shmobile/cpufreq.c
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/earlysuspend.h>
#include <linux/slab.h>
#include <asm/system.h>
#include <mach/common.h>
#include <mach/pm.h>
#include <linux/stat.h>
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <memlog/memlog.h>
#include <linux/platform_data/sh_cpufreq.h>
#include <linux/cpumask.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <asm/uaccess.h>
#endif

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "dvfs[cpufreq.c<%d>]:" fmt, __LINE__
#endif

#define EXTAL1 26000
#define FREQ_MAX15 1456000
#define FREQ_MID_UPPER_LIMIT15 910000
#define FREQ_MID_LOWER_LIMIT15 455000
#define FREQ_MIN_UPPER_LIMIT15 364000
#define FREQ_MIN_LOWER_LIMIT15 273000
#define FREQ_LIMIT_MIN_CPUFREQ15 364000
#define FREQ_MAX12 1196000
#define FREQ_MID_UPPER_LIMIT12 897000
#define FREQ_MID_LOWER_LIMIT12 448500
#define FREQ_MIN_UPPER_LIMIT12 373750
#define FREQ_MIN_LOWER_LIMIT12 299000
#define FREQ_LIMIT_MIN_CPUFREQ12 373750

#define MODE_PER_STATE       3

/* Clocks State */
enum clock_state {
	MODE_NORMAL = 0,
	MODE_EARLY_SUSPEND,
	MODE_MOVIE720P,
	MODE_SUSPEND,
	MODE_NUM
};

enum dfs_state {
	DFS_STOP = 0,
	DFS_START,
};

/* reference counting enums */
enum dfs_ref_type {
	DFS_REF_EARLY_SUSPEND = 0,
	DFS_REF_START_STOP,
	DFS_REF_MIN_MODE,
	DFS_REF_MAX,
};

#define SUSPEND_CPUFREQ15 FREQ_MID_LOWER_LIMIT15	/* Suspend */
#define SUSPEND_CPUFREQ12 FREQ_MID_LOWER_LIMIT12
#define CORESTB_CPUFREQ CPUFREQ_ENTRY_INVALID   /* CoreStandby */
#define FREQ_TRANSITION_LATENCY  (CONFIG_SH_TRANSITION_LATENCY * NSEC_PER_USEC)

#define MAX_ZS_DIVRATE	DIV1_6
#define MAX_HP_DIVRATE	DIV1_12

/* For change sampling rate & down factor dynamically */
#define SAMPLING_RATE_DEF FREQ_TRANSITION_LATENCY
#define SAMPLING_RATE_LOW 500000
#define SAMPLING_DOWN_FACTOR_DEF (CONFIG_SH_SAMPLING_DOWN_FACTOR)
#define SAMPLING_DOWN_FACTOR_LOW 1

#define INIT_STATE	1
#define BACK_UP_STATE	2
#define NORMAL_STATE	3
#define STOP_STATE	4

#ifndef CONFIG_HOTPLUG_CPU_MGR
#ifdef CONFIG_HOTPLUG_CPU
#define cpu_up_manager(x, y)    cpu_up(x)
#define cpu_down_manager(x, y)    cpu_down(x)
#else /*!CONFIG_HOTPLUG_CPU*/
#define cpu_up_manager(x, y) do { } while (0)
#define cpu_down_manager(x, y) do { } while (0)
#endif /*CONFIG_HOTPLUG_CPU*/
#endif /*CONFIG_HOTPLUG_CPU_MGR*/

#define pr_cpufreq(level, args...)	\
	do {	\
		if (SH_CPUFREQ_PRINT_##level == SH_CPUFREQ_PRINT_ERROR) \
			pr_err(args); \
		else if (SH_CPUFREQ_PRINT_##level & hp_debug_mask) \
			pr_info(args); \
	} while (0) \

/* Resource */
struct cpufreq_policy_resource {
	struct cpufreq_governor *gov;
	unsigned int max;
	unsigned int min;
	unsigned int core_min_num;
};

struct shmobile_hp_data {
	struct sh_plat_hp_data *pdata;
	struct cpumask hlgmask;
	struct cpumask locked_cpus;
	unsigned int *load_history;
	unsigned int history_idx;
	unsigned int history_samples;
	int hlg_enabled;
};

/* CPU info */
struct shmobile_cpuinfo {
	struct cpufreq_policy_resource dfs_save;
	struct cpufreq_policy_resource suspend_save;
	struct shmobile_hp_data hpdata;
	unsigned int freq;
	unsigned int req_rate[CONFIG_NR_CPUS];
	unsigned int freq_max;
	unsigned int freq_mid_upper_limit;
	unsigned int freq_mid_lower_limit;
	unsigned int freq_min_upper_limit;
	unsigned int freq_min_lower_limit;
	unsigned int freq_suspend;
	atomic_t dfs_ref[DFS_REF_MAX];
	int sgx_flg;
	int clk_state;
	int scaling_locked;
	spinlock_t lock;
	int dfs_state;
};

/**************** static ****************/
static unsigned int hp_debug_mask = SH_CPUFREQ_PRINT_ERROR |
				SH_CPUFREQ_PRINT_WARNING |
				SH_CPUFREQ_PRINT_UPDOWN |
				SH_CPUFREQ_PRINT_INIT;
static int zclk12_flg;
#ifdef CONFIG_PM_DEBUG
int cpufreq_enabled = 1;
#endif /* CONFIG_PM_DEBUG */
#ifndef CPUFREQ_TEST_MODE
static
#endif /* CPUFREQ_TEST_MODE */
struct shmobile_cpuinfo	the_cpuinfo;
static struct cpufreq_frequency_table
/* ES2.x */
main_freqtbl_es2_x[11],
*freq_table = NULL;

static struct {
	int pllratio;
	int div_rate;
	int waveform;
} zdiv_table15[] = {
	{ PLLx56, DIV1_1, LLx16_16},	/* 1,456 MHz	*/
	{ PLLx49, DIV1_1, LLx14_16},	/* 1,274 MHz	*/
	{ PLLx42, DIV1_1, LLx12_16},	/* 1,092 MHz	*/
	{ PLLx35, DIV1_1, LLx10_16},	/*   910 MHz	*/
	{ PLLx56, DIV1_2, LLx8_16},	/*   728 MHz	*/
	{ PLLx49, DIV1_2, LLx7_16},	/*   637 MHz	*/
	{ PLLx42, DIV1_2, LLx6_16},	/*   546 MHz	*/
	{ PLLx35, DIV1_2, LLx5_16},	/*   455 MHz	*/
	{ PLLx56, DIV1_4, LLx4_16},	/*   364 MHz	*/
	{ PLLx42, DIV1_4, LLx3_16},	/*   273 MHz	*/
},
zdiv_table12[] = {
	{ PLLx46, DIV1_1, LLx16_16},	/* 1196    MHz	*/
	{ PLLx46, DIV1_1, LLx14_16},	/* 1046.5  MHz	*/
	{ PLLx46, DIV1_1, LLx12_16},	/*  897    MHz	*/
	{ PLLx46, DIV1_1, LLx10_16},	/*  747.5  MHz	*/
	{ PLLx46, DIV1_2, LLx9_16},	/*  672.75 MHz	*/
	{ PLLx46, DIV1_2, LLx7_16},	/*  523.25 MHz	*/
	{ PLLx46, DIV1_2, LLx6_16},	/*  448.5  MHz	*/
	{ PLLx46, DIV1_2, LLx5_16},	/*  373.75 MHz	*/
	{ PLLx46, DIV1_4, LLx4_16},	/*  299    MHz	*/
},
*zdiv_table = NULL;

static int debug = 1;
#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND
static int sampling_flag = INIT_STATE;
#else
static int sampling_flag = STOP_STATE;
#endif /* CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND */
module_param(debug, int, S_IRUGO | S_IWUSR | S_IWGRP);
#ifdef DYNAMIC_HOTPLUG_CPU

static int shmobile_sysfs_init(void);
static void dynamic_hlg_exit(void);
static void hlg_work_handler(struct work_struct *work);

/* Hotplug works */
static struct workqueue_struct *dfs_queue;
static DECLARE_WORK(hlg_work, hlg_work_handler);

static inline int sh_ref_lock(int ref_type)
{
	BUG_ON((ref_type < 0) || (ref_type > DFS_REF_MAX - 1));

	return (atomic_inc_return(&the_cpuinfo.dfs_ref[ref_type]) == 1);
}

static inline int sh_ref_unlock(int ref_type)
{
	BUG_ON((ref_type < 0) || (ref_type > DFS_REF_MAX - 1));

	if (!atomic_read(&the_cpuinfo.dfs_ref[ref_type]))
		return 0;

	return atomic_dec_and_test(&the_cpuinfo.dfs_ref[ref_type]);
}

/*
 * pr_his_req: print requested frequency buffer
 *
 * Argument:
 *		None
 *
 * Return:
 *		None
 */
static inline void pr_his_req(void)
{
#ifdef DVFS_DEBUG_MODE
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;

	int i = 0;
	int j = hpdata->history_idx - 1;

	for (i = 0; i < hpdata->history_samples; i++, j--) {
		if (j < 0)
			j = hpdata->history_samples - 1;
		pr_cpufreq(VERBOSE, "[%2d]%07u\n", j, hpdata->load_history[j]);
	}
#endif
}

/*
 * hlg_cpu_updown: plug-in/plug-out cpus
 *
 * Arguments :
 *      @hlgmask : cpumask of requested online cpus
 *
 * Return :
 *		None
 */
static void hlg_cpu_updown(const struct cpumask *hlgmask)
{
	int cpu, ret = 0;

	for_each_present_cpu(cpu) {
		if (cpu_online(cpu) && !cpumask_test_cpu(cpu, hlgmask)) {
			pr_cpufreq(UPDOWN, "plug-out cpu<%d>...\n", cpu);
			ret = cpu_down_manager(cpu, DFS_HOTPLUG_ID);
			if (ret)
				pr_cpufreq(ERROR, "(%s):cpu%d down, err %d\n",
							__func__, cpu, ret);
			pr_cpufreq(UPDOWN, "plug-out cpu<%d> done\n", cpu);
		} else if (cpu_is_offline(cpu) &&
					cpumask_test_cpu(cpu, hlgmask)) {
			pr_cpufreq(UPDOWN, "plug-in cpu<%d>...\n", cpu);
			ret = cpu_up_manager(cpu, DFS_HOTPLUG_ID);
			if (ret)
				pr_cpufreq(ERROR, "(%s):cpu%d up, err %d\n",
						__func__, cpu, ret);
			pr_cpufreq(UPDOWN, "plug-in cpu<%d> done\n", cpu);
		}
	}
}

/*
 * hlg_work_handler: work to handle hotplug of cpus
 *
 * Arguments :
 *            @work : work instance
 * Return :
 *			None
 */
static void hlg_work_handler(struct work_struct *work)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;
	struct cpumask *hlgmask;

	if (!hpdata->hlg_enabled) {
		dynamic_hlg_exit();
		return;
	}

	spin_lock(&the_cpuinfo.lock);
	hlgmask = &hpdata->hlgmask;
	cpumask_or(hlgmask, hlgmask, &hpdata->locked_cpus);
	spin_unlock(&the_cpuinfo.lock);

	hlg_cpu_updown(hlgmask);
}

static inline void hp_clear_history(struct shmobile_hp_data *hpdata)
{
	struct sh_plat_hp_data *hpplat = hpdata->pdata;

	hpdata->history_idx = 0;
	memset(hpdata->load_history, 0,
		sizeof(*hpdata->load_history) * hpplat->hotplug_samples);
}

/*
 * shmobile_cpufreq_load_info: callback from cpufreq governor to notify
 *                             current cpu load
 *
 * Arguments :
 *            @policy: cpu policy
 *            @load: current cpu load
 * Return :
 *			None
 */
static void shmobile_cpufreq_load_info(struct cpufreq_policy *policy,
				 unsigned int load)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;
	struct sh_plat_hp_data *hpplat = hpdata->pdata;
	int cnt, cpu;
	unsigned long avg_load = 0;

	spin_lock(&the_cpuinfo.lock);

	hpdata->load_history[hpdata->history_idx++] = load;
	if (hpdata->history_idx < hpdata->history_samples)
		goto unlock;

	/* calculate average of load */
	for (cnt = 0; cnt < hpdata->history_idx; cnt++)
		avg_load += hpdata->load_history[cnt];
	avg_load = avg_load / cnt;

	cpumask_copy(&hpdata->hlgmask, cpu_online_mask);
	/* any cpu's to plug-in / plug-out, get them into mask */
	for_each_possible_cpu(cpu) {
		if (cpu == 0)
			continue;

		if (cpumask_test_cpu(cpu, &hpdata->locked_cpus))
			continue;

		if (avg_load >= hpplat->thresholds[cpu].thresh_plugin)
			cpu_set(cpu, hpdata->hlgmask);
		else if (avg_load <= hpplat->thresholds[cpu].thresh_plugout)
			cpu_clear(cpu, hpdata->hlgmask);
	}

	if (cpumask_equal(&hpdata->hlgmask, cpu_online_mask))
		goto clear_history;

	pr_his_req();
	/* schedule work on cpu0 to hotplug */
	queue_work_on(0, dfs_queue, &hlg_work);

clear_history:
	hp_clear_history(hpdata);
unlock:
	spin_unlock(&the_cpuinfo.lock);
}

/*
 * dynamic_hlg_exit: clear hotplug history and wakeup non-boot cpus
 *
 * Arguments:
 *          None
 * Return :
 *			None
 */
static void dynamic_hlg_exit(void)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);

	if (the_cpuinfo.clk_state != MODE_SUSPEND)
		hlg_cpu_updown(cpu_present_mask);

	/* clear hotplug history */
	cpumask_clear(&hpdata->hlgmask);
	hp_clear_history(hpdata);

	pr_cpufreq(VERBOSE, "-- %s\n", __func__);
}
#endif /* DYNAMIC_HOTPLUG_CPU */

int cpufreq_get_cpu_min_limit(void)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;

	return cpumask_weight(&hpdata->locked_cpus);
}
EXPORT_SYMBOL(cpufreq_get_cpu_min_limit);

int cpufreq_set_cpu_min_limit(int input)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;
	unsigned int nr_cpus = num_possible_cpus();
	int cnt;

	if (!input || input > nr_cpus)
		return -EINVAL;

	spin_lock(&the_cpuinfo.lock);
	cpumask_clear(&hpdata->locked_cpus);
	for (cnt = 0; cnt < input; cnt++)
		cpumask_set_cpu(cnt, &hpdata->locked_cpus);

	/* If locked cpus are offline, bring them online */
	if (cpumask_equal(&hpdata->locked_cpus, cpu_online_mask)) {
		spin_unlock(&the_cpuinfo.lock);
		return 0;
	}

 /* In case of cpu num lock, it will be applied immediatley.
		but in restore case, it will be applied at next load calculation time */
	if(input >1)
		queue_work_on(0, dfs_queue, &hlg_work);
	spin_unlock(&the_cpuinfo.lock);
	return 0;
}
EXPORT_SYMBOL(cpufreq_set_cpu_min_limit);


/* This function must be called only in spin lock region */
static int cpufreq_set_cpu_min_limit_withoutlock(int input)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;
	unsigned int nr_cpus = num_possible_cpus();
	int cnt;

	if (!input || input > nr_cpus)
		return -EINVAL;

	cpumask_clear(&hpdata->locked_cpus);
	for (cnt = 0; cnt < input; cnt++)
		cpumask_set_cpu(cnt, &hpdata->locked_cpus);

	the_cpuinfo.dfs_save.core_min_num=input;

	/* If locked cpus are offline, bring them online */
	if (cpumask_equal(&hpdata->locked_cpus, cpu_online_mask)) {
		return 0;
	}

 /* In case of cpu num lock, it will be applied immediatley.
		but in restore case, it will be applied at next load calculation time */
	if(input >1)
		queue_work_on(0, dfs_queue, &hlg_work);
	return 0;
}

/*
 * find_target: find freq value in the freq table
 *		which is nearest the target freq
*		and return the freq table index
 * Argument:
 *		@table: freq table
 *		@freq: target freq
 *		@index: freq table index returned
 *
 * Return:
 *		0: Operation success
 *		negative: Operation fail
 */
static int find_target(struct cpufreq_frequency_table *table,
			unsigned int freq, unsigned int *index)
{
	struct cpufreq_policy *policy = NULL;
	int ret = 0;

	policy = cpufreq_cpu_get(0);
	if (!policy) {
		ret = -EINVAL;
		goto out;
	}

	if (cpufreq_frequency_table_target(policy, table, freq,
		CPUFREQ_RELATION_H, index)) {
		ret = -EINVAL;
		goto out;
	}

out:
	cpufreq_cpu_put(policy);
	return ret;
}

/*
 * __to_freq_level: convert from frequency to frequency level
 *
 * Argument:
 *		@freq: the frequency will be set
 *
 * Return:
 *		freq level
 */
static int __to_freq_level(unsigned int freq)
{
	int idx = -1;
	int found = -1;

	for (idx = 0; freq_table[idx].frequency != CPUFREQ_TABLE_END; idx++)
		if (freq_table[idx].frequency == freq)
			found = idx;
	return found;
}
#ifndef ZFREQ_MODE
/*
 * __to_div_rate: convert from frequency to divrate
 *
 * Argument:
 *		@freq: the frequency will be set
 *
 * Return:
 *		divrate
 */
static int __to_div_rate(unsigned int freq)
{
	int arr_size = 0;
	int idx = __to_freq_level(freq);

	if (zclk12_flg == 0)
		arr_size = (int)ARRAY_SIZE(zdiv_table15);
	else
		arr_size = (int)ARRAY_SIZE(zdiv_table12);
	if ((idx >= arr_size) || (idx < 0)) {
		pr_cpufreq(ERROR, "invalid param, freq<%7u> index<%d>\n",
			freq, idx);
		return -EINVAL;
	}
	return zdiv_table[idx].div_rate;
}
#endif

int cpg_dbg_clkmode;

/*
 * __clk_get_rate: get the set of clocks
 *
 * Argument:
 * Return:
 *     address of element of array clocks_div_data
 *     NULL: if input parameters are invaid
 */
#ifndef CPUFREQ_TEST_MODE
static
#endif /* CPUFREQ_TEST_MODE */
int __clk_get_rate(struct clk_rate *rate, bool override)
{
	unsigned int target_freq = the_cpuinfo.freq;
	int clkmode = 0;
	int ret = 0;
	int level = 0;

	if (!rate) {
		pr_cpufreq(ERROR, "invalid parameter<NULL>\n");
		return -EINVAL;
	}

	if (target_freq <= the_cpuinfo.freq_mid_upper_limit)
		level++;
	if (target_freq <= the_cpuinfo.freq_min_upper_limit)
		level++;

	if (the_cpuinfo.clk_state == MODE_SUSPEND)
		clkmode = 0;
	else if (override)
		clkmode = MODE_NORMAL * MODE_PER_STATE + level + 1;
	else
		clkmode = the_cpuinfo.clk_state * MODE_PER_STATE + level + 1;

	cpg_dbg_clkmode = clkmode; /* CFG_DEBUG */

	/* get clocks setting according to clock mode */
	ret = pm_get_clock_mode(clkmode, rate);
	
	if (ret)
		pr_cpufreq(ERROR, "error! fail to get clock mode<%d>\n",
				clkmode);

	return ret;
}

/*
 * __notify_all_cpu: notify frequency change to all present CPUs
 *
 * Argument:
 *		@fold: the old frequency value
 *		@fnew: the new frequency value
 *		@cpu_nr: number of CPUS
 *		@flag: notification flag
 *
 * Return:
 *		none
 */
static void __notify_all_cpu(unsigned int fold, unsigned int fnew, int flag)
{
	struct cpufreq_freqs freqs;
	struct cpufreq_policy *policy;

	freqs.old = fold;
	freqs.new = fnew;
	policy = cpufreq_cpu_get(0);

	cpufreq_notify_transition(policy, &freqs, flag);
	cpufreq_cpu_put(policy);
}

/*
 * __set_rate: set SYS-CPU frequency
 *`
 * Argument:
 *		@freq: the frequency will be set
 *
 * Return:
 *     0: setting is normal
 *     negative: operation fail
 */
#ifndef CPUFREQ_TEST_MODE
static
#endif /* CPUFREQ_TEST_MODE */
int __set_rate(unsigned int freq)
{
#ifndef ZFREQ_MODE
	int div_rate = DIV1_1;
	int pllratio = PLLx56;
#endif /* ZFREQ_MODE */
	int freq_old = 0;
	int ret = 0;
	int level = 0;

	if (freq == the_cpuinfo.freq)
		goto done;

	freq_old = the_cpuinfo.freq;
	level = __to_freq_level(freq);
	if (level < 0)
		return -EINVAL;
#ifndef ZFREQ_MODE
	div_rate = __to_div_rate(freq);
	if (div_rate < 0)
		return -EINVAL;

	ret = pm_set_syscpu_frequency(div_rate);
	if (!ret) {
		/* change PLL0 if need */
		pllratio = zdiv_table[level].pllratio;
		if (pm_get_pll_ratio(PLL0) != pllratio) {
			ret = pm_set_pll_ratio(PLL0, pllratio);
			if (ret)
				goto done;
		}

		the_cpuinfo.freq = freq;
		__notify_all_cpu(freq_old, freq, CPUFREQ_POSTCHANGE);
		memory_log_dump_int(PM_DUMP_ID_DFS_FREQ, freq);
	}
#else /* ZFREQ_MODE */
	ret = pm_set_syscpu_frequency(zdiv_table[level].waveform);
	if (!ret) {
		the_cpuinfo.freq = freq;
		__notify_all_cpu(freq_old, freq, CPUFREQ_POSTCHANGE);
		memory_log_dump_int(PM_DUMP_ID_DFS_FREQ, freq);
	}
#endif /* ZFREQ_MODE */
done:
	return ret;
}

static inline void __change_sampling_values(void)
{
	int ret = 0;
	static unsigned int downft = SAMPLING_DOWN_FACTOR_DEF;
	static unsigned int samrate = SAMPLING_RATE_DEF;
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(0);

	if (!policy || !policy->governor)
		return;
	if (strcmp(policy->governor->name, "ondemand") != 0)
		return;

	if (STOP_STATE == sampling_flag) /* ondemand gov is stopped! */
		return;
	if (INIT_STATE == sampling_flag) {
		samplrate_downfact_change(policy, SAMPLING_RATE_DEF,
					SAMPLING_DOWN_FACTOR_DEF,
					0);
		sampling_flag = NORMAL_STATE;
	}
	if ((the_cpuinfo.clk_state == MODE_EARLY_SUSPEND &&
		the_cpuinfo.freq <= the_cpuinfo.freq_min_upper_limit) ||
		the_cpuinfo.clk_state == MODE_SUSPEND) {
		if (NORMAL_STATE == sampling_flag) {/* Backup old values */
			samplrate_downfact_get(policy, &samrate, &downft);
			sampling_flag = BACK_UP_STATE;
		}
		ret = samplrate_downfact_change(policy, SAMPLING_RATE_LOW,
				SAMPLING_DOWN_FACTOR_LOW, 1);
	} else { /* Need to restore the previous values if any */
		if (BACK_UP_STATE == sampling_flag) {
			sampling_flag = NORMAL_STATE;
			ret = samplrate_downfact_change(policy, samrate,
					downft, 0);
		}
	}
	if (ret)
		pr_cpufreq(ERROR, "%s: samplrate_downfact_change() err<%d>\n",
			__func__, ret);
	cpufreq_cpu_put(policy);
}

/*
 * __set_sys_clocks: verify and set system clocks
 *`
 * Argument:
 *		@override: if yes, normal mode clocks set
 *
 * Return:
 *		none
 */
static inline int __set_sys_clocks(bool override)
{
	int ret = 0;
	struct clk_rate rate;

	/* get and set clocks setting based on new SYS-CPU clock */
	ret = __clk_get_rate(&rate, override);
	if (!ret)
		ret = pm_set_clocks(rate);
	__change_sampling_values();

	return ret;
}

/*
 * __set_all_clocks: set SYS-CPU frequency and other clocks
 *`
 * Argument:
 *		@freq: the SYS frequency will be set
 *
 * Return:
 *     0: setting is normal
 *     negative: operation fail
 */
static inline int __set_all_clocks(unsigned int z_freq)
{
	int ret = 0;

	ret = __set_rate(z_freq);
	if (ret)
		return ret;

	return __set_sys_clocks(false);
}

/*
 * disable_early_suspend_clock:
 *  force to use the clock setting for Normal state even at Early Suspend state
 *  Manage status and "disable_early_clock" structure for changeing
 *  the Normal frequency in  __clk_get_rate().
 *
 * Argument:	none
 * Return:	0: normal
 *		negative: operation fail
 */
int disable_early_suspend_clock(void)
{
	int ret = 0;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);

	if (!sh_ref_lock(DFS_REF_EARLY_SUSPEND))
		goto out;

	if (MODE_EARLY_SUSPEND == the_cpuinfo.clk_state) {
		ret = __set_sys_clocks(true);
		if (ret < 0) {
			pr_cpufreq(ERROR, "[%s][%d]: cannot __set_sys_clocks\n",
						__func__, ret);
			goto out;
		}
	}

out:
	spin_unlock(&the_cpuinfo.lock);
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);
	return ret;
}
EXPORT_SYMBOL(disable_early_suspend_clock);

/*
 * API:remove the restriction for function of "disable_early_suspend_clock".
 *
 * Argument:	none
 * Return:	none
 * NOTE: call this function only once after disable_early_suspend_clock
 * succeeded (returned 0)
*/
void enable_early_suspend_clock(void)
{
	int ret = 0;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);

	if (!sh_ref_unlock(DFS_REF_EARLY_SUSPEND))
		goto out;

	if (MODE_EARLY_SUSPEND == the_cpuinfo.clk_state) {
		ret = __set_sys_clocks(false);
		if (ret < 0) {
			pr_cpufreq(ERROR, "[%s][%d]: cannot __set_all_clocks\n",
						__func__, ret);
			goto out;
		}
	}

out:
	spin_unlock(&the_cpuinfo.lock);
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);
}
EXPORT_SYMBOL(enable_early_suspend_clock);

/*
 * start_cpufreq: start dynamic frequency scaling, the SYS-CPU frequency
 * is changed automatically based on the system load.
 *
 * Argument:
 *		none
 *
 * Return: none
 */
void start_cpufreq(void)
{
	struct cpufreq_policy *cur_policy;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);

	if (!sh_ref_unlock(DFS_REF_START_STOP)) {
		spin_unlock(&the_cpuinfo.lock);
		goto out;
	}

	cur_policy = cpufreq_cpu_get(0);
	BUG_ON(!cur_policy);

	cur_policy->user_policy.governor = the_cpuinfo.dfs_save.gov;
	cur_policy->user_policy.max = the_cpuinfo.dfs_save.max;
	cur_policy->user_policy.min = the_cpuinfo.dfs_save.min;

	cpufreq_cpu_put(cur_policy);
	the_cpuinfo.dfs_state = DFS_START;
	spin_unlock(&the_cpuinfo.lock);
	cpufreq_update_policy(0);
	if(the_cpuinfo.dfs_save.core_min_num) 
		cpufreq_set_cpu_min_limit(the_cpuinfo.dfs_save.core_min_num);

out:
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);
}
EXPORT_SYMBOL(start_cpufreq);

/*
 * stop_cpufreq: stop dynamic frequency scaling, the SYS-CPU frequency
 * is changed to maximum.
 *
 * Argument:
 *		none
 *
 * Return:
 *		0: normal
 *		negative: operation fail
 */
int stop_cpufreq(void)
{
	struct cpufreq_policy *cur_policy;
	int ret = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);

	if (!sh_ref_lock(DFS_REF_START_STOP)) {
		spin_unlock(&the_cpuinfo.lock);
		goto out;
	}

	cur_policy = cpufreq_cpu_get(0);
	BUG_ON(!cur_policy);

	/* save current policy */
	the_cpuinfo.dfs_save.gov = cur_policy->governor;
	the_cpuinfo.dfs_save.max = cur_policy->max;
	the_cpuinfo.dfs_save.min = cur_policy->min;

	/* switch to performance governor to set maximum freq */
	cur_policy->user_policy.governor = &cpufreq_gov_performance;
	cur_policy->user_policy.max = cur_policy->max;
	cur_policy->user_policy.min = cur_policy->min;

	cpufreq_cpu_put(cur_policy);
	the_cpuinfo.dfs_state = DFS_STOP;
	spin_unlock(&the_cpuinfo.lock);
	cpufreq_update_policy(0);
	cpufreq_set_cpu_min_limit(2);


out:
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);
	return ret;
}
EXPORT_SYMBOL(stop_cpufreq);

/*
 * disable_dfs_mode_min: prevent the CPU from running at minimum frequency,
 * in this case, the SYS-CPU only runs at middle frequency, or maximum
 * frequency.
 *
 * Argument:
 *		none
 *
 * Return: none
 */
void disable_dfs_mode_min(void)
{
	struct cpufreq_policy *cur_policy;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);

	if (!sh_ref_lock(DFS_REF_MIN_MODE)) {
		spin_unlock(&the_cpuinfo.lock);
		goto out;
	}

	cur_policy = cpufreq_cpu_get(0);
	BUG_ON(!cur_policy);

	cur_policy->user_policy.min = the_cpuinfo.freq_mid_lower_limit;
	cpufreq_cpu_put(cur_policy);
	spin_unlock(&the_cpuinfo.lock);
	cpufreq_update_policy(0);

out:
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);
}
EXPORT_SYMBOL(disable_dfs_mode_min);

/*
 * enable_dfs_mode_min: allow the CPU to run at minimum frequency, in this case,
 * the SYS-CPU can run at minimum frequency, or middle frequency, or maximum
 * frequency.
 *
 * Argument:
 *		none
 *
 * Return:
 *		none
 */
void enable_dfs_mode_min(void)
{
	struct cpufreq_policy *cur_policy;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */

	if (!validate())
		return;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);

	if (!sh_ref_unlock(DFS_REF_MIN_MODE)) {
		spin_unlock(&the_cpuinfo.lock);
		goto out;
	}

	cur_policy = cpufreq_cpu_get(0);
	BUG_ON(!cur_policy);

	cur_policy->user_policy.min = the_cpuinfo.freq_min_lower_limit;
	cpufreq_cpu_put(cur_policy);
	spin_unlock(&the_cpuinfo.lock);
	cpufreq_update_policy(0);

out:
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);
}
EXPORT_SYMBOL(enable_dfs_mode_min);

/*
 * movie_cpufreq: change the SYS-CPU frequency to middle before entering
 * uspend state.
 * Argument:
 *		sw720p
 *
 * Return:
 *		0: normal
 *		negative: operation fail
 */
int movie_cpufreq(int sw720p)
{
	int ret = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);

	if ((the_cpuinfo.clk_state != MODE_MOVIE720P) &&
	    (the_cpuinfo.clk_state != MODE_NORMAL)) {
		ret = -EBUSY;
		goto done;
	}

	if (sw720p)
		the_cpuinfo.clk_state = MODE_MOVIE720P;
	else
		the_cpuinfo.clk_state = MODE_NORMAL;

	/* change SYS-CPU frequency */
	ret = __set_sys_clocks(false);

done:
	spin_unlock(&the_cpuinfo.lock);
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);

	return ret;
}
EXPORT_SYMBOL(movie_cpufreq);

/*
 * corestandby_cpufreq: change clocks in case of corestandby
 *
 * Argument:
 *		none
 * Return:
 *		0: normal
 *		negative: operation fail
 */
int corestandby_cpufreq(void)
{
	int ret = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);

	if (CORESTB_CPUFREQ != CPUFREQ_ENTRY_INVALID) {
		ret = __set_rate(CORESTB_CPUFREQ);
		if (ret)
			pr_cpufreq(ERROR, "fail to do frequency setting\n");
	}

	spin_unlock(&the_cpuinfo.lock);
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);
	return ret;
}
EXPORT_SYMBOL(corestandby_cpufreq);

/*
 * suspend_cpufreq: change the SYS-CPU frequency to middle before entering
 * uspend state.
 * Argument:
 *		none
 *
 * Return:
 *		0: normal
 *		negative: operation fail
 */
int suspend_cpufreq(void)
{
	struct cpufreq_policy *cur_policy;
	int ret = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);

	cur_policy = cpufreq_cpu_get(0);
	BUG_ON(!cur_policy);
	the_cpuinfo.clk_state = MODE_SUSPEND;

	/* save the current set policy */
	the_cpuinfo.suspend_save.gov = cur_policy->governor;
	the_cpuinfo.suspend_save.max = cur_policy->max;
	the_cpuinfo.suspend_save.min = cur_policy->min;

	/* switch to suspend freq */
	if (the_cpuinfo.dfs_state != DFS_STOP) {
		cur_policy->user_policy.governor = &cpufreq_gov_performance;
		cur_policy->user_policy.max = the_cpuinfo.freq_suspend;
		cur_policy->user_policy.min = the_cpuinfo.freq_suspend;
	}

	cpufreq_cpu_put(cur_policy);
	spin_unlock(&the_cpuinfo.lock);
	cpufreq_update_policy(0);
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);

	return ret;
}
EXPORT_SYMBOL(suspend_cpufreq);

/*
 * resume_cpufreq: change the SYS-CPU and others frequency to middle
 * if the dfs has already been started.
 * Argument:
 *		none
 *
 * Return:
 *		0: normal
 *		negative: operation fail
 */
int resume_cpufreq(void)
{
	struct cpufreq_policy *cur_policy;
	int ret = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);

	cur_policy = cpufreq_cpu_get(0);
	BUG_ON(!cur_policy);
	the_cpuinfo.clk_state = MODE_EARLY_SUSPEND;

	/* restore to saved policy in suspend_cpufreq */
	cur_policy->user_policy.governor = the_cpuinfo.suspend_save.gov;
	cur_policy->user_policy.max = the_cpuinfo.suspend_save.max;
	cur_policy->user_policy.min = the_cpuinfo.suspend_save.min;

	cpufreq_cpu_put(cur_policy);
	spin_unlock(&the_cpuinfo.lock);
	cpufreq_update_policy(0);
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);

	return ret;
}
EXPORT_SYMBOL(resume_cpufreq);

/*
 * sgx_cpufreq: change the DFS mode to handle for SGX ON/OFF.
 *
 * Argument:
 *		@flag: the status of SGX module
 *			CPUFREQ_SGXON: the SGX is on
 *			CPUFREQ_SGXOFF: the SGX is off
 * Return:
 *		0: normal
 *		negative: operation fail
 */
int sgx_cpufreq(int flag)
{
	int ret = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (validate()) {
		spin_lock(&the_cpuinfo.lock);
		the_cpuinfo.sgx_flg = flag;
		spin_unlock(&the_cpuinfo.lock);
	}
	return ret;
}
EXPORT_SYMBOL(sgx_cpufreq);

#ifdef CONFIG_PM_FORCE_SLEEP
void suspend_cpufreq_hlg_work(void)
{
#ifdef DYNAMIC_HOTPLUG_CPU
	cancel_work_sync(&hlg_work);
#endif
}
EXPORT_SYMBOL(suspend_cpufreq_hlg_work);
#endif

/*
 * control_dfs_scaling: enable or disable dynamic frequency scaling.
 *
 * Argument:
 *		@enabled
 *			true: enable dynamic frequency scaling
 *			false: disable dynamic frequency scaling
 * Return:
 *		none
 */
void control_dfs_scaling(bool enabled)
{
	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return;

	pr_cpufreq(VERBOSE, "++ %s enable = %d\n", __func__, enabled);
	if (enabled)
		start_cpufreq();
	else
		stop_cpufreq();
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);
}
EXPORT_SYMBOL(control_dfs_scaling);

void disable_hotplug_duringPanic(void)
{
#if defined(DYNAMIC_HOTPLUG_CPU)
	the_cpuinfo.hpdata.hlg_enabled = 0;
#endif /* DYNAMIC_HOTPLUG_CPU */
}
EXPORT_SYMBOL(disable_hotplug_duringPanic);

#ifdef CONFIG_PM_DEBUG
/*
 * control_cpufreq: runtime enable/disable cpufreq features
 *
 * Argument:
 *		@is_enable: enable flag: enable(1)/disable(0)
 *
 * Return:
 *		0: normal
 *		negative: operation fail
 */
int control_cpufreq(int is_enable)
{
	int ret = 0;

	if (is_enable == cpufreq_enabled)
		return ret;

	if (!is_enable) {
		ret = stop_cpufreq();
		spin_lock(&the_cpuinfo.lock);
		cpufreq_enabled = 0;
		spin_unlock(&the_cpuinfo.lock);
	} else {
		spin_lock(&the_cpuinfo.lock);
		cpufreq_enabled = 1;
		spin_unlock(&the_cpuinfo.lock);
		start_cpufreq();
	}

	pr_cpufreq(VERBOSE, "cpufreq<%s>\n", (cpufreq_enabled) ? "on" : "off");
	return ret;
}
EXPORT_SYMBOL(control_cpufreq);

/*
 * is_cpufreq_enable: get runtime status
 *
 * Argument:
 *		None
 *
 * Return:
 *		0: disabled
 *		1: enabled
 */
int is_cpufreq_enable(void)
{
	return cpufreq_enabled;
}
EXPORT_SYMBOL(is_cpufreq_enable);
#endif /* CONFIG_PM_DEBUG */

#ifdef CONFIG_EARLYSUSPEND
/*
 * function: change clock state and set clocks, support for early suspend state
 * in system suspend.
 *
 * Argument:
 *		@h: the early_suspend interface
 *
 * Return:
 *		none
 */
#ifndef CPUFREQ_TEST_MODE
static
#endif /* CPUFREQ_TEST_MODE */
void shmobile_cpufreq_early_suspend(struct early_suspend *h)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return;

	pr_cpufreq(VERBOSE, "++ %s\n", __func__);
	spin_lock(&the_cpuinfo.lock);
	the_cpuinfo.clk_state = MODE_EARLY_SUSPEND;
	__set_sys_clocks(false);

	hpdata->history_samples = hpdata->pdata->hotplug_es_samples;
	hpdata->history_idx = 0;

	spin_unlock(&the_cpuinfo.lock);
	pr_cpufreq(VERBOSE, "-- %s\n", __func__);
}

/*
 * shmobile_cpufreq_late_resume: change clock state and set clocks, support for
 * late resume state in system suspend.
 *
 * Argument:
 *		@h: the early_suspend interface
 *
 * Return:
 *		none
 */
#ifndef CPUFREQ_TEST_MODE
static
#endif /* CPUFREQ_TEST_MODE */
void shmobile_cpufreq_late_resume(struct early_suspend *h)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (validate()) {
		pr_cpufreq(VERBOSE, "++ %s\n", __func__);
		spin_lock(&the_cpuinfo.lock);
		the_cpuinfo.clk_state = MODE_NORMAL;
		__set_sys_clocks(false);

		hpdata->history_samples = hpdata->pdata->hotplug_samples;
		hpdata->history_idx = 0;

		spin_unlock(&the_cpuinfo.lock);
		pr_cpufreq(VERBOSE, "-- %s\n", __func__);
	}
}

static struct early_suspend shmobile_cpufreq_suspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 50,
	.suspend = shmobile_cpufreq_early_suspend,
	.resume  = shmobile_cpufreq_late_resume,
};
#endif /* CONFIG_EARLYSUSPEND */

/*
 * shmobile_cpufreq_verify: verify the limit frequency of the policy with the
 * limit frequency of the CPU.
 *
 * Argument:
 *		@policy: the input policy
 *
 * Return:
 *		0: normal
 *		negative: operation fail
 */
#ifndef CPUFREQ_TEST_MODE
static
#endif /* CPUFREQ_TEST_MODE */
int shmobile_cpufreq_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}

/*
 * shmobile_cpufreq_getspeed: Retrieve the current frequency of a SYS-CPU.
 *
 * Argument:
 *		@cpu: the ID of CPU
 *
 * Return:
 *		the frequency value of input CPU.
 */
#ifndef CPUFREQ_TEST_MODE
static
#endif /* CPUFREQ_TEST_MODE */
unsigned int shmobile_cpufreq_getspeed(unsigned int cpu)
{
	return the_cpuinfo.freq;
}

/*
 * shmobile_cpufreq_target: judgle frequencies
 *
 * Argument:
 *		@policy: the policy
 *		@target_freq: the target frequency passed from CPUFreq framework
 *		@relation: not used
 *
 * Return:
 *		0: normal
 *		negative: operation fail
 */
#ifndef CPUFREQ_TEST_MODE
static
#endif /* CPUFREQ_TEST_MODE */
int shmobile_cpufreq_target(struct cpufreq_policy *policy,
	unsigned int target_freq, unsigned int relation)
{
	unsigned int old_freq = 0;
	unsigned int freq = ~0;
	int index = 0;
	int ret = 0;
	int cpu = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	spin_lock(&the_cpuinfo.lock);

	the_cpuinfo.req_rate[policy->cpu] = target_freq;
	/* only reduce the CPU frequency if all CPUs need to reduce */
	for_each_online_cpu(cpu)
		target_freq = max(the_cpuinfo.req_rate[cpu], target_freq);

	old_freq = the_cpuinfo.freq;
	ret = find_target(freq_table, target_freq, &index);
	if (ret)
		goto done;

	freq = freq_table[index].frequency;

	/* current frequency is set */
	if ((the_cpuinfo.freq == freq))
		goto done;

	ret = __set_all_clocks(freq);

/* block due to many log */
#if 0
	/* the_cpuinfo.freq == freq when frequency changed */
	if (the_cpuinfo.freq == freq)
		pr_info("[%07uKHz->%07uKHz]%s\n", old_freq, freq,
			(old_freq < freq) ? "^" : "v");
#endif

done:
	spin_unlock(&the_cpuinfo.lock);
	return ret;
}

/*
 * shmobile_cpufreq_init: initialize the DFS module.
 *
 * Argument:
 *		@policy: the policy will change the frequency.
 *
 * Return:
 *		0: normal initialization
 *		negative: operation fail
 */
#ifndef CPUFREQ_TEST_MODE
static
#endif /* CPUFREQ_TEST_MODE */
int shmobile_cpufreq_init(struct cpufreq_policy *policy)
{
	static unsigned int not_initialized = 1;
	int i = 0;
	int ret = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	if (!policy)
		return -EINVAL;

	/* init frequency table */
	freq_table = main_freqtbl_es2_x;

	/* for other governors which are used frequency table */
	cpufreq_frequency_table_get_attr(freq_table, policy->cpu);

	/* check whatever the driver has been not_initialized */
	if (!not_initialized) {
		/* the driver has already initialized. */
		ret = cpufreq_frequency_table_cpuinfo(policy, freq_table);
		if (ret)
			goto done;

		policy->cur = the_cpuinfo.freq;
		policy->governor = CPUFREQ_DEFAULT_GOVERNOR;
		policy->cpuinfo.transition_latency = FREQ_TRANSITION_LATENCY;
		/* policy sharing between dual CPUs */
		cpumask_copy(policy->cpus, cpu_online_mask);
		policy->shared_type = CPUFREQ_SHARED_TYPE_ALL;
		goto done;
	}

	spin_lock_init(&the_cpuinfo.lock);
#ifdef CONFIG_EARLYSUSPEND
	register_early_suspend(&shmobile_cpufreq_suspend);
#endif /* CONFIG_EARLYSUSPEND */
	the_cpuinfo.clk_state = MODE_NORMAL;
	the_cpuinfo.dfs_state = DFS_START;
	if (0 != zclk12_flg) {
		the_cpuinfo.freq_max = FREQ_MAX12;
		the_cpuinfo.freq_mid_upper_limit = FREQ_MID_UPPER_LIMIT12;
		the_cpuinfo.freq_mid_lower_limit = FREQ_MID_LOWER_LIMIT12;
		the_cpuinfo.freq_min_upper_limit = FREQ_MIN_UPPER_LIMIT12;
		the_cpuinfo.freq_min_lower_limit = FREQ_MIN_LOWER_LIMIT12;
		the_cpuinfo.freq_suspend = SUSPEND_CPUFREQ12;
	} else {
		the_cpuinfo.freq_max = FREQ_MAX15;
		the_cpuinfo.freq_mid_upper_limit = FREQ_MID_UPPER_LIMIT15;
		the_cpuinfo.freq_mid_lower_limit = FREQ_MID_LOWER_LIMIT15;
		the_cpuinfo.freq_min_upper_limit = FREQ_MIN_UPPER_LIMIT15;
		the_cpuinfo.freq_min_lower_limit = FREQ_MIN_LOWER_LIMIT15;
		the_cpuinfo.freq_suspend = SUSPEND_CPUFREQ15;
	}

	/*
	 * loader had set the frequency to MAX, already.
	 */
	the_cpuinfo.freq = pm_get_syscpu_frequency();
	ret = cpufreq_frequency_table_cpuinfo(policy, freq_table);
	if (ret)
		goto done;

	policy->cur = the_cpuinfo.freq;
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;
	policy->cpuinfo.transition_latency = FREQ_TRANSITION_LATENCY;
	/* policy sharing between dual CPUs */
	cpumask_copy(policy->cpus, cpu_online_mask);
	policy->shared_type = CPUFREQ_SHARED_TYPE_ALL;
	not_initialized--;

#ifdef DYNAMIC_HOTPLUG_CPU
	/*Hotplug workqueue */
	dfs_queue = alloc_workqueue("dfs_queue",
			WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_MEM_RECLAIM, 1);

#endif /* DYNAMIC_HOTPLUG_CPU */
	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
		pr_cpufreq(INIT, "[%2d]:%7u KHz", i, freq_table[i].frequency);
done:
	return ret;
}

static struct freq_attr *shmobile_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver shmobile_cpufreq_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= shmobile_cpufreq_verify,
	.target		= shmobile_cpufreq_target,
	.get		= shmobile_cpufreq_getspeed,
	.load		= shmobile_cpufreq_load_info,
	.init		= shmobile_cpufreq_init,
	.attr		= shmobile_cpufreq_attr,
	.name		= "shmobile"
};

static int shmobile_policy_changed_notifier(struct notifier_block *nb,
			unsigned long type, void *data)
{
	int i = 0, len = 0, enable_hlg = 0;
	struct cpufreq_policy *policy;
	static const char * const governors[] = {
		"conservative", "ondemand", "interactive"
	};

	if (CPUFREQ_GOVERNOR_CHANGE_NOTIFY != type)
		return 0;

	policy = (struct cpufreq_policy *)data;
	len = ARRAY_SIZE(governors);

	if (policy) {
		spin_lock(&the_cpuinfo.lock);

		/* governor switched? update hotplug status */
		for (i  = 0; i < len; i++) {
			if (strcmp(governors[i], policy->governor->name) == 0) {
				enable_hlg = 1;
				break;
			}
		}
		if (!enable_hlg) {
			the_cpuinfo.hpdata.hlg_enabled = 0;
			queue_work_on(0, dfs_queue, &hlg_work);
		} else
			the_cpuinfo.hpdata.hlg_enabled = 1;

		/* For changing sampling rate dynamically */
		if (0 == strcmp(policy->governor->name, "ondemand")) {
			if (STOP_STATE != sampling_flag)
				goto end;
			/* when governor is changed from non-ondemand */
			sampling_flag = INIT_STATE;
			spin_unlock(&the_cpuinfo.lock);
			__change_sampling_values();
			return 0;
		} else {
			sampling_flag = STOP_STATE;
		}
	}
end:
	spin_unlock(&the_cpuinfo.lock);
	return 0;
}
static struct notifier_block policy_notifier = {
	.notifier_call = shmobile_policy_changed_notifier,
};

static inline void shmobile_validate_thresholds(
			struct sh_plat_hp_data *hpdata)
{
	struct sh_hp_thresholds *tl = hpdata->thresholds;
	unsigned int min_freq = ~0;
	unsigned int max_freq = 0;
	int cpu, i;

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		unsigned int freq = freq_table[i].frequency;
		if (freq < min_freq)
			min_freq = freq;
		if (freq > max_freq)
			max_freq = freq;
	}

	/* validate hotplug thresholds for non-boot cpus */
	for_each_possible_cpu(cpu) {
		if (cpu == 0)
			continue;

		BUG_ON(tl[cpu].thresh_plugout > max_freq ||
			tl[cpu].thresh_plugout < min_freq ||
			tl[cpu].thresh_plugin > max_freq ||
			tl[cpu].thresh_plugin < min_freq);
	}
}


/*
 * rmobile_cpufreq_init: register the cpufreq driver with the cpufreq
 * governor driver.
 *
 * Arguments:
 *		none.
 *
 * Return:
 *		0: normal registration
 *		negative: operation fail
 */
static int __init cpufreq_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	int pll0_ratio = 0;
	int arr_size = 0;
	struct sh_cpufreq_plat_data *pdata = pdev->dev.platform_data;

	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;

#ifdef DVFS_DEBUG_MODE
	debug = 1;
#else  /* !DVFS_DEBUG_MODE */
	debug = 0;
#endif /* DVFS_DEBUG_MODE */
	/* build frequency table */

#ifdef CONFIG_CPUFREQ_OVERDRIVE
	zclk12_flg = 0;
#else
	zclk12_flg = 1;
#endif /* CONFIG_CPUFREQ_OVERDRIVE */

	if (!pdata) {
		pr_cpufreq(ERROR, "No platform init data supplied.\n");
		return -ENODEV;
	}

	if (!pdata->hp_data) {
		pr_cpufreq(ERROR, "No hotplug params supplied.\n");
		return -ENODEV;
	}

	if (0 != zclk12_flg) {
		pll0_ratio = PLLx46;
		zdiv_table = zdiv_table12;
		arr_size = (int)ARRAY_SIZE(zdiv_table12);
	} else {
		pll0_ratio = PLLx56;
		zdiv_table = zdiv_table15;
		arr_size = (int)ARRAY_SIZE(zdiv_table15);
	}

	if (pll0_ratio != pm_get_pll_ratio(PLL0)) {
		pr_cpufreq(INIT, "Try to set PLL0 = x%d...", pll0_ratio);
		pm_set_pll_ratio(PLL0, pll0_ratio);
	}
	pr_cpufreq(INIT, "----> PLL0 = x%d", pm_get_pll_ratio(PLL0));
	for (i = 0; i < arr_size; i++) {
		main_freqtbl_es2_x[i].index = i;
		main_freqtbl_es2_x[i].frequency =
		pll0_ratio * EXTAL1 * zdiv_table[i].waveform / 16;
	}
	main_freqtbl_es2_x[i].index = i;
	main_freqtbl_es2_x[i].frequency = CPUFREQ_TABLE_END;

	/* setup clocksuspend */
	ret = pm_setup_clock();
	if (ret)
		return ret;

	for (i = 0; i < DFS_REF_MAX; i++)
		atomic_set(&the_cpuinfo.dfs_ref[i], 0);

#ifdef DYNAMIC_HOTPLUG_CPU
	the_cpuinfo.hpdata.pdata = pdata->hp_data;
	the_cpuinfo.hpdata.history_samples = pdata->hp_data->hotplug_samples;
	the_cpuinfo.hpdata.load_history = kzalloc(
			sizeof(*the_cpuinfo.hpdata.load_history) *
			pdata->hp_data->hotplug_samples,
			GFP_KERNEL);
	if (!the_cpuinfo.hpdata.load_history)
		return -ENOMEM;

	the_cpuinfo.hpdata.history_idx = 0;
	the_cpuinfo.hpdata.hlg_enabled = 1;

	cpumask_set_cpu(0, &hpdata->locked_cpus);
#endif /* DYNAMIC_HOTPLUG_CPU */

	/* register cpufreq driver to cpufreq core */
	ret = cpufreq_register_driver(&shmobile_cpufreq_driver);
	if (ret)
		return ret;
	ret = cpufreq_register_notifier(&policy_notifier,
					CPUFREQ_POLICY_NOTIFIER);
#ifdef CONFIG_PM_BOOT_SYSFS /* Restrain DVFS at the time of start */
	/* stop cpufreq */
	ret = stop_cpufreq();
	if (ret)
		pr_cpufreq(ERROR, "%s : stop_cpufreq error!!(%d)\n",
					__func__, ret);
#endif

	shmobile_validate_thresholds(pdata->hp_data);
	ret = shmobile_sysfs_init();
	return ret;
}

static struct platform_driver cpufreq_driver = {
	.driver = {
		.name = "shmobile-cpufreq",
	},
};

static int __init cpufreq_drv_init(void)
{
	return platform_driver_probe(&cpufreq_driver, cpufreq_drv_probe);
}

module_init(cpufreq_drv_init);

/*
 * Append SYSFS interface for MAX/MIN frequency limitation control
 * path: /sys/power/{cpufreq_table, cpufreq_limit_max, cpufreq_limit_min}
 *	- cpufreq_table: listup all available frequencies
 *	- cpufreq_limit_max: maximum frequency current supported
 *	- cpufreq_limit_min: minimum frequency current supported
 */
static inline struct attribute *find_attr_by_name(struct attribute **attr,
				const char *name)
{
	/* lookup for an attibute(by name) from attribute list */
	while ((attr) && (*attr)) {
		if (strcmp(name, (*attr)->name) == 0)
			return *attr;
		attr++;
	}

	return NULL;
}
/*
 * show_available_freqs - show available frequencies
 * /sys/power/cpufreq_table
 */
static ssize_t show_available_freqs(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	unsigned int prev = 0;
	ssize_t count = 0;
	int i = 0;

	/* show all available freuencies */
	for (i = 0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (freq_table[i].frequency == prev)
			continue;
		count += sprintf(&buf[count], "%d ", freq_table[i].frequency);
		prev = freq_table[i].frequency;
	}
	count += sprintf(&buf[count], "\n");

	return count;
}
/* for cpufreq_limit_max & cpufreq_limit_min, create a shortcut from
 * default sysfs node (/sys/devices/system/cpu/cpu0/cpufreq/) so bellow
 * mapping table indicate the target name and alias which will be use
 * for creating shortcut.
 */
static struct {
	const char *target;
	const char *alias;
} attr_mapping[] = {
	{"scaling_max_freq", "cpufreq_max_limit"},
	{"scaling_min_freq", "cpufreq_min_limit"}
};

/*
 * cpufreq_max_limit/cpufreq_min_limit - show max/min frequencies
 * this handler is used for both MAX/MIN limit
 */
static ssize_t show_freq(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	ssize_t ret = -EINVAL;
	struct cpufreq_policy cur_policy;
	struct kobj_type *ktype = NULL;
	struct attribute *att = NULL;
	int i = 0;

	ret = cpufreq_get_policy(&cur_policy, 0);
	if (ret)
		return -EINVAL;

	ktype = get_ktype(&cur_policy.kobj);
	if (!ktype)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(attr_mapping); i++) {
		if (strcmp(attr->attr.name, attr_mapping[i].alias) == 0) {
			att = find_attr_by_name(ktype->default_attrs,
				attr_mapping[i].target);

			/* found the attibute, pass message to its ops */
			if (att && ktype->sysfs_ops && ktype->sysfs_ops->show)
					return ktype->sysfs_ops->show(
					&cur_policy.kobj, att, buf);
		}
	}

	return -EINVAL;
}

/*
 * cpufreq_max_limit/cpufreq_min_limit - store max/min frequencies
 * this handler is used for both MAX/MIN limit
 */
static ssize_t store_freq(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct cpufreq_policy *cur_policy = NULL;
	struct cpufreq_policy data;
	unsigned int min_freq = 0;
	unsigned int max_freq = 0;
	int freq = 0;
	int index = -1;
	int att_id = 0;
	ssize_t ret = -EINVAL;

	spin_lock(&the_cpuinfo.lock);
	if (strcmp(attr->attr.name, "cpufreq_max_limit") == 0)
		att_id = 1;
	else if (strcmp(attr->attr.name, "cpufreq_min_limit") == 0)
		att_id = 0;
	else
		goto end;

	cur_policy = cpufreq_cpu_get(0);
	if (!cur_policy)
		goto end;

	memcpy(&data, cur_policy, sizeof(struct cpufreq_policy));
	if (sscanf(buf, "%d", &freq) != 1)
		goto end;
	ret = cpufreq_frequency_table_cpuinfo(&data, freq_table);
	if (ret)
		goto end;

	if (att_id) {
		/*
		 * if input == -1 then restore original setting
		 * else apply new setting
		 */
		if (freq < 0) /* Restore to original value */
			freq = data.max;
		/* Max limit: need lower value compare with
		 * new one then CPUFREQ_RELATION_H will be used
		 */
		ret = cpufreq_frequency_table_target(&data, freq_table,
			freq, CPUFREQ_RELATION_H, &index);

		the_cpuinfo.dfs_save.max=freq;

		if (ret)
			goto end;
		max_freq = freq_table[index].frequency;
		if (cur_policy->min > max_freq)
			min_freq = max_freq;
	} else {
		if (freq < 0) /* Restore to original value */
		{
			freq = data.min;
			cpufreq_set_cpu_min_limit_withoutlock(1);
		}
		else
			cpufreq_set_cpu_min_limit_withoutlock(2);
		/* Min limit: need upper value compare with
		 * new one then CPUFREQ_RELATION_L will be used
		 */
		ret = cpufreq_frequency_table_target(&data, freq_table, freq,
			CPUFREQ_RELATION_L, &index);

		the_cpuinfo.dfs_save.min=freq;

		if (ret)
			goto end;
		/* If min_freq > max -> min_freq = max */
		min_freq = min(freq_table[index].frequency, cur_policy->max);
	}

end:
	spin_unlock(&the_cpuinfo.lock);
	cpufreq_cpu_put(cur_policy);

	/* Update the MIN - MAX limitations */
	/* Firstly, update MIN limitation if any */
	if (min_freq > 0) {
		cur_policy->user_policy.min = min_freq;
		the_cpuinfo.dfs_save.min=min_freq;
		ret = cpufreq_update_policy(0);
	}
	/* Then, update MAX limitation if any */
	if (max_freq > 0) {
		cur_policy->user_policy.max = max_freq;
		the_cpuinfo.dfs_save.max=max_freq;
		ret = cpufreq_update_policy(0);
	}
	pr_cpufreq(VERBOSE, "%s(): update min/max freq: ret<%d>",
				__func__, ret);
	return ret ? ret : count;
}

static ssize_t show_cur_freq(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", the_cpuinfo.freq);
}

/*
 * cpufreq_max_limit/cpufreq_min_limit - store max/min frequencies
 * this handler is used for both MAX/MIN limit
 */
static ssize_t store_cpu_num_min_limit(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int min_cpu_limit;
	ssize_t ret = 0;
	printk(" ++ store_cpu_num_min_limit \n");
	unsigned int nr_cpus = num_possible_cpus();
	if (sscanf(buf, "%d", &min_cpu_limit) != 1)
		return -EINVAL;

	if (!min_cpu_limit || min_cpu_limit > nr_cpus)
		return -EINVAL;

	cpufreq_set_cpu_min_limit(min_cpu_limit);

	printk(" -- store_cpu_num_min_limit \n");
	return count;
}

static ssize_t show_cpu_num_min_limit(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cpufreq_get_cpu_min_limit());
}

static ssize_t show_cpucore_table(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "2 1\n");
}

static struct kobj_attribute cpufreq_table_attribute =
	__ATTR(cpufreq_table, 0444, show_available_freqs, NULL);
static struct kobj_attribute cur_freq_attribute =
	__ATTR(cpufreq_cur_freq, 0444, show_cur_freq, NULL);
static struct kobj_attribute max_limit_attribute =
	__ATTR(cpufreq_max_limit, 0644, show_freq, store_freq);
static struct kobj_attribute min_limit_attribute =
	__ATTR(cpufreq_min_limit, 0644, show_freq, store_freq);
/* 
static struct kobj_attribute cpu_num_min_limit_attribute =
	__ATTR(cpu_num_min_limit, 0644, show_cpu_num_min_limit, store_cpu_num_min_limit);
static struct kobj_attribute cpucore_table_attribute =
	__ATTR(cpucore_table, 0444, show_cpucore_table, NULL);
*/

/*
 * Create a group of attributes so that can create and destroy them all
 * at once.
 */
static struct attribute *attrs[] = {
	&min_limit_attribute.attr,
	&max_limit_attribute.attr,
	&cpufreq_table_attribute.attr,
	&cur_freq_attribute.attr,
/*
	&cpu_num_min_limit_attribute.attr,
	&cpucore_table_attribute.attr,
*/
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

#ifdef CONFIG_DEBUG_FS
static int debugfs_hp_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debugfs_hp_thresholds_show(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;
	struct sh_plat_hp_data *hpplat = hpdata->pdata;
	char out_buf[500];
	u32 len = 0;
	int idx;

	memset(out_buf, 0, sizeof(out_buf));
	len += snprintf(out_buf + len, sizeof(out_buf) - len,
				"cpu\t\tplugout\t\tplugin\n");

	for_each_possible_cpu(idx)
		len += snprintf(out_buf + len, sizeof(out_buf) - len,
			"cpu%u\t\t%u\t\t%u\n", idx,
			hpplat->thresholds[idx].thresh_plugout,
			hpplat->thresholds[idx].thresh_plugin);

	return simple_read_from_buffer(user_buf, count, ppos,
			out_buf, len);
}

static ssize_t debugfs_hp_thresholds_store(struct file *file,
			char const __user *buf, size_t count, loff_t *offset)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;
	struct sh_plat_hp_data *hpplat = hpdata->pdata;
	struct cpufreq_policy *cur_policy;
	char in_buf[100];
	u32 cpu, thresh_plugout, thresh_plugin;
	int ret = count;

	memset(in_buf, 0, sizeof(in_buf));

	if (count > sizeof(in_buf) - 1)
		return -EFAULT;

	if (copy_from_user(in_buf, buf, count))
		return -EFAULT;

	cur_policy = cpufreq_cpu_get(0);
	if (!cur_policy)
		return -EINVAL;

	sscanf(in_buf, "%u%u%u", &cpu, &thresh_plugout, &thresh_plugin);

	if (!cpu || !cpu_possible(cpu)) {
		ret = -EINVAL;
		goto out;
	}

	if (thresh_plugout > cur_policy->cpuinfo.max_freq ||
			thresh_plugout < cur_policy->cpuinfo.min_freq ||
			thresh_plugin > cur_policy->cpuinfo.max_freq ||
			thresh_plugin < cur_policy->cpuinfo.min_freq) {
		ret = -EINVAL;
		goto out;
	}

	spin_lock(&the_cpuinfo.lock);
	hpplat->thresholds[cpu].thresh_plugout = thresh_plugout;
	hpplat->thresholds[cpu].thresh_plugin = thresh_plugin;
	spin_unlock(&the_cpuinfo.lock);

out:
	cpufreq_cpu_put(cur_policy);
	return ret;
}

static const struct file_operations debugfs_hp_thresholds = {
	.open = debugfs_hp_open,
	.write = debugfs_hp_thresholds_store,
	.read = debugfs_hp_thresholds_show,
};

static ssize_t debugfs_hp_samples_show(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;
	struct sh_plat_hp_data *hpplat = hpdata->pdata;
	char out_buf[10];
	u32 len = 0;

	memset(out_buf, 0, sizeof(out_buf));
	len += snprintf(out_buf + len, sizeof(out_buf) - len,
			"%d\n", hpplat->hotplug_samples);

	return simple_read_from_buffer(user_buf, count, ppos,
			out_buf, len);
}

static ssize_t debugfs_hp_samples_store(struct file *file,
			char const __user *buf, size_t count, loff_t *offset)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;
	struct sh_plat_hp_data *hpplat = hpdata->pdata;
	unsigned int new_sample, *new_history;
	char in_buf[10];
	int ret = count;

	memset(in_buf, 0, sizeof(in_buf));
	if (copy_from_user(in_buf, buf, count))
		return -EFAULT;

	sscanf(in_buf, "%d", &new_sample);
	spin_lock(&the_cpuinfo.lock);
	if (new_sample <= hpplat->hotplug_samples) {
		/* existing array is sufficient, update count and index */
		if (hpdata->history_idx >= new_sample)
			hpdata->history_idx = new_sample - 1;

		hpplat->hotplug_samples = new_sample;
		hpdata->history_samples = new_sample;
		goto unlock;
	}

	new_history = kzalloc(sizeof(*hpdata->load_history) * new_sample,
					GFP_KERNEL);
	if (!new_history) {
		ret = -ENOMEM;
		goto unlock;
	}

	/* copy the existing buffer */
	memcpy(new_history, hpdata->load_history,
		sizeof(*hpdata->load_history) * hpplat->hotplug_samples);
	kfree(hpdata->load_history);

	hpdata->load_history = new_history;
	hpplat->hotplug_samples = new_sample;
	hpdata->history_samples = new_sample;

unlock:
	spin_unlock(&the_cpuinfo.lock);
	return ret;
}

static const struct file_operations debugfs_hp_samples = {
	.open = debugfs_hp_open,
	.write = debugfs_hp_samples_store,
	.read = debugfs_hp_samples_show,
};

static ssize_t debugfs_hp_es_samples_show(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;
	struct sh_plat_hp_data *hpplat = hpdata->pdata;
	char out_buf[10];
	u32 len = 0;

	memset(out_buf, 0, sizeof(out_buf));
	len += snprintf(out_buf + len, sizeof(out_buf) - len,
			"%d\n", hpplat->hotplug_es_samples);

	return simple_read_from_buffer(user_buf, count, ppos,
			out_buf, len);
}

static ssize_t debugfs_hp_es_samples_store(struct file *file,
			char const __user *buf, size_t count, loff_t *offset)
{
	struct shmobile_hp_data *hpdata = &the_cpuinfo.hpdata;
	struct sh_plat_hp_data *hpplat = hpdata->pdata;
	unsigned int new_sample;
	char in_buf[10];
	int ret = count;

	memset(in_buf, 0, sizeof(in_buf));
	if (copy_from_user(in_buf, buf, count))
		return -EFAULT;

	sscanf(in_buf, "%d", &new_sample);
	spin_lock(&the_cpuinfo.lock);

	if (new_sample > hpplat->hotplug_samples) {
		ret = -EINVAL;
		goto unlock;
	}

	hpplat->hotplug_es_samples = new_sample;

unlock:
	spin_unlock(&the_cpuinfo.lock);

	return ret;
}

static const struct file_operations debugfs_hp_es_samples = {
	.open = debugfs_hp_open,
	.write = debugfs_hp_es_samples_store,
	.read = debugfs_hp_es_samples_show,
};

static struct dentry *debug_dir;

static int cpufreq_debug_init(void)
{
	struct dentry *debug_file;
	int ret = 0;

	debug_dir = debugfs_create_dir("sh_cpufreq", NULL);
	if (IS_ERR_OR_NULL(debug_dir)) {
		ret = PTR_ERR(debug_dir);
		goto err;
	}

	debug_file = debugfs_create_file("hp_samples", S_IWUSR,
		debug_dir, NULL, &debugfs_hp_samples);
	if (IS_ERR_OR_NULL(debug_file)) {
		ret = PTR_ERR(debug_file);
		goto err;
	}

	debug_file = debugfs_create_file("hp_es_samples", S_IWUSR,
		debug_dir, NULL, &debugfs_hp_es_samples);
	if (IS_ERR_OR_NULL(debug_file)) {
		ret = PTR_ERR(debug_file);
		goto err;
	}

	debug_file = debugfs_create_file("hp_thresholds", S_IWUSR,
			debug_dir, NULL, &debugfs_hp_thresholds);
	if (IS_ERR_OR_NULL(debug_file)) {
		ret = PTR_ERR(debug_file);
		goto err;
	}

	debug_file = debugfs_create_u32("hp_debug_mask", S_IWUSR | S_IRUGO,
			debug_dir, &hp_debug_mask);
	if (IS_ERR_OR_NULL(debug_file)) {
		ret = PTR_ERR(debug_file);
		goto err;
	}

	return 0;
err:
	if (!IS_ERR_OR_NULL(debug_dir))
		debugfs_remove_recursive(debug_dir);

	return ret;
}

#else /* CONFIG_DEBUG_FS */

static int cpufreq_debug_init(void)
{
	return 0;
}

#endif /* CONFIG_DEBUG_FS */

static int shmobile_sysfs_init(void)
{
	int ret;
	/* Create the files associated with power kobject */
	ret = sysfs_create_group(power_kobj, &attr_group);
	if (ret) {
		pr_cpufreq(ERROR, "failed to create sysfs\n");
		return ret;
	}

	ret = cpufreq_debug_init();
	if (ret)
		pr_cpufreq(ERROR, "failed to create debugfs\n");

	return ret;
}

