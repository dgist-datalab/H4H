/*
The MIT License (MIT)

Copyright (c) 2014-2015 CSAIL, MIT

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#if defined(KERNEL_MODE)
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>

#elif defined(USER_MODE)
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include "debug.h"
#include "params.h"
#include "h4h_drv.h"
#include "umemory.h"
#include "pmu.h"
#include "utime.h"


#ifdef USE_PMU
void pmu_create (h4h_drv_info_t* bdi)
{
	uint64_t i, punit;
	h4h_device_params_t* np = H4H_GET_DEVICE_PARAMS(bdi);

	h4h_spin_lock_init (&bdi->pm.pmu_lock);

	/* # of I/O operations */
	/*atomic64_set (&bdi->pm.exetime_us, time_get_timestamp_in_us ());*/
	h4h_stopwatch_start (&bdi->pm.exetime);
	atomic64_set (&bdi->pm.page_read_cnt, 0);
	atomic64_set (&bdi->pm.page_write_cnt, 0);
	atomic64_set (&bdi->pm.rmw_read_cnt, 0);
	atomic64_set (&bdi->pm.rmw_write_cnt, 0);
	atomic64_set (&bdi->pm.gc_cnt, 0);
	atomic64_set (&bdi->pm.gc_erase_cnt, 0);
	atomic64_set (&bdi->pm.gc_read_cnt, 0);
	atomic64_set (&bdi->pm.gc_write_cnt, 0);

	/* elapsed times taken to handle normal I/Os */
	bdi->pm.time_r_sw = 0;
	bdi->pm.time_r_q = 0;
	bdi->pm.time_r_tot = 0;

	bdi->pm.time_w_sw = 0;
	bdi->pm.time_w_q = 0;
	bdi->pm.time_w_tot = 0;

	bdi->pm.time_rmw_sw = 0;
	bdi->pm.time_rmw_q = 0;
	bdi->pm.time_rmw_tot = 0;

	/* elapsed times taken to handle gc I/Os */
	bdi->pm.time_gc_sw = 0;
	bdi->pm.time_gc_q = 0;
	bdi->pm.time_gc_tot = 0;

	/* channel / chip utilization */
	punit = np->nr_chips_per_channel * np->nr_channels;
	bdi->pm.util_r = h4h_malloc_atomic (punit * sizeof (atomic64_t));
	if (bdi->pm.util_r)  {
		for (i = 0; i < punit; i++) 
			atomic64_set (&bdi->pm.util_r[i], 0);
	}

	bdi->pm.util_w = h4h_malloc_atomic (punit * sizeof (atomic64_t));
	if (bdi->pm.util_w) {
		for (i = 0; i < punit; i++) 
			atomic64_set (&bdi->pm.util_w[i], 0);
	}
}

void pmu_destory (h4h_drv_info_t* bdi)
{
	if (bdi->pm.util_r)
		h4h_free_atomic (bdi->pm.util_r);
	if (bdi->pm.util_w)
		h4h_free_atomic (bdi->pm.util_w);
}

/* 
 * increase the number of I/O operations according to their types 
 */
void pmu_inc (h4h_drv_info_t* bdi, h4h_llm_req_t* llm_req)
{
#ifdef OLD_HLM
	uint64_t pid = llm_req->phyaddr->punit_id;
#else
	uint64_t pid = llm_req->phyaddr.punit_id;
#endif

	switch (llm_req->req_type) {
	case REQTYPE_READ:
		pmu_inc_read (bdi);
		pmu_inc_util_r (bdi, pid);
		break;
	case REQTYPE_WRITE:
		pmu_inc_write (bdi);
		pmu_inc_util_w (bdi, pid);
		break;
	case REQTYPE_RMW_READ:
		pmu_inc_rmw_read (bdi);
		pmu_inc_util_r (bdi, pid);
		break;
	case REQTYPE_RMW_WRITE:
		pmu_inc_rmw_write (bdi);
		pmu_inc_util_w (bdi, pid);
		break;
	case REQTYPE_GC_READ:
		pmu_inc_gc_read (bdi);
		pmu_inc_util_r (bdi, pid);
		break;
	case REQTYPE_GC_WRITE:
		pmu_inc_gc_write (bdi);
		pmu_inc_util_w (bdi, pid);
		break;
	case REQTYPE_GC_ERASE:
		pmu_inc_gc_erase (bdi);
		break;
	case REQTYPE_META_READ:
		pmu_inc_meta_read (bdi);
		pmu_inc_util_r (bdi, pid);
		break;
	case REQTYPE_META_WRITE:
		pmu_inc_meta_write (bdi);
		pmu_inc_util_w (bdi, pid);
		break;
	default:
		break;
	}
}

