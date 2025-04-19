#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fsFunctions.h"
#include "fatFS.h"
#include "fsUtils.h"
#include "minilogger.h"

void clear_block(const FAT_FS* fs, const block_num_t block_to_clear){
    if(is_not_null_fs(fs) && is_block_valid(fs, block_to_clear)){
        memset(((get_fs_block_section(fs) + block_to_clear)), 0, BLOCK_SIZE);
        //printf("EXTRA (clear_block) Azzerato blocco %d\n", block_to_clear);
        //mini_log(LOG, "clear_block", "Blocco azzerato");
    }
    else{
        //printf("EXTRA (clear_block) FALLITA pulizia blocco %d\n", block_to_clear);
        mini_log(ERROR, "clear_block", "Impossibile azzerare un blocco");
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

int write_to_block(const FAT_FS* fs, const block_num_t target_block, unsigned int offset, char* src_buffer, size_t length){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "write_to_block", "Impossibile operare su un fs nullo o non valido.");
        return -1;
    }

    if(is_block_valid(fs, target_block) == false ){
        mini_log(ERROR, "write_to_block", "Blocco su cui scrivere inesistente!");
        return -1;
    }

    if(is_block_free(fs, target_block)){
        mini_log(ERROR, "write_to_block", "Blocco su cui scrivere non allocato!");
        return -1;
    }

    if(offset >= BLOCK_SIZE){
        mini_log(ERROR, "write_to_block", "Offset errato");
        return -1;
    }

    if(length <= 0 || length > BLOCK_SIZE){
        mini_log(ERROR, "write_to_block", "Il numero di byte da scrivere supera BLOCK_SIZE oppure è inferiore o uguale a 0");
        return -1;
    }

    if(offset + length > BLOCK_SIZE){
        mini_log(ERROR, "write_to_block", "Non è possibile scrivere il numero di caratteri richiesto a partire dall'offset specificato");
        return -1;
    }

    BLOCK* blck = ((BLOCK*)get_fs_block_section(fs)) + target_block;
    char* write_start = ((char*)blck) + offset;

    memcpy(write_start, src_buffer, length);

    return 0;
}

int read_block(const FAT_FS* fs, const block_num_t target_block, unsigned int offset, char* dst_buffer, size_t length){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "read_block", "Impossibile operare su un fs nullo o non valido.");
        return -1;
    }

    if(is_block_valid(fs, target_block) == false ){
        mini_log(ERROR, "read_block", "Blocco da leggere inesistente!");
        return -1;
    }

    if(is_block_free(fs, target_block)){
        mini_log(ERROR, "read_block", "Blocco da leggere non allocato!");
        return -1;
    }

    if(offset >= BLOCK_SIZE){
        mini_log(ERROR, "read_block", "Offset errato");
        return -1;
    }

    if(length <= 0 || length > BLOCK_SIZE){
        mini_log(ERROR, "read_block", "Il numero di byte da leggere supera BLOCK_SIZE oppure è inferiore o uguale a 0");
        return -1;
    }

    if(offset + length > BLOCK_SIZE){
        mini_log(ERROR, "read_block", "Non è possibile leggere il numero di caratteri richiesto a partire dall'offset specificato");
        return -1;
    }

    BLOCK* blck = ((BLOCK*)get_fs_block_section(fs)) + target_block;
    char* read_start = ((char*)blck) + offset;

    memcpy(dst_buffer, read_start, length);

    return 0;
}

void delete_all_file_handles(MOUNTED_FS* m_fs){
    if(m_fs == NULL){
        return;
    }

    FH_STACK_ELEM* curr;
    while(m_fs->open_file_handles != NULL){
        curr = m_fs->open_file_handles;

        m_fs->open_file_handles = curr->next;

        free(curr->file_handle);
        free(curr);
    }
}

