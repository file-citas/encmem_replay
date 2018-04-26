#include <fcntl.h>
#include <stropts.h>
#include <stdlib.h>
#include <stdio.h>
#include "ioctl.h"

int main(int argc, char** argv) {
   int devfd;
   int result;
   struct target_data td;

   if(argc != 3)
      return -1;

   td.len = atoi(argv[1]);
   td.ptr = argv[2];

   if(td.len < 1 || td.len > 1024) {
      printf("Error\n");
      return 1;
   }

   devfd = open("/dev/kaper", O_WRONLY);
   if (devfd < 0) {
      fprintf(stderr, "Error opening replaid device\n");
      return 1;
   }

   result = ioctl(devfd, KAPER_IOCTL_SET_STAGE_TRACE, &td);
   if(result) {
      printf("Error\n");
      return 1;
   }
}
