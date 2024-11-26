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


// if its a subdir, loop through until arriving at subdir, search through directory entries, find file, go to its  starting block and track number of blocks and open the file on host curr dir and write file into it
int main(int argc, char *argv[]) {
    char *fs_file_name = argv[1];
    char *get_location = argv[2];
    char *get_file_name = argv[3];

    // open file system descriptor and map it as a copy to memory 
    int fs_fd = open(fs_file_name, O_RDONLY);
    struct stat fs_buffer;
    int fs_status = fstat(fs_fd, &fs_buffer);
    void* fs_address = mmap(NULL, fs_buffer.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fs_fd, 0);
    
    struct superblock_t *superblock = (struct superblock_t *)fs_address;
    superblock->block_size = ntohs(superblock->block_size);
    superblock->file_system_block_count = ntohl(superblock->file_system_block_count);
    superblock->fat_start_block = ntohl(superblock->fat_start_block);
    superblock->fat_block_count = ntohl(superblock->fat_block_count);
    superblock->root_dir_start_block = ntohl(superblock->root_dir_start_block);
    superblock->root_dir_block_count = ntohl(superblock->root_dir_block_count);

    // locate the file specified in input 
    char *token = strtok(get_location, "/");
    void* get_file_dir_address = (uint8_t*)fs_address + superblock->root_dir_start_block * superblock->block_size;
    struct dir_entry_t* dir_entry = (struct dir_entry_t*)get_file_dir_address;
    while (token != NULL) {
        char *next_token = strtok(NULL, "/");
        bool found_file = false;
        bool found_dir = false;
        
        // in correct directory, look for file 
        if (next_token == NULL) { 
            for (int i = 0; i < superblock->block_size * superblock->root_dir_block_count / sizeof(struct dir_entry_t); i++) {
                if (dir_entry[i].status == 0x03 && strcmp((char *)dir_entry[i].filename, token) == 0) {
                    FILE *get_file = fopen(get_file_name, "wb");

                    // get the file information to locate the address in the file system 
                    uint32_t starting_block = ntohl(dir_entry[i].starting_block);
                    uint32_t block_count = ntohl(dir_entry[i].block_count);
                    uint32_t file_size = ntohl(dir_entry[i].size);
                    
                    // get file address in file system and copy the file to host OS current directory 
                    get_file_dir_address = (uint8_t*)fs_address + starting_block * superblock->block_size;
                    uint32_t total_bytes_to_copy = file_size;
                    for (uint32_t block = 0; block < block_count && total_bytes_to_copy > 0; block++) {
                        void *block_address = (uint8_t *)get_file_dir_address + block * superblock->block_size;                        
                        size_t bytes_to_copy;
                        if (total_bytes_to_copy < superblock->block_size) {
                            bytes_to_copy = total_bytes_to_copy;
                        } else {
                            bytes_to_copy = superblock->block_size;
                        }
                        fwrite(block_address, 1, bytes_to_copy, get_file);
                        total_bytes_to_copy -= bytes_to_copy;
                    }
                    fclose(get_file);
                    close(fs_fd);
                    munmap(fs_address, fs_buffer.st_size);
                    return 0;                   
                }
            }

            if (!found_file) {
                // didnt find the file in the specified location
                printf("File not found.\n");          
                munmap(fs_address, fs_buffer.st_size);
                close(fs_fd);
                return 1;
            }
        }
        // look for correct directory 
        else {
            for (int i = 0; i < superblock->root_dir_block_count * superblock->block_size / sizeof(struct dir_entry_t); i++) {
                // if found, update address pointer and break out of for loop 
                if (dir_entry[i].status == 0x05 && strcmp((char *)dir_entry[i].filename, token) == 0) {
                    get_file_dir_address = (uint8_t*)fs_address + ntohl(dir_entry[i].starting_block) * superblock->block_size;                    
                    dir_entry = (struct dir_entry_t*)get_file_dir_address;
                    found_dir = true;
                    break;
                }
            }
            // if not found, close and unmap files and return
            if (!found_dir) {
                printf("File not found.\n");          
                munmap(fs_address, fs_buffer.st_size);
                close(fs_fd);
                return 1;
            }
        }

        token = next_token;
    }
    munmap(fs_address, fs_buffer.st_size);
    close(fs_fd);
    return 1;
}