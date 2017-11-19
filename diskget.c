#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define SECTOR_SIZE 512

// Copies a file from the file system (root directory) to the current directory in Linux: ./diskget disk.IMA ANS1.PDF
// 1.Convert the given filename to upper case, then search this filename from directory entries in root folder.
// 2.If filename matching, get the first cluster number & file size.
// 3.Rely on FAT entries to copy.
// 4.If (Last cluster of file && reach the filesize value), stop copy.


char* concat_name_extension(char* file_name, char* file_extension) {
  int i;
  for (i = 0; i < 8; i++) {
    if (p[i] == ' ') {
      break;
    }
    file_name[i] = p[i];
  }

  for (i = 0; i < 3; i++) {
    file_extension[i] = p[i+8];
  }

  strcat(file_name, ".");
  strcat(file_name, file_extension);

  return file_name;
}

// return file size if file exists, otherwise return -1
int get_file_size(char* file_name, char* p) {\
  int file_size;
  while (p[0] != 0x00) {
    if ((p[11] & 0x02) == 0 && (p[11] & 0x08) == 0) { // not hidden, not volume label
      char* curr_file_name = malloc(sizeof(char) * 8);
      char* curr_file_extension = malloc(sizeof(char) * 3);
      char* curr_file = malloc(strlen(curr_file_name) + strlen(curr_file_extension) + 1);
      curr_file = concat_name_extension(curr_file_name, curr_file_extension);

      if (strcmp(file_name, curr_file) == 0) {
        file_size = (p[28] & 0xFF) + ((p[29] & 0xFF) << 8) + ((p[30] & 0xFF) << 16) + ((p[31] & 0xFF) << 24);
        return file_size;
      }
    }
    p += 32;
  }

  return -1;
}

// return first logical sector if it exists, otherwise return -1
int get_first_logical_sector(char* file_name, char* p) {
  while (p[0] != 0x00) {
    if ((p[11] & 0x02) == 0 && (p[11] & 0x08) == 0) { // not hidden, not volume label
      char* curr_file_name = malloc(sizeof(char) * 8);
      char* curr_file_extension = malloc(sizeof(char) * 3);
      char* curr_file = malloc(strlen(curr_file_name) + strlen(curr_file_extension) + 1);
      curr_file = concat_name_extension(curr_file_name, curr_file_extension);

      if (strcmp(file_name, curr_file) == 0) {
        return p[26] + (p[27] << 8);
      }
    }
    p += 32;
  }

  return -1;
}

int get_fat_entry(int entry, char* p) {
  int a;
  int b;
  int result;
  if ((entry % 2) == 0) {
    a = p[SECTOR_SIZE + ((3 * entry) / 2) + 1] & 0x0F;  // low 4 bits
    b = p[SECTOR_SIZE + ((3 * entry) / 2)];
    result = (a << 8) + b;
  } else {
    a = p[SECTOR_SIZE + (int)((3 * entry) / 2)] & 0xF0;  // high 4 bits
    b = p[SECTOR_SIZE + (int)((3 * entry) / 2) + 1];
    result = (a >> 4) + (b << 4);
  }
  return result;
}

//search uppercased filename from directory entries in root folder
int search_file(char* file_name, char* p) {

}

int copy_file(char* p, char* p2, char* file_name) {

}


int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage: 'diskget <disk image> <file>'\n");
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
    printf("Error: failed to map memory for disk image");
    close(file);
    exit(1);
  }


  // int file_size = get_file_size(argv[2], p + SECTOR_SIZE * 19);
  // if (file_size <= 0) {
  //   printf("File not found.\n");
  // } else {
  //   // file to copy
  //   int file2 = open(argv[2], O_RDWR | O_CREAT, 0666);
  //   if (file2 == -1) {
  //     printf("Error: failed to open file\n");
  //     exit(1);
  //   }
  //   char* p2 = mmap(NULL, file_size, PROT_WRITE, MAP_SHARED, file2, 0);
  //   if (p2 == MAP_FAILED) {
  //     printf("Error: failed to map memory for file");
  //     close(file);
  //     exit(1);
  //   }

  //   copy_file(p,p2,argv[2]);
  //   munmap(p2, file_size);
  //   close(file2);
  // }

  munmap(p, file_stat.st_size);
  close(file);
  return 0;
}
