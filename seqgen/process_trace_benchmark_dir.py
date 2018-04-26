# longest tailing sequence
import sys
import os
import utils_trace as ut
import utils_math as um
import ref_seq as rs
import pickle


def main(benchmark_dir, sequence_dir, reordered_dir="reordered",
        min_len=0, offset_filter=None):

    refs = []

    print("Offsets: " + ", ".join(offset_filter))
    reordered_path, off2trace = ut.reorder_benchmark_dir(benchmark_dir,
            reordered_dir=reordered_dir,
            min_len=min_len,
            offset_filter=offset_filter)

    for offset, syscalls_match in off2trace.items():
        #print("++ %s +++++++++++++++++++++++++++++++++" % offset)
        syscall_lists = syscalls_match["SC"]
        match_ids = syscalls_match["M"]

        syscall_lists_matched = []
        for scl, match_id in zip(syscall_lists, match_ids):
            adj = scl
            for mid in map(lambda t: int(t), match_id):
                scl[mid] += 0x1000
            syscall_lists_matched.append(scl)

        #print("NUM:\t%d" % (len(syscall_lists)))


        lcs = um.get_lcs_set(syscall_lists_matched)
        #print("LCS:\t" + ", ".join(map(ut.fhex, lcs)))
        maxlen = 0
        maxskip = 0
        for scl, match_id in zip(syscall_lists_matched, match_ids):
            cost, skip = um.lcs_cost(scl, match_id, lcs, maxbt=len(scl)*1.5, maxskip=len(scl), maxskip_head=8)
            #if cost != 0:
            #    print("\tlen %d, cost %d, offset %d" % (len(scl), cost, skip))
            if len(scl) > maxlen:
                maxlen = len(scl)
            if skip > maxskip:
                maxskip = skip

        lcs = list(map(lambda t: t > 0x1000 and t - 0x1000 or t, lcs))
        #print("LCS:\t" + ", ".join(map(ut.fhex, lcs)))

        #print("TAILS:")
        tails = um.get_tr_seqs(syscalls_match, min_matched=0.7)
        for tail_str in sorted(tails.keys(), key=len, reverse=True):
            tail = list(map(lambda t: int(t, 16), tail_str.split()))
            tail = list(map(lambda t: t > 0x1000 and t - 0x1000 or t, tail))
            #print(tail)
            #print(", ".join(map(ut.fhex, tail)))
            #print("%s: %f" % (tail_str, tails[tail_str]))
            ref = rs.RefSeq(offset)
            ref.add_lcs(lcs, maxlen * 1.5, maxlen, maxskip)
            ref.add_tail(
                    tail,
                    maxlen * 1.5, maxlen, maxskip)
            refs.append(ref)

    ref_seq_path = os.path.join(sequence_dir, "refs.pkl")
    pickle.dump(refs, open(ref_seq_path, "wb"))

    ut.ref_to_fs(sequence_dir, refs)

if __name__ == "__main__":
    benchmark_dir = sys.argv[1]
    sequence_dir = sys.argv[2]
    if not os.path.exists(sequence_dir):
        os.mkdir(sequence_dir)
    offset_filter = None
    if len(sys.argv) > 2:
        offset_filter = sys.argv[3:]
    main(benchmark_dir, sequence_dir, offset_filter=offset_filter)
