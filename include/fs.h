#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISK_SIZE 0xFFFF   // 64Kib
#define BLOCK_SIZE 0x0200  // 512b

#define DIR_MAX_SIZE 32
#define BITMAP_SIZE 16

#define MAX_FILE_SIZE 0x2800 // 10Kib
#define MAX_FILENAME_SIZE 15 

#define FILE_HEADER_SIZE 16

#define MAX_FILE_BLOCKS 64
#define MAX_DIR_FILES 16

#define FREE_BLOCK(OFFSET) fsmeta->bitmap[OFFSET/8] &= ~(1 << (OFFSET % 8))
#define ALLOC_BLOCK(OFFSET) fsmeta->bitmap[OFFSET/8] |= (1 << (OFFSET % 8))

typedef struct {
    short disk_size;
    short block_size;
    short block_quantity;
    short bitmap_addr;
    short iNODE_table_addr;
    short iNODES_addr;
    short data_addr;
    short last_inode_write_addr;

    char bitmap[16];
    unsigned char root_inode;
} meta;

#pragma pack(push,1)
struct tuple {
    short baseaddr; 
    short offset;
};

typedef struct {
    char filetype;
    char filename[MAX_FILENAME_SIZE];
    struct tuple* dataseg;
} iNODE;
#pragma pack(pop)

unsigned char inode(char* filename, size_t len);
int create_fs(FILE*);
int read_meta_fs(meta* fsmeta, FILE* disk);
int write_file_fs(char* filename, FILE* disk, meta* fsmeta);
int write_dir_fs(char* dirname, FILE* disk, meta* fsmeta);
int write_content_fs(char* filename, char* buffer, FILE* disk, meta* fsmeta);
int remove_fs(char* dirname, char* filename, FILE* disk, meta* fsmeta);
int rename_fs(char* oldfilename, char* newfilename, FILE* disk, meta* fsmeta);
int move_fs(char* dest, char* src, char* filename, FILE* disk, meta* fsmeta);
int read_fs(char* dirname, char* filename, FILE* disk, meta* fsmeta);

