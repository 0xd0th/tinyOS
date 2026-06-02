
#include "../include/fs.h"

unsigned char inode( char* filename, size_t len) {

    size_t id = 0;

    for ( int i = 0 ; i < len; i ++ ) {
        id += filename[i];
    }

    return id % 256;    
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

    fsmeta->root_inode = inode("raiz", strlen("raiz"));
    if ( *(short*)(content+0x00D6) == 0 ) {
        write_dir_fs("raiz", disk, fsmeta);
        printf("diretorios root escrito com sucesso!\n");
        free(content);
        return 0;
    }

    fsmeta->last_inode_write_addr += DIR_MAX_SIZE+1;


    free(content);
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
    
    unsigned char id = inode(filename, strlen(filename));

    // salva na raiz
    fseek(disk, fsmeta->iNODE_table_addr+fsmeta->root_inode, SEEK_SET);

    struct tuple table_entry;
    fread(&table_entry, sizeof(struct tuple), 1, disk);

    fseek(disk, table_entry.baseaddr+FILE_HEADER_SIZE, SEEK_SET);
    unsigned char curr_inode = 0;
    for ( int i = 0 ; i < MAX_DIR_FILES ; i ++ ){
        fread(&curr_inode, sizeof(char), 1, disk);
        if (curr_inode == 0){
            fseek(disk, -1, SEEK_CUR); 
            fwrite(&id, sizeof(char), 1, disk);
            break;
        }
    }

    iNODE file = {};
    file.filetype = 0;
    strncpy(file.filename, filename, MAX_FILENAME_SIZE);

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
        ALLOC_BLOCK(offset);

        prev = offset;
        offset +=1;

    };

    block_quantity++;

    size_t allocated_data_size = (block_quantity*4)+FILE_HEADER_SIZE;

    // (iNODEaddr, len) na tabela iNODE
    fseek(disk, fsmeta->iNODE_table_addr+id, SEEK_SET);
    fwrite((short[]){fsmeta->last_inode_write_addr, allocated_data_size}, sizeof(short), 2, disk);


    // segmento de dados no iNODES
    fseek(disk, fsmeta->last_inode_write_addr, SEEK_SET);

    char iNODEskel[allocated_data_size]; 
    memcpy(iNODEskel, (char*)&file, 16);
    for( int i = 0 ; i < block_quantity; i++)
        memcpy((iNODEskel+FILE_HEADER_SIZE+(i*4)), (char*)&file.dataseg[i], 4);
    
    fwrite(iNODEskel, sizeof(char), allocated_data_size, disk);


    // escrevendo dados no disco
    for ( int i = 0 ; i < block_quantity ; i ++) {
        fseek(disk, file.dataseg[i].baseaddr, SEEK_SET);
        fwrite((content+(i*BLOCK_SIZE)), sizeof(char), file.dataseg[i].offset*BLOCK_SIZE, disk);
    }


    fsmeta->last_inode_write_addr += allocated_data_size+1;

    // atualizando o bitmap
    fseek(disk, fsmeta->bitmap_addr, SEEK_SET);
    fwrite(fsmeta->bitmap, sizeof(char), BITMAP_SIZE, disk);

    free(file.dataseg);
    free(content);
    return 0;

}

int write_content_fs(char* filename, char* buffer, FILE* disk, meta* fsmeta) {

    size_t size = strlen(buffer);

    unsigned char id = inode(filename, strlen(filename));

    // salva na raiz
    fseek(disk, fsmeta->iNODE_table_addr+fsmeta->root_inode, SEEK_SET);

    struct tuple table_entry;
    fread(&table_entry, sizeof(struct tuple), 1, disk);

    fseek(disk, table_entry.baseaddr+FILE_HEADER_SIZE, SEEK_SET);
    unsigned char curr_inode = 0;
    for ( int i = 0 ; i < MAX_DIR_FILES ; i ++ ){
        fread(&curr_inode, sizeof(char), 1, disk);
        if (curr_inode == 0){
            fseek(disk, -1, SEEK_CUR); 
            fwrite(&id, sizeof(char), 1, disk);
            break;
        }
    }

    iNODE file = {};
    file.filetype = 0;
    strncpy(file.filename, filename, MAX_FILENAME_SIZE);

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
        ALLOC_BLOCK(offset);

        prev = offset;
        offset +=1;

    };

    block_quantity++;

    size_t allocated_data_size = (block_quantity*4)+FILE_HEADER_SIZE;

    // (iNODEaddr, len) na tabela iNODE
    fseek(disk, fsmeta->iNODE_table_addr+id, SEEK_SET);
    fwrite((short[]){fsmeta->last_inode_write_addr, allocated_data_size}, sizeof(short), 2, disk);


    // segmento de dados no iNODES
    fseek(disk, fsmeta->last_inode_write_addr, SEEK_SET);

    char iNODEskel[allocated_data_size]; 
    memcpy(iNODEskel, (char*)&file, 16);
    for( int i = 0 ; i < block_quantity; i++)
        memcpy((iNODEskel+FILE_HEADER_SIZE+(i*4)), (char*)&file.dataseg[i], 4);

    fwrite(iNODEskel, sizeof(char), allocated_data_size, disk);


    // escrevendo dados no disco
    for ( int i = 0 ; i < block_quantity ; i ++) {
        fseek(disk, file.dataseg[i].baseaddr, SEEK_SET);
        fwrite((buffer+(i*BLOCK_SIZE)), sizeof(char), file.dataseg[i].offset*BLOCK_SIZE, disk);
    }


    fsmeta->last_inode_write_addr += allocated_data_size+1;

    // atualizando o bitmap
    fseek(disk, fsmeta->bitmap_addr, SEEK_SET);
    fwrite(fsmeta->bitmap, sizeof(char), BITMAP_SIZE, disk);

    free(file.dataseg);
    return 0;

}

