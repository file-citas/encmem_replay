KERNELTREE ?= /home/file/project/replay/linux_kaslr

obj-m += kaper.o
kaper-objs := \
	./source/kaper.o \
	./source/kaper_handlers.o \
  	./source/kaper_debugfs.o \
  	./source/kaper_vcpu_takeover.o \
  	./source/mem_tracer.o \
  	./source/slot_to_syscall_debugfs.o \
  	./source/syscall_tracer_pt.o \
	./source/pt_mod.o \
 	./source/guest_syscalls.o \
  	./source/kaper_stage_trace.o \
  	./source/kaper_stage_capture.o \
  	./source/capture_debugfs.o \
  	./source/dist_measure.o
ul-y = ./source/ul.o
add_target-y = ./source/add_target.o
set_check-y = ./source/set_check.o
set_seq-y = ./source/set_seq.o
ccflags-y=-I$(SOURCE)/arch/x86/kvm

ifeq ($(DEBUG),1)
	ccflags-y += -DDEBUG
endif

target: intel amd

source/guest_syscalls.c: ../../linux_kaslr/vmlinux.d
	python utils/get_guest_syscalls.py $< $@

va: source/va.c
	gcc -o va source/va.c

pa: source/pa.c
	gcc -o pa source/pa.c

set_check: source/set_check.c
	gcc -static -o set_check source/set_check.c

set_seq: source/set_seq.c
	gcc -static -o set_seq source/set_seq.c

get_baseline: source/get_baseline.c
	gcc -o get_baseline source/get_baseline.c

get_baseline2: source/get_baseline2.c
	gcc -o get_baseline2 source/get_baseline2.c

intel:
	KCPPFLAGS=" -D\"TARGET_INTEL\"" make -C $(KERNELTREE) M=$(PWD) modules

amd:
	KCPPFLAGS=" -D\"TARGET_AMD\"" make -C $(KERNELTREE) M=$(PWD) modules

install:
	scp kaper.ko epyc:starterpack/

clean:
	make -C $(KERNELTREE) M=$(PWD) clean

