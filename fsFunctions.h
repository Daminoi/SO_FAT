#ifndef FS_FUNCTIONS_H
#define FS_FUNCTIONS_H

#include <stdbool.h>
#include <stdio.h>

#include "fatFS.h"

/* Pone tutti i byte del blocco a 0, non lo dealloca */
void clear_block(const FAT_FS* fs, const block_num_t block_to_clear);
// Restituisce il numero del blocco allocato
block_num_t allocate_block(const FAT_FS* fs);
// Dealloca un blocco
bool free_block(const FAT_FS* fs, const block_num_t block_to_free);
// Sovrascrive il contenuto del blocco block_to_write, length deve essere minore di BLOCK_SIZE
int write_to_file_block(const FAT_FS* fs, const block_num_t block_to_write, char* from_buffer, size_t length);

FILE_HANDLE* get_file_handle(MOUNTED_FS* m_fs, block_num_t first_file_block);
FILE_HANDLE* get_or_create_file_handle(MOUNTED_FS* m_fs, block_num_t first_file_block);
void delete_file_handle(FILE_HANDLE* file_handle);
void delete_all_file_handles(MOUNTED_FS* m_fs);

int extend_file(FILE_HANDLE* file, block_num_t n_blocks_to_allocate);
int extend_dir(MOUNTED_FS* m_fs, block_num_t dir_block, block_num_t n_blocks_to_allocate);

int update_dir_elem_added(const FAT_FS* fs, block_num_t dir_block);
int update_dir_elem_removed(const FAT_FS* fs, block_num_t dir_block);
#endif /* FS_FUNCTIONS_H */