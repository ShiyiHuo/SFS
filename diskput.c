#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>



int get_free_disk_size(char* p) {
  int num_free_sector = 0;
  int total_sector_count = p[19] + (p[20] << 8);
  int entry;

  for (entry = 2; entry <= total_sector_count-1-33+2; entry++) {   // potential bug
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
    if (result == 0x000) {
      num_free_sector++;
    }
  }
  return num_free_sector * SECTOR_SIZE;
}

/*
(1) Create a new directory entry in root folder
(2) Check if the disk has enough space to store the file
(3) Go through the FAT entries to find unused sectors in disk
and copy the file content to these sectors.
(4) Update the first cluster number field of directory entry we just created, update the FAT entries we used.
*/
void copy_file_to_disk(char* p, char* p2, char* file_name, int file_size) {

}


int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage: 'diskput <disk image> <file>'\n");
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

  // open file and map memory
  int file2 = open(argv[2], O_RDWR);
  if (file2 == -1) {
    printf("File not found.\n");
    exit(1);
  }
  struct stat file_stat2;
  if (fstat(file2, &file_stat2) < 0) {
    exit(1);
  }
  int file_size = file_stat2.st_size;
  char* p2 = mmap(NULL, file_stat2.st_size, PROT_READ, MAP_SHARED, file2, 0);  // ptr to map memory
  if (p2 == MAP_FAILED) {
    printf("Error: failed to map memory for file");
    close(file);
    exit(1);
  }

  // copy file from currect directory into root directory of disk
  int free_disk_size = get_free_disk_size(p);
  if (file_size <= free_disk_size) {
    copy_file_to_disk(p, p2, argv[2], file_size);
  } else {
    printf("No enough free space in the disk image.\n");
  }

  munmap(p, file_stat.st_size);
  munmap(p2, file_stat2.st_size);
  close(file);
  close(file2);
  return 0;
}