void pmu_inc_read (h4h_drv_info_t* bdi) 
{
	atomic64_inc (&bdi->pm.page_read_cnt);
}

void pmu_inc_write (h4h_drv_info_t* bdi) 
{
	atomic64_inc (&bdi->pm.page_write_cnt);
}

void pmu_inc_rmw_read (h4h_drv_info_t* bdi) 
{
	atomic64_inc (&bdi->pm.rmw_read_cnt);
}

void pmu_inc_rmw_write (h4h_drv_info_t* bdi) 
{
	atomic64_inc (&bdi->pm.rmw_write_cnt);
}

void pmu_inc_gc (h4h_drv_info_t* bdi)
{
	atomic64_inc (&bdi->pm.gc_cnt);
}

void pmu_inc_gc_erase (h4h_drv_info_t* bdi)
{
	atomic64_inc (&bdi->pm.gc_erase_cnt);
}

void pmu_inc_gc_read (h4h_drv_info_t* bdi)
{
	atomic64_inc (&bdi->pm.gc_read_cnt);
}

void pmu_inc_gc_write (h4h_drv_info_t* bdi)
{
	atomic64_inc (&bdi->pm.gc_write_cnt);
}

void pmu_inc_util_r (h4h_drv_info_t* bdi, uint64_t id)
{
	atomic64_inc (&bdi->pm.util_r[id]);
}

void pmu_inc_util_w (h4h_drv_info_t* bdi, uint64_t id)
{
	atomic64_inc (&bdi->pm.util_w[id]);
}

void pmu_inc_meta_read (h4h_drv_info_t* bdi)
{
	atomic64_inc (&bdi->pm.meta_read_cnt);
}

void pmu_inc_meta_write (h4h_drv_info_t* bdi)
{
	atomic64_inc (&bdi->pm.meta_write_cnt);
}

/* update the time taken to run sw algorithms */
void pmu_update_sw (h4h_drv_info_t* bdi, h4h_llm_req_t* req) 
{
	h4h_hlm_req_t* h = (h4h_hlm_req_t*)req->ptr_hlm_req;

	/*return;*/

	switch (req->req_type) {
	case REQTYPE_READ:
		h4h_bug_on (h == NULL);
		pmu_update_r_sw (bdi, &h->sw);
		break;
	case REQTYPE_WRITE:
		h4h_bug_on (h == NULL);
		pmu_update_w_sw (bdi, &h->sw);
		break;
	case REQTYPE_RMW_READ:
		h4h_bug_on (h == NULL);
		pmu_update_rmw_sw (bdi, &h->sw);
		break;
	case REQTYPE_META_READ:
		break;
	case REQTYPE_META_WRITE:
		break;
	}
}

void pmu_update_r_sw (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) 
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.page_read_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_r_sw = (bdi->pm.time_r_sw * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}

void pmu_update_w_sw (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) 
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.page_write_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_w_sw = (bdi->pm.time_w_sw * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}

void pmu_update_rmw_sw (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw)
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.rmw_read_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_rmw_sw = (bdi->pm.time_rmw_sw * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}

void pmu_update_gc_sw (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) 
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.gc_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_gc_sw = (bdi->pm.time_gc_sw * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}


/* 
 * update the time taken to stay in the queue 
 **/
void pmu_update_q (h4h_drv_info_t* bdi, h4h_llm_req_t* req)
{
	h4h_hlm_req_t* h = (h4h_hlm_req_t*)req->ptr_hlm_req;

	/*return;*/

	switch (req->req_type) {
	case REQTYPE_READ:
		h4h_bug_on (h == NULL);
		pmu_update_r_q (bdi, &h->sw);
		break;
	case REQTYPE_WRITE:
		h4h_bug_on (h == NULL);
		pmu_update_w_q (bdi, &h->sw);
		break;
	case REQTYPE_RMW_READ:
		h4h_bug_on (h == NULL);
		pmu_update_rmw_q (bdi, &h->sw);
		break;
	case REQTYPE_META_READ:
		break;
	case REQTYPE_META_WRITE:
		break;
	}
}

void pmu_update_r_q (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) 
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.page_read_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_r_q = (bdi->pm.time_r_q * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}

void pmu_update_w_q (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) 
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.page_write_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_w_q = (bdi->pm.time_w_q * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}

void pmu_update_rmw_q (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw)
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.rmw_read_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_rmw_q = (bdi->pm.time_rmw_q * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}

