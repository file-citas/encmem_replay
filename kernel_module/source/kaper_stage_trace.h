#ifndef KAPER_STAGE_TRACE_H
#define KAPER_STAGE_TRACE_H

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <asm/errno.h>

void set_stage_trace(const char* search_data, size_t search_data_len);
void unset_stage_trace(void);

// returns the first occurence of target_data in data after start_idx
// or -1 if not found
int contains_target_data(size_t start_idx, char* data, size_t data_size);
int contains_target_data_dbg(size_t start_idx, char* data, size_t data_size);

#endif
