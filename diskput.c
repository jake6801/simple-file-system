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


    // check root directory for reference to subdirectory 
        // if it doesnt exist then create it and reserve 8 blocks for it //! how many should we be reserving for subdirectories?
        // if it does create the directory entry in here instead 
    
    // breakup location into directories - ex. /subdir/subdir2/foo3.txt 
        // tokenize it delimited by /, if its only one token then this is the file name and going in root dir, if its multiple 
    char *token = strtok(put_location, "/");
    char *prev_token = NULL;
    // have a pointer for the location to search through, first being root, then update this as we dive deeper into subdirs
    void* put_file_dir_address = (uint8_t*)fs_address + superblock->root_dir_start_block * superblock->block_size;
    printf("root dir address: %p\n", put_file_dir_address);
    struct dir_entry_t* dir_entry = (struct dir_entry_t*)put_file_dir_address;
    char *new_filename;
    while (token != NULL) {  
        char *next_token = strtok(NULL, "/");
        bool found_dir = false;        
        printf("curr token: %s\n", token);
        // if next_token == NULL then file is getting put in this directory under the name token
        if (next_token == NULL) {    
            printf("In correct dir, getting new file name\n");        
            new_filename = token;
            found_dir = true; // do i need this if im already breaking out of the while loop?
            break;
        }
        // if next_token != NULL then this is a subdir so search through root 
        for (int i = 0; i < superblock->root_dir_block_count * superblock->block_size / sizeof(struct dir_entry_t); i++) { //! change block count based on directory were searching through 
            printf("Searching for %s\n", token);
            // if the subdir exists
            if (dir_entry[i].status == 0x05 && strcmp((char *)dir_entry[i].filename, token) == 0) {
                printf("Found %s at block %d\n", dir_entry[i].filename, ntohl(dir_entry[i].starting_block));
                put_file_dir_address = (uint8_t*)fs_address + ntohl(dir_entry[i].starting_block) * superblock->block_size;
                printf("updating address to %p\n", put_file_dir_address);
                dir_entry = (struct dir_entry_t*)put_file_dir_address;
                found_dir = true;
                break;
            }
        }
        // subdir doesnt exist, create it 
        if (!found_dir) {
            printf("Did not find subdir, creating new subdir\n");
            // search through fat table for 8?? free blocks and reserve them in the fat table
            //! should create a function that does this so I can just call it in here and later rather than pasting same code twice 
            int num_blocks_needed = 8;
            int fat_size = superblock->block_size * superblock->fat_block_count;
            uint32_t* fat_table = (uint32_t*)((uint8_t*)fs_address + superblock->fat_start_block * superblock->block_size);

            int first_free_block = 0;
            int free_block_count = 0;
            for (int i = 0; i < fat_size / sizeof(uint32_t); i++) {
                if (fat_table[i] == 0x00000000) {
                    if (free_block_count == 0) {
                        first_free_block = i;
                        printf("First free block = %d\n", first_free_block);
                    }
                    free_block_count++;
                    printf("next free block = %d\n", i);

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
                printf("free block: %d\n", first_free_block + i);
            }

            // allocate the free blocks needed in the fat
            for (int i = 0; i < num_blocks_needed - 1; i++) {
                fat_table[free_blocks[i]] = htonl(free_blocks[i+1]); //! do i assign this or do i set them all to reserved - 0x00000001?
                printf("assigning fat table at block %d the value %x\n", free_blocks[i], htonl(free_blocks[i]));
            }
            fat_table[free_blocks[num_blocks_needed - 1]] = htonl(0xFFFFFFFF); //! do you still do this for directories?
            printf("assigning fat table at block %d the value %x\n", free_blocks[num_blocks_needed - 1], htonl(0xFFFFFFFF));
            
            // create directory entry in current directory for the new directory and update the put_file_dir_address and dir_entry to this new location
            int dir_entry_found = 0;
            for (int i = 0; i < superblock->root_dir_block_count * superblock->block_size / sizeof(struct dir_entry_t); i++) {
                if (dir_entry[i].status == 0x00) {
                    printf("Found free block in directory at %d and inserting directory entry\n", i);
                    printf("dir_entry[i] is at %p\n", dir_entry + i*64);
                    dir_entry_found = 1;
                    dir_entry[i].status = 0x05;
                    dir_entry[i].starting_block = htonl(first_free_block);
                    dir_entry[i].block_count = htonl(num_blocks_needed);
                    dir_entry[i].size = htonl(0);
                    
                    //TODO fix this to be actual time stamps 
                    struct dir_entry_timedate_t now = {2024, 11, 19, 12, 0, 1};
                    dir_entry[i].create_time = now;
                    dir_entry[i].modify_time = now;


                    strncpy((char*)dir_entry[i].filename, token, 31);
                    dir_entry[i].filename[30] = '\0';
                    memset(dir_entry[i].unused, 0xFF, sizeof(dir_entry[i].unused));
                    
                    // put_file_dir_address = (uint8_t*)fs_address + first_free_block * superblock->block_size;
                    // dir_entry = (struct dir_entry_t*)put_file_dir_address;
                    break;
                }
            }

            if (!dir_entry_found) {
                printf("Current directory is full. Cannot create a new file entry.\n");
                return -1;
            }

            put_file_dir_address = (uint8_t*)fs_address + first_free_block * superblock->block_size;
            printf("updating address to %p\n", put_file_dir_address);
            printf("resetting address to block %d\n", first_free_block);
            dir_entry = (struct dir_entry_t*)put_file_dir_address;
        }
        
        prev_token = token;
        token = next_token;
    }

    // figure out how many blocks I need for the file 
    int num_blocks_needed = (put_file_buffer.st_size + superblock->block_size - 1) / superblock->block_size; 

    // find free blocks in the fat
        // read groups of 8 bits to find the first free blocks that is enough for the file 
            // allocate the blocks, then write into them with the file, then write directory entry in root 
    
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

    // // assign root directory pointer to point to its mapped space in memory
    // void* root_dir_address = (uint8_t*)fs_address + superblock->root_dir_start_block * superblock->block_size;
    // struct dir_entry_t* dir_entry = (struct dir_entry_t*)root_dir_address;
    int dir_entry_found = 0;
    
    // add directory entries for the new file 
    for (int i = 0; i < superblock->root_dir_block_count * superblock->block_size / sizeof(struct dir_entry_t); i++) {
        if (dir_entry[i].status == 0x00) {
            dir_entry_found = 1;
            dir_entry[i].status = 0x03;
            dir_entry[i].starting_block = htonl(first_free_block);
            dir_entry[i].block_count = htonl(num_blocks_needed);
            dir_entry[i].size = htonl(put_file_buffer.st_size);
            
            //TODO fix this to be actual time stamps 
            struct dir_entry_timedate_t now = {2024, 11, 19, 12, 0, 1};
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



    
    // check if specified directory exists in FAT image 
    //TODO how the fuck do I do this when I dont even know how to see one with multiple directories and files 
    // check through the root directory entries to see if the directory exists
    //     if so go to this directory and find the next available block
    //     else create this directory by updating root directory with new entry for this directory in next available block and then put file in this new directory by creating entry and putting it in next available block?

    //* if its going in root directory, just put new directory entry in directory and file in next open block?
    // create new directory entry 
    //! struct dir_entry_t *directory_entry = malloc(sizeof(struct dir_entry_t));

    //* if its not going in root directory, check if subdirectory already exists by browsing root directory entries?
        // if it does exist, create new directory entry for this subdirectory for the file 
        // else, create new directory entry for root directory, then create new directory entry for this subdirectory for the file 

    

    // check if disk has enough space to store the file 

    // create a new directory entry in the given path 

    // go through the FAT entries to find unused sectors in disk and copy the file content to these sectors 

    // update the first logical cluster number and file size field of directory entry we just created and update the FAT entries we used 

    return 0;
}