void pmu_update_gc_q (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw)
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.gc_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_gc_q = (bdi->pm.time_gc_q * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}


/* 
 * update the time taken for NAND devices to handle reqs 
 **/
void pmu_update_tot (h4h_drv_info_t* bdi, h4h_llm_req_t* req)
{
	h4h_hlm_req_t* h = (h4h_hlm_req_t*)req->ptr_hlm_req;

	/*return;*/

	switch (req->req_type) {
	case REQTYPE_READ:
		h4h_bug_on (h == NULL);
		pmu_update_r_tot (bdi, &h->sw);
		break;
	case REQTYPE_WRITE:
		h4h_bug_on (h == NULL);
		pmu_update_w_tot (bdi, &h->sw);
		break;
	case REQTYPE_RMW_READ:
		h4h_bug_on (h == NULL);
		pmu_update_rmw_tot (bdi, &h->sw);
		break;
	case REQTYPE_META_READ:
		break;
	case REQTYPE_META_WRITE:
		break;
	}
}

void pmu_update_r_tot (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) 
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.page_read_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_r_tot = (bdi->pm.time_r_tot * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}

void pmu_update_w_tot (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) 
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.page_write_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_w_tot = (bdi->pm.time_w_tot * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}

void pmu_update_rmw_tot (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw)
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.rmw_read_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_rmw_tot = (bdi->pm.time_rmw_q * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}

void pmu_update_gc_tot (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw)
{
	unsigned long flags;
	int64_t delta = h4h_stopwatch_get_elapsed_time_us (sw);
	int64_t n = atomic64_read (&bdi->pm.gc_cnt);
	h4h_spin_lock_irqsave (&bdi->pm.pmu_lock, flags);
	bdi->pm.time_gc_tot = (bdi->pm.time_gc_tot * n + delta) / (n + 1);
	h4h_spin_unlock_irqrestore (&bdi->pm.pmu_lock, flags);
}


/* display performance results */
char format[1024];
char str[1024];

void pmu_display (h4h_drv_info_t* bdi) 
{
	uint64_t i, j;
	struct timeval exetime;
	h4h_device_params_t* np = H4H_GET_DEVICE_PARAMS(bdi);

	h4h_msg ("-----------------------------------------------");
	h4h_msg ("< PERFORMANCE SUMMARY >");

	exetime = h4h_stopwatch_get_elapsed_time (&bdi->pm.exetime);
	h4h_msg ("[0] Execution Time (us): %ld.%ld", 
		exetime.tv_sec, exetime.tv_usec);
	h4h_msg ("");

	h4h_msg ("[1] Total I/Os");
	h4h_msg ("# of page reads: %ld", 
		atomic64_read (&bdi->pm.page_read_cnt) + 
		atomic64_read (&bdi->pm.rmw_read_cnt) + 
		atomic64_read (&bdi->pm.gc_read_cnt) + 
		atomic64_read (&bdi->pm.meta_read_cnt)
	);
	h4h_msg ("# of page writes: %ld", 
		atomic64_read (&bdi->pm.page_write_cnt) +
		atomic64_read (&bdi->pm.rmw_write_cnt) +
		atomic64_read (&bdi->pm.gc_write_cnt) +
		atomic64_read (&bdi->pm.meta_write_cnt)
	);

	h4h_msg ("# of block erase: %ld", 
		atomic64_read (&bdi->pm.gc_erase_cnt));
	h4h_msg ("");

	h4h_msg ("[2] Normal I/Os");
	h4h_msg ("# of page reads: %ld", 
		atomic64_read (&bdi->pm.page_read_cnt));
	h4h_msg ("# of page writes: %ld", 
		atomic64_read (&bdi->pm.page_write_cnt));
	h4h_msg ("# of page rmw reads: %ld", 
		atomic64_read (&bdi->pm.rmw_read_cnt));
	h4h_msg ("# of page rmw writes: %ld", 
		atomic64_read (&bdi->pm.rmw_write_cnt));
	h4h_msg ("");


	h4h_msg ("[3] GC I/Os");
	h4h_msg ("# of GC invocation: %ld",
		atomic64_read (&bdi->pm.gc_cnt));
	h4h_msg ("# of page reads: %ld",
		atomic64_read (&bdi->pm.gc_read_cnt));
	h4h_msg ("# of page writes: %ld",
		atomic64_read (&bdi->pm.gc_write_cnt));
	h4h_msg ("# of block erase: %ld", 
		atomic64_read (&bdi->pm.gc_erase_cnt));
	h4h_msg ("");

	h4h_msg ("[4] Meta I/Os");
	h4h_msg ("# of meta page reads: %ld",
		atomic64_read (&bdi->pm.meta_read_cnt));
	h4h_msg ("# of meta page writes: %ld",
		atomic64_read (&bdi->pm.meta_write_cnt));
	h4h_msg ("");

	h4h_msg ("[5] Elapsed Time");
	h4h_msg ("page read (us): %llu (S:%llu + Q:%llu + D:%llu)",
		bdi->pm.time_r_tot, 
		bdi->pm.time_r_sw,
		bdi->pm.time_r_q - bdi->pm.time_r_sw,
		bdi->pm.time_r_tot - bdi->pm.time_r_q);
	h4h_msg ("page write (us): %llu (S:%llu + Q:%llu + D:%llu)",
		bdi->pm.time_w_tot, 
		bdi->pm.time_w_sw,
		bdi->pm.time_w_q - bdi->pm.time_w_sw,
		bdi->pm.time_w_tot - bdi->pm.time_w_q);
	h4h_msg ("rmw (us): %llu (S:%llu + Q:%llu + D:%llu)",
		bdi->pm.time_rmw_tot, 
		bdi->pm.time_rmw_sw,
		bdi->pm.time_rmw_q - bdi->pm.time_rmw_sw,
		bdi->pm.time_rmw_tot - bdi->pm.time_rmw_q);
	h4h_msg ("");

	h4h_msg ("[6] Utilization (R)");
	for (i = 0; i < np->nr_chips_per_channel; i++) {
		for (j = 0; j < np->nr_channels; j++) {
			sprintf (str, "% 8ld ", atomic64_read (&bdi->pm.util_r[j*np->nr_chips_per_channel+i]));
			strcat (format, str);
		}
		h4h_msg ("%s", format);
		h4h_memset (format, 0x00, sizeof (format));
	}
	h4h_msg ("");

	h4h_msg ("[7] Utilization (W)");
	for (i = 0; i < np->nr_chips_per_channel; i++) {
		for (j = 0; j < np->nr_channels; j++) {
			sprintf (str, "% 8ld ", atomic64_read (&bdi->pm.util_w[j*np->nr_chips_per_channel+i]));
			strcat (format, str);
		}
		h4h_msg ("%s", format);
		h4h_memset (format, 0x00, sizeof (format));
	}

	h4h_msg ("-----------------------------------------------");
	h4h_msg ("-----------------------------------------------");
}

