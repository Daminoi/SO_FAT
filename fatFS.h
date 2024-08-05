#ifndef FAT_FS_H
#define FAT_FS_H

#include "minilogger.h"

#define mem_addr_t long int
#define block_num_t int             // Deve avere il valore -1 nel suo dominio

#define CURRENT_FS_VERSION 1

#define BLOCK_SIZE 512

#define MAX_BLOCKS (1LL << ((sizeof(block_num_t) - 1) * 8)) - 1

#define ROOT_DIR_STARTING_BLOCK 0

// Valori possibili del campo is_dir di FILE_ENTRY
#define IS_DIR 1
#define IS_DATA 0

struct FAT_FS_HEADER{
    int version;

    int n_blocks;

    unsigned int n_free_blocks;
    block_num_t first_free_block;
    block_num_t last_free_block;
} FAT_FS_HEADER;

struct FILE_ENTRY{
    char file_name[27];         // 26 byte + 1 byte per il terminatore di stringa
    char is_dir;

    char file_extension[4];     // 3 byte + 1 byte per il terminatore di stringa

    block_num_t start_block;
    unsigned int file_size;
} FILE_ENTRY;                   // 40 Bytes

#define FILE_ENTRIES_PER_DIR_BLOCK BLOCK_SIZE / (sizeof(FILE_ENTRY))
#define DIR_BLOCK_WASTED_BYTES BLOCK_SIZE - (FILE_ENTRIES_PER_DIR_BLOCK * sizeof(FILE_ENTRY))

// Se un blocco è l'ultimo di un file, avrà questo valore nel campo next_block
#define LAST_BLOCK -1

// Valori che può assumere il campo block_status nella fat
#define FREE_BLOCK 0
#define ALLOCATED_BLOCK 1

struct FAT_TABLE_ENTRY{
    int block_status;
    block_num_t next_block;
} FAT_TABLE_ENTRY;

// Definizione di tutti i prototipi delle funzioni implementate
int create_new_fat_fs_on_file(const char* const file_name);


#endif /* FAT_FS_H */