# tinyFS

Um sistema de arquivos simples escrito em C, implementado sobre um único arquivo binário que simula um disco.

O objetivo do projeto é estudar conceitos internos de sistemas de arquivos como:

- Superbloco
- Bitmap de blocos
- Tabela de iNODEs
- Segmentação de dados
- Alocação de blocos
- Operações básicas de arquivos e diretórios

## Estrutura do disco

O tinyFS utiliza um disco virtual armazenado em um arquivo chamado:

```txt
./disco
````

Caso o disco não exista, ele é criado automaticamente na inicialização. 

### Layout do disco

O disco possui tamanho fixo de **64 KiB** e é organizado da seguinte forma: 

```txt
0x0000 ──────────────────────────
HEADER (2 bytes)

SUPERBLOCK (6 bytes)
├── tamanho do disco
├── tamanho do bloco
└── quantidade de blocos

ENDEREÇOS (8 bytes)
├── bitmap
├── tabela iNODE
├── iNODEs
└── área de dados

BITMAP (16 bytes / 128 bits)

TABELA iNODE (1 KiB)
0x0020 - 0x0420

iNODEs (4 KiB)
0x0421 - 0x1421

DADOS (~59 KiB)
0x1601 - 0xFFFF
────────────────────────────────
```

### Configuração do sistema

| Propriedade          |     Valor |
| -------------------- | --------: |
| Tamanho do disco     |    64 KiB |
| Tamanho do bloco     | 512 bytes |
| Quantidade de blocos |       128 |
| Bitmap               |  128 bits |

Os blocos livres e ocupados são gerenciados através de um bitmap de 128 bits. 

---

## Estrutura dos arquivos

Cada arquivo ou diretório é identificado por um **iNODE**, indexado por um hash simples baseado no nome do arquivo. 

Os iNODEs armazenam:

```c
struct iNODE {
    char filetype;
    char filename[15];
    struct tuple* dataseg;
}
```

Onde:

* `filetype = 0` → arquivo
* `filetype = 1` → diretório 

Os dados dos arquivos podem ser segmentados em múltiplos blocos contíguos utilizando:

```txt
(baseaddr, offset)
```

representados pela struct:

```c
struct tuple {
    short baseaddr;
    short offset;
};
```

---

## Diretório raiz

Na inicialização, o sistema cria automaticamente um diretório raiz chamado:

```txt
raiz
```
Ele é usado para armazenar todo os arquivos recém criados.

---

## Shell interativo

O tinyFS possui um shell simples para interação com o sistema de arquivos. 

Ao executar:

```bash
./tinyfs
```

você verá:

```txt
$
```

---

## Comandos

### Criar diretório

```bash
mkdir <diretorio>
```

Exemplo:

```bash
mkdir docs
```

Cria um novo diretório dentro da raiz. 

---

### Importar arquivo do host

```bash
file <arquivo>
```

Exemplo:

```bash
file texto.txt
```

Importa um arquivo do sistema operacional para dentro do tinyFS. 

---

### Escrever conteúdo diretamente

```bash
write "conteudo" <arquivo>
```

Exemplo:

```bash
write "ola mundo" teste.txt
```

Cria/escreve um arquivo diretamente no disco virtual sem depender de um arquivo externo. 

---

### Ler arquivo

```bash
read <diretorio> <arquivo>
```

Exemplo:

```bash
read raiz teste.txt
```

Exibe o conteúdo de um arquivo armazenado no tinyFS. 

---

### Mover arquivo

```bash
mv <origem> <destino> <arquivo>
```

Exemplo:

```bash
mv raiz docs texto.txt
```

Move um arquivo entre diretórios. 

---

### Renomear arquivo

```bash
rename <arquivo> <novo_nome>
```

Exemplo:

```bash
rename teste.txt novo.txt
```

Renomeia um arquivo atualizando seu iNODE correspondente, mas somente no diretório `raiz`; 

---

### Remover arquivo

```bash
rm <diretorio> <arquivo>
```

Exemplo:

```bash
rm raiz teste.txt
```

Remove um arquivo do diretório e libera seus blocos no bitmap. 

---

### Encerrar shell

```bash
exit
```

Sai do shell interativo. 

---

## Compilação

Compile o projeto com:

```bash
gcc src/*.c -Iinclude -o tinyfs
```

Depois execute:

```bash
./tinyfs
```

---

## Exemplo de uso

```txt
$ mkdir docs
diretorio docs criado com sucesso!

$ write "hello world" teste.txt

$ read raiz teste.txt
hello world

$ rename teste.txt hello.txt

$ mkdir backup

$ mv raiz backup hello.txt

$ rm backup hello.txt

$ exit
```

---
