#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "ioctl.h"

struct target_ioctl {
    unsigned long gfn;
    unsigned int offset;
    char data[16];
    void* handle;
};

struct sys_ref_ioctl {
    void* handle;
    unsigned long rax;
};

struct mem_ref_ioctl {
    void* handle;
    unsigned long rax;
};

int main(int argc, char **argv) {
    struct target_ioctl t;
	void *filemap;
	struct stat filestat;
	int filefd;
	int devfd;
	int ioctl_no = 0;
    int iter = 0;
    int i;
    int result;

    if (argc != 4) {
       printf("Error wrong number of args\n");
       return -1; 
    }
	ioctl_no = KAPER_IOCTL_ADD_TARGET;
    t.gfn = atol(argv[1]);
    t.offset = atoi(argv[2]);
    t.handle = NULL;
    for(i=0; i<16; ++i)
        t.data[i]  = '\0';

    for(i=0; i<16 && argv[3][i] != '\0'; ++i)
        t.data[i] = argv[3][i];

	devfd = open("/dev/kaper", O_WRONLY);
	if (devfd < 0) {
        fprintf(stderr, "Error opening replaid device\n");
        return 1;
    }

    printf("Adding new target gfn %ld, offset %x, data %s\n", 
            t.gfn, t.offset, t.data);
	result = ioctl(devfd, KAPER_IOCTL_ADD_TARGET, &t);
    printf("res %d, Got handle %p\n", result, t.handle);
}


