#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
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
    // parse command line 
    char *fs_file_name = argv[1];
    char *put_file_name = argv[2];
    char *put_location = argv[3];

    // map filesystem to memory
    int fs_fd = open(fs_file_name, O_RDWR);
    struct stat fs_buffer;
    int fs_status = fstat(fs_fd, &fs_buffer);
    void* fs_address = mmap(NULL, fs_buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);

    // check if file exists in current linux directory 
    int put_fd = open(put_file_name, O_RDONLY);
    if (put_fd < 0) {
        printf("File not found.\n");
        exit;
    }

    // open the file to be put into the file system
    struct stat put_file_buffer;
    int put_file_status = fstat(put_fd, &put_file_buffer);
    void* put_file_address = mmap(NULL, put_file_buffer.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, put_fd, 0);

    // get superblock information 
    struct superblock_t *superblock = (struct superblock_t *)fs_address;
    superblock->block_size = ntohs(superblock->block_size);
    superblock->file_system_block_count = ntohl(superblock->file_system_block_count);
    superblock->fat_start_block = ntohl(superblock->fat_start_block);
    superblock->fat_block_count = ntohl(superblock->fat_block_count);
    superblock->root_dir_start_block = ntohl(superblock->root_dir_start_block);
    superblock->root_dir_block_count = ntohl(superblock->root_dir_block_count);

    char *token = strtok(put_location, "/");
    char *prev_token = NULL;

    void* put_file_dir_address = (uint8_t*)fs_address + superblock->root_dir_start_block * superblock->block_size;
    struct dir_entry_t* dir_entry = (struct dir_entry_t*)put_file_dir_address;
    char *new_filename;
    while (token != NULL) {  
        char *next_token = strtok(NULL, "/");
        bool found_dir = false;        
        // if next_token == NULL then file is getting put in this directory under the name token
        if (next_token == NULL) {    
            new_filename = token;
            found_dir = true; 
            break;
        }
        // if next_token != NULL then this is a subdir so search through curr dir 
        for (int i = 0; i < superblock->root_dir_block_count * superblock->block_size / sizeof(struct dir_entry_t); i++) { 
            // if the subdir exists
            if (dir_entry[i].status == 0x05 && strcmp((char *)dir_entry[i].filename, token) == 0) {
                put_file_dir_address = (uint8_t*)fs_address + ntohl(dir_entry[i].starting_block) * superblock->block_size;
                dir_entry = (struct dir_entry_t*)put_file_dir_address;
                found_dir = true;
                break;
            }
        }
        // subdir doesnt exist, create it 
        if (!found_dir) {
            // search through fat table for 8 free blocks and reserve them in the fat table
            int num_blocks_needed = 8;
            int fat_size = superblock->block_size * superblock->fat_block_count;
            uint32_t* fat_table = (uint32_t*)((uint8_t*)fs_address + superblock->fat_start_block * superblock->block_size);

            int first_free_block = 0;
            int free_block_count = 0;
            for (int i = 0; i < fat_size / sizeof(uint32_t); i++) {
                if (fat_table[i] == 0x00000000) {
                    if (free_block_count == 0) {
                        first_free_block = i;
                    }
                    free_block_count++;

                    if (free_block_count == num_blocks_needed) {
                        break;
                    }
                } else {
                    free_block_count = 0;
                    first_free_block = 0;
                }
            }

            if (free_block_count < num_blocks_needed) {
                printf("Not enough contiguous free blocks available.\n");
                return -1;
            }

            int free_blocks[num_blocks_needed];
            for (int i = 0; i < num_blocks_needed; i++) {
                free_blocks[i] = first_free_block + i;
            }

            // allocate the free blocks needed in the fat
            for (int i = 0; i < num_blocks_needed - 1; i++) {
                fat_table[free_blocks[i]] = htonl(free_blocks[i+1]); 
            }
            fat_table[free_blocks[num_blocks_needed - 1]] = htonl(0xFFFFFFFF); 
            
            // create directory entry in current directory for the new directory and update the put_file_dir_address and dir_entry to this new location
            int dir_entry_found = 0;
            for (int i = 0; i < superblock->root_dir_block_count * superblock->block_size / sizeof(struct dir_entry_t); i++) {
                if (dir_entry[i].status == 0x00) {
                    dir_entry_found = 1;
                    dir_entry[i].status = 0x05;
                    dir_entry[i].starting_block = htonl(first_free_block);
                    dir_entry[i].block_count = htonl(num_blocks_needed);
                    dir_entry[i].size = htonl(0);
                    
                    time_t curr_time = time(NULL);
                    struct tm *time_struct = localtime(&curr_time);
                    uint16_t year = (uint16_t)(1900 + time_struct->tm_year);
                    struct dir_entry_timedate_t now = {ntohs(year), time_struct->tm_mon+1, time_struct->tm_mday, time_struct->tm_hour, time_struct->tm_min, time_struct->tm_sec};
                    dir_entry[i].create_time = now;
                    dir_entry[i].modify_time = now;

                    strncpy((char*)dir_entry[i].filename, token, 31);
                    dir_entry[i].filename[30] = '\0';
                    memset(dir_entry[i].unused, 0xFF, sizeof(dir_entry[i].unused));
                    
                    break;
                }
            }

            if (!dir_entry_found) {
                printf("Current directory is full. Cannot create a new file entry.\n");
                return -1;
            }

            put_file_dir_address = (uint8_t*)fs_address + first_free_block * superblock->block_size;
            dir_entry = (struct dir_entry_t*)put_file_dir_address;
        }
        
        prev_token = token;
        token = next_token;
    }

    // figure out how many blocks I need for the file 
    int num_blocks_needed = (put_file_buffer.st_size + superblock->block_size - 1) / superblock->block_size; 
    
    // assign fat_table pointer to point to its mapped space in memory 
    int fat_size = superblock->block_size * superblock->fat_block_count;
    uint32_t* fat_table = (uint32_t*)((uint8_t*)fs_address + superblock->fat_start_block * superblock->block_size);

    // search the fat for free blocks 
    int first_free_block = 0;
    int free_block_count = 0;
    for (int i = 0; i < fat_size / sizeof(uint32_t); i++) {
        if (fat_table[i] == 0x00000000) {
            if (free_block_count == 0) {
                first_free_block = i;
            }
            free_block_count++;

            if (free_block_count == num_blocks_needed) {
                break;
            }
        } else {
            free_block_count = 0;
            first_free_block = 0;
        }
    }

    if (free_block_count < num_blocks_needed) {
        printf("Not enough contiguous free blocks available.\n");
        return -1;
    }

    int free_blocks[num_blocks_needed];
    for (int i = 0; i < num_blocks_needed; i++) {
        free_blocks[i] = first_free_block + i;
    }

    // allocate the free blocks needed in the fat
    for (int i = 0; i < num_blocks_needed - 1; i++) {
        fat_table[free_blocks[i]] = htonl(free_blocks[i+1]);
    }
    fat_table[free_blocks[num_blocks_needed - 1]] = htonl(0xFFFFFFFF);

    // write the file to be put in the file system into the allocated blocks 
    int remaining_bytes = put_file_buffer.st_size;
    for (int i = 0; i < num_blocks_needed; i++) {
        void* block_address = (uint8_t*)fs_address + free_blocks[i] * superblock->block_size;
        int bytes_to_write = remaining_bytes > superblock->block_size ? superblock->block_size : remaining_bytes;
        memcpy(block_address, (uint8_t*)put_file_address + i * superblock->block_size, bytes_to_write);
        remaining_bytes -= bytes_to_write;
    }

    int dir_entry_found = 0;
    
    // add directory entries for the new file 
    for (int i = 0; i < superblock->root_dir_block_count * superblock->block_size / sizeof(struct dir_entry_t); i++) {
        if (dir_entry[i].status == 0x00) {
            dir_entry_found = 1;
            dir_entry[i].status = 0x03;
            dir_entry[i].starting_block = htonl(first_free_block);
            dir_entry[i].block_count = htonl(num_blocks_needed);
            dir_entry[i].size = htonl(put_file_buffer.st_size);
            
            time_t curr_time = time(NULL);
            struct tm *time_struct = localtime(&curr_time);
            uint16_t year = (uint16_t)(1900 + time_struct->tm_year);
            struct dir_entry_timedate_t now = {ntohs(year), time_struct->tm_mon+1, time_struct->tm_mday, time_struct->tm_hour, time_struct->tm_min, time_struct->tm_sec};
            dir_entry[i].create_time = now;
            dir_entry[i].modify_time = now;


            strncpy((char*)dir_entry[i].filename, new_filename, 31);
            dir_entry[i].filename[30] = '\0';

            memset(dir_entry[i].unused, 0xFF, sizeof(dir_entry[i].unused));
            break;
        }
    }

    if (!dir_entry_found) {
        printf("Root directory is full. Cannot create a new file entry.\n");
        return -1;
    }

    // restore original endianness in the file 
    superblock->block_size = htons(superblock->block_size);
    superblock->file_system_block_count = htonl(superblock->file_system_block_count);
    superblock->fat_start_block = htonl(superblock->fat_start_block);
    superblock->fat_block_count = htonl(superblock->fat_block_count);
    superblock->root_dir_start_block = htonl(superblock->root_dir_start_block);
    superblock->root_dir_block_count = htonl(superblock->root_dir_block_count);

    msync(fs_address, fs_buffer.st_size, MS_SYNC); //? whats this doing
    munmap(put_file_address, put_file_buffer.st_size);
    close(put_fd);
    munmap(fs_address, fs_buffer.st_size);
    close(fs_fd);

    return 0;
}


