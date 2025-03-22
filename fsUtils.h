#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <stdbool.h>

#include "fatFS.h"

bool is_not_null_fs(const FAT_FS* fs);
// Restituisce la dimensione dell'intero fs in bytes
unsigned long int fs_size(FAT_FS* fs);

FAT_FS_HEADER* get_fs_header(const FAT_FS* fs);
FAT_ENTRY* get_fs_fat(const FAT_FS* fs);
BLOCK* get_fs_block_section(const FAT_FS* fs);

unsigned int get_n_blocks(const FAT_FS* fs);
unsigned int get_n_free_blocks(const FAT_FS* fs);

bool is_block_valid(const FAT_FS* fs, block_num_t block);
bool is_free_block(const FAT_FS* fs, block_num_t block);

bool has_next_block(const FAT_FS* fs, block_num_t curr_block);
block_num_t get_next_block(const FAT_FS* fs, block_num_t curr_block);

block_num_t get_parent_dir_block(const FAT_FS* fs, block_num_t curr_dir_block);

#endif /* FS_UTILS_H */