void delete_file_handle(FILE_HANDLE* file_handle){
    if(file_handle == NULL){
        return;
    }
    if(file_handle->m_fs == NULL){
        return;
    }

    MOUNTED_FS* m_fs = file_handle->m_fs;

    if(m_fs->open_file_handles == NULL){
        // Non c'è nulla da cancellare perchè la pila di file_handles è vuota
        return;
    }

    if(m_fs->open_file_handles->file_handle == file_handle){
        // Se il file_handle da cancellare è il primo della pila
        FH_STACK_ELEM* delete_me = m_fs->open_file_handles;

        m_fs->open_file_handles = delete_me->next;

        free(delete_me->file_handle);
        free(delete_me);

        return;
    }
    else{
        FH_STACK_ELEM* prev = m_fs->open_file_handles;
        while(prev->next != NULL){
            FH_STACK_ELEM* curr = prev->next;

            if(curr->file_handle == file_handle){
                prev->next = curr->next;

                free(curr->file_handle);
                free(curr);

                return;
            }
            
            prev = curr;
        }
    }

    return; // Non è stato trovato il file_handle da cancellare
}

FILE_HANDLE* get_file_handle(MOUNTED_FS* m_fs, block_num_t first_file_block){
    if(m_fs == NULL || m_fs->fs == NULL){
        return NULL;
    }
    if(is_block_valid(m_fs->fs, first_file_block) == false || is_block_free(m_fs->fs, first_file_block)){
        return NULL;
    }

    FH_STACK_ELEM* curr = m_fs->open_file_handles;

    while(curr != NULL){
        if(curr->file_handle->first_file_block == first_file_block){
            return curr->file_handle;
        }

        curr = curr->next;
    }

    // Non trovato
    return NULL;
}

FILE_HANDLE* get_or_create_file_handle(MOUNTED_FS* m_fs, block_num_t first_file_block){
    if(m_fs == NULL || m_fs->fs == NULL){
        return NULL;
    }
    if(is_block_valid(m_fs->fs, first_file_block) == false || is_block_free(m_fs->fs, first_file_block)){
        return NULL;
    }

    FILE_HANDLE* fh = get_file_handle(m_fs, first_file_block);
    
    if(fh == NULL){
        // Non è già presente nella pila, va creato
        mini_log(LOG, "get_or_create_file_handle", "File handle non trovato nella pila, lo creo per questo file");

        fh = (FILE_HANDLE*) malloc(sizeof(FILE_HANDLE));
        if(fh == NULL){
            mini_log(ERROR, "get_or_create_file_handle", "RAM insufficiente per allocare un nuovo file handle");
            return NULL;
        }

        fh->first_file_block = first_file_block;
        fh->head_pos = 0;
        fh->m_fs = m_fs;
        fh->status_flags = 0;
    }
    else{
        mini_log(LOG, "get_or_create_file_handle", "File handle già trovato, non ne verrà allocato uno nuovo per il file");
    }

    return fh;
}

int extend_file(FILE_HANDLE* file, block_num_t n_blocks_to_allocate){
    if(is_file_handle_valid(file) == false){
        return -1;
    }

    if(n_blocks_to_allocate <= 0){
        return -1;
    }

    if(n_blocks_to_allocate >= get_n_free_blocks(file->m_fs->fs)){
        return -1;
    }

    // Raggiungo l'ultimo blocco già allocato del file

    block_num_t blk = get_last_block_file_or_dir(file->m_fs->fs, file->head_pos);
    if(blk == INVALID_BLOCK){
        mini_log(ERROR, "extend_file", "Impossibile raggiungere l'ultimo blocco del file");
        return -1;
    }

    // Alloco n_blocks_to_allocate, uno dopo l'altro, appendendoli all'ultimo blocco precedentemente allocato
    // Non dovrebbero verificarsi errori di allocazione in questa fase viste le verifiche precedenti sullo spazio libero disponibile.
    
    FAT_ENTRY* fat = get_fs_fat(file->m_fs->fs);

    for(int i=0; i < n_blocks_to_allocate; ++i){
        block_num_t new_block = allocate_block(file->m_fs->fs);

        fat[blk].next_block = new_block;
        blk = new_block;
    }

    mini_log(LOG, "extend_file", "File esteso con successo, blocchi(blocco) aggiunti in coda");
    return 0;
}

