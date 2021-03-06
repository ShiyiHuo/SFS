#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SECTOR_SIZE 512   // bytes per sector

void get_os_name(char* os_name, char* p) {
  int i;
  for (i = 0; i < 8; i++) {
    os_name[i] = p[i+3];
  }
}

// read volume label from boot sector
// if empty, read it from directory entry
// if volume label bit = 1, the corresponding filename is the volume label for the disk
void get_disk_label(char* disk_label, char* p) {
  int i;
  for (i = 0; i < 11; i++) {
    disk_label[i] = p[i+43];
  }
  if (disk_label[0] == ' ') {
    p += SECTOR_SIZE * 19;
    while (p[0] != 0x00) {
      if (p[11] == 8) {  // if bit3 (volume label) = 1
        for (i = 0; i < 8; i++) {
          disk_label[i] = p[i];
        }
      }
      p += 32;
    }
  }
}

int get_total_disk_size(char* p) {
  int total_sector_count = p[19] + (p[20] << 8);
  return total_sector_count * SECTOR_SIZE;
}

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

// TODO: need final check!
// the number of root files = the number of directory entries in root folder...
// except for those whose attribute = 0x0f (long filename) or 0x08 (volume label),
// and those whose filename is 0xE5
int get_num_root_files(char* p) {
  p += SECTOR_SIZE * 19;
  int result = 0;
  while (p[0] != 0x00) {
    // if ((p[0] != 0xE5) && (p[11] != 0x0F) && (p[11] != 0x08)) {
    if ((p[11] != 0x0F) && (p[11] != 0x08)) {
      result++;
    }
    p += 32;
  }

  return result;
}

int get_num_FAT_copy(char* p) {
  return p[16];
}

int get_sector_per_FAT(char* p) {
  return p[22] + (p[23] << 8);
}


int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Usage: 'diskinfo <disk image>'\n");
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

  // read and print info
  char* os_name = malloc(sizeof(char) * 8);
  get_os_name(os_name, p);
  char* disk_label = malloc(sizeof(char) * 11);
  get_disk_label(disk_label, p);
  int total_disk_size = get_total_disk_size(p);
  int free_disk_size = get_free_disk_size(p);
  int num_root_files = get_num_root_files(p);
  int num_FAT_copy = get_num_FAT_copy(p);
  int sector_per_FAT = get_sector_per_FAT(p);

  printf("OS Name: %s\n", os_name);
  printf("Label of the disk: %s\n", disk_label);
  printf("Total size of the disk: %d bytes\n", total_disk_size);
  printf("Free size of the disk: %d bytes\n\n", free_disk_size);
  printf("==============\n");
  printf("The number of files in the root directory (not including subdirectories): %d\n\n", num_root_files);
  printf("=============\n");
  printf("Number of FAT copies: %d\n", num_FAT_copy);
  printf("Sectors per FAT: %d\n", sector_per_FAT);

  munmap(p, file_stat.st_size);
  close(file);
  return 0;
}
