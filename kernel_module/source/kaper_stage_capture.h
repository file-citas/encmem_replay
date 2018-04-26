#ifndef KAPER_STAGE_CAPTURE_H
#define KAPER_STAGE_CAPTURE_H

#include <linux/types.h>

enum METRIC_TYPE {FIN_SEQ, EDIT_DIST, LCS_COST, FREQ_COST};

int set_stage_capture(const int* search_seq, size_t search_seq_len, int typ);
void unset_stage_capture(void);

int add_target_sequence(const int* search_seq, size_t search_seq_len,
      int typ, int offset,
      int maxbt, int maxskip, int maxskip_head);
int match_target_sequence(const struct list_head* seq, size_t seq_len, int* offset);

#endif