int extend_dir(MOUNTED_FS* m_fs, block_num_t dir_block, block_num_t n_blocks_to_allocate){
    if(m_fs == NULL || m_fs->fs == NULL){
        return -1;
    }

    if(is_block_valid(m_fs->fs, dir_block) == false || is_block_free(m_fs->fs, dir_block)){
        return -1;
    }
    
    if(n_blocks_to_allocate <= 0){
        return -1;
    }

    if(n_blocks_to_allocate >= get_n_free_blocks(m_fs->fs)){
        return -1;
    }

    // Ottengo il numero del primo blocco della cartella
    block_num_t first_dir_block = get_first_dir_block_from_curr_dir_block(m_fs->fs, dir_block);
    if(first_dir_block == INVALID_BLOCK){
        mini_log(ERROR, "extend_dir", "Impossibile ottenere il primmo blocco della cartella attuale");
        return -1;
    }

    // Raggiungo l'ultimo blocco della cartella
    block_num_t blk = get_last_block_file_or_dir(m_fs->fs, dir_block);
    if(blk == INVALID_BLOCK){
        mini_log(ERROR, "extend_dir", "Impossibile raggiungere l'ultimo blocco della directory");
        return -1;
    }

    // Alloco n_blocks_to_allocate e li appendo uno alla volta, scrivo i dati necessari nella file_entry 0 di ogni blocco.
    FAT_ENTRY* fat = get_fs_fat(m_fs->fs);

    for(int i=0; i < n_blocks_to_allocate; ++i){
        block_num_t new_block = allocate_block(m_fs->fs);

        fat[blk].next_block = new_block;
        blk = new_block;

        // La conversione di puntatore può essere fatta così perchè bisogna accedere proprio alla dir_entry con indice 0 del blocco
        DIR_ENTRY* de = (DIR_ENTRY*) block_num_to_block_pointer(m_fs->fs, blk);
        if(de != NULL){
            de->is_dir = DIR_REF;
            de->internal_dir_ref.ref.block = first_dir_block;
        }
        else{
            mini_log(ERROR, "extend_dir", "Non è stato possibile ottenere la dir_entry 0 del nuovo blocco allocato, il fs è corrotto");
        }
    }

    mini_log(LOG, "extend_dir", "Cartella estesa con successo, blocchi(blocco) aggiunti(o) in coda");
    return 0;
}

int update_dir_elem_added(const FAT_FS* fs, block_num_t dir_block){
    if(fs == NULL){
        mini_log(ERROR, "update_dir_elem_added", "Fs nullo");
        return -1;
    }

    block_num_t fdb = get_first_dir_block_from_curr_dir_block(fs, dir_block);
    if(fdb == INVALID_BLOCK){
        mini_log(ERROR, "update_dir_elem_added", "Impossibile ottenere il primo blocco della cartella");
        return -1;
    }

    DIR_ENTRY* de = (DIR_ENTRY*) block_num_to_block_pointer(fs, fdb);
    if(de == NULL){
        mini_log(ERROR, "update_dir_elem_added", "Impossibile ottenere il puntatore al blocco");
        return -1;
    }

    if(fdb == ROOT_DIR_STARTING_BLOCK){
        ++(de->file.n_elems);
    }
    else{
        DIR_ENTRY* de_to_update = dir_entry_pos_to_dir_entry_pointer(fs, de->internal_dir_ref.ref);
        ++(de_to_update->file.n_elems);
    }

    return 0;
}

int update_dir_elem_removed(const FAT_FS* fs, block_num_t dir_block){
    if(fs == NULL){
        mini_log(ERROR, "update_dir_elem_removed", "Fs nullo");
        return -1;
    }

    block_num_t fdb = get_first_dir_block_from_curr_dir_block(fs, dir_block);
    if(fdb == INVALID_BLOCK){
        mini_log(ERROR, "update_dir_elem_removed", "Impossibile ottenere il primo blocco della cartella");
        return -1;
    }

    DIR_ENTRY* de = (DIR_ENTRY*) block_num_to_block_pointer(fs, fdb);
    if(de == NULL){
        mini_log(ERROR, "update_dir_elem_removed", "Impossibile ottenere il puntatore al blocco");
        return -1;
    }

    if(fdb == ROOT_DIR_STARTING_BLOCK){
        --(de->file.n_elems);
    }
    else{
        DIR_ENTRY* de_to_update = dir_entry_pos_to_dir_entry_pointer(fs, de->internal_dir_ref.ref);
        --(de_to_update->file.n_elems);
    }

    return 0;
}