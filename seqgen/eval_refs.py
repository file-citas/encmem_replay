# longest tailing sequence
import sys
import os
import utils_trace as ut
import utils_math as um
import ref_seq as rs
import pickle


def main(refs_path, benchmark_dirs):

    refs = pickle.load(open(refs_path, "rb"))


    for ref in refs:
        print(ref)

    ref_score = []
    best_score = 0.0
    best_ref_idx = 0
    for rid, ref in enumerate(refs):
        not_matched = 0
        total = 0
        for benchmark_dir in benchmark_dirs:
            reordered_path, off2trace = ut.reorder_benchmark_dir(benchmark_dir,
                    reordered_dir="reordered")
            for offset, syscalls_match in off2trace.items():
                syscall_lists = syscalls_match["SC"]
                match_ids = syscalls_match["M"]
                for scl, match_id in zip(syscall_lists, match_ids):
                    total += 1
                    #cost = ref.match(scl, match_id, skip_cost=["tail"])
                    cost = ref.match(scl, match_id)
                    if cost != 0:
                        not_matched += 1
                        print("WARNING could not match offset %s: cost %d" % (offset, cost))
                        #print(", ".join(map(ut.fhex, scl)))

            print("NOT MATCHED %d / %d" % (not_matched, total))
            score = float(not_matched) / total
            ref_score.append(score)
            if score > best_score:
                best_score = score
                best_ref_id = rid

    print(best_score)
    print(refs[rid])

    if not os.path.exists("bestseq"):
        os.mkdir("bestseq")
    ut.ref_to_fs("bestseq", refs[rid:rid+1])
    #for score in ref_score:
    #    print(score)

if __name__ == "__main__":
    refs_path = sys.argv[1]
    benchmark_dirs = sys.argv[2:]
    main(refs_path, benchmark_dirs)
