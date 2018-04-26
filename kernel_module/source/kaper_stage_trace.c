#include "kaper_stage_trace.h"

#define MAX_TARGET_DATA_LEN 256
// target_data can only be written withtin user context
DEFINE_MUTEX(target_data_write_lock);
static char* target_data;
static size_t target_data_len;
static atomic_t stage_tracing;

static bool is_stage_tracing(void)
{
   return atomic_read(&stage_tracing);
}

void set_stage_trace(const char* search_data, size_t search_data_len)
{
   if(is_stage_tracing()) {
      pr_err("ERROR set_stage_trace Tracing stage already in progress\n");
      return;
   }

   if(search_data_len > MAX_TARGET_DATA_LEN) {
      pr_err("ERROR set_stage_trace invalid search data\n");
      return;
   }

   mutex_lock(&target_data_write_lock);
   target_data = kmalloc(search_data_len, GFP_KERNEL);
   memcpy(target_data, search_data, search_data_len);
   target_data_len = search_data_len;
   atomic_inc(&stage_tracing);
   mutex_unlock(&target_data_write_lock);

   //pr_info("Initiated Tracing Stage with target %s (%d)\n", target_data, target_data_len);
}

void unset_stage_trace(void)
{
   if(!is_stage_tracing()) {
      pr_err("ERROR unset_stage_trace Tracing stage not in progress\n");
      return;
   }

   mutex_lock(&target_data_write_lock);
   if(target_data)
      kfree(target_data);
   target_data = NULL;
   atomic_dec(&stage_tracing);
   mutex_unlock(&target_data_write_lock);
}

int contains_target_data(size_t start_idx, char* data, size_t data_size)
{
   size_t i,j;

   if(unlikely(!is_stage_tracing())) {
      //pr_err("ERROR contains_target_data Tracing stage not in progress\n");
      return -1;
   }

   for(i=start_idx; i<data_size - target_data_len; ++i) {
      bool found = true;
      for(j=0; j<target_data_len; ++j) {
         if(data[i+j] != target_data[j]) {
            found = false;
            break;
         }
      }
      if(found) {
         return i;
      }
   }
   return -1;
}

static char target_data_dbg[] = {"1x2y3z4a"};
static size_t target_data_len_dbg = 8;
int contains_target_data_dbg(size_t start_idx, char* data, size_t data_size)
{
   size_t i,j;

   if(unlikely(!is_stage_tracing())) {
      //pr_err("ERROR contains_target_data Tracing stage not in progress\n");
      return -1;
   }

   for(i=start_idx; i<data_size - target_data_len_dbg; ++i) {
      bool found = true;
      for(j=0; j<target_data_len_dbg; ++j) {
         if(data[i+j] != target_data_dbg[j]) {
            found = false;
            break;
         }
      }
      if(found) {
         return i;
      }
   }
   return -1;
}
