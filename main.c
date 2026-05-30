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

typedef struct {
    short disk_size;
    short block_size;
    short block_quantity;
    short bitmap_addr;
    short iNODE_table_addr;
    short data_addr;
} meta;

struct tuple {
    short baseaddr; 
    int offset;
};

typedef struct {
    char filetype;
    char filename[16];
    struct tuple* dataseg;
} iNODE;

int create_fs(FILE*);
int read_meta_fs(meta* fsmeta, FILE* disk);
int write_fs(iNODE file, FILE* disk);


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
        fprintf(stderr, "error ao obter metadados do sistema de arquivos!!!\n");
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
        0x0E, 0x00,   // BITMAP
        0x1E, 0x00,   // TABELA iNODE
        0x20, 0x14,   // DADOS

        // BITMAP 16bytes 128 blocos
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00

    };

    // TABELA iNODE (iNODE 0-255) 1Kib 0x001E-0x041E
    memset(skel+0x001E, 0, 1024); // (iNODE, FILEADDR) 

    // iNODES 4Kib 0x041F-0x141F
    memset(skel+0x041F, 0, 4096); // (filetype, filename, (baseaddr, offset)...)

    // DADOS 60383bytes 0x1420-0xFFFF 
    memset(skel+0x1420, 0, 60383);

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
    fsmeta->data_addr = *(short*)(content+12);

    return 0;

}

int write_fs(iNODE file, FILE* disk) {
    
    FILE* fp = fopen(file.filename, "rb");
    if ( fp == NULL ) {
        fprintf(stderr, "arquivo %s não existe", file.filename);
        return 1;
    }

    char* content = malloc(MAX_FILE_SIZE);
    size_t size = fread(content, sizeof(char), MAX_FILE_SIZE, fp);

    size_t len = strlen(file.filename);
    int id = inode(file.filename, len);

    file.dataseg = calloc((size/BLOCK_SIZE), sizeof(struct tuple));




    fclose(fp);

}