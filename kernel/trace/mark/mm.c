/**
 * COPYRIGHT (C)  SAMSUNG Electronics CO., LTD (Suwon, Korea). 2009
 * All rights are reserved. Reproduction and redistiribution in whole or
 * in part is prohibited without the written consent of the copyright owner.
 */

/**
 * Trace mm
 *
 * @autorh kumhyun.cho@samsung.com
 * @since 2014.02.14
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <trace/mark.h>
#include <trace/mark_base.h>

#define K(x) ((x) << (PAGE_SHIFT - 10))

static const char rss_stat_str[NR_MM_COUNTERS][16] = {
	"file", "anon", "swap",
};

void trace_mark_mm_rss_stat(struct mm_struct* mm, int member)
{
	long val;

	if (!trace_mark_enabled()) return;

	val = get_mm_counter(mm, member);

	trace_mark_int(current->pid, rss_stat_str[member], K(val), "");

	switch (member) {
		case MM_FILEPAGES:
			trace_mark_int(current->pid, "rss", K(val + get_mm_counter(mm, MM_ANONPAGES)), "");
			break;
		case MM_ANONPAGES:
			trace_mark_int(current->pid, "rss", K(val + get_mm_counter(mm, MM_FILEPAGES)), "");
			break;
	}
}
