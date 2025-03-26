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
#endif /* FS_FUNCTIONS_H */