int write_dir_fs(char* dirname, FILE* disk, meta* fsmeta) {

    // char filetype
    // char dirname[15]
    // char inodes[16]

    unsigned char id = inode(dirname, strlen(dirname));

    if( id != fsmeta->root_inode ) {
        // salva na raiz
        fseek(disk, fsmeta->iNODE_table_addr+fsmeta->root_inode, SEEK_SET);

        struct tuple table_entry;
        fread(&table_entry, sizeof(struct tuple), 1, disk);

        fseek(disk, table_entry.baseaddr+FILE_HEADER_SIZE, SEEK_SET);
        unsigned char curr_inode = 0;
        for ( int i = 0 ; i < MAX_DIR_FILES ; i ++ ){
            fread(&curr_inode, sizeof(char), 1, disk);
            if (curr_inode == 0){
                fseek(disk, -1, SEEK_CUR); 
                fwrite(&id, sizeof(char), 1, disk);
                break;
            }
        }
    }


    // da entrada na tabela iNODE
    fseek(disk, fsmeta->iNODE_table_addr+id, SEEK_SET);
    fwrite((short[]){fsmeta->last_inode_write_addr, DIR_MAX_SIZE}, sizeof(short), 2, disk);

    // escrever meta do arquivo
    iNODE file;
    file.filetype = 1;
    strncpy(file.filename, dirname, MAX_FILENAME_SIZE);

    fseek(disk, fsmeta->last_inode_write_addr, SEEK_SET);
    fwrite((char*)&file, sizeof(char), FILE_HEADER_SIZE, disk);

    fsmeta->last_inode_write_addr += DIR_MAX_SIZE+1;

    return 0;
}

int remove_fs(char* dirname, char* filename, FILE* disk, meta* fsmeta) {

    unsigned char id = inode(filename, strlen(filename));
    unsigned char dir_id = inode(dirname, strlen(dirname));

    struct tuple table_entry;
    fseek(disk, fsmeta->iNODE_table_addr+dir_id, SEEK_SET);
    fread(&table_entry, sizeof(struct tuple), 1, disk);
    
    fseek(disk, table_entry.baseaddr+FILE_HEADER_SIZE, SEEK_SET);
    unsigned char curr_inode = 0;
    char flag = 0; // 0 nao esta no diretorio
                  // 1 esta no diretorio
    for ( int i = 0 ; i < MAX_DIR_FILES ; i ++ ){
        fread(&curr_inode, sizeof(char), 1, disk);
        if (curr_inode == id){
            flag = 1;
        }
    }

    if ( flag == 0 ) {
        fprintf(stderr, "arquivo nao encontrado!\n");
        return 1;
    }

    fseek(disk, fsmeta->iNODE_table_addr+id, SEEK_SET);
    fread(&table_entry, sizeof(struct tuple), 1, disk);

    // remocao da tabela
    fseek(disk, fsmeta->iNODE_table_addr+id, SEEK_SET);
    fwrite((short[]){0,0}, sizeof(short), 2, disk);

    // liberacao do bitmap
    int dataseg_size = (table_entry.offset-FILE_HEADER_SIZE)/4;
    struct tuple dataseg[dataseg_size];
    fseek(disk, table_entry.baseaddr+FILE_HEADER_SIZE, SEEK_SET);
    fread(&dataseg, sizeof(struct tuple), dataseg_size, disk);
    
    unsigned short offsets[MAX_FILE_BLOCKS];
    for ( int i = 0, j = 0 ;  i < dataseg_size ;) {
        for ( j = 0; j < dataseg[i].offset ; j ++ ) {
            offsets[i+j] = ((dataseg[i].baseaddr/BLOCK_SIZE)+j);
        }
        i += j;
    }

    for ( int i = 0 ; offsets[i] != 0; i ++)
        FREE_BLOCK(offsets[i]);
    
    return 0;
}

