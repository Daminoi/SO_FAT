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
        mini_log(ERROR, "get_parent_dir_block", "Il numero di blocco passato come argomento non è valido");
        return INVALID_BLOCK;
    }

    if(curr_dir_block == ROOT_DIR_STARTING_BLOCK){
        mini_log(LOG, "get_parent_dir_block", "ROOT_DIR_STARTING_BLOCK è stato passato come parametro");
        return ROOT_DIR_HAS_NO_PARENT;
    }

    BLOCK* block = (((BLOCK*)get_fs_block_section(fs)) + curr_dir_block);
    DIR_ENTRY* dir_info = (DIR_ENTRY*) block;

    if(dir_info->is_dir == EMPTY || dir_info->is_dir == DATA)
        return INVALID_BLOCK;

    else if(dir_info->is_dir == DIR_REF){
        mini_log(LOG, "get_parent_dir_block", "Eseguo una chiamata ricorsiva per risalire al primo blocco della cartella attuale");
        return get_parent_dir_block(fs, dir_info->internal_dir_ref.ref.block);
    }

    else if(dir_info->is_dir == DIR_REF_TOP){
        mini_log(LOG, "get_parent_dir_block", "Cartella genitore trovata");
        return dir_info->internal_dir_ref.ref.block;
    }

    else{
        // QUESTO CASO NON DOVREBBE VERIFICARSI A MENO CHE NON VI SIANO ERRORI DI PROGRAMMAZIONE
        mini_log(ERROR, "get_parent_dir_block", "Il tipo di blocco della cartella è sconosciuto");
        printf("Tipo di blocco della cartella trovato: %d", dir_info->is_dir);
        return INVALID_BLOCK;
    }
}

unsigned long int fs_size(FAT_FS* fs){
    // Da implementare TODO
    return 0;
}

DIR_ENTRY_POSITION get_available_dir_entry(const FAT_FS* fs, block_num_t curr_dir_block){
    DIR_ENTRY_POSITION de_p = {-1, 0};
    
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "get_available_dir_entry", "File system nullo");
        return de_p;
    }

    if(curr_dir_block == LAST_BLOCK){
        // qui vuoldire che (ricorsivamente) è arrivato all'ultimo blocco della cartella e non ci ha trovato uno spazio vuoto
        mini_log(LOG, "get_available_dir_entry", "Non è stata trovata una file entry libera nella cartella");
        return de_p;
    }

    if(is_block_valid(fs, curr_dir_block) == false){
        mini_log(ERROR, "get_available_dir_entry", "Impossibile effettuare la ricerca su un blocco invalido");
        return de_p;
    }

    if(is_block_free(fs, curr_dir_block)){
        mini_log(ERROR, "get_available_dir_entry", "Accesso a un blocco non allocato!");
        return de_p;
    }

    BLOCK* blck = get_fs_block_section(fs) + curr_dir_block;
    DIR_ENTRY* de = (DIR_ENTRY*) blck;
    DIR_ENTRY* curr_de;

    // Inizia da i=1 perchè la file_entry 0 contiene informazioni relative all'organizzazione dei blocchi
    for(int i=1; i < FILE_ENTRIES_PER_DIR_BLOCK; ++i){
        curr_de = de + i;
        
        if(curr_de->is_dir == EMPTY){
            mini_log(LOG, "get_available_dir_entry", "Trovata file entry libera nella cartella");
            de_p.block = curr_dir_block;
            de_p.offset = i-1;
            return de_p;
        }
    }

    return get_available_dir_entry(fs, get_next_block(fs, curr_dir_block));
}

bool is_dir_entry_position_null(DIR_ENTRY_POSITION de_p){
    if(de_p.block < 0)
        return true;
    else
        return false;
}

DIR_ENTRY* dir_entry_pos_to_dir_entry_pointer(const FAT_FS* fs, DIR_ENTRY_POSITION de_p){
    if(fs == NULL || is_block_valid(fs, de_p.block) == false || is_block_free(fs, de_p.block)){
        mini_log(ERROR, "dir_entry_pos_to_dir_entry", "Tentativo di ottenere un puntatore a una dir entry non valida");
        return NULL;
    }

    if(de_p.offset > FILE_ENTRIES_PER_DIR_BLOCK-1){
        mini_log(ERROR, "dir_entry_pos_to_dir_entry", "Tentativo di ottenere un puntatore a una dir entry non valida, offset errato");
        return NULL;
    }

    BLOCK* desired_block = get_fs_block_section(fs) + de_p.block;
    return ((DIR_ENTRY*) desired_block ) + de_p.offset;
}

unsigned int file_size_bytes_to_file_size_blocks(size_t file_size, bool include_first_block_data){
    unsigned int file_size_blocks = 0;

    if(include_first_block_data){
        if(file_size <= FIRST_BLOCK_DATA_SIZE)
            return 1;
        else{
            file_size -= FIRST_BLOCK_DATA_SIZE;
            ++file_size_blocks;
        }
    }

    file_size_blocks += (unsigned int) (file_size / BLOCK_SIZE);
    
    if(file_size % BLOCK_SIZE > 0){
        ++file_size_blocks;
    }

    return file_size_blocks;
}

bool can_create_new_dir(const FAT_FS* fs, block_num_t curr_dir_block){
    if(is_block_valid(fs, curr_dir_block) == false || is_block_free(fs, curr_dir_block)){
        mini_log(ERROR, "can_create_new_dir", "Blocco non valido o non allocato");
        return NULL;
    }

    DIR_ENTRY_POSITION de_p = get_available_dir_entry(fs, curr_dir_block);
    if(is_dir_entry_position_null(de_p)){
        return get_n_free_blocks(fs) >= 2; // Dato che sarà necessario estendere la cartella in cui vogliamo inserire la nuova cartella
    }
    else{
        return get_n_free_blocks(fs) >= 1;
    }
}

bool can_create_new_file(const FAT_FS* fs, block_num_t curr_dir_block){
    
    if(is_block_valid(fs, curr_dir_block) == false || is_block_free(fs, curr_dir_block)){
        mini_log(ERROR, "can_create_new_file", "Blocco della cartella attuale non valido o non allocato");
        return false;
    }
    
    DIR_ENTRY_POSITION de_p = get_available_dir_entry(fs, curr_dir_block);
    if(is_dir_entry_position_null(de_p)){
        return get_n_free_blocks(fs) >= 1;
    }
    else{
        return get_n_free_blocks(fs) >= 2;
    }
}

BLOCK* block_num_to_block_pointer(const FAT_FS* fs, block_num_t block_num){
    if(fs == NULL || is_block_valid(fs, block_num) == false || is_block_free(fs, block_num)){
        mini_log(ERROR, "block_num_to_block_pointer", "Tentativo di ottenere il puntatore a un blocco non valido o non allocato");
        return false;
    }
    
    return (get_fs_block_section(fs) + block_num);
}