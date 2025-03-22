#ifndef FS_FUNCTIONS_H
#define FS_FUNCTIONS_H

#include "fatFS.h"

/* Pone tutti i byte del blocco a 0, non lo dealloca */
void clear_block(const FAT_FS* fs, const block_num_t block_to_clear);
// Restituisce il numero del blocco allocato
block_num_t allocate_block(const FAT_FS* fs);
// Dealloca un blocco, il blocco deve essere l'ultimo del file a cui appartiene (fat[block_to_free] == LAST_BLOCK)
bool free_block(const FAT_FS* fs, const block_num_t block_to_free);

#endif /* FS_FUNCTIONS_H */