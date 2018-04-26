#!/usr/bin/python

import sys
import os

fhex = lambda t: "%x" % t

def sort_traces_by_offset(benchmark_dir_path, min_len=0, offset_filter=None):
    off2trace = {}

    for run_dir in os.listdir(benchmark_dir_path):
        run_dir_path = os.path.join(benchmark_dir_path, run_dir)
        if not os.path.isdir(run_dir_path):
            continue

        print(run_dir_path)

        for trace_name in os.listdir(run_dir_path):


            trace_path = os.path.join(run_dir_path, trace_name)
            if not os.path.isfile(trace_path):
                continue

            offset = trace_name.split('_')[-1]
            if offset_filter and offset not in offset_filter:
                continue

            syscalls = []
            last_match_idx = 0
            match_indices = []
            with open(trace_path, "r") as f:
                for scid_match in f.readlines():
                    parts = scid_match.split()

                    scid = parts[0].rstrip()
                    if scid == "12345":
                        continue

                    if len(parts) == 2:
                        match_indices.append("%d" % len(syscalls))
                        last_match_idx = len(syscalls)

                    syscalls.append(scid)

            if last_match_idx == 0:
                last_match_idx = len(syscalls) - 1

            if len(syscalls) < min_len:
                continue

            try:
                #off2trace[offset].append([syscalls[:last_match_idx], match_indices])
                off2trace[offset]["SC"].append(syscalls[:last_match_idx + 1])
                off2trace[offset]["M"].append(match_indices)
            except KeyError:
                #off2trace[offset] = [[syscalls[:last_match_idx], match_indices], ]
                off2trace[offset] = {"SC": [syscalls, ], "M": [match_indices, ]}

    #print(off2trace.keys())
    return off2trace

def read_syscalls_matches(offset_path, min_len=0):
    syscalls_list = []
    matches_list = []
    for run_id in os.listdir(offset_path):
        # filter out matched
        if run_id.endswith(".m"):
            continue

        run_id_path = os.path.join(offset_path, run_id)
        if not os.path.isfile(run_id_path):
            continue

        syscalls = []
        match_indices = []
        with open(run_id_path, "r") as fd:
            syscalls = list(map(lambda t: int(t.rstrip(), 16), fd.readlines()))
        with open(run_id_path + ".m", "r") as fd:
            match_indices = list(map(lambda t: int(t.rstrip(), 10), fd.readlines()))

        if len(syscalls) < min_len:
            continue

        syscalls_list.append(syscalls)
        matches_list.append(match_indices)

    return {"SC": syscalls_list, "M": matches_list}


def read_offset2trace_dir(reordered_path, min_len=0, offset_filter=None):
    off2sc = {}

    for offset in os.listdir(reordered_path):

        if offset_filter and offset not in offset_filter:
            continue

        offset_path = os.path.join(reordered_path, offset)
        if not os.path.isdir(offset_path):
            continue

        off2sc[offset] = read_syscalls_matches(offset_path)

    return off2sc


def reorder_benchmark_dir(
        benchmark_dir_path, reordered_dir="reordered",
        min_len=0, offset_filter=None):
    reordered_path = benchmark_dir_path + "_" + reordered_dir
    if os.path.exists(reordered_path):
        print("%s already exists" % reordered_path)
        return [reordered_path, read_offset2trace_dir(reordered_path, min_len, offset_filter)]
    os.mkdir(reordered_path)

    off2trace = sort_traces_by_offset(benchmark_dir_path, min_len, offset_filter)
    for offset, syscalls_match in off2trace.items():
        offset_path = os.path.join(reordered_path, offset)
        os.mkdir(offset_path)
        i = 0
        for syscalls in syscalls_match["SC"]:
            with open(os.path.join(offset_path, "%s" % i), "w") as fd:
                fd.write("\n".join(syscalls))
            i += 1
        i = 0
        for match_indices in syscalls_match["M"]:
            with open(os.path.join(offset_path, "%s.m" % i), "w") as fd:
                fd.write("\n".join(match_indices))
            i += 1

    return [reordered_path, off2trace]

def ref_to_fs(sequence_dir, refs):
    SC_SEQUENCE_OFF = []
    SC_SEQUENCE_TYP = []
    SC_SEQUENCE = []
    SC_SEQUENCE_ARGS = []
    for ri, ref in enumerate(refs):
        SEQUENCE, SEQUENCE_ARGS, SEQUENCE_TYP, SEQUENCE_OFF = \
                ref.to_file(os.path.join(sequence_dir, "%d" % ri), ri * 0x1000)
        SC_SEQUENCE.extend(SEQUENCE)
        SC_SEQUENCE_ARGS.extend(SEQUENCE_ARGS)
        SC_SEQUENCE_OFF.extend(SEQUENCE_OFF)
        SC_SEQUENCE_TYP.extend(SEQUENCE_TYP)

    with open(os.path.join(sequence_dir, "kaper.conf.part"), "w") as fd:
        fd.write("SC_SEQUENCE: " + ",".join(SC_SEQUENCE) + "\n")
        fd.write("SC_SEQUENCE_ARGS: " + ",".join(SC_SEQUENCE_ARGS) + "\n")
        fd.write("SC_SEQUENCE_TYP: " + ",".join(SC_SEQUENCE_TYP) + "\n")
        fd.write("SC_SEQUENCE_OFF: " + ",".join(SC_SEQUENCE_OFF) + "\n")


