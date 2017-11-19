#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SECTOR_SIZE 512


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

  int file_size = get_file_size(argv[2], p + SECTOR_SIZE * 19);
  if (file_size <= 0) {
    printf("File not found.\n");
  } else {
    // file to copy
    int file2 = open(argv[2], O_RDWR | O_CREAT, 0666);
    if (file2 == -1) {
      printf("Error: failed to open file\n");
      exit(1);
    }
    char* p2 = mmap(NULL, file_size, PROT_WRITE, MAP_SHARED, file2, 0);
    if (p2 == MAP_FAILED) {
      printf("Error: failed to map memory for file");
      close(file);
      exit(1);
    }

    copy_file(p,p2,argv[2]);
    munmap(p2, file_size);
    close(file2);
  }

  munmap(p, file_stat.st_size);
  close(file);
  return 0;
}
