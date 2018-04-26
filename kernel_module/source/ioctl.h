#ifndef KAPER_IOCTL_H
#define KAPER_IOCTL_H

#define	KAPER_IOCTL_BASE              0xDEADBEEF
#define	KAPER_IOCTL_UPDATE_H           (KAPER_IOCTL_BASE -0)
#define KAPER_IOCTL_KAPER             (KAPER_IOCTL_BASE -1)
#define KAPER_IOCTL_TRACE_CR3         (KAPER_IOCTL_BASE -2)
#define KAPER_IOCTL_UNTRACE_CR3         (KAPER_IOCTL_BASE -3)
#define KAPER_IOCTL_TRACE_SYSCALLS    (KAPER_IOCTL_BASE -4)
#define KAPER_IOCTL_UNTRACE_SYSCALLS    (KAPER_IOCTL_BASE -5)
#define KAPER_IOCTL_ADD_TARGET                (KAPER_IOCTL_BASE -6)
#define KAPER_IOCTL_ADD_SYS_REF                (KAPER_IOCTL_BASE -7)
#define KAPER_IOCTL_ADD_MEM_REF                (KAPER_IOCTL_BASE -8)
#define KAPER_IOCTL_RESET                (KAPER_IOCTL_BASE -9)
#define KAPER_IOCTL_EXP                (KAPER_IOCTL_BASE -10)
#define KAPER_IOCTL_TRACE_SYSCALLS_PT                (KAPER_IOCTL_BASE -11)
#define KAPER_IOCTL_UNTRACE_SYSCALLS_PT                (KAPER_IOCTL_BASE -12)
#define KAPER_IOCTL_SET_STAGE_TRACE                (KAPER_IOCTL_BASE -13)
#define KAPER_IOCTL_SET_STAGE_CAPTURE                (KAPER_IOCTL_BASE -14)
#define KAPER_IOCTL_SET_STAGE_REPLAY                (KAPER_IOCTL_BASE -15)
#define KAPER_IOCTL_GET_N_CAP                (KAPER_IOCTL_BASE -16)
#define KAPER_IOCTL_BASELINE_START                (KAPER_IOCTL_BASE -17)
#define KAPER_IOCTL_BASELINE_STOP                (KAPER_IOCTL_BASE -18)
#define KAPER_IOCTL_BASELINE_GET                (KAPER_IOCTL_BASE -19)
#define KAPER_IOCTL_BASELINE_GET_LEN                (KAPER_IOCTL_BASE -20)
#define KAPER_IOCTL_SET_STALLING                (KAPER_IOCTL_BASE -21)
#define KAPER_IOCTL_UNSET_STALLING                (KAPER_IOCTL_BASE -22)
#define KAPER_IOCTL_ADVANCE                (KAPER_IOCTL_BASE -23)
#define KAPER_IOCTL_TOGGLE_EPT                (KAPER_IOCTL_BASE -24)
#define KAPER_IOCTL_RESET_EPT                (KAPER_IOCTL_BASE -25)
#define KAPER_IOCTL_DECRYPT                (KAPER_IOCTL_BASE -26)
#define KAPER_IOCTL_V2P                (KAPER_IOCTL_BASE -27)
#define KAPER_IOCTL_P2V                (KAPER_IOCTL_BASE -28)
#define KAPER_IOCTL_DM                (KAPER_IOCTL_BASE -29)
#define KAPER_IOCTL_PHYS                (KAPER_IOCTL_BASE -30)
#define KAPER_IOCTL_GET_REPLAYED                (KAPER_IOCTL_BASE -31)
#define KAPER_IOCTL_STAGE_RESET                (KAPER_IOCTL_BASE -32)
#define KAPER_IOCTL_TOTAL_RESET                (KAPER_IOCTL_BASE -33)
#define KAPER_IOCTL_SET_CAPTURE_SEQ                (KAPER_IOCTL_BASE -34)


struct target_data {
    size_t len;
    void* ptr;
};


struct target_data_capture {
    size_t len;
    int typ;
    int offset;
    int maxbt;
    int maxskip;
    int maxskip_head;
    void* ptr;
};

#endif
