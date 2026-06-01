/*
  /bin - executaveis
    ./ls    - listar diretorios
    ./cd    - navegar diretorios
    ./rm    - remover diretorios
    ./cat   - mostrar conteudo de arquivos
    ./touch - criar arquivos
    ./mkdir - criar diretorios
    ./mv    - mover/rename arquivos/diretorios
    ./read  - escreve para um arquivos 

  /dev/sda1 - disco fs
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISK_SIZE 0xFFFF   // 64Kib
#define BLOCK_SIZE 0x0200  // 512b

#define MAX_FILE_SIZE 0x2800 // 10Kib
#define MAX_FILENAME_SIZE 15 

typedef struct {
    short disk_size;
    short block_size;
    short block_quantity;
    short bitmap_addr;
    short iNODE_table_addr;
    short iNODES_addr;
    short data_addr;

    char bitmap[16];
    short last_inode_write_addr;
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

int create_fs(FILE*);
int read_meta_fs(meta* fsmeta, FILE* disk);
int write_file_fs(char* filename, FILE* disk, meta* fsmeta);
int write_dir_fs(char* dirname, FILE* disk, meta* fsmeta);
int remove_fs(char* filename, FILE* disk, meta* fsmeta);

int inode( char* filename, size_t size ) {

    size_t id = 0;

    for ( int i = 0 ; i < size ; i ++ ) {
        id += filename[i];
    }

    return id % 256;    
}

int main( void ) {

    FILE* fp = fopen("./disco", "rb+");
    
    if ( fp == NULL ) {
        
        printf("Criando disco...\n");
        fp = fopen("./disco", "wb+"); 
        if ( fp == NULL ) {
            fprintf(stderr,"Não foi possível criar disco\n");
            return 1;
        }
        
        printf("Criando partição com o sistema de arquivos tinyFS....\n");
        if ( create_fs(fp) == 1 ) {
            fprintf(stderr, "Não foi possivel criar a partição\n");
            fclose(fp);
            return 1;
        }

        printf("disco e partição criados com sucesso!!!\n");

    }

    meta* fsmeta = malloc(sizeof(meta));
    if (read_meta_fs(fsmeta, fp) == 1 ) {
        fprintf(stderr, "erro ao obter metadados do sistema de arquivos!!!\n");
        fclose(fp);
        return 1;
    }

    
    fclose(fp);
    return 0;

}


int create_fs(FILE* fp) {
    char skel[DISK_SIZE] = {

        // 0x0000 - 0x0x000C
        //HEADER 2bytes
        0x46, 0x53,

        //SUPERBLOCO 6bytes
        0xFF, 0xFF,   // tamanho do disco 64kib
        0xFF, 0x03,   // tamanho dos blocos 512b
        0x80, 0x00,   // quantidade de blocos 128
        
        // ENDERECOS 4bytes 
        0x10, 0x00,   // BITMAP
        0x20, 0x00,   // TABELA iNODE
        0x21, 0x04,   // iNODES
        0x22, 0x14,   // DADOS

        // BITMAP 16bytes 128 blocos
        0xFF, 0x07, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00

    };

    // TABELA iNODE (iNODE 0-255) 1Kib 0x0020-0x0420
    memset(skel+0x0020, 0, 1024); // (FILEADDR, OFFSET) 

    // iNODES 4Kib 0x0421-0x1421
    memset(skel+0x0421, 0, 4096); // (filetype, filename, (baseaddr, offset)...)

    // DADOS 59902bytes 0x1601-0xFFFF 
    memset(skel+0x1601, 0, 59902);

    size_t size = fwrite(skel, sizeof(char), DISK_SIZE, fp);
    if ( size != DISK_SIZE )
        return 1;

    return 0;
}
int read_meta_fs(meta* fsmeta, FILE* disk) {

    fseek(disk, 0, SEEK_SET);

    char* content = malloc(DISK_SIZE);
    size_t size = fread(content, sizeof(char), DISK_SIZE, disk);
    if ( size != DISK_SIZE) {
        return 1;
    }

    fsmeta->disk_size = *(short *)(content+2);
    fsmeta->block_size = *(short *)(content+4);
    fsmeta->block_quantity = *(short *)(content+6);
    fsmeta->bitmap_addr = *(short *)(content+8);
    fsmeta->iNODE_table_addr = *(short *)(content+10);
    fsmeta-> iNODES_addr = *(short*)(content+12);
    fsmeta->data_addr = *(short*)(content+14);

    memcpy(fsmeta->bitmap, (content+fsmeta->bitmap_addr), 16);
 
    short i = 0;
    for ( i = fsmeta->iNODES_addr ; *(short*)(content+i) != 0x0000 ; i += 2 );
    
    fsmeta->last_inode_write_addr = i;

    return 0;

}
int write_file_fs(char* filename, FILE* disk, meta* fsmeta) {

    FILE* fp = fopen(filename, "rb");
    if ( fp == NULL ) {
        fprintf(stderr, "arquivo %s não existe\n", filename);
        return 1;
    }

    char* content = malloc(MAX_FILE_SIZE);
    size_t size = fread(content, sizeof(char), MAX_FILE_SIZE, fp);

    fclose(fp);
    
    int id = inode(filename, strlen(filename));

    iNODE file = {};
    file.filetype = 0;
    strncpy(file.filename, filename, 15);

    int max_block_quantity = (size/BLOCK_SIZE) + ((size % BLOCK_SIZE == 0 ) ? 0 : 1);
    file.dataseg = calloc(max_block_quantity, sizeof(struct tuple));

    size_t prev = 0;
    size_t offset = 0;
    char flag = 0; // 0 - ainda n iniciado
                   // 1 - iniciado 
    int block_quantity = 0;
    for (int j = 0; j < max_block_quantity ; j++ ) {

        for ( long bitmap = (*(long*)(fsmeta->bitmap+((offset > 63) ? 8 : 0))); offset < 127 && (bitmap>>offset & 0x1) != 0 ; offset++ );

        if ( offset >= 127 ) {
            fprintf(stderr, "disco cheio: não foi possível salvar o arquivo no disco!\n");
            return 1;
        }

        if ( prev != 0 && prev != (offset-1) ) {
            flag = 0;
            ++block_quantity;
        };

        if ( !flag ) {
            file.dataseg[block_quantity] = (struct tuple) {offset*BLOCK_SIZE,  0};
            flag = 1;
        } 

        file.dataseg[block_quantity].offset++;

        prev = offset;
        offset +=1;
    };

    block_quantity++;

    size_t allocated_data_size = (block_quantity*4)+16;

    // (iNODEaddr, len) na tabela iNODE
    fseek(disk, fsmeta->iNODE_table_addr+id, SEEK_SET);
    fwrite((short[]){fsmeta->last_inode_write_addr, allocated_data_size}, sizeof(short), 2, disk);


    // segmento de dados no iNODES
    fseek(disk, fsmeta->last_inode_write_addr, SEEK_SET);

    char iNODEskel[allocated_data_size]; 
    memcpy(iNODEskel, (char*)&file, 16);
    for( int i = 0 ; i < block_quantity; i++)
        memcpy((iNODEskel+16+(i*4)), (char*)&file.dataseg[i], 4);
    
    fwrite(iNODEskel, sizeof(char), allocated_data_size, disk);


    // escrevendo dados no disco
    for ( int i = 0 ; i < block_quantity ; i ++) {
        fseek(disk, file.dataseg[i].baseaddr, SEEK_SET);
        fwrite(content, sizeof(char), file.dataseg[i].offset*BLOCK_SIZE, disk);
    }

    fsmeta->last_inode_write_addr += allocated_data_size+1;

    return 0;

}

#define DIR_MAX_SIZE 32

int write_dir_fs(char* dirname, FILE* disk, meta* fsmeta) {

    // char filetype
    // char dirname[15]
    // char inodes[16]

    // da entrada na tabela iNODE
    char id = inode(dirname, strlen(dirname));
    fseek(disk, fsmeta->iNODE_table_addr+id, SEEK_SET);
    fwrite((short[]){fsmeta->last_inode_write_addr, DIR_MAX_SIZE}, sizeof(short), 2, disk);


    // escrever meta do arquivo
    iNODE file;
    file.filetype = 1;
    strncpy(file.filename, dirname, MAX_FILENAME_SIZE);

    fseek(disk, fsmeta->last_inode_write_addr, SEEK_SET);
    fwrite((char*)&file, sizeof(char), DIR_MAX_SIZE, disk);

    fsmeta->last_inode_write_addr += DIR_MAX_SIZE+1;

    return 0;
}


int remove_fs(char* filename, FILE* disk, meta* fsmeta) {

    int id = inode(filename, strlen(filename));
    fseek(disk, fsmeta->iNODE_table_addr+id, SEEK_SET);
    fwrite((short[]){0,0}, sizeof(short), 2, disk);

    return 0;
}