#!/usr/bin/python

import subprocess
import os
import shutil
import time
import sys
import paramiko
import configparser
import signal
import psutil
import shlex
import time
import utils as u

def main(config_path, bench_id, dry_run=False):
    print("BENCH %s, config %s" % (bench_id, config_path))
    if dry_run:
        print("DRY RUN")

    u.DRY_RUN = dry_run
    config = configparser.ConfigParser()
    config.read(config_path)
    cd = config

    bodir = os.path.join(cd["BENCH"]["OUTDIR"], bench_id)
    if os.path.exists(bodir):
        print("ERROR: %s already exists" % bodir)
        return

    wait = int(cd["BENCH"]["WAIT"])

    n_runs = 1
    if not dry_run:
        n_runs = int(cd["BENCH"]["RUNS"])

    for i in range(n_runs):
        print("=" * 80)
        print(" [+] RUN %d" % i)

        u.remove_kaper_module(cd, wait)
        qemu = u.start_qemu(cd, 60)

        try:
            u.kaper_init(cd, wait, qemu)
            # DEBUG
            u.set_search_target(cd, wait)
            # /DEBUG
            u.set_search_sequences(cd, wait)
            u.start_tracing(cd, wait)
            u.start_capture(cd, wait)
            if not dry_run:
                max_try = int(cd["SSH"]["RETRY"])
                cap_status = False
                for ridx in range(max_try):
                    print(" [+] capture try %d/%d" % (ridx, max_try))
                    u.do_ssh(cd, wait)
                    cap_status = u.wait_for_capture(cd, wait)
                    if cap_status:
                        u.start_replay(cd, wait)
                        break
                if cap_status:
                    for ridx in range(max_try):
                        print(" [+] replay try %d/%d" % (ridx, max_try))
                        ssh_success = u.do_ssh(cd, wait, pw=cd["SSH"]["WRONG_PASS"])
                        if ssh_success:
                             print("+" * 80)
                             break
            u.copy_output_dir(cd, wait, bench_id, i)
            u.kill_qemu(cd, wait, qemu)
            u.remove_kaper_module(cd, wait)
        except u.RestartException:
            u.kill_qemu(cd, wait, qemu)
            u.remove_kaper_module(cd, wait)
        except Exception as e:
            print(e)
            u.kill_qemu(cd, wait, qemu)
            u.remove_kaper_module(cd, wait)
            break

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("python capture_replay.py config_path bench_id [dry_run]")
        sys.exit(1)

    config_path = sys.argv[1]
    bench_id = sys.argv[2]
    dry_run = True
    if len(sys.argv) > 3 and sys.argv[3] == "1":
        dry_run = False
    main(config_path, bench_id, dry_run)
