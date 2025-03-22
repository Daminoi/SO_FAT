#include <stddef.h>
#include <stdio.h>

#include "fsUtils.h"
#include "fatFS.h"
#include "minilogger.h"

FAT_FS_HEADER* get_fs_header(const FAT_FS* fs){
    return (FAT_FS_HEADER*) fs->start_location;
}

// Restituisce il puntatore all'inizio della sezione della fat
FAT_ENTRY* get_fs_fat(const FAT_FS* fs){
    if(is_not_null_fs(fs)){
        return (FAT_ENTRY*) (fs->start_location + fs->fat_offset);
    }
    else{
        mini_log(ERROR, "_get_fat", "fs nullo");
        return NULL;
    }
}

// Restituisce il puntatore all'inizio della sezione dei blocchi del fs
BLOCK* get_fs_block_section(const FAT_FS* fs){
    if(is_not_null_fs(fs)){
        return (BLOCK*) (fs->start_location + fs->block_section_offset);
    }
    else{
        mini_log(ERROR, "_get_block_section", "fs nullo");
        return NULL;
    }
}

unsigned int get_n_blocks(const FAT_FS* fs){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "get_n_blocks", "Fs nullo!");
        return 0;
    }
    else return get_fs_header(fs)->n_blocks;
}

unsigned int get_n_free_blocks(const FAT_FS* fs){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "get_n_free_blocks", "Fs nullo!");
        return 0;
    }
    else return get_fs_header(fs)->n_free_blocks;
}

/* Semplice, per ora, controllo di validità del fs TODO */
bool is_not_null_fs(const FAT_FS* fs){
    
    if(fs == NULL) return false;
    /*
    if(fs->header == NULL) return false;
    if(fs->fat == NULL) return false;
    if(fs->block_section == NULL) return false;
    */
    return true; 
}

// Restituisce true se il blocco è libero (fat[block].block_status == FREE_BLOCK)
bool is_block_free(const FAT_FS* fs, block_num_t block){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "_is_free_block", "Fs nullo!");
        return false;
    }
    else if(!is_block_valid(fs, block)){
        return false;
    }
    else
        return get_fs_fat(fs)[block].block_status == FREE_BLOCK;
}

bool has_next_block(const FAT_FS* fs, block_num_t curr_block){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "_has_next_block", "Fs nullo!");
        return false;
    }
    else if(!is_block_valid(fs, curr_block)){
        return false;
    }
    else 
        return get_fs_fat(fs)[curr_block].next_block >= 0;
}

block_num_t get_next_block(const FAT_FS* fs, block_num_t curr_block){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "_get_next_block", "Fs nullo!");
        return INVALID_BLOCK;
    }
    else if(!is_block_valid(fs, curr_block)){
        return INVALID_BLOCK;
    }
    else 
        return get_fs_fat(fs)[curr_block].next_block;
}

bool is_block_valid(const FAT_FS* fs, block_num_t block){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "_is_valid_block", "Fs nullo!");
        return false;
    }

    return block >= 0 && block < get_n_blocks(fs);
}

// Rischio ricorsione infinita TODO ?
block_num_t get_parent_dir_block(const FAT_FS* fs, block_num_t curr_dir_block){
    if(!is_not_null_fs(fs)){
        return INVALID_BLOCK;
    }

    if(!is_block_valid(fs, curr_dir_block) || is_block_free(fs, curr_dir_block)){
        mini_log(ERROR, "get_parent_dir_block", "Invalid block as parameter");
        return INVALID_BLOCK;
    }

    if(curr_dir_block == ROOT_DIR_STARTING_BLOCK){
        mini_log(LOG, "get_parent_dir_block", "ROOT_DIR_STARTING_BLOCK as parameter");
        return ROOT_DIR_HAS_NO_PARENT;
    }

    BLOCK* block = (((BLOCK*)get_fs_block_section(fs)) + curr_dir_block);
    DIR_ENTRY* dir_info = (DIR_ENTRY*) block;

    if(dir_info->is_dir == EMPTY || dir_info->is_dir == DATA)
        return INVALID_BLOCK;

    else if(dir_info->is_dir == DIR_REF){
        mini_log(LOG, "get_parent_dir_block", "Recursive call, looking for the first block of the directory.");
        return get_parent_dir_block(fs, dir_info->internal_dir_ref.block);
    }

    else if(dir_info->is_dir == DIR_REF_TOP){
        mini_log(LOG, "get_parent_dir_block", "Parent directory found");
        return dir_info->internal_dir_ref.block;
    }

    else{
        // QUESTO CASO NON DOVREBBE VERIFICARSI A MENO CHE NON VI SIANO ERRORI DI PROGRAMMAZIONE
        mini_log(ERROR, "get_parent_dir_block", "Unknown directory block type");
        printf("Dir block type found: %d", dir_info->is_dir);
        return INVALID_BLOCK;
    }
}

unsigned long int fs_size(FAT_FS* fs){
    // Da implementare TODO
    return 0;
}