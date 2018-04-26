#pragma once
#include <linux/types.h>

// must hold tseq_lock
int check_final_seq(const struct list_head* seq, const struct list_head* ref,
      size_t seq_len, size_t ref_len,
      int maxbt, int maxskip, int maxskip_head);
// must hold tseq_lock
int edit_dist(const struct list_head* seq, const struct list_head* ref,
      size_t seq_len, size_t ref_len);
// must hold tseq_lock
int check_lcs(const struct list_head* seq, const struct list_head* ref,
      size_t seq_len, size_t ref_len,
      int maxbt, int maxskip, int maxskip_head);
// must hold tseq_lock
int check_freq(const struct list_head* seq, const struct list_head* ref,
      size_t seq_len, size_t ref_len);
