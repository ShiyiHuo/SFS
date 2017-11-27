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
    b = p[SECTOR_SIZE + ((3 * entry) / 2)];
    result = (a << 8) + b;
  } else {
    a = p[SECTOR_SIZE + (int)((3 * entry) / 2)] & 0xF0;  // high 4 bits
    b = p[SECTOR_SIZE + (int)((3 * entry) / 2) + 1];
    result = (a >> 4) + (b << 4);
  }
  return result;
}

int get_next_unused_FAT_entry(char* p) {
  int entry = 2;
  while (get_FAT_entry(entry,p) != 0x000) {
    entry++;
  }
  return entry;
}

int get_next_free_sector(char *p, int cur_entry){
	cur_entry++;
	while (get_FAT_entry(cur_entry, p) != 0x000) {
		cur_entry++;
	}

	return cur_entry;
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

  // set attribute
  p[11] = 0x00;

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

  unsigned short *p_time = (unsigned short *) (p + 14);
  unsigned short *p_date = (unsigned short *) (p + 16);
  *p_date = ((year - 1980) << 9) + (month << 5) + day;
  *p_time = (hour << 11) + (minute << 5);

  // set first logical cluster
  unsigned short *p_first_logical_sector = (unsigned short *) (p + 26);
  *p_first_logical_sector = first_logical_sector;

  // set file size
  p[28] = (file_size & 0x000000FF);
  p[29] = (file_size & 0x0000FF00) >> 8;
  p[30] = (file_size & 0x00FF0000) >> 16;
  p[31] = (file_size & 0xFF000000) >> 24;

}

void update_FAT_entry(int entry, int value, char* p) {
	unsigned char *p_ = (unsigned char *)p;
	unsigned short value2 = value;
	int pos;

	if ((entry % 2) == 0) {
		pos = SECTOR_SIZE + ((3 * entry) / 2);
		p_[pos + 1] = (p_[pos + 1] & 0xf0) + (value2 >> 8);
		p_[pos] = value2 & 0x00ff;
	}
	else{
		pos = SECTOR_SIZE + (int)((3 * entry) / 2);
		p_[pos] = (p_[pos] & 0x0f) + ((value2 & 0x000f) << 4);
		p_[pos + 1] = value2 >> 4;
	}
}

/*
(1) Create a new directory entry in root folder
(2) Update the first cluster number field of directory entry we just created, update the FAT entries we used.
(3) Go through the FAT entries to find unused sectors in disk and copy the file content to these sectors.
*/
void copy_file_to_disk(char* p, char* p2, char* file_name, int file_size) {

	int wsize, remaining_byte = file_size, tmp;
  int curr_FAT_entry = get_next_unused_FAT_entry(p);
	int data_offset = SECTOR_SIZE * 33;
	char *p_base_disk = p;
	p += data_offset;

	if (file_exists(file_name, p_base_disk + SECTOR_SIZE * 19) != -1) {
		printf("File already exists.\n");
		exit(0);
	}
	create_root_directory(file_name, file_size, curr_FAT_entry, p_base_disk);
	while(remaining_byte > 0){
		wsize = remaining_byte < 512 ? remaining_byte % SECTOR_SIZE : SECTOR_SIZE;
		remaining_byte -= wsize;
		memcpy(p + (curr_FAT_entry - 2) * SECTOR_SIZE, p2, wsize);
		p2 += wsize;
		tmp = curr_FAT_entry;
		if(remaining_byte <= 0) {
      curr_FAT_entry = 0xFFF;
    }
		else {
      curr_FAT_entry = get_next_free_sector(p_base_disk, curr_FAT_entry);
    }
		update_FAT_entry(tmp, curr_FAT_entry, p_base_disk);
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
