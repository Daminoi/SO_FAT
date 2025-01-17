#ifndef FAT_FS_H
#define FAT_FS_H

#define CURRENT_FS_VERSION 1

#include <stdbool.h>

#include "minilogger.h"

#define byte_offset unsigned long int
#define block_num_t int             // Deve essere intero ed avere il valore -1 nel suo dominio

#define MAX_BLOCKS (1LL << ((sizeof(block_num_t) - 1) * 8)) - 1

#define ROOT_DIR_STARTING_BLOCK 0   // Deve rimanere 0 !!

// Valori possibili del campo is_dir di DIR_ENTRY
#define EMPTY 0
#define DATA 1
#define DIR 2

/*
    Ogni cartella ha nel suo primo blocco la prima DIR_ENTRY con valore is_dir = IS_DIR_REF_TOP che 
    contiene il numero del blocco e l'offset dove si trovano le informazioni sulla cartella 
    (quindi una DIR_ENTRY con is_dir = IS_DIR posizionata in un'altra cartella) 
*/
#define DIR_REF_TOP 3

/* 
    Stesso significato della costante precedente, ma usato SOLAMENTE nella cartella ROOT che contiene direttamente nel suo
    primo blocco i dati che descrivono la cartella (quindi solo i campi di "file" della union sono validi).
*/
#define DIR_REF_ROOT 4

/*
    La prima DIR_ENTRY di ogni blocco (tranne nel caso che quel blocco sia il primo di un file o di una cartella)
    punta al primo blocco di quel file o quella cartella
*/
#define DIR_REF 5


typedef struct FAT_FS_HEADER{
    int version;

    unsigned int n_blocks;
    unsigned int n_free_blocks;

    block_num_t first_free_block;
    block_num_t last_free_block;
} FAT_FS_HEADER;

#define MAX_FILENAME_SIZE 26
#define MAX_FILE_EXTENSION_SIZE 3

typedef struct DIR_ENTRY{
    
    char name[27];         // 26 byte + 1 byte per il terminatore di stringa
    char is_dir;
    char file_extension[4];     // 3 byte + 1 byte per il terminatore di stringa
    
    union {
        
        struct{
            block_num_t start_block;
            
            union{
                unsigned int file_size;     // dimensione del file, se è una cartella allora è sensato n_elems
                unsigned int n_elems;       // valido solo per le cartelle (non sono contati gli elementi necessari al funzionamento interno)
            };
        } file;
        
        struct{
            /*
                L'entry si troverà nella cartella nel blocco block a una certa posizione (es. il primo file in una cartella ha offset 0, il 120esimo ha offset 119)
                Nel calcolo dell'offset sono escluse le entry necessarie per il mantenimento della gerarchia che si trovano all'inizio di ogni blocco
                (2 nel primo blocco di ogni cartella, uno nei blocchi successivi)
            */
            block_num_t block;
            unsigned int file_entry_offset; 
        } internal_dir_ref;
    };

} DIR_ENTRY;                   // 40 Bytes?

typedef struct BLOCK{
    char block_data[512];
} BLOCK;

#define BLOCK_SIZE sizeof(BLOCK)

// NOTA: si tratta di divisione intera! 9 / 4 = 2!
#define FILE_ENTRIES_PER_DIR_BLOCK (unsigned int)(BLOCK_SIZE / (sizeof(DIR_ENTRY)))

#define DIR_BLOCK_WASTED_BYTES (unsigned int)(BLOCK_SIZE - (FILE_ENTRIES_PER_DIR_BLOCK * sizeof(DIR_ENTRY)))

// Se un blocco è l'ultimo di un file, avrà questo valore nel campo next_block della fat
#define LAST_BLOCK -1
// un valore invalido generico
#define INVALID_BLOCK -2

// Valori che può assumere il campo block_status nella fat
#define FREE_BLOCK 0
#define ALLOCATED_BLOCK 1

typedef struct FAT_ENTRY{
    int block_status;
    block_num_t next_block;             // se vale -1, allora è l'ultimo blocco di un file o di una cartella
} FAT_ENTRY;

typedef struct FAT_FS{
    char* start_location;               // dove inizia lo spazio allocato dalla mmap per il fs
    //FAT_FS_HEADER* header;            // L'header è formato dai primi sizeof(FAT_FS_HEADER) byte
    byte_offset fat_offset;             // offset in byte dove inizia la fat rispetto all'inizio del fs, viene calcolato quando si carica il fs
    byte_offset block_section_offset;   // offset in byte da cui iniziano i blocchi con i dati e le cartelle, viene calcolato quando si carica il fs
} FAT_FS;

typedef struct MOUNTED_FS{
    FAT_FS* fs;
    block_num_t curr_dir_block;    // curr_dir punta al primo blocco della cartella attuale (deve essere sempre valido)
} MOUNTED_FS;

/* file_name per ora non è utilizzato, viene sempre creato un nuovo file fat.myfat */
int create_fs_on_file(const char* const file_name, block_num_t n_blocks);

MOUNTED_FS* mount_fs_from_file(const char* const file_name);
int unmount_fs(FAT_FS* fs);

unsigned long int _fs_size(FAT_FS* fs);


// Funzioni a "basso" livello

unsigned int get_n_blocks(const FAT_FS* fs);
unsigned int get_n_free_blocks(const FAT_FS* fs);

bool _has_next_block(const FAT_FS* fs, block_num_t curr_block);
block_num_t _get_next_block(const FAT_FS* fs, block_num_t curr_block);

block_num_t _get_parent_dir_block(const FAT_FS* fs, block_num_t curr_dir_block);

int _create_file(FAT_FS* fs, DIR_ENTRY* dir, DIR_ENTRY* new_file); // il contenuto puntato da new_file verrà copiato nel nuovo file nella cartella directory
int _erase_file(FAT_FS* fs, DIR_ENTRY* file);

int _create_dir(FAT_FS* fs, DIR_ENTRY* dir, DIR_ENTRY* new_dir);
int _erase_dir(FAT_FS* fs, DIR_ENTRY* dir_to_erase);

int _write(FAT_FS* fs, DIR_ENTRY* file, unsigned int n_bytes, void* from_buffer);
// Da vedere come dividere su più file
int _read(/*FAT_FS* fs,*/DIR_ENTRY* file, unsigned int n_bytes, void* dest_buffer);


// Richieste da completare affinché il progetto sia completo

//int create_file(MOUNTED_FS* fs, DIR_ENTRY* new_file);
//int erase_file(MOUNTED_FS* fs, char* file_name);

//int create_dir(MOUNTED_FS* fs, DIR_ENTRY* new_dir);
//int erase_dir(MOUNTED_FS* fs, char* dir_name);
//int change_dir(MOUNTED_FS* fs, char* dir_name); // NON RIGUARDA IL FS
//int list_dir(MOUNTED_FS* fs);  // NON RIGUARDA IL FS

//int write(MOUNTED_FS* fs, FILE_HANDLE* file_handle, unsigned int n_bytes, void* from_buffer);  // tra le ultime da vedere
//int read(MOUNTED_FS* fs, FILE_HANDLE* file_handle, unsigned int n_bytes, void* dest_buffer);   // tra le ultime da vedere

//int seek(MOUNTED_FS* fs, FILE_HANDLE* file_handle, long int offset, int whence);   // tra le ultime da vedere
#endif /* FAT_FS_H */