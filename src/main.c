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
*/

#include "../include/fs.h"

int shell( FILE* disk, meta* fsmeta );

int main( void ) {

    FILE* disk = fopen("./disco", "rb+");
    
    if ( disk == NULL ) {
        
        printf("Criando disco...\n");
        disk = fopen("./disco", "wb+"); 
        if ( disk == NULL ) {
            fprintf(stderr,"Não foi possível criar disco\n");
            return 1;
        }
        
        printf("Criando partição com o sistema de arquivos tinyFS....\n");
        if ( create_fs(disk) == 1 ) {
            fprintf(stderr, "Não foi possivel criar a partição\n");
            fclose(disk);
            return 1;
        }

        printf("disco e partição criados com sucesso!!!\n");

    }

    meta* fsmeta = malloc(sizeof(meta));
    if (read_meta_fs(fsmeta, disk) == 1 ) {
        fprintf(stderr, "erro ao obter metadados do sistema de arquivos!!!\n");
        fclose(disk);
        return 1;
    }

    shell(disk, fsmeta);

    free(fsmeta);   
    fclose(disk);

    return 0;

}

int shell( FILE* disk, meta* fsmeta ) {

    char input[256];

    while( 1 ) { 

        printf("$ ");
        fflush(stdout);


        if (!fgets(input, sizeof(input), stdin)) 
            break;

        input[strcspn(input, "\n")] = '\0';
        
        if (strlen(input) == 0) {
            printf("encerrando o shell\n");
            fflush(stdout);
            continue;
        }

        if (strcmp(input, "exit") == 0) {
            break;
        }
        
        char dir[15];
        if (sscanf(input, "mkdir %14s", dir) == 1) {
            write_dir_fs(dir, disk, fsmeta);
            printf("diretorio %s criado com sucesso!\n", dir);
            fflush(stdout);
            continue;
        }

        char file[15];
        if (sscanf(input, "file %14s", file) == 1) {
            write_file_fs(file, disk, fsmeta);
            printf("arquivo salvo com sucesso!\n");
            fflush(stdout);
            continue;
        }

        char src[15], dest[15], filename[15];
        if (sscanf(input, "mv %14s %14s %14s",
                   src, dest, filename) == 3) {
            move_fs(dest, src, filename, disk, fsmeta);
            printf("arquivo movido de %s para %s!\n", src, dest);
            fflush(stdout);
            continue;
        }
        

        char old_name[15], new_name[15];
        if (sscanf(input, "rename %14s %14s",
                   old_name, new_name) == 2) {
            rename_fs(old_name, new_name, disk, fsmeta);
            printf("arquivo renomeado para %s\n", new_name);
            fflush(stdout);
            continue;
        }

        if (sscanf(input, "read %14s %14s", dir, file) == 2) {
            read_fs(dir, file, disk, fsmeta);
            continue;
        }

        if (sscanf(input, "rm %14s %14s" , dir, file) == 2) {
            remove_fs(dir, file, disk, fsmeta);
            printf("arquivo removido com sucesso!\n");
            fflush(stdout);
            continue;
        }

        
        char content[512];
        if (sscanf(input,
                   "write \"%511[^\"]\" %255s",
                   content, filename) == 2) {

            write_content_fs(filename, content, disk, fsmeta);
            continue;
        }

    }

}