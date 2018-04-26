#define pr_fmt(fmt) "XX kaper_stage_capture: " fmt
#include "kaper_stage_capture.h"
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/printk.h>

#include "mem_tracer.h"
#include "dist_measure.h"

struct target_seq {
   size_t len;
   int offset;
   int typ;
   int off2c_key;
   int maxbt;
   int maxskip;
   int maxskip_head;
   struct list_head seq_list;
   struct list_head list;
};
static int off2c_key_cntr = 0;
#define MAX_OFFSET 64
static int offset2cost[MAX_OFFSET];
static int key2offset[MAX_OFFSET];

DEFINE_SPINLOCK(tseq_lock);
static LIST_HEAD(tseq);

static struct sc_list* add_to_sc_list(size_t id, struct list_head* ts_sc)
{
   struct sc_list* r = kmalloc(sizeof(struct sc_list), GFP_KERNEL);
   if(r == NULL) {
      pr_err("ERROR: kmalloc(sizeof(struct sc_list), GFP_KERNEL)\n");
      return NULL;
   }
   r->id = id;
   r->match = false;
   list_add_tail(&r->list, ts_sc);
   return r;
}

int get_key2offset(int offset)
{
   int i;
   for(i=0; i<off2c_key_cntr; ++i) {
      if(key2offset[i] == offset)
         return i;
   }
   key2offset[off2c_key_cntr] = offset;
   off2c_key_cntr++;
   if(off2c_key_cntr >= MAX_OFFSET) {
      pr_err("Error; max number of offsets reached (%d)\n", MAX_OFFSET);
      return -EFAULT;
   }
   return off2c_key_cntr - 1;
}

int add_target_sequence(const int* search_seq, size_t search_seq_len,
      int typ, int offset,
      int maxbt, int maxskip, int maxskip_head)
{
   unsigned long flags;
   size_t i;
   int off2c_key;
   struct target_seq* ts;

   off2c_key = get_key2offset(offset);
   if(off2c_key < 0)
      return -EFAULT;

   ts = kmalloc(sizeof(struct target_seq), GFP_KERNEL);
   if(!ts) {
      pr_err("Error allocating target_seq for offset %x\n", offset);
      return -EFAULT;
   }
   ts->len = search_seq_len;
   ts->typ = typ;
   ts->offset = offset;
   ts->off2c_key = off2c_key;
   ts->maxbt = maxbt;
   ts->maxskip = maxskip;
   ts->maxskip_head = maxskip_head;
   INIT_LIST_HEAD(&ts->seq_list);

   for(i=0; i<search_seq_len; ++i) {
      if(add_to_sc_list(search_seq[i], &ts->seq_list) == NULL) {
         pr_err("ERROR: add_to_sc_list(%d)\n", search_seq[i]);
         return -EFAULT;
      }
   }
   spin_lock_irqsave(&tseq_lock, flags);
   list_add_tail(&ts->list, &tseq);
   spin_unlock_irqrestore(&tseq_lock, flags);

   pr_info("add_seq l %ld, t %d, o %x, bt %d, s %d, sh %d, k %d\n",
         ts->len, ts->typ, ts->offset, ts->maxbt, ts->maxskip, ts->maxskip_head, ts->off2c_key);

   return 0;
}

int set_stage_capture(const int* search_seq, size_t search_seq_len, int typ)
{
   memset(key2offset, 0, MAX_OFFSET * sizeof(unsigned int));
   return 0;
}

void unset_stage_capture(void)
{
   unsigned long flags;
   struct sc_list *sc, *sc_tmp;
   struct target_seq* ts, *ts_tmp;

   spin_lock_irqsave(&tseq_lock, flags);
   list_for_each_entry_safe(ts, ts_tmp, &tseq, list) {
      list_for_each_entry_safe(sc, sc_tmp, &tseq, list) {
         list_del(&sc->list);
         kfree(sc);
      }
      list_del(&ts->list);
      kfree(ts);
   }
   spin_unlock_irqrestore(&tseq_lock, flags);
}

int match_target_sequence(const struct list_head* seq, size_t seq_len, int* offset)
{
   unsigned long flags;
   struct target_seq* ts;
   int r;
   int min_cost = 100;

   memset(offset2cost, 0, MAX_OFFSET * sizeof(unsigned int));

   spin_lock_irqsave(&tseq_lock, flags);
   list_for_each_entry(ts, &tseq, list) {
      switch(ts->typ) {
         case FIN_SEQ:
            //r = check_final_seq(seq, &ts->seq_list, seq_len, ts->len, ts->maxbt, ts->maxskip, ts->maxskip_head);
            r = 0;
            break;
         case EDIT_DIST:
            r = edit_dist(seq, &ts->seq_list, seq_len, ts->len);
            break;
         case LCS_COST:
            r = check_lcs(seq, &ts->seq_list, seq_len, ts->len, ts->maxbt, ts->maxskip, ts->maxskip_head);
            break;
         case FREQ_COST:
            r = check_freq(seq, &ts->seq_list, seq_len, ts->len);
            break;
         default:
            r = 100;
            pr_info("Unknown metric id %d for offset %x\n", ts->typ, ts->offset);
      }
      offset2cost[ts->off2c_key] += r;
      //if(r == 0)
      //   pr_info("cost %d, typ %d, len %lx, offset %x\n", r, ts->typ, ts->len, ts->offset);
      //if(r<min_cost) {
      //   min_cost = r;
      //   *offset = ts->offset;
      //}
   }

   *offset = 0;
   list_for_each_entry(ts, &tseq, list) {
      if(offset2cost[ts->off2c_key] < min_cost) {
         min_cost = offset2cost[ts->off2c_key];
         *offset = ts->offset;
      }
   }
   spin_unlock_irqrestore(&tseq_lock, flags);

   return min_cost;
}
