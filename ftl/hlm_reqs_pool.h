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

#ifndef _H4H_HLM_REQ_POOL_H
#define _H4H_HLM_REQ_POOL_H

typedef struct {
	h4h_spinlock_t lock;
	struct list_head used_list;
	struct list_head free_list;
	int32_t pool_size; 	/* # of items */
	int32_t map_unit;	/* bytes */
	int32_t io_unit;	/* bytes */
	int8_t in_place_rmw; /* if it is set (1), the FTL uses in-place-rmw */
} h4h_hlm_reqs_pool_t;

h4h_hlm_reqs_pool_t* h4h_hlm_reqs_pool_create (int32_t mapping_unit_size, int32_t io_unit_size);
void h4h_hlm_reqs_pool_destroy (h4h_hlm_reqs_pool_t* pool);
h4h_hlm_req_t* h4h_hlm_reqs_pool_get_item (h4h_hlm_reqs_pool_t* pool);
void h4h_hlm_reqs_pool_free_item (h4h_hlm_reqs_pool_t* pool, h4h_hlm_req_t* req);
int h4h_hlm_reqs_pool_build_req (h4h_hlm_reqs_pool_t* pool, h4h_hlm_req_t* hr, h4h_blkio_req_t* br);

typedef enum {
	RP_MEM_VIRT = 0,
	RP_MEM_PHY = 1,
} h4h_rp_mem;

void hlm_reqs_pool_allocate_llm_reqs (h4h_llm_req_t* llm_reqs, int32_t nr_llm_reqs, h4h_rp_mem flag);
void hlm_reqs_pool_release_llm_reqs (h4h_llm_req_t* llm_reqs, int32_t nr_llm_reqs, h4h_rp_mem flag);

void hlm_reqs_pool_reset_fmain (h4h_flash_page_main_t* fmain);
void hlm_reqs_pool_alloc_fmain_pad (h4h_flash_page_main_t* fmain);
void hlm_reqs_pool_reset_logaddr (h4h_logaddr_t* logaddr);
void hlm_reqs_pool_relocate_kp (h4h_llm_req_t* lr, uint64_t new_sp_ofs);
void hlm_reqs_pool_write_compaction (h4h_hlm_req_gc_t* dst, h4h_hlm_req_gc_t* src, h4h_device_params_t* np);

#endif
