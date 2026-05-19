
# /bin - executaveis
#  ./ls    - listar diretorios
#  ./cd    - navegar diretorios
#  ./rm    - remover diretorios
#  ./cat   - mostrar conteudo de arquivos
#  ./touch - criar arquivos
#  ./mkdir - criar diretorios
#  ./mv    - mover/rename arquivos/diretorios
#  ./read  - escreve para um arquivos 

# /dev/sda1 - disco fs

def create_fs(disco):
    fs = [
        #HEADER 
        0x46,
        0x53, 
        
        #SUPERBLOCO
        0xFF, # tamanho do disco 64Kib
        0xFF, 

        0x03, # tamanho dos blocos 1Kib
        0xFF, 

        0x40, # quantidade de blocos 64 

        0x0B, # ADDR BITMAP
        0x0F, # ADDR TABELA iNODE 
        0x00, # ADDR DADOS

        #BITMAP
        0x00,
        0x00,
        0x00,
        0x00
    ]

    # TABELA iNODE 1 byte (256 arquivos)
    for i in range(256):
        fs.extend([
            0x00, # tipo arquivo/diretorio 1byte
            
            0x00, 0x00, 0x00, 0x00, # filename 16bytes
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,

            0x00, 0x00, # baseaddr 4bytes
            0x00, 0x00,

            0x00, 0x00, # offset 4bytes
            0x00, 0x00
            ])

    #DADOS 6414-65535
    for i in range(6414, 65536):
        fs.append(0x00)


    disco.write(bytes(fs))


if __name__ == "__main__":

    disco = open("sda1", "w+b")
    create_fs(disco)

    disco.close()

