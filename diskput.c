#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define SECTOR_SIZE 512

// returns 1 if file exists and returns -1 if file doesn't exist
int file_exists(char* file_name, char* p) {
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
        return 1;
      }
    }
    p += 32;
  }
  return -1;
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

int get_FAT_entry(int entry, char* p) {
  int a;
  int b;
  int result;
  if ((entry % 2) == 0) {
    a = p[SECTOR_SIZE + ((3 * entry) / 2) + 1] & 0x0F;  // low 4 bits
    b = p[SECTOR_SIZE + ((3 * entry) / 2)] & 0xFF;
    result = (a << 8) + b;
  } else {
    a = p[SECTOR_SIZE + (int)((3 * entry) / 2)] & 0xF0;  // high 4 bits
    b = p[SECTOR_SIZE + (int)((3 * entry) / 2) + 1] & 0xFF;
    result = (a >> 4) + (b << 4);
  }
  return result;
}

int get_next_unused_FAT_entry(char* p) {
  p += SECTOR_SIZE;

  int entry = 2;
  while (get_FAT_entry(entry,p) != 0x000) {
    entry++;
  }
  return entry;
}

void create_root_directory(char* file_name, int file_size, int first_logical_sector, char* p) {
  p += SECTOR_SIZE * 19;
  // find a free root directory
  while (p[0] != 0x00) {
    p += 32;
  }

  // set filename and extension
  int i;
  int stop = -1; // index of .
  for (i = 0; i < 8; i++) {
    if (file_name[i] == '.') {
      stop = i;
      break;
    }
    p[i] = file_name[i];
  }
  if (stop != -1) {
    for (i = stop; i < 8; i++) {
      p[i] = ' ';
    }
  }
  for (i = 0; i < 3; i++) {
    p[i+8] = file_name[i+stop+1];
  }

  // TODO: what should it be?
  // set attribute
  p[11] = 0x20;

  // TODO: SET DATE AND TIME
  // set create date & time
  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  int year = (timeinfo->tm_year + 1900);
  int month = (timeinfo->tm_mon + 1);
  int day = timeinfo->tm_mday;
  int hour = timeinfo->tm_hour;
  int minute = timeinfo->tm_min;

  // TODO: PROBLEM...
  // set first logical cluster

  // set file size
  p[28] = (file_size & 0x000000FF);
  p[29] = (file_size & 0x0000FF00) >> 8;
  p[30] = (file_size & 0x00FF0000) >> 16;
  p[31] = (file_size & 0xFF000000) >> 24;

}

void update_FAT_entry(int entry, int value, char* p) {
  p += SECTOR_SIZE;

  if ((entry % 2) == 0) {
    p[SECTOR_SIZE + ((3 * entry) / 2) + 1] = (value >> 8) & 0x0F;
    p[SECTOR_SIZE + ((3 * entry) / 2)] = value & 0xFF;
  } else {
    p[SECTOR_SIZE + (int)((3 * entry) / 2)] = (value << 4) & 0xF0;
    p[SECTOR_SIZE + (int)((3 * entry) / 2) + 1] = (value >> 4) & 0xFF;
  }
}

/*
(1) Create a new directory entry in root folder
(2) Update the first cluster number field of directory entry we just created, update the FAT entries we used.
(3) Go through the FAT entries to find unused sectors in disk and copy the file content to these sectors.
*/
void copy_file_to_disk(char* p, char* p2, char* file_name, int file_size) {
  if (file_exists(file_name, p + SECTOR_SIZE * 19) == -1) {
    int remaining_byte = file_size;
    int curr_FAT_entry = get_next_unused_FAT_entry(p);
    int physical_entry;
    int i;

    create_root_directory(file_name, file_size, curr_FAT_entry, p);
    while (remaining_byte > 0) {
      physical_entry = SECTOR_SIZE * (31 + curr_FAT_entry);
      for (i = 0; i < SECTOR_SIZE; i++) {
        if (remaining_byte == 0) {
          update_FAT_entry(curr_FAT_entry, 0xFFF, p);
          return;
        }
        p[i + physical_entry] = p2[file_size - remaining_byte];
        remaining_byte--;
      }
      int next_FAT_entry = get_next_unused_FAT_entry(p);
      update_FAT_entry(curr_FAT_entry, next_FAT_entry, p);
      curr_FAT_entry = next_FAT_entry;
    }
  } else {
    printf("File already exists.\n");
    exit(1);
  }
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
  char* p = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);  // ptr to map memory
  if (p == MAP_FAILED) {
    printf("Error: failed to map memory for disk image");
    close(file);
    exit(1);
  }

  // open file and map memory
  int file2 = open(argv[2], O_RDWR);
  if (file2 == -1) {
    printf("File not found.\n");
    close(file);
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
