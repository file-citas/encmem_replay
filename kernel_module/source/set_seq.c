#include <fcntl.h>
#include <stropts.h>
#include <stdlib.h>
#include <stdio.h>
#include "ioctl.h"

int main(int argc, char** argv) {
   int devfd;
   int result;
   size_t i;
   int* seq;
   struct target_data_capture td;

   if(argc < 3) {
      printf("Error no seq data\n");
      return -1;
   }

   td.len = argc - 6;
   td.typ = atoi(argv[1]);
   td.offset = strtol(argv[2], NULL, 16);
   td.maxbt = atoi(argv[3]);
   td.maxskip = atoi(argv[4]);
   td.maxskip_head = atoi(argv[5]);

   seq = malloc(td.len * sizeof(int));
   for(i=6; i<argc; ++i) {
      seq[i-6] = strtol(argv[i], NULL, 16);
      //printf("seq[%d]\t%x\n", i-1, seq[i-1]);
   }
   td.ptr = seq;

   devfd = open("/dev/kaper", O_WRONLY);
   if (devfd < 0) {
      fprintf(stderr, "Error opening replaid device\n");
      return -1;
   }

   result = ioctl(devfd, KAPER_IOCTL_SET_CAPTURE_SEQ, &td);
   if(result) {
      printf("Error\n");
      return -1;
   }
}
