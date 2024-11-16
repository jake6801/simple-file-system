#include <stdio.h>
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

int main() {    
    struct superblock_t superblock = {0};
    int fd = open("test.img", O_RDONLY);
    read(fd, &superblock, sizeof(superblock));
    close(fd);

    // Convert fields to host endianness
    superblock.block_size = ntohs(superblock.block_size);
    superblock.file_system_block_count = ntohl(superblock.file_system_block_count);
    superblock.fat_start_block = ntohl(superblock.fat_start_block);
    superblock.fat_block_count = ntohl(superblock.fat_block_count);
    superblock.root_dir_start_block = ntohl(superblock.root_dir_start_block);
    superblock.root_dir_block_count = ntohl(superblock.root_dir_block_count);

    printf("Super block information:\n");
    printf("Block size: %d\nBlock count: %d\nFAT starts: %d\nFAT blocks: %d\nRoot directory start: %d\nRoot directory blocks: %d\n", superblock.block_size, superblock.file_system_block_count, superblock.fat_start_block, superblock.fat_block_count, superblock.root_dir_start_block, superblock.root_dir_block_count);



    return 0;
}
