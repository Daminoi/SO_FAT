#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <stdbool.h>
#include <stddef.h>

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
bool is_block_free(const FAT_FS* fs, block_num_t block);

bool has_next_block(const FAT_FS* fs, block_num_t curr_block);
block_num_t get_next_block(const FAT_FS* fs, block_num_t curr_block);

BLOCK* block_num_to_block_pointer(const FAT_FS* fs, block_num_t block);

block_num_t get_parent_dir_block(const FAT_FS* fs, block_num_t curr_dir_block);

unsigned int file_size_bytes_to_file_size_blocks(size_t file_size, bool include_first_block_data);

// curr_dir_block deve essere il primo blocco della cartella, PER IL MOMENTO
bool can_create_new_file(const FAT_FS* fs, block_num_t curr_dir_block);
bool can_create_new_dir(const FAT_FS* fs, block_num_t curr_dir_block);
// restituisce uno spazio libero (se esiste) della cartella il cui primo blocco è curr_dir_block
DIR_ENTRY_POSITION get_available_dir_entry(const FAT_FS* fs, block_num_t curr_dir_block);
bool is_dir_entry_position_null(DIR_ENTRY_POSITION de_p);
DIR_ENTRY* dir_entry_pos_to_dir_entry_pointer(const FAT_FS* fs, DIR_ENTRY_POSITION de_p);

#endif /* FS_UTILS_H */