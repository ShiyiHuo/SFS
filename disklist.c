#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#define SECTOR_SIZE 512

int get_file_size(char* file_name, char* p) {
  while (p[0] != 0x00) {
    if ((p[11] & 0x02) == 0 && (p[11] & 0x08) == 0) {
      char* curr_file_name = malloc(sizeof(char)*20);
      char* curr_file_extension = malloc(sizeof(char)*3);
      
      int i;
      for (i = 0; i < 8; i++) {
        if (p[i] == ' ') {
          break;
        }
        curr_file_name[i] = p[i];
      }

      for (i = 0; i < 3; i++) {
        curr_file_extension[i] = p[i+8];
      }

      strcat(curr_file_name, ".");
      strcat(curr_file_name, curr_file_extension);

      if (strcmp(file_name, curr_file_name) == 0) {
        return (p[28] & 0xFF) + ((p[29] & 0xFF) << 8) + ((p[30] & 0xFF) << 16) + ((p[31] & 0xFF) << 24);
      }
    }
    p += 32;
  }

  return -1;
}

void display_directory_listing(char* p) {
  while (p[0] != 0x00) {
    char file_type;
    if ((p[11] & 0b00010000) == 0b00010000) {   // if bit 4 (subdirectory) is 1
      file_type = 'D';
    } else {
      file_type = 'F';
    }

    char* file_name = malloc(sizeof(char) * 20);
    int i;
    for (i = 0; i < 8; i++) {
      if (p[i] == ' ') {
        break;
      }
      file_name[i] = p[i];
    }

    char* file_extension = malloc(sizeof(char) * 3);
    for (i = 0; i < 3; i++) {
      file_extension[i] = p[i+8];
    }

    strcat(file_name, ".");
    strcat(file_name, file_extension);

    //int file_size = (p[28] & 0xFF) + ((p[29] & 0xFF) << 8) + ((p[30] & 0xFF) << 16) + ((p[31] & 0xFF) << 24);
    int file_size = get_file_size(file_name, p);
    
    int date = *(unsigned short *)(p + 16);
    int time = *(unsigned short *)(p + 14);

    int year = ((date & 0xFE00) >> 9) + 1980;
    int month = ((date & 0x01E0) >> 5);
    int day = (date & 0x001F);
    int hour = (time & 0xF800) >> 11;
    int minute = (time & 0x07E0) >> 5;

    if ((p[11] & 0b00000010) == 0 && (p[11] & 0b00001000) == 0) {  // if not hidden and not volume label
      printf("%c %10d %20s %d-%02d-%02d %02d:%02d\n", file_type, file_size, file_name, year, month, day, hour, minute);
    }

    p += 32;
  }
}


int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Usage: 'disklist <disk image>'\n");
    exit(1);
  }

  // open disk image and map memory
  int file = open(argv[1], O_RDWR);
  if (file == -1) {
    printf("Error: failed to open disk image\n");
    exit(1);
  }
  struct stat file_stat;
  if (fstat(file, &file_stat) < 0) {
    exit(1);
  }
  char* p = mmap(NULL, file_stat.st_size, PROT_READ, MAP_SHARED, file, 0);  // ptr to map memory
  if (p == MAP_FAILED) {
    printf("Error: failed to map memory");
    exit(1);
  }

  display_directory_listing(p + SECTOR_SIZE * 19);

  munmap(p, file_stat.st_size);
  close(file);
  return 0;
}
