# longest tailing sequence
import sys
import os

MAXDIST = 100
MAXSKIP = 100
MAXSKIP_HEAD = 8

#  6 #define MAXDIST 100
#  7
#  8 static int ncost(int cost, int max_cost)
#  9 {
# 10    int tmp, ncost;
# 11    ncost = 0;
# 12    tmp = cost * MAXDIST;
# 13
# 14    // DEBUG
# 15    //pr_info("cost %d, maxcost %d\n", cost, max_cost);
# 16
# 17    if(cost > max_cost)
# 18       return MAXDIST;
# 19
# 20    if(cost < 0) {
# 21       pr_warn("Warning invalid cost: %d\n", cost);
# 22       return MAXDIST;
# 23    }
# 24
# 25    while(tmp > 0) {
# 26       tmp -= max_cost;
# 27       ncost++;
# 28    }
# 29
# 30    // DEBUG
# 31    //pr_info("ncost %d\n", ncost);
# 32
# 33    return ncost;
# 34 }
# 35

def ncost(cost, max_cost):
    nc = 0
    tmp = cost * MAXDIST

    if cost > MAXDIST:
        return MAXDIST

    if cost < 0:
        print("Warning invalid cost: %d" % cost)
        return MAXDIST

    while tmp > 0:
        tmp -= max_cost
        nc += 1

    return nc

def get_freq(seq_set):
    max_freqs = {}
    for seq in seq_set:
        max_freqs_seq = {}
        for x in seq:
            try:
                max_freqs_seq[x] += 1
            except KeyError:
                max_freqs_seq[x] = 1

        for x, freq in max_freqs_seq.items():
            try:
                if max_freqs[x] > freq:
                    max_freqs[x] = freq
            except KeyError:
                max_freqs[x] = freq

        for x in max_freqs.keys():
            if x not in max_freqs_seq.keys():
                max_freqs[x] = 0

    freq_seq = []
    for x, freq in max_freqs.items():
        freq_seq.extend([x] * freq)
    freq_seq.sort()
    return freq_seq

def lcs_cost(test, match_ids, ref, maxbt=None, maxskip=MAXSKIP, maxskip_head=MAXSKIP_HEAD):
    lref = len(ref)
    ltest = len(test)
    rref = list(reversed(ref))
    rtest = list(reversed(test))
    iref = 0
    itest = 0

    n = 0
    n_skipped = 0
    n_skipped_head = 0
    cost = lref

    if ltest < lref:
        return [MAXDIST, 0]

    while iref < lref and itest < ltest:

        if rref[iref] != rtest[itest]:
            itest += 1
            n_skipped += 1
            if iref == 0: # not started yes
                n_skipped_head += 1
                if n_skipped_head > maxskip_head:
                    return [MAXDIST, 0]
            continue

        if n_skipped > maxskip:
            return [MAXDIST, 0]

        cost -= 1
        iref += 1
        itest += 1
        n += 1

        if maxbt and n > maxbt:
            break

        if cost == 0:
            break

    return [ncost(cost, lref), n_skipped_head]


#  51 #define MAXSKIP_HEAD 8
# 50 #define MAXSKIP 80
# 49 int check_lcs(const struct list_head* seq, const struct list_head* ref,
# 48       size_t seq_len, size_t ref_len)
# 47 {
# 46    struct sc_list* seq_entry = list_last_entry(seq, struct sc_list, list);
# 45    struct sc_list* ref_entry = list_last_entry(ref, struct sc_list, list);
# 44    int cost = ref_len;
# 43    int n_skipped = 0;
# 42    //size_t n = 0;
# 41
# 40    //if(seq_len > 100)
# 39    //   return MAXDIST;
# 38
# 37    if(seq_len < ref_len)
# 36       return MAXDIST;
# 35
# 34    while(&ref_entry->list != ref &&
# 33          &seq_entry->list != seq) {
# 32
# 31       //n++;
# 30       //if(n>=MAXN)
# 29       //   break;
# 28
# 27       // compare
# 26       if(seq_entry->id != ref_entry->id) {
# 25          seq_entry = list_prev_entry(seq_entry, list);
# 24          n_skipped++;
# 23          if(cost >= ref_len-2) {
# 22             if(n_skipped >= MAXSKIP_HEAD) {
# 21                return MAXDIST;
# 20             }
# 19          }
# 18          continue;
# 17       }
# 16
# 15       if(n_skipped >= MAXSKIP)
# 14          return MAXDIST;
# 13
# 12       cost--;
# 11
# 10       // advance
#  9       seq_entry = list_prev_entry(seq_entry, list);
#  8       ref_entry = list_prev_entry(ref_entry, list);
#  7    }
#  6    return ncost(cost, ref_len);
#  5 }


def get_lcs(a, b):
    lengths = [[0 for j in range(len(b)+1)] for i in range(len(a)+1)]
    # row 0 and column 0 are initialized to 0 already
    for i, x in enumerate(a):
        for j, y in enumerate(b):
            if x == y:
                lengths[i+1][j+1] = lengths[i][j] + 1
            else:
                lengths[i+1][j+1] = max(lengths[i+1][j], lengths[i][j+1])
    # read the substring out from the matrix
    result = []
    x, y = len(a), len(b)
    while x != 0 and y != 0:
        if lengths[x][y] == lengths[x-1][y]:
            x -= 1
        elif lengths[x][y] == lengths[x][y-1]:
            y -= 1
        else:
            assert a[x-1] == b[y-1]
            result = [a[x-1]] + result
            x -= 1
            y -= 1
    return result

