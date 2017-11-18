#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void get_os_name(char* os_name, char* p) {
  int i;
  for (i = 0; i < 8; i++) {
    os_name[i] = p[i+3];
  }
}

void get_disk_label(char* disk_label, char* p) {
  return;
}

int get_total_disk_size(char* p) {
  return 0;
}

int get_free_disk_size(int total_disk_size, char* p) {
  return 0;
}

int get_num_root_files(char* p) {
  return 0;
}

int get_num_FAT_copy(char* p) {
  return p[16];
}

int get_sector_per_FAT(char* p) {
  return 0;
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
  int free_disk_size = get_free_disk_size(total_disk_size, p);
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
  printf("Sectors per FAT: %d\n", sector_per_FAT);\

  munmap(p, file_stat.st_size);
  close(file);
  return 0;
}
