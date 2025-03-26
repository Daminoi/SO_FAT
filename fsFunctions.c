#include <stdio.h>
#include <string.h>

#include "fsFunctions.h"
#include "fatFS.h"
#include "fsUtils.h"
#include "minilogger.h"

void clear_block(const FAT_FS* fs, const block_num_t block_to_clear){
    if(is_not_null_fs(fs) && is_block_valid(fs, block_to_clear)){
        memset(((get_fs_block_section(fs) + block_to_clear)), 0, BLOCK_SIZE);
        printf("EXTRA (clear_block) Azzerato blocco %d\n", block_to_clear);
        //mini_log(LOG, "clear_block", "Blocco azzerato");
    }
    else{
        printf("EXTRA (clear_block) FALLITA pulizia blocco %d\n", block_to_clear);
        //mini_log(ERROR, "clear_block", "Impossibile azzerare un blocco");
    }
}

block_num_t allocate_block(const FAT_FS* fs){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "allocate_block", "Impossibile operare su un fs nullo.");
        return -1;
    }

    if(get_n_free_blocks(fs) < 0){
        mini_log(ERROR, "allocate_block", "File system corrotto: blocchi liberi negativi!");
        return -1;
    }

    if(get_n_free_blocks(fs) == 0){
        mini_log(WARNING, "allocate_block", "Blocchi liberi terminati.");
        return -1;
    }

    FAT_FS_HEADER* header = get_fs_header(fs);
    FAT_ENTRY* fat = get_fs_fat(fs);

    /* Non è un controllo sufficiente per tutti i casi */
    if(header->first_free_block < 0 || header->first_free_block == ROOT_DIR_STARTING_BLOCK || header->last_free_block < 0 || header->last_free_block == ROOT_DIR_STARTING_BLOCK ){
        mini_log(ERROR, "allocate_block", "File system corrotto: invalido first_free_block o last_free_block.");
        return -1;
    }

    if(fat[header->first_free_block].block_status != FREE_BLOCK || fat[header->last_free_block].block_status != FREE_BLOCK){
        mini_log(ERROR, "allocate_block", "File system corrotto: first_free_block o last_free_block non sono blocchi liberi.");
        return -1;
    }

    block_num_t allocated_block;

    if(header->n_free_blocks == 1){
        if(header->first_free_block != header->last_free_block){
            mini_log(ERROR, "allocate_block", "File system corrotto: disponibile un solo blocco libero ma first_free_block e last_free_block sono diversi!");
            return -1;
        }

        allocated_block = header->first_free_block;
        header->first_free_block = -1;
        header->last_free_block = -1;

    }
    else{   /* n_free_blocks >= 2 */

        allocated_block = header->first_free_block;
        header->first_free_block = fat[header->first_free_block].next_block;
    }

    fat[allocated_block].block_status = ALLOCATED_BLOCK;
    fat[allocated_block].next_block = LAST_BLOCK;

    header->n_free_blocks--;
    
    clear_block(fs, allocated_block);
    return allocated_block;
}

bool free_block(const FAT_FS* fs, const block_num_t block_to_free){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "free_block", "Impossibile operare su un fs nullo o non valido.");
        return false;
    }

    if(is_block_valid(fs, block_to_free) == false){
        mini_log(ERROR, "free_block", "Blocco da liberare non esistente!");
        return false;
    }

    if(block_to_free == ROOT_DIR_STARTING_BLOCK){
        mini_log(ERROR, "free_block", "Tentativo di liberare il blocco 0 (inizio root directory)!");
        return false;
    }

    FAT_FS_HEADER* header = get_fs_header(fs);
    FAT_ENTRY* fat = get_fs_fat(fs);
    
    if(is_block_free(fs, block_to_free)){
        mini_log(ERROR, "free_block", "Non si può deallocare un blocco già libero!");
        return false;
    }
    
    /*
    if(fat[block_to_free].next_block != LAST_BLOCK){
        mini_log(ERROR, "free_block", "Non può essere deallocato un blocco che non sia l'ultimo del file! (next_block != LAST_BLOCK)!");
        return false;
    }
    */

    // Da qui la logica per la deallocazione di un blocco

    fat[block_to_free].block_status = FREE_BLOCK;
    fat[block_to_free].next_block = LAST_BLOCK;

    if(get_n_free_blocks(fs) == 0){
        header->first_free_block = block_to_free;
        header->last_free_block = block_to_free;
    }
    else if(get_n_free_blocks(fs) >= 1){
        fat[header->last_free_block].next_block = block_to_free;
        header->last_free_block = block_to_free;
    }

    header->n_free_blocks++;

    mini_log(LOG, "free_block", "Blocco deallocato con successo.");
    return true;
}

int write_to_file_block(const FAT_FS* fs, const block_num_t block_to_write, char* from_buffer, size_t length){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "write_to_block", "Impossibile operare su un fs nullo o non valido.");
        return -1;
    }

    if(is_block_valid(fs, block_to_write) == false ){
        mini_log(ERROR, "write_to_block", "Blocco su cui scrivere inesistente!");
        return -1;
    }

    if(is_block_free(fs, block_to_write)){
        mini_log(ERROR, "write_to_block", "Blocco su cui scrivere non allocato!");
        return -1;
    }

    if(length <= 0 || length > BLOCK_SIZE){
        mini_log(ERROR, "write_to_block", "Il numero di byte da scrivere supera BLOCK_SIZE oppure è inferiore o uguale a 0");
        return -1;
    }

    BLOCK* blck = ((BLOCK*)get_fs_block_section(fs)) + block_to_write;

    memcpy(blck, from_buffer, length);

    return 0;
}