int rename_fs(char* oldfilename, char* newfilename, FILE* disk, meta* fsmeta) {

    unsigned char old_id = inode(oldfilename, strlen(oldfilename));
    unsigned char new_id = inode(newfilename, strlen(newfilename));

    printf("old_entry %.4x\n", fsmeta->iNODE_table_addr+old_id);
    printf("new_entry %.4x\n", fsmeta->iNODE_table_addr+new_id);

    fseek(disk, fsmeta->iNODE_table_addr+old_id, SEEK_SET);
    struct tuple table_entry;
    fread(&table_entry, sizeof(table_entry), 1, disk);

    fseek(disk, fsmeta->iNODE_table_addr+old_id, SEEK_SET);
    fwrite((short[]){0x00,0x00}, sizeof(short), 2, disk);

    fseek(disk, table_entry.baseaddr+1, SEEK_SET);
    fwrite(newfilename, sizeof(char), strlen(newfilename)+1, disk);

    // setando table entry na nova localizacao
    fseek(disk, fsmeta->iNODE_table_addr+new_id, SEEK_SET);
    fwrite(&table_entry, sizeof(struct tuple), 1, disk);

    // muda inode na raiz
    fseek(disk, fsmeta->iNODE_table_addr+fsmeta->root_inode, SEEK_SET);

    fread(&table_entry, sizeof(struct tuple), 1, disk);

    fseek(disk, table_entry.baseaddr+FILE_HEADER_SIZE, SEEK_SET);
    unsigned char curr_inode = 0;
    for ( int i = 0 ; i < MAX_DIR_FILES ; i ++ ){
        fread(&curr_inode, sizeof(char), 1, disk);
        if (curr_inode == old_id){
            fseek(disk, -1, SEEK_CUR); 
            fwrite(&new_id, sizeof(char), 1, disk);
            break;
        }
    }

    return 0; 
}

int move_fs(char* dest, char* src, char* filename, FILE* disk, meta* fsmeta) {


    unsigned char dest_id = inode(dest, strlen(dest));
    unsigned char src_id = inode(src, strlen(src));
    unsigned char id = inode(filename, strlen(filename));

    struct tuple table_entry;
    fseek(disk, fsmeta->iNODE_table_addr+src_id, SEEK_SET);
    fread(&table_entry, sizeof(struct tuple), 1, disk);
    
    fseek(disk, table_entry.baseaddr+FILE_HEADER_SIZE, SEEK_SET);
    unsigned char curr_inode = 0;
    for ( int i = 0 ; i < MAX_DIR_FILES ; i ++ ){
        fread(&curr_inode, sizeof(char), 1, disk);
        if (curr_inode == id){
            fseek(disk, -1, SEEK_CUR); 
            fwrite((char[]){0}, sizeof(char), 1, disk);
            break;
        }
    }


    fseek(disk, fsmeta->iNODE_table_addr+dest_id, SEEK_SET);
    fread(&table_entry, sizeof(struct tuple), 1, disk);


    fseek(disk, table_entry.baseaddr+FILE_HEADER_SIZE, SEEK_SET);
    curr_inode = 0;
    for ( int i = 0 ; i < MAX_DIR_FILES ; i ++ ){
        fread(&curr_inode, sizeof(char), 1, disk);
        if (curr_inode == 0){
            fseek(disk, -1, SEEK_CUR); 
            fwrite(&id, sizeof(char), 1, disk);
            break;
        }
    }

    return 0;
}


int read_fs(char* dirname, char* filename, FILE* disk, meta* fsmeta) {

    char buffer[MAX_FILE_SIZE];

    unsigned char id = inode(filename, strlen(filename));
    unsigned char dir_id = inode(dirname, strlen(dirname));

    struct tuple table_entry;
    fseek(disk, fsmeta->iNODE_table_addr+dir_id, SEEK_SET);
    fread(&table_entry, sizeof(struct tuple), 1, disk);
    
    fseek(disk, table_entry.baseaddr+FILE_HEADER_SIZE, SEEK_SET);
    unsigned char curr_inode = 0;
    char flag = 0; // 0 nao esta no diretorio
                  // 1 esta no diretorio
    for ( int i = 0 ; i < MAX_DIR_FILES ; i ++ ){
        fread(&curr_inode, sizeof(char), 1, disk);
        if (curr_inode == id){
            flag = 1;
        }
    }

    if ( flag == 0 ) {
        fprintf(stderr, "arquivo nao encontrado!\n");
        return 1;
    }


    fseek(disk, fsmeta->iNODE_table_addr+id, SEEK_SET);
    fread(&table_entry, sizeof(struct tuple), 1, disk);

    int pos = 0;
    for(int i = 0 ; i < (table_entry.offset-FILE_HEADER_SIZE)/4 ; i ++) {
        fseek(disk, table_entry.baseaddr+FILE_HEADER_SIZE+(i*4), SEEK_SET);
        struct tuple dataseg;
        fread(&dataseg, sizeof(struct tuple), 1, disk);
        for( int j = 0  ; j < dataseg.offset ; j ++ ) {
            fseek(disk, dataseg.baseaddr+(j*BLOCK_SIZE), SEEK_SET);
            fread((buffer+pos), sizeof(char), BLOCK_SIZE, disk);
            pos+= BLOCK_SIZE;
        }
    }

    buffer[pos] = '\0';

    printf("%s\n", buffer);
    

    return 0;

}