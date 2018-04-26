#define pr_fmt(fmt) "XX dist_measure: " fmt
#include "dist_measure.h"
#include "mem_tracer.h"
#include <linux/list.h>

#define MAXDIST 100

static int ncost(int cost, int max_cost)
{
   int tmp, ncost;
   ncost = 0;
   tmp = cost * MAXDIST;

   // DEBUG
   //pr_info("cost %d, maxcost %d\n", cost, max_cost);

   if(cost > max_cost)
      return MAXDIST;

   if(cost < 0) {
      pr_warn("Warning invalid cost: %d\n", cost);
      return MAXDIST;
   }

   while(tmp > 0) {
      tmp -= max_cost;
      ncost++;
   }

   // DEBUG
   //pr_info("ncost %d\n", ncost);

   return ncost;
}

static int mask_bytemap(char* bytemap, const struct list_head* ref, int val)
{
   struct sc_list* ref_entry = list_last_entry(ref, struct sc_list, list);
   size_t idx = 0;
   while(&ref_entry->list != ref)
   {
      if(ref_entry->id == val && bytemap[idx] == 0) {
         bytemap[idx] = 1;
         break;
      }
      idx++;
      ref_entry = list_prev_entry(ref_entry, list);
   }
   return idx;
}

#define MAXBT 100
int check_freq(const struct list_head* seq, const struct list_head* ref,
      size_t seq_len, size_t ref_len)
{
   int cost;
   size_t i;
   char* bytemap = kzalloc(ref_len * sizeof(char), GFP_KERNEL);
   size_t n = 0;

   struct sc_list* seq_entry = list_last_entry(seq, struct sc_list, list);


   if(seq_len < ref_len)
      return MAXDIST;

   while(&seq_entry->list != seq) {

      n++;
      if(n>=MAXBT)
         break;

      // compare
      mask_bytemap(bytemap, ref, seq_entry->id);
      seq_entry = list_prev_entry(seq_entry, list);
   }

   cost = 0;
   // count bytes
   for(i=0; i<ref_len; ++i) {
      if(bytemap[i] == 0) {
         cost++;
      }
   }
   kfree(bytemap);
   return ncost(cost, ref_len);
}

int check_lcs(const struct list_head* seq, const struct list_head* ref,
      size_t seq_len, size_t ref_len,
      int maxbt, int maxskip, int maxskip_head)
{
   struct sc_list* seq_entry = list_last_entry(seq, struct sc_list, list);
   struct sc_list* ref_entry = list_last_entry(ref, struct sc_list, list);
   int cost = ref_len;
   int n_skipped = 0;
   int n_skipped_head = 0;
   size_t bt = 0;

   if(seq_len < ref_len)
      return MAXDIST;

   while(&ref_entry->list != ref &&
         &seq_entry->list != seq) {

      bt++;
      if(bt > maxbt)
         break;

      // compare
      if(seq_entry->id != ref_entry->id) {
         // advance test sequence
         seq_entry = list_prev_entry(seq_entry, list);

         n_skipped++;

         if(cost == ref_len)
            n_skipped_head++;

         if(n_skipped_head > maxskip_head)
            break;

         continue;
      }

      if(n_skipped > maxskip)
         break;

      cost--;
      // advance
      seq_entry = list_prev_entry(seq_entry, list);
      ref_entry = list_prev_entry(ref_entry, list);
   }

   return ncost(cost, ref_len);
}

/*
 * iterate backwards over system call sequence and compare each
 * entry with reference sequence.
 * Comparison is termiated on shortest sequence.
 * Returns 0 if reference sequence matches system call sequence,
 * MAXDIST otherwise.
 */
// must hold tseq_lock
int check_final_seq(const struct list_head* seq, const struct list_head* ref,
      size_t seq_len, size_t ref_len,
      int maxbt, int maxskip, int maxskip_head)
{
   int cost = ref_len;

   // get the last sequence element
   struct sc_list* seq_entry = list_last_entry(seq, struct sc_list, list);
   struct sc_list* ref_entry = list_last_entry(ref, struct sc_list, list);

   size_t idx = 0;
   size_t n = 0;
   int n_skipped_head = 0;

   if(seq_len < ref_len) {
      return MAXDIST;
   }

   while(&ref_entry->list != ref &&  &seq_entry->list != seq) {
      // compare and break if there is no match
      if(seq_entry->id != ref_entry->id) {
         if(n!=0)
            goto out;
         //pr_info("%ld: %lx != %lx\n", idx, seq_entry->id, ref_entry->id);
         if(n_skipped_head > maxskip_head)
            goto out;

         n_skipped_head++;
         seq_entry = list_prev_entry(seq_entry, list);
         continue;
      }
      n++;
      idx++;
      cost--;

      // advance
      seq_entry = list_prev_entry(seq_entry, list);
      ref_entry = list_prev_entry(ref_entry, list);
   }

out:
   return ncost(cost, ref_len);
}

static unsigned int* dmat = NULL;
static size_t dmat_ref_len = 0;
#define ins_penalty 1
#define sub_penalty 1
#define del_penalty 1
int edit_dist(const struct list_head* seq, const struct list_head* ref,
      size_t seq_len, size_t ref_len)
{
   size_t i0, i1;
   size_t i_del, i_ins, i_sub;
   size_t cols, rows;
   size_t i_bot_right = 0;
   unsigned int sub_cost;
   struct sc_list *ref_entry, *seq_entry;
   int cost = ref_len;

   // if seqence is empty: return ref len (full replacement)
   if(seq_len == 0) {
      goto out;
   }

   if(seq_len > ref_len) {
      seq_len = ref_len;
   }

   // +--- i1 / seq--->
   // |
   // i0 / ref
   // |
   // \/

   rows = ref_len + 1;
   cols = seq_len + 1;

   if(ref_len != dmat_ref_len) {
      kfree(dmat);
      dmat = NULL;
   }
   if(dmat == NULL) {
      dmat = kmalloc(rows * rows * sizeof(unsigned int), GFP_KERNEL);
      dmat_ref_len = ref_len;
   }

   memset(dmat, 0, rows * rows * sizeof(unsigned int));

   i_bot_right = rows * cols - 1;

	for(i0=1; i0<rows; ++i0) dmat[i0 * cols + 0 ] = i0 * del_penalty;
	for(i1=1; i1<cols; ++i1) dmat[0  * cols + i1] = i1 * ins_penalty;

   i1 = 1;
   seq_entry = list_last_entry(seq, struct sc_list, list);
	while(&seq_entry->list != seq) {
      i0 = 1;
      ref_entry = list_last_entry(ref, struct sc_list, list);
		while(&ref_entry->list != ref) {

			if(seq_entry->id != ref_entry->id) sub_cost = sub_penalty;
			else sub_cost = 0;

			i_del = (i0 - 1) * cols +  i1;
			i_ins =  i0      * cols + (i1 - 1);
			i_sub = (i0 - 1) * cols + (i1 - 1);

			dmat[i0 * cols + i1] = min3(
               dmat[i_del] + del_penalty,
               dmat[i_ins] + ins_penalty,
               dmat[i_sub] + sub_cost);

         // advance reference
			i0++;
			ref_entry = list_prev_entry(ref_entry, list);
		}

      // advance target
		i1++;
      seq_entry = list_prev_entry(seq_entry, list);

      // only check up to ref_len + 1
      if(i1 >= rows) break;
	}
	cost = dmat[i_bot_right];

out:
   return ncost(cost, ref_len);
}
