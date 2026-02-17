#include <stdio.h>
#include <stdbool.h>  // not needed in C23
int main() {
   FILE *fp;
   unsigned char buf[2];
   fp = fopen("roms/pic.ch8", "rb");
   while (fread(&buf, sizeof(char), 2, fp) > 0) {
      printf("byte1:%x\n", buf[0]);
      printf("byte2:%x\n", buf[1]);
   }
}
