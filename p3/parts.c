#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <time.h>

//Super block
struct __attribute__((__packed__)) superblock_t {
 uint8_t fs_id [8];
 uint16_t block_size;
 uint32_t block_count;
 uint32_t fat_starts;
 uint32_t fat_blocks;
 uint32_t root_dir_starts;
 uint32_t root_dir_blocks;
};

//FAT block
struct fat_t {
 int free_blocks;
 int reserved_blocks;
 int allocated_blocks;
};

// Time and Date Entry Struct
struct __attribute__((__packed__)) dir_entry_timedate_t {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

//Directory entry
struct __attribute__((__packed__)) dir_entry_t {
    uint8_t status;
    uint32_t starting_block;
    uint32_t block_count;
    uint32_t size;
    struct dir_entry_timedate_t create_time;
    struct dir_entry_timedate_t modify_time;
    uint8_t filename[31];
    uint8_t unused[6];
};

uint32_t convInt32(char* adr);
uint16_t convInt16(char* adr);
void printDir(struct dir_entry_t* root_dir);
int tokenizePath(char* path, char* tokens[]);
void setFileAttributes(struct dir_entry_t* root_dir, const char* file_name, int starting_block, int file_size) ;
void setModifyTime(struct dir_entry_timedate_t* modify_time);

struct superblock_t sb;
struct fat_t ft;
char* address;

// convert 4-byte character entry to uint32_t in host byte order
uint32_t convInt32(char* adr){
    return ntohl(*(uint32_t*)adr);

}

// convert 2-byte character entry to uint16_t in host byte order
uint16_t convInt16(char* adr){
    return htons(*(uint16_t*)adr);

}

//print information about the file system 
void diskinfo(int argc, char* argv[]){

    if (argc != 2) {
        printf("Usage: %s <file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int cur_adr = sb.fat_starts * sb.block_size;
    int i =0;
    while (i < sb.block_count){             //parse data regarding blocks
        int cur_adr_val = convInt32(address + cur_adr);

        if (cur_adr_val == 0){
            ft.free_blocks++;
        } else if (cur_adr_val == 1){
            ft.reserved_blocks++;
        } else{
            ft.allocated_blocks++;
        }
        i++;
        cur_adr += 4;

    }

    printf("Super block information:\n"    
			"Block size: %d\n"
			"Block count: %d\n"
			"FAT starts: %d\n"
			"FAT blocks: %d\n"
			"Root directory start: %d\n"
			"Root directory blocks: %d\n\n"
			"FAT information:\n"
			"Free Blocks: %d\n"
			"Reserved Blocks: %d\n"
			"Allocated Blocks: %d\n",
			sb.block_size, sb.block_count, sb.fat_starts, sb.fat_blocks, sb.root_dir_starts, sb.root_dir_blocks, ft.free_blocks,
			ft.reserved_blocks, ft.allocated_blocks);

}

// Function to tokenize a path and return the count of tokens
int tokenizePath(char* path, char* tokens[]) {
    int token_count = 0;

    char* args = strtok(path, "/");
    int i = 0;
    while (args) {
        tokens[i++] = args;
        args = strtok(NULL, "/");
    }
    tokens[i] = NULL;

    while (tokens[token_count] != NULL) {
        token_count++;
    }

    return token_count;
}

//print the contents of a directory
void disklist(int argc, char* argv[]){            

    if (argc < 3) {
        printf("Usage: %s <file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* dir_name = argv[2];
    char* tokens[128];
    int dir_count = tokenizePath(dir_name, tokens);

    int cur_adr = sb.root_dir_starts * sb.block_size;
    struct dir_entry_t* root_dir;
    int count;

    if (argc == 3 && (strcmp(dir_name, "/") != 0)){            
        for (int i = 0; i < dir_count; i++){          // Traverses through directory entries to find a matching filename
            for (int j = cur_adr; j < cur_adr + sb.block_size; j+= 64){
                root_dir = (struct dir_entry_t*)(address + j);
                char* file_name = (char*)root_dir->filename;

                if (strcmp(file_name, tokens[count]) == 0){
                    cur_adr = ntohl(root_dir->starting_block) * sb.block_size;
                    count++;
                    break;
                }
            }

        }

    }

    if (argc == 2 || count == dir_count || (strcmp(dir_name, "/") == 0)){
        for (int i = cur_adr; i <= cur_adr + sb.block_size; i+=64){
            root_dir = (struct dir_entry_t*)(address + i);
            if (ntohl(root_dir->size) == 0) {
		 		continue;      
			}
    		printDir(root_dir);

        }

    }

}

//print information about a directory entry
void printDir(struct dir_entry_t* root_dir) {    

    if (root_dir->status == 5) {
        printf("D");
    } else {
        printf("F");
    }          
    printf("%10d %30s %4d/%02d/%02d %02d:%02d:%02d\n",
    
	ntohl(root_dir->size),
	root_dir->filename,	    
	htons(root_dir->modify_time.year),
	root_dir->modify_time.month,
	root_dir->modify_time.day,
	root_dir->modify_time.hour,
	root_dir->modify_time.minute,
	root_dir->modify_time.second); 
}

//copy file from the file system to the local file system
void diskget(int argc, char* argv[]){

    if (argc < 4) {
        printf("Usage: %s <file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char* dir_name = argv[2];
    char* tokens[128];
    int dir_count = tokenizePath(dir_name, tokens);

    int cur_adr = sb.root_dir_starts * sb.block_size;
    struct dir_entry_t* root_dir;
    int count = 0;

    for (int i = 0; i < dir_count; i++) {           // Traverses through directory entries to find a matching filename
        for (int j = cur_adr; j < cur_adr + sb.block_size; j += 64) {
            root_dir = (struct dir_entry_t*)(address + j);
            char* file_name = (char*)root_dir->filename;

            if (strcmp(file_name, tokens[count]) == 0) {
                count++;
                cur_adr = ntohl(root_dir->starting_block) * sb.block_size;
                break;
            }
        }
    }

    if (count != dir_count) {
        printf("File not found.\n");
        exit(EXIT_FAILURE);
    }

    FILE* dest_file = fopen(argv[3], "wb");        // Open the destination file for writing
    if (!dest_file) {
        perror("Error opening destination file");
        exit(EXIT_FAILURE);
    }

    for (int block = ntohl(root_dir->starting_block); block < ntohl(root_dir->starting_block) + ntohl(root_dir->block_count); ++block) {
        fwrite(address + block * sb.block_size, 1, sb.block_size, dest_file);
      
    }

    fclose(dest_file);

}

//copy a file from the local file system to the file system
void diskput(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage: %s <file_system_img> <source_file> <destination_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE* src_file = fopen(argv[2], "rb");       //Open the destination file for reading.
    if (!src_file) {
        printf("File not found.\n");
        exit(EXIT_FAILURE);
    }

    fseek(src_file, 0, SEEK_END);
    int src_file_size = ftell(src_file);      //source file size
    fseek(src_file, 0, SEEK_SET);

    if (src_file_size <= 0) {
        printf("Source file is empty.\n");
        fclose(src_file);
        exit(EXIT_FAILURE);
    }

    char* dest_path = argv[3];
    char* tokens[128];
    int dir_count = tokenizePath(dest_path, tokens);

    int cur_adr = sb.root_dir_starts * sb.block_size;
    struct dir_entry_t* root_dir;
    int count = 0;

    for (int i = 0; i < dir_count - 1; i++) {        // Iterate through directory entries to find a matching directory name          
        for (int j = cur_adr; j < cur_adr + sb.block_size; j += 64) {
            root_dir = (struct dir_entry_t*)(address + j);
            char* dir_name = (char*)root_dir->filename;

            if (strcmp(dir_name, tokens[count]) == 0 && root_dir->status == 5) {
                count++;
                cur_adr = ntohl(root_dir->starting_block) * sb.block_size;
                break;
            }
        }
    }

    if (count != dir_count - 1) {
        printf("Destination path does not exist.\n");
        fclose(src_file);
        exit(EXIT_FAILURE);
    }

    int free_entry_index = -1;
    for (int j = cur_adr; j < cur_adr + sb.block_size; j += 64) {      //Check if file already exists
        root_dir = (struct dir_entry_t*)(address + j);
        char* file_name = (char*)root_dir->filename;

        if (strcmp(file_name, tokens[dir_count - 1]) == 0) {
            printf("File already exists in the destination directory. Finding another free entry.\n");
            free_entry_index = -1;
            break;
        }

        if (root_dir->status == 0) {
            free_entry_index = j;
        }
    }

    if (free_entry_index == -1) {
        printf("No free entry found in the destination directory. Aborting.\n");
        fclose(src_file);
        exit(EXIT_FAILURE);
    }

    int first_data_block = 0;
    for (int i = 0; i < sb.block_count; ++i) {
        int cur_adr_val = convInt32(address + sb.fat_starts * sb.block_size + i * 4);
        if (cur_adr_val == 0) {
            first_data_block = i;
            break;
        }
    }

    int current_fat_block = first_data_block;       //update FAT 
    for (int i = 0; i < src_file_size / sb.block_size + 1; ++i) {
        int next_fat_block = (i == src_file_size / sb.block_size) ? 0 : first_data_block + i + 1;
        if (next_fat_block == 0) {
        // Set the last block in the file
            *(uint32_t*)(address + sb.fat_starts * sb.block_size + current_fat_block * 4) = htonl(0xFFFFFFFF);
        } else {
        // Set a regular allocated block
            *(uint32_t*)(address + sb.fat_starts * sb.block_size + current_fat_block * 4) = htonl(next_fat_block);
        }

        ft.free_blocks--;        // Decrease the count of free blocks
        ft.allocated_blocks++; 
    }

    if (free_entry_index == -1) {
        root_dir = (struct dir_entry_t*)(address + cur_adr);
    } else {
        root_dir = (struct dir_entry_t*)(address + free_entry_index);
    }

    setFileAttributes(root_dir, tokens[dir_count - 1], first_data_block, src_file_size);

    for (int i = first_data_block; i < first_data_block + src_file_size / sb.block_size + 1; ++i) {
        size_t bytesRead = fread(address + i * sb.block_size, 1, sb.block_size, src_file);

         if (bytesRead < sb.block_size) {     // Check for the end of the file

        memset(address + i * sb.block_size + bytesRead, 0, sb.block_size - bytesRead);    // Fill the remaining space in the block with zeros
        fseek(src_file, i * sb.block_size + bytesRead, SEEK_SET);         // Move the file pointer back to the beginning of the block
     }
    }

    fclose(src_file);
}

//set file attributes in a directory entry
void setFileAttributes(struct dir_entry_t* root_dir, const char* file_name, int starting_block, int file_size) {
    strcpy((char*)root_dir->filename, file_name);
    root_dir->status = 3;
    root_dir->starting_block = htonl(starting_block);
    root_dir->block_count = htonl(file_size / sb.block_size + 1);
    root_dir->size = htonl(file_size);
    setModifyTime(&root_dir->modify_time);
}

//set the modify time in a directory entry
void setModifyTime(struct dir_entry_timedate_t* modify_time) {
    time_t current_time = time(NULL);
    struct tm* time_info = localtime(&current_time);

    modify_time->year = htons(time_info->tm_year + 1900);
    modify_time->month = time_info->tm_mon + 1;
    modify_time->day = time_info->tm_mday;
    modify_time->hour = time_info->tm_hour;
    modify_time->minute = time_info->tm_min;
    modify_time->second = time_info->tm_sec;
}

//main
int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Usage: %s <file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int fp = open(argv[1], O_RDWR);     //Open the destination file for reading or writing.
    if (fp == -1){
        printf("Error in opening file.\n");
        exit(EXIT_FAILURE);
    }

    struct stat file_data;
    fstat(fp, &file_data);

    address = mmap(NULL, file_data.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);  //map data to memory.
    if (address == MAP_FAILED) {
        perror("Error mapping file to memory");
        close(fp);
        exit(EXIT_FAILURE);
    }

    sb.block_size = convInt16(address + 8);     
    sb.block_count = convInt32(address + 10);
    sb.fat_starts = convInt32(address + 14);
    sb.fat_blocks = convInt32(address + 18);
    sb.root_dir_starts = convInt32(address + 22);
    sb.root_dir_blocks = convInt32(address + 26);

    #if defined(PART1)
        diskinfo(argc, argv);
    #elif defined(PART2)
        disklist(argc, argv);
    #elif defined(PART3)
        diskget(argc, argv);
    #elif defined(PART4)
        diskput(argc,argv);
    #else
    #   error "PART[1234] must be defined"
    #endif
        munmap(address, file_data.st_size);
        close(fp);
        return 0;
}








































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































   