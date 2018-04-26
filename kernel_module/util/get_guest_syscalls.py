#!/usr/bin/python

import sys
import re

vmlinuxd = sys.argv[1]
out = sys.argv[2]
loadbase = 0xffffffff80000000

psyscall = re.compile("([0-9a-f]+) <(SyS_.+)>:")
pentry = re.compile("([0-9a-f]+) <(entry_SYSCALL_64)>:")
ppageoffset = re.compile("# ([0-9a-f]+) <(page_offset_base)>")
pphysbase = re.compile("# ([0-9a-f]+) <(phys_base)>")
papic_timer_interrupt = re.compile("([0-9a-f]+) <(apic_timer_interrupt)>:")

sc = {}
nsc = 0
syscallentry = loadbase
pageoffset = loadbase
physbase = loadbase
apic_timer_interrupt = loadbase
outstr = "#include \"guest_syscalls.h\"\nu64 guest_syscalls["

with open(vmlinuxd, "r") as f:
    for l in f.readlines():
        if apic_timer_interrupt == loadbase:
            m = papic_timer_interrupt.search(l)
            if m:
                apic_timer_interrupt = int(m.group(1), 16)
                continue
        if syscallentry == loadbase:
            m = pentry.search(l)
            if m:
                syscallentry = int(m.group(1), 16)
                continue
        if pageoffset == loadbase:
            m = ppageoffset.search(l)
            if m:
                pageoffset = int(m.group(1), 16)
                continue
        if physbase == loadbase:
            m = pphysbase.search(l)
            if m:
                physbase = int(m.group(1), 16)
                continue
        m = psyscall.search(l)
        if m:
            addr = m.group(1)
            name = m.group(2)
            sc[name] = int(addr, 16)
            nsc = nsc + 1

outstr = outstr + "%d] = {\n" % (nsc + 1)
for k, v in sc.items():
    outstr = outstr + "\t0x%x, // %s\n" % ((v - loadbase), k)
outstr = outstr + "\t0};\n"

outstr += "u64* guest_syscalls_spte[334] = {\n"
for i in range(len(sc.keys())):
    outstr += "\tNULL,\n"
outstr = outstr + "\t0};\n"

outstr = outstr + "u64 ptr_entry_SYSCALL_64 = 0x%x;\n" % (syscallentry - loadbase)
outstr = outstr + "u64 ptr_apic_timer_interrupt = 0x%x;\n" % (apic_timer_interrupt - loadbase)
with open(out, "w") as f:
    f.write(outstr)
