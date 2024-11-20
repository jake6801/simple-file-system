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

void print_directory_info(int file_descriptor, struct superblock_t superblock, int start_block, int block_count) {    
    int directory_size = block_count * superblock.block_size;
    struct dir_entry_t *directory = malloc(directory_size);
    lseek(file_descriptor, start_block * superblock.block_size, SEEK_SET);
    read(file_descriptor, directory, directory_size);
    int num_entries = directory_size / sizeof(struct dir_entry_t);
    for (int i = 0; i < num_entries; i++) {
        struct dir_entry_t *directory_entry = &directory[i];
        // printf("status: 0x%02X\nfilename: %s\n", directory_entry.status, directory_entry.filename);
        if (directory_entry->status == 0x00) {
            continue;
        } // File type
        char type;
        if (directory_entry->status == 0x03) {
            type = 'F'; // Regular file
        } else if (directory_entry->status == 0x05) {
            type = 'D'; // Directory
        } else {
            type = '?'; // Unknown
        }

        // File size (host-endian)
        int file_size = ntohl(directory_entry->size);

        // Print entry details
        printf("%c %10d %-30s ", type, file_size, directory_entry->filename);
        printf("\n");
    }

    free(directory);
}

int main(int argc, char *argv[]) {    
    // read first 30 bytes into superblock struct
    struct superblock_t superblock = {0}; //TODO dynamically allocate memory for this?
    int file_descriptor = open(argv[1], O_RDONLY); //? why do i need this O_RDONLY
    read(file_descriptor, &superblock, sizeof(superblock));    

    // convert all ints into big endian
    superblock.block_size = ntohs(superblock.block_size);
    superblock.file_system_block_count = ntohl(superblock.file_system_block_count);
    superblock.fat_start_block = ntohl(superblock.fat_start_block);
    superblock.fat_block_count = ntohl(superblock.fat_block_count);
    superblock.root_dir_start_block = ntohl(superblock.root_dir_start_block);
    superblock.root_dir_block_count = ntohl(superblock.root_dir_block_count);
    // printf("Super block information:\n");
    // printf("Block size: %d\nBlock count: %d\nFAT starts: %d\nFAT blocks: %d\nRoot directory start: %d\nRoot directory blocks: %d\n\n", superblock.block_size, superblock.file_system_block_count, superblock.fat_start_block, superblock.fat_block_count, superblock.root_dir_start_block, superblock.root_dir_block_count);

    // read in groups of 64 bytes to get this information? 
    print_directory_info(file_descriptor, superblock, superblock.root_dir_start_block, superblock.root_dir_block_count);

    close(file_descriptor);
    
    return 0;
}