#else

void pmu_create (h4h_drv_info_t* bdi) {}
void pmu_destroy (h4h_drv_info_t* bdi) {}
void pmu_display (h4h_drv_info_t* bdi) {}

void pmu_inc (h4h_drv_info_t* bdi, h4h_llm_req_t* llm_req) {}
void pmu_inc_read (h4h_drv_info_t* bdi) {}
void pmu_inc_write (h4h_drv_info_t* bdi) {}
void pmu_inc_rmw_read (h4h_drv_info_t* bdi) {}
void pmu_inc_rmw_write (h4h_drv_info_t* bdi) {}
void pmu_inc_gc (h4h_drv_info_t* bdi) {}
void pmu_inc_gc_erase (h4h_drv_info_t* bdi) {}
void pmu_inc_gc_read (h4h_drv_info_t* bdi) {}
void pmu_inc_gc_write (h4h_drv_info_t* bdi) {}
void pmu_inc_util_r (h4h_drv_info_t* bdi, uint64_t id) {}
void pmu_inc_util_w (h4h_drv_info_t* bdi, uint64_t id) {}
void pmu_inc_meta_read (h4h_drv_info_t* bdi) {}
void pmu_inc_meta_write (h4h_drv_info_t* bdi) {}

void pmu_update_sw (h4h_drv_info_t* bdi, h4h_llm_req_t* req) {}
void pmu_update_r_sw (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}
void pmu_update_w_sw (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}
void pmu_update_rmw_sw (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}
void pmu_update_gc_sw (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}

void pmu_update_q (h4h_drv_info_t* bdi, h4h_llm_req_t* req) {}
void pmu_update_r_q (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}
void pmu_update_w_q (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}
void pmu_update_rmw_q (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}

void pmu_update_tot (h4h_drv_info_t* bdi, h4h_llm_req_t* req) {}
void pmu_update_r_tot (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}
void pmu_update_w_tot (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}
void pmu_update_rmw_tot (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}
void pmu_update_gc_tot (h4h_drv_info_t* bdi, h4h_stopwatch_t* sw) {}

#endif
