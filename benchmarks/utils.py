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

DRY_RUN = False

class RestartException(Exception):
    pass

def try_login(ip, port, pw, user, timeout):
    r = False
    ssh = paramiko.SSHClient()
    ssh.load_system_host_keys()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        ssh.connect(ip, password=pw, username=user, port=int(port), timeout=int(timeout))
    except paramiko.ssh_exception.AuthenticationException as e:
        print(e)
    except Exception as e:
        print(e)
        raise RestartException()
    else:
        r = True
    ssh.close()
    return r

#def read_sc_sequences(seq_paths):
#    r = []
#    for seq_path in seq_paths:
#        seq_name = seq_path.split('/')[-1]
#        offset = seq_name.split('_')[0]
#        with open(seq_path, "r") as fd:
#            r.append([offset, " ".join(map(lambda t: t.rstrip(), fd.readlines()))])

def run_cmd(cmd, wait):
    if not DRY_RUN:
        p = subprocess.Popen(cmd, shell=True)
        p.wait()
        time.sleep(wait)
    else:
        print(cmd[0])

def remove_kaper_module(cd, wait):
    print(" [+] removing kaper module")
    cmd = ["rmmod kaper"]
    run_cmd(cmd, wait)

def kaper_init(cd, wait, qemu):
    print(" [+] loading kaper module")
    cmd = ["insmod %s" % cd["PATH"]["KAPER_MODULE"]]
    run_cmd(cmd, wait)

    h = 0
    if not DRY_RUN:
        cro = subprocess.Popen(["cat /sys/module/kvm_amd/sections/.rodata"],
                               stdout=subprocess.PIPE,
                               shell=True)
        h = cro.stdout.read()
        h = int(h.strip(), 16)

    print(" [+] updating handler base (%x / %x)" % (h, h-0xffffffffa0000000))
    cmd = ["%s 0 %d" % (cd["PATH"]["KAPER_UL"], h-0xffffffffa0000000)]
    run_cmd(cmd, wait)

    print(" [+] kapering qemu process")
    if not DRY_RUN:
        cmd = ["%s 1 %d" % (cd["PATH"]["KAPER_UL"], qemu.pid)]
    else:
        cmd = ["%s 1 %d" % (cd["PATH"]["KAPER_UL"], 0)]
    run_cmd(cmd, wait)

def start_tracing(cd, wait):
    print(" [+] activating memory tracing")
    cmd = ["%s 2 1" % cd["PATH"]["KAPER_UL"]]
    run_cmd(cmd, wait)

    print(" [+] activating syscall tracing")
    cmd = ["%s 11 1" % cd["PATH"]["KAPER_UL"]]
    run_cmd(cmd, wait)

def start_capture(cd, wait):
    print(" [+] activating memory capture")
    cmd = ["%s 14 1" % cd["PATH"]["KAPER_UL"]]
    run_cmd(cmd, wait)

def start_replay(cd, wait):
    print(" [+] setting replay mode")
    cmd = ["%s 15 1" % cd["PATH"]["KAPER_UL"]]
    run_cmd(cmd, wait)


def set_signal_handler(qemu):
    def signal_handler(signal, frame):
        print(" [+] Killing qemu")
        qemu.kill()
        sys.exit(0)
    signal.signal(signal.SIGINT, signal_handler)

def start_qemu(cd, wait):
    print(" [+] starting qemu")
    print(cd["BENCH"]["QEMU_CMD"])
    qemu = None
    cmd = shlex.split(cd["BENCH"]["QEMU_CMD"])
    if not DRY_RUN:
        DEVNULL = open(os.devnull, 'wb')
        qemu = subprocess.Popen(cmd,
                                stdout=DEVNULL,
                                stderr=subprocess.STDOUT,
                                shell=False)
        set_signal_handler(qemu)
        time.sleep(wait)
        print(" [+] Qemu pid %d" % qemu.pid)
    else:
        print(cmd[0])
    return qemu

def kill_qemu(cd, wait, qemu):
    print(" [+] terminating qemu")
    if not DRY_RUN:
        qemu.kill()
        time.sleep(wait)
    print(" [+] terminated qemu")

def set_search_target(cd, wait):
    print(" [+] setting search target (%s, %s)" %
            (cd["SSH"]["PASS_LEN"], cd["SSH"]["PASS"]))
    cmd = ["%s %s %s" % (cd["PATH"]["KAPER_SETTARGET"], cd["SSH"]["PASS_LEN"], cd["SSH"]["PASS"])]
    run_cmd(cmd, wait)

def set_search_sequence(cd, wait, seq, args, typ, offset):
    print(" [+] setting search sequence %s, typ %s, offset %s" %
            (seq, typ, offset))
    cmd = ["%s %s %s %s %s" % (cd["PATH"]["KAPER_SETSEQ"], typ, offset, args, seq)]
    run_cmd(cmd, wait)

def read_sc_sequences(seq_paths):
    r = []
    for seq_path in seq_paths:
        with open(seq_path, "r") as fd:
            r.append(" ".join(map(lambda t: t.rstrip(), fd.readlines())))
    return r

def read_sc_sequence_args(arg_paths):
    r = []
    for arg_path in arg_paths:
        with open(arg_path, "r") as fd:
            r.append(" ".join(map(lambda t: t.rstrip(), fd.readlines())))
    return r

def set_search_sequences(cd, wait):
    seqs = read_sc_sequences(cd["BENCH"]["SC_SEQUENCE"].split(","))
    seq_args = read_sc_sequence_args(cd["BENCH"]["SC_SEQUENCE_ARGS"].split(","))
    for (seq, arg, typ, off) in zip(seqs, seq_args,
            cd["BENCH"]["SC_SEQUENCE_TYP"].split(","),
            cd["BENCH"]["SC_SEQUENCE_OFF"].split(",")):
        set_search_sequence(cd, wait, seq, arg, typ, off)


def do_ssh(cd, wait, pw=None):
    if pw is None:
        pw = cd["SSH"]["PASS"]
    print(" [+] Initiating SSH connection")
    ssh_success = True
    if not DRY_RUN:
        ssh_success = try_login(
            cd["SSH"]["IP"],
            cd["SSH"]["PORT"],
            pw,
            cd["SSH"]["USER"],
            cd["SSH"]["TIMEOUT"])
        if ssh_success:
            print(" [+] SSH connection established")
        else:
            print(" [+] SSH connection failed")
    return ssh_success

def copy_output_dir(cd, wait, bench_id, run_id):
    bodir = os.path.join(cd["BENCH"]["OUTDIR"], bench_id, "%d" % run_id)
    print(" [+] Transfering traces from %s to %s" %
        (cd["PATH"]["KAPER_DBG_SLOT"], bodir))
    if not DRY_RUN:
        shutil.copytree(cd["PATH"]["KAPER_DBG_SLOT"], bodir);
        time.sleep(wait)

def cap_timeout_handler(signum, frame):
    print("wait_for_capture timed out")
    raise Exception("timed out")

def wait_for_capture(cd, wait):
    status_path = cd["PATH"]["CAP_STATUS"]
    timeout = cd["BENCH"]["CAP_TIMEOUT"]
    #print(" [+] Waiting for capture for %s seconds" % timeout)
    r = False
    if not DRY_RUN:
        signal.signal(signal.SIGALRM, cap_timeout_handler)
        signal.alarm(int(timeout))
        try:
            while True:
                with open(status_path, "r") as fd:
                    status = fd.read().rstrip()
                    if int(status) != 0:
                        r = True
                        time.sleep(int(timeout))
                    time.sleep(1)
        except Exception as e:
            print(e)
    return r

