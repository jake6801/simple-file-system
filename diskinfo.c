#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h> 
#include <arpa/inet.h> 

// Super block 
struct __attribute__((__packed__)) superblock_t {
    __uint8_t fs_id[8];
    __uint16_t block_size;
    __uint32_t file_system_block_count;
    __uint32_t fat_start_block;
    __uint32_t fat_block_count;
    __uint32_t root_dir_start_block;
    __uint32_t root_dir_block_count;
};

// Time and date entry 
struct __attribute__((__packed__)) dir_entry_timedate_t {
    __uint16_t year;
    __uint8_t month;
    __uint8_t day;
    __uint8_t hour;
    __uint8_t minute;
    __uint8_t second;
};

// Directory entry 
struct __attribute__((__packed__)) dir_entry_t {
    __uint8_t status;
    __uint32_t starting_block;
    __uint32_t block_count;
    __uint32_t size;
    struct dir_entry_timedate_t create_time;
    struct dir_entry_timedate_t modify_time;
    __uint8_t filename[31];
    __uint8_t unused[6];
};

int main(int argc, char *argv[]) {    
    // read first 30 bytes into superblock struct    
    int fd = open(argv[1], O_RDWR); 
    struct stat buffer;
    int status = fstat(fd, &buffer);
    void* address = mmap(NULL, buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    struct superblock_t* superblock;
    superblock = (struct superblock_t*)address;     

    int fssize;
    memcpy(&fssize, address+10, 4);
    fssize = ntohl(fssize);

    //! this is how I was doing it before, now doing it the way TA said to do it in example code 
    // struct superblock_t superblock = {0}; //TODO dynamically allocate memory for this?
    // int fd = open(argv[1], O_RDWR); 
    // read(fd, &superblock, sizeof(superblock));    

    // convert all ints into big endian
    superblock->block_size = ntohs(superblock->block_size);
    superblock->file_system_block_count = ntohl(superblock->file_system_block_count);
    superblock->fat_start_block = ntohl(superblock->fat_start_block);
    superblock->fat_block_count = ntohl(superblock->fat_block_count);
    superblock->root_dir_start_block = ntohl(superblock->root_dir_start_block);
    superblock->root_dir_block_count = ntohl(superblock->root_dir_block_count);

    // print superblock information 
    printf("Super block information:\n");
    printf("Block size: %d\nBlock count: %d\nFAT starts: %d\nFAT blocks: %d\nRoot directory start: %d\nRoot directory blocks: %d\n\n", superblock->block_size, superblock->file_system_block_count, superblock->fat_start_block, superblock->fat_block_count, superblock->root_dir_start_block, superblock->root_dir_block_count);

    // dynamically allocate memory for FAT 
    int fat_size = superblock->block_size * superblock->fat_block_count;
    __uint32_t *FAT = malloc(fat_size); 
    
    // skip ahead to where FAT starts 
    lseek(fd, superblock->fat_start_block * superblock->block_size, SEEK_SET); // lseek skips ahead in the file using the offset of superblock.fat_start_block * superblock.block_size from the start of the file using SEEK_SET  
    
    // read FAT from the file into the dynamically allocated FAT memory
    read(fd, FAT, fat_size); 
    
    // calculate the FAT information
    int free_blocks = 0, reserved_blocks = 0, allocated_blocks = 0;
    int fat_entries = fat_size / 4; 
    for(int i = 0; i < fat_entries; i++) {
        __uint32_t fat_entry = ntohl(FAT[i]);
        if (fat_entry == 0x00000000) { // 0x00000000 = block is available
            free_blocks++;
        } 
        else if (fat_entry == 0x00000001) { // 0x00000001 = block is reserved
            reserved_blocks++;
        }
        else { // 0x00000002 - 0xFFFFFF00 means allocated blocks as part of files and 0xFFFFFFFF means its the last block in a file so count both as allocated 
            allocated_blocks++;
        }
        //? do i need something for recognizing the last block in a file? (when fat_entry == 0xFFFFFFFF)?
    }
    
    // free dynamically allocated memory for FAT and close the file 
    free(FAT);
    munmap(address, buffer.st_size);
    close(fd);
    
    // print FAT information
    printf("FAT information:\n");
    printf("Free Blocks: %d\nReserved Blocks: %d\nAllocated Blocks: %d\n", free_blocks, reserved_blocks, allocated_blocks);
    
    return 0;
}