def get_lcs_set(seq_set):
    lcs = seq_set[0]
    for s_idx in range(1, len(seq_set)):
        lcs = get_lcs(lcs, seq_set[s_idx])
    return lcs

def get_top_perc(lcs, seq_set, max_len, perc):
    min_len = int(max_len * perc)
    selected = list(filter(lambda t: len(t[1]) >= min_len, lcs))
    r = []
    covered = []
    for s in selected:
        i = s[0][0]
        j = s[0][1]
        if i not in covered:
            r.append(seq_set[i])
            covered.append(i)
        if j not in covered:
            r.append(seq_set[j])
            covered.append(j)
    return r

def get_lcs_set_rec(seq_set, drop):
    if len(seq_set) == 2:
        print("Keep 2")
        return get_lcs(seq_set[0], seq_set[1])

    l = len(seq_set)
    lcs_a = []
    max_len = 0
    for i in range(0, l):
        for j in range(i + 1, l):
            tmp = get_lcs(seq_set[i], seq_set[j])
            if max_len < len(tmp):
                max_len = len(tmp)
            lcs_a.append([[i,j], tmp])

    new_seq_set = get_top_perc(lcs_a, seq_set, max_len, drop)

    if len(new_seq_set) >= l:
        print("Keep %d" % len(new_seq_set))
        return get_lcs_set(new_seq_set)

    return get_lcs_set_rec(new_seq_set, drop)

def get_lcstr(s1, s2):
   m = [[0] * (1 + len(s2)) for i in range(1 + len(s1))]
   longest, x_longest = 0, 0
   for x in range(1, 1 + len(s1)):
       for y in range(1, 1 + len(s2)):
           if s1[x - 1] == s2[y - 1]:
               m[x][y] = m[x - 1][y - 1] + 1
               if m[x][y] > longest:
                   longest = m[x][y]
                   x_longest = x
           else:
               m[x][y] = 0
   return s1[x_longest - longest: x_longest]

def get_lcstr_set(seq_set):
    lcstr = seq_set[0]
    for s_idx in range(1, len(seq_set)):
        lcstr = get_lcstr(lcstr, seq_set[s_idx])
    return lcstr

def get_tr_seqs(seq_set, min_matched=0.6, mind=3, maxd=32):
    ts = {}
    thresh = min_matched
    p = 1.0 / float(len(seq_set["SC"]))

    for s, m in zip(seq_set["SC"], seq_set["M"]):
        accounted = set()
        for idx in m:
            for jdx in range(mind, maxd):
                if jdx >= idx - 1:
                    break
                sidx = idx - jdx
                k = " ".join(map(lambda t: "%x" % t, s[sidx:idx]))
                if k not in accounted:
                    accounted.add(k)
                    try:
                        ts[k] += p
                    except KeyError:
                        ts[k] = p
    ts_del = []
    for k, v in ts.items():
        if v < thresh:
            ts_del.append(k)
    for k in ts_del:
        del ts[k]
    return ts

#  3 int check_final_seq(const struct list_head* seq, const struct list_head* ref,
#  4       size_t seq_len, size_t ref_len)
#  5 {
#  6    int cost = ref_len;
#  7
#  8    // get the last sequence element
#  9    struct sc_list* seq_entry = list_last_entry(seq, struct sc_list, list);
# 10    struct sc_list* ref_entry = list_last_entry(ref, struct sc_list, list);
# 11
# 12    size_t idx = 0;
# 13    size_t n = 0;
# 14
# 15    if(seq_len < ref_len) {
# 16       goto out;
# 17    }
# 18
# 19    while(&ref_entry->list != ref &&  &seq_entry->list != seq) {
# 20
# 21       //n++;
# 22       //if(n>=MAXN)
# 23       //   break;
# 24
# 25       // compare
# 26       if(seq_entry->id != ref_entry->id) {
# 27
# 28          //if(idx == 0) {
# 29          //   seq_entry = list_prev_entry(seq_entry, list);
# 30          //   continue;
# 31          //}
# 32
# 33          //pr_info("%ld: %lx != %lx\n", idx, seq_entry->id, ref_entry->id);
# 34          goto out;
# 35       }
# 36       n++;
# 37       idx++;
# 38       cost--;
# 39
# 40       // advance
# 41       seq_entry = list_prev_entry(seq_entry, list);
# 42       ref_entry = list_prev_entry(ref_entry, list);
# 43    }
# 44
# 45 out:
# 46    return ncost(cost, ref_len * 8);
# 47 }

def tail_cost(test, match_ids, ref, maxbt=None, maxskip=MAXSKIP, maxskip_head=MAXSKIP_HEAD):
    min_cost = MAXDIST
    min_skip = MAXDIST
    for mid in match_ids:
        cost, skip = __tail_cost(
                test[:mid], ref,
                maxbt=maxbt, maxskip=maxskip, maxskip_head=maxskip_head)
        if cost < min_cost:
            min_cost = cost
            min_skip = len(test) - mid - 1
    return [min_cost, min_skip]


def __tail_cost(test, ref, maxbt=None, maxskip=MAXSKIP, maxskip_head=MAXSKIP_HEAD):
    lref = len(ref)
    ltest = len(test)
    rref = list(reversed(ref))
    rtest = list(reversed(test))
    iref = 0
    itest = 0

    n = 0
    n_skipped = 0
    n_skipped_head = 0
    cost = lref

    if ltest < lref:
        return [MAXDIST, 0]

    while iref < lref and itest < ltest:
        if rref[iref] != rtest[itest]:
            return [MAXDIST, 0]
        iref += 1
        itest += 1

    return [0, 0]


