#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int main(int argc, char **argv) {
	void *filemap;
	struct stat filestat;
	int filefd;
	int devfd;
	int ioctl_no = 0;
    int iter = 0;
    int result;

    if (argc != 3) {
       printf("Error wrong number of args\n");
       return -1; 
    }
	ioctl_no = atoi(argv[1]);
    iter = atoi(argv[2]);


	devfd = open("/dev/kaper", O_WRONLY);
	if (devfd < 0) {
        fprintf(stderr, "Error opening replaid device\n");
        return 1;
    }

	result = ioctl(devfd, 0xDEADBEEF - ioctl_no, iter);

}


