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

// prints the directories contents such as files and directories 
void print_directory_info(int file_descriptor, char *list_location, struct superblock_t superblock, int start_block, int block_count) {    
    // read the blocks in the file corresponding to the directory  
    int directory_size = block_count * superblock.block_size;
    struct dir_entry_t *directory = malloc(directory_size);
    lseek(file_descriptor, start_block * superblock.block_size, SEEK_SET);
    read(file_descriptor, directory, directory_size);
    int num_entries = directory_size / sizeof(struct dir_entry_t);
    for (int i = 0; i < num_entries; i++) {
        struct dir_entry_t *directory_entry = &directory[i];
        if (directory_entry->status == 0x00) {
            continue;
        } // File type
        char type;
        if (directory_entry->status == 0x03) {
            type = 'F'; // Regular file
        } else if (directory_entry->status == 0x05) {
            type = 'D'; // Directory
            // continue;
        } else {
            type = '?'; // Unknown
            printf("Uknown file type\n");
            continue;
        }

        int file_size = ntohl(directory_entry->size);

        // Print entry details
        printf("%c %10d %30s ", type, file_size, directory_entry->filename);
        printf("%d/%d/%d %.2d:%.2d:%.2d", ntohs(directory_entry->create_time.year), directory_entry->create_time.month, directory_entry->create_time.day, directory_entry->create_time.hour, directory_entry->create_time.minute, directory_entry->create_time.second);
        printf("\n");
    }
    free(directory);
}

// locate the address of the final directory in given path 
void find_dir(int file_descriptor, char *list_location, struct superblock_t superblock) {
    // get root block information
    int current_block = superblock.root_dir_start_block;
    int current_block_count = superblock.root_dir_block_count;

    // while theres subdirectories to find in the list_location path, loop through list_location until arriving at destination directory or determining it doesnt exist 
    char *token;
    token = strtok(list_location, "/");
    while(token!=NULL) {
        // read directory from file into memory 
        int directory_size = current_block_count * superblock.block_size;
        struct dir_entry_t *directory = malloc(directory_size);
        lseek(file_descriptor, current_block * superblock.block_size, SEEK_SET);
        read(file_descriptor, directory, directory_size);

        // traverse through directory entries 
        int dir_found = 0;
        int num_entries = directory_size / sizeof(struct dir_entry_t);
        for (int i = 0; i < num_entries; i++) {
            struct dir_entry_t *entry = &directory[i];
            // if directory entry matching subdirectory is found, break out of for loop 
            if(entry->status == 0x05 && strcmp((char *)entry->filename, token) == 0) {
                current_block = ntohl(entry->starting_block);
                current_block_count = ntohl(entry->block_count);
                dir_found = 1;
                break;
            }
        }        
        free(directory);

        // if directory wasnt found then return, otherwise move to the next directory in path 
        if (!dir_found) {
            printf("No matching directory found.\n"); 
            return;
        }
        token = strtok(NULL, "/");
    }
    // once found, call print_directory_info to print all the directory information
    print_directory_info(file_descriptor, list_location, superblock, current_block, current_block_count);
}

int main(int argc, char *argv[]) {    
    char *fs_file_name = argv[1];
    char *list_location = argv[2];
    // read first 30 bytes into superblock struct
    struct superblock_t superblock = {0}; 
    int file_descriptor = open(fs_file_name, O_RDONLY); 
    read(file_descriptor, &superblock, sizeof(superblock));    

    // convert all ints into big endian 
    superblock.block_size = ntohs(superblock.block_size);
    superblock.file_system_block_count = ntohl(superblock.file_system_block_count);
    superblock.fat_start_block = ntohl(superblock.fat_start_block);
    superblock.fat_block_count = ntohl(superblock.fat_block_count);
    superblock.root_dir_start_block = ntohl(superblock.root_dir_start_block);
    superblock.root_dir_block_count = ntohl(superblock.root_dir_block_count);

    // read in groups of 64 bytes to get this information? 
    find_dir(file_descriptor, list_location, superblock);

    close(file_descriptor);
    
    return 0;
}
