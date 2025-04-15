#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "fatFS.h"
#include "fsUtils.h"
#include "fsFunctions.h"
#include "minilogger.h"

// Il parametro file_name è inutilizzato, per ora il file avrà sempre nome fat.myfat.
// Restituisce 0 in caso di successo, -1 altrimenti.
int create_fs_on_file(const char* const file_name, block_num_t n_blocks){
    if(FILE_ENTRIES_PER_DIR_BLOCK <= 1){
        mini_log(ERROR, "create_fs_on_file", "Dimensione dei blocchi troppo piccola per il funzionamento del File System (FILE_ENTRIES_PER_DIR_BLOCK <= 1)!");
        return -1;
    }
    
    void* mmapped_fs; 

    // Calcolo dello spazio necessario per l'intero fs, in byte
    unsigned long file_size = 0;

    if(n_blocks <= 100){
        printf("Chiesto fs da %d blocchi, verrà allocato da 2001 blocchi (dimensione minima di 100 non raggiunta).\n", n_blocks);
        n_blocks = 2000 + 1; // Approssimativamente 1 MB di dimensione se BLOCK_SIZE = 512 (il blocco in più è per la root directory)
    }

    // Dimensione delle informazioni sul fs
    file_size += sizeof(FAT_FS_HEADER); 

    // Dimensione della FAT
    file_size += n_blocks * sizeof(FAT_ENTRY);
    
    // Dimensione della sezione dei blocchi
    file_size += n_blocks * sizeof(BLOCK);

    // Creo un file di dimensione (almeno) pari a file_size
    FILE* fs_file;

    // Nota la modalità di apertura del file! wb+ significa apri in lettura/scrittura un file binario, se esiste elimina il contenuto (o crealo se non esiste)!
    if((fs_file = fopen("fat.myfat", "wb+")) == NULL){
        mini_log(ERROR, "create_fs_on_file", "Impossibile creare un nuovo file, potrebbe già essere aperto da un altro processo.");
        return -1;
    }
    mini_log(LOG, "create_fs_on_file", "Nuovo file creato (non ancora formattato).");

    if(ftruncate(fileno(fs_file), file_size) == -1){
        mini_log(ERROR, "create_fs_on_file", "Impossibile impostare la dimensione necessaria per il file.");
        fclose(fs_file);
        return -1;
    }
    mini_log(LOG, "create_fs_on_file", "File ridimensionato correttamente per il fs.");


    // Secondo passo, mmap del file
    mmapped_fs = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(fs_file), 0);

    if(mmapped_fs == MAP_FAILED){
        mini_log(ERROR, "create_fs_on_file", "Impossibile eseguire la mappatura del file su RAM?");
        perror("");
        fclose(fs_file);
        return -1;
    }
    mini_log(LOG, "create_fs_on_file", "File mmappato con successo.");

    /* 
        Terzo passo, formatta correttamente il file mmappato,
        inizializza la FAT e la cartella di root (e organizza i blocchi liberi)
    */

    FAT_FS new_fs;
    
    new_fs.start_location = (char*) mmapped_fs;
    new_fs.fat_offset = sizeof(FAT_FS_HEADER);
    new_fs.block_section_offset = new_fs.fat_offset + (sizeof(FAT_ENTRY) * n_blocks);

    // Inizializzo l'header
    FAT_FS_HEADER* header = get_fs_header(&new_fs);


    header->n_blocks = n_blocks;
    header->version = CURRENT_FS_VERSION;

    /*  
        Vi saranno n_blocks - 1 liberi dopo l'inizializzazione,
        sapendo che il blocco iniziale (quello all'indice 0 della root directory) viene immediatamente allocato
    */
    header->n_free_blocks = header->n_blocks - 1; 

    header->first_free_block = 1;
    header->last_free_block = header->n_blocks - 1;
    
    /* Inizializzo i blocchi vuoti (come se fossero i blocchi di un unico grande file) */
    block_num_t free_blocks_to_validate = header->n_free_blocks;

    FAT_ENTRY* fat = get_fs_fat(&new_fs);
    // curr_block inizia da 1 perchè 0 è il primo blocco della root dir
    for(int curr_block = 1; free_blocks_to_validate > 0; free_blocks_to_validate--){
        
        fat[curr_block].block_status = FREE_BLOCK;
        clear_block(&new_fs, curr_block);

        if(free_blocks_to_validate > 1){
            fat[curr_block].next_block = curr_block + 1;
        }
        else{   /* free_block_validate == 1 */
            fat[curr_block].next_block = LAST_BLOCK;
        }

        ++curr_block;
    }

    mini_log(LOG, "create_fs_on_file", "Header inizializzato e blocchi liberi organizzati.");

    /*
        Creo la cartella root, che non è una cartella qualsiasi perchè non ha un predecessore nella gerarchia
        e non ha una DIR_ENTRY in qualche altra cartella con le sue informazioni, quindi queste sono salvate direttamente nella DIR_ENTRY[0] del blocco 0.        
    */
    fat[ROOT_DIR_STARTING_BLOCK].block_status = ALLOCATED_BLOCK;
    fat[ROOT_DIR_STARTING_BLOCK].next_block = LAST_BLOCK;
    clear_block(&new_fs, ROOT_DIR_STARTING_BLOCK);

    // Creo la DIR_ENTRY[0] della root dir, con le informazioni necessarie
    BLOCK* first_root_dir_block = get_fs_block_section(&new_fs);
    DIR_ENTRY* first_dir_entry = ((DIR_ENTRY*)first_root_dir_block);

    first_dir_entry->name[0] = 'r';
    first_dir_entry->name[1] = 'o';
    first_dir_entry->name[2] = 'o';
    first_dir_entry->name[3] = 't';

    first_dir_entry->name[4] = '\0';

    first_dir_entry->file_extension[0] = '\0';

    first_dir_entry->is_dir = DIR_REF_ROOT;

    first_dir_entry->file.start_block = ROOT_DIR_STARTING_BLOCK;
    first_dir_entry->file.n_elems = 0;

    mini_log(LOG, "create_fs_on_file", "Cartella root creata.");

    if(munmap(mmapped_fs, file_size) == -1){
        mini_log(WARNING, "create_fs_on_file", "munmap fallita.");
        perror("");
        fclose(fs_file);
        return -1;
    }
    
    fclose(fs_file);

    mini_log(INFO, "create_fs_on_file", "File System creato con successo sul file.");

    return 0;
}

MOUNTED_FS* mount_fs_from_file(const char* const file_name){
    /*
        Passo 1: apri il file (se esiste)
    */

    FILE* file_with_fs;

    // Nota la modalità di apertura del file! rb+ significa apri in lettura/scrittura un file binario che esiste già (e non crearlo se non esiste)!
    if((file_with_fs = fopen(file_name, "rb+")) == NULL){
        mini_log(ERROR, "mount_fs_from_file", "Impossibile aprire il file.");
        return NULL;
    }
    mini_log(LOG, "mount_fs_from_file", "File aperto.");


    /*
        Passo 2: Verifica che il file rispetti la dimensione minima (maggiore di sizeof(FAT_FS_HEADER)), se la rispetta esegui mmap del file
    */

    struct stat file_stats;
    fstat(fileno(file_with_fs), &file_stats);
    unsigned long file_size = file_stats.st_size;

    if(file_size <= sizeof(FAT_FS_HEADER)){
        mini_log(ERROR, "mount_fs_from_file", "Il file non è formattato correttamente (dimensione minima non rispettata)");
        fclose(file_with_fs);
        return NULL;
    }

    void* mmapped_fs;
    mmapped_fs = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file_with_fs), 0);
    if(mmapped_fs == MAP_FAILED){
        mini_log(ERROR, "mount_fs_from_file", "Impossibile eseguire la mappatura del file su RAM?");
        perror("");
        fclose(file_with_fs);
        return NULL;
    }

    /*
        Passo 3: Leggi i primi sizeof(FAT_FS_HEADER) byte (interpretandoli come fs_fat_header).
        Verifica poi che il file sia di dimensione sufficiente in base alle caratteristiche riportate nell'header.
    */

    FAT_FS_HEADER* header = (FAT_FS_HEADER*)mmapped_fs;

    if(header->version != CURRENT_FS_VERSION){
        mini_log(ERROR, "mount_fs_from_file", "Versione del fs sul file diversa da quella supportata.");
        fclose(file_with_fs);
        munmap(mmapped_fs, file_size);
        return NULL;
    }
    if(header->n_blocks <= 0){
        mini_log(ERROR, "mount_fs_from_file", "Numero di blocchi non corretto (n_blocchi <= 0) !.");
        fclose(file_with_fs);
        munmap(mmapped_fs, file_size);
        return NULL;
    }

    unsigned int expected_file_size = sizeof(FAT_FS_HEADER) + (sizeof(FAT_ENTRY) * (header->n_blocks)) + (sizeof(BLOCK) * (header->n_blocks)); 
    if(file_size < expected_file_size){
        mini_log(ERROR, "mount_fs_from_file", "File non formattato correttamente (dimensione insufficiente rispetto alle informazioni nell'header)!");
        fclose(file_with_fs);
        munmap(mmapped_fs, file_size);
        return NULL;
    }

    /*
        Altri controlli di correttezza (?)
    */






    /*
        Passo 5: Crea una struttura FS_FAT (allocandola dinamicamente) e inserisci i valori rilevanti. 
        Crea una struttura MOUNTED_FS (allocandola dinamicamente)
    */

    FAT_FS* fs = (FAT_FS*) malloc(sizeof(FAT_FS));
    MOUNTED_FS* mounted_fs = (MOUNTED_FS*) malloc(sizeof(MOUNTED_FS));
    if(fs == NULL || mounted_fs == NULL){
        fclose(file_with_fs);
        mini_log(ERROR, "mount_fs_from_file", "Impossibile allocare dinamicamente le strutture dati necessarie per il fs");
        munmap(mmapped_fs, file_size);
        return NULL;
    }

    fs->start_location = (char*)mmapped_fs;
    fs->fat_offset = sizeof(FAT_FS_HEADER);
    fs->block_section_offset = sizeof(FAT_ENTRY) * header->n_blocks;

    /*
        Passo 6: Popola la struttura MOUNTED_FS e poni a NULL la lista di file_handle aperti
    */

    mounted_fs->fs = fs;
    mounted_fs->open_file_handles = NULL;
    

    fclose(file_with_fs); // chiudo il flusso, il file resterà comunque a disposizione del processo dato che è mmappato

    mini_log(INFO, "mount_fs_from_file", "File System montato con successo.");
    return mounted_fs;
}

int unmount_fs(MOUNTED_FS* m_fs){
    if(m_fs == NULL){
        mini_log(WARNING, "unmount_fs", "Tentativo di unmount di un fs nullo");
        return 0;
    }

    if(is_not_null_fs(m_fs->fs) == false){
        mini_log(ERROR, "unmount_fs", "Il fs non è popolato con la struttura dati necessaria!");
        return -1;
    }

    // Passo 1: Leggi l'header del fs, verificane la correttezza (?), ottieni le dimensioni del fs
    // Salto la verifica della correttezza
    unsigned n_blocks = ((FAT_FS_HEADER*)m_fs->fs->start_location)->n_blocks;
    unsigned long fs_size = sizeof(FAT_FS_HEADER) + (n_blocks * (sizeof(FAT_ENTRY) + sizeof(BLOCK)));

    // Passo 2: Esegui munmap del fs
    if(munmap(m_fs->fs->start_location, fs_size)){
        mini_log(ERROR, "unmount_fs", "Munmap fallita");
        perror("");

        free(m_fs->fs);
        free(m_fs);
        return -1;
    }

    // Passo 3: dealloca tutte le strutture dati allocate dinamicamente

    delete_all_file_handles(m_fs);

    free(m_fs->fs);
    free(m_fs);

    mini_log(INFO, "unmount_fs", "File System unmount completato con successo.");

    return 0;
}

bool is_file_handle_valid(FILE_HANDLE* file_handle){
    if(file_handle == NULL)
        return false;

    if(file_handle->m_fs == NULL || file_handle->m_fs->fs == NULL || is_block_valid(file_handle->m_fs->fs, file_handle->first_file_block) == false || is_block_free(file_handle->m_fs->fs, file_handle->first_file_block)){
        return false;
    }

    if(get_file_handle(file_handle->m_fs, file_handle->first_file_block) != file_handle){
        mini_log(WARNING, "is_file_handle_valid", "File_handle non presente nella lista di file_handle aperti, probabilmente ne è stato usato uno già deallocato!");
        return false;
    }

    return true;
}

FILE_HANDLE* create_file(MOUNTED_FS* m_fs, block_num_t directory_block, char* file_name_buf, char* extension_buf){
    if(m_fs == NULL || is_not_null_fs(m_fs->fs) == false){
        mini_log(ERROR, "create_file", "File System non valido");
        return NULL;
    }
    
    block_num_t first_dir_block = get_first_dir_block_from_curr_dir_block(m_fs->fs, directory_block);
    if(first_dir_block == INVALID_BLOCK){
        mini_log(ERROR, "create_file", "Impossibile ottenere il primo blocco della cartella");
        return NULL;
    }

    // Verifica che non vi sia già un file con lo stesso nome ed estensione in questa cartella
    DIR_ENTRY* already_exists = get_file_by_name(m_fs->fs, first_dir_block, file_name_buf, extension_buf);
    if(already_exists != NULL){
        mini_log(WARNING, "create_file", "Esiste già un file con lo stesso nome ed estensione");
        return NULL;
    }

    if(can_create_new_file(m_fs->fs, directory_block) == false){
        mini_log(WARNING, "create_file", "Impossibile creare un nuovo file");
        return NULL;
    }

    DIR_ENTRY_POSITION de_p = get_available_dir_entry(m_fs->fs, directory_block);
    if(is_dir_entry_position_null(de_p)){
        if(extend_dir(m_fs, first_dir_block, 1)){
            mini_log(ERROR, "create_file", "Estensione della cartella fallita");    
            return NULL;
        }
        
        mini_log(LOG, "create_file", "Cartella estesa per accomodare una nuova file_entry");
    }

    DIR_ENTRY* de = dir_entry_pos_to_dir_entry_pointer(m_fs->fs, de_p);

    strncpy(de->name, file_name_buf, MAX_FILENAME_SIZE);
    strncpy(de->file_extension, extension_buf, MAX_FILE_EXTENSION_SIZE);
    de->name[MAX_FILENAME_SIZE] = '\0';
    de->file_extension[MAX_FILE_EXTENSION_SIZE] = '\0';

    de->is_dir = DATA; // Vuol dire che questa dir_entry rappresenta un file vero e proprio (non una cartella figlia o una struttura interna alla cartella)


    // aggiorno la directory in cui si trova questo file (deve avere n_elems += 1)
    if(update_dir_elem_added(m_fs->fs, directory_block)){
        mini_log(ERROR, "create_file", "Fallito aggiornamento della cartella genitore del file creato");
    }

    de->file.file_size = 0;
    de->file.start_block = allocate_block(m_fs->fs);

    FIRST_FILE_BLOCK* ffb = (FIRST_FILE_BLOCK*) block_num_to_block_pointer(m_fs->fs, de->file.start_block);
    ffb->dir_entry_pos.block = de_p.block;
    ffb->dir_entry_pos.offset = de_p.offset;

    mini_log(LOG, "create_file", "File creato con successo");

    FILE_HANDLE* file_handle = get_or_create_file_handle(m_fs, de->file.start_block);
    if(file_handle == NULL)
        mini_log(ERROR, "create_file", "Impossibile allocare il file handle (RAM disponibile insufficiente?)");

    return file_handle;
}

int erase_file(FILE_HANDLE* file){
    if(file == NULL || file->m_fs == NULL || is_not_null_fs(file->m_fs->fs) == false){
        mini_log(ERROR, "erase_file", "File handle e/o file system non valido");
        return -1;
    }

    FIRST_FILE_BLOCK* ffb = (FIRST_FILE_BLOCK*) block_num_to_block_pointer(file->m_fs->fs, file->first_file_block);

    // Elimino tutti i blocchi del file (saltando il primo per ora)
    block_num_t next_block_to_free;
    block_num_t curr_block_to_free = get_next_block(file->m_fs->fs, file->first_file_block);
    while(curr_block_to_free != LAST_BLOCK){
        next_block_to_free = get_next_block(file->m_fs->fs, curr_block_to_free);

        free_block(file->m_fs->fs, curr_block_to_free);
    }

    // Elimino la file entry
    DIR_ENTRY* de = dir_entry_pos_to_dir_entry_pointer(file->m_fs->fs, ffb->dir_entry_pos);
    
    memset(de, 0, sizeof(DIR_ENTRY));
    de->is_dir = EMPTY;

    // Aggiorno la cartella che lo conteneva (n_elems -= 1)
    if(update_dir_elem_removed(file->m_fs->fs, ffb->dir_entry_pos.block)){
        mini_log(ERROR, "erase_file", "Fallito aggiornamento della cartella genitore del file eliminato");
    }

    // Dealloco il primo blocco del file

    free_block(file->m_fs->fs, file->first_file_block);

    // Dealloco il FILE_HANDLE

    file->m_fs = NULL;
    file->first_file_block = INVALID_BLOCK;

    delete_file_handle(file);

    return 0;
}

unsigned int get_file_size(const FAT_FS* fs, block_num_t first_file_block){
    if(fs == NULL){
        mini_log(ERROR, "get_file_size", "Tentativo di ottenere file_size fallito!");
        return 0;
    }

    if(is_block_valid(fs, first_file_block) == false || is_block_free(fs, first_file_block)){
        mini_log(ERROR, "get_file_size", "Tentativo di ottenere file_size fallito!");
        return 0;
    }

    // operazione pericolosa 1
    FIRST_FILE_BLOCK* ffb = (FIRST_FILE_BLOCK*)block_num_to_block_pointer(fs, first_file_block);
    if(ffb == NULL){
        mini_log(ERROR, "get_file_size", "Tentativo di ottenere file_size fallito!");
        return 0;
    }

    // operazione pericolosa 2
    DIR_ENTRY* de = dir_entry_pos_to_dir_entry_pointer(fs, ffb->dir_entry_pos);
    if(de == NULL){
        mini_log(ERROR, "get_file_size", "Tentativo di ottenere file_size fallito!");
        return 0;
    }

    return de->file.file_size;
}

unsigned int file_tell(FILE_HANDLE* file){
    if(is_file_handle_valid(file) == false){
        mini_log(ERROR, "file_tell", "file non valido");
        return 0;
    }

    return file->head_pos;
}

int file_seek(FILE_HANDLE* file, long int offset, int whence){
    if(is_file_handle_valid(file) == false){
        mini_log(ERROR, "file_seek", "file non valido");
        return -1;
    }

    unsigned int file_size = get_file_size(file->m_fs->fs, file->first_file_block);
    unsigned int simulated_file_pos;

    if(whence == FILE_SEEK_SET){
        simulated_file_pos = file->head_pos + offset;
    }
    else if(whence == FILE_SEEK_START){
        if(offset < 0){
            mini_log(ERROR, "file_seek", "FILE_SEEK_START con offset negativo!");
            return -1;
        }
        else{
            // offset >= 0
            simulated_file_pos = offset;
        }
    }
    else if(whence == FILE_SEEK_END){
        if(offset > 0){
            mini_log(ERROR, "file_seek", "FILE_SEEK_END con offset > 0!");
            return -1;
        }
        else{
            // offset quindi è <= 0
            simulated_file_pos = (file_size-1) + offset;
        }
    }
    else{
        mini_log(ERROR, "file_seek", "Argomento whence non valido");
        return -1;
    }

    if(simulated_file_pos < 0){
        mini_log(ERROR, "file_seek", "Impossibile porre la testa di lettura del file negativa!");
        return -1;
    }
    else if(simulated_file_pos >= file_size){
        mini_log(ERROR, "file_seek", "Impossibile porre la testa di lettura del file oltre le sue dimensioni!");
        return -1;
    }
    else{
        file->head_pos = simulated_file_pos;
        return 0;
    }
}

block_num_t create_dir(MOUNTED_FS* m_fs, block_num_t curr_dir_block, char* dir_name_buf){
    if(m_fs == NULL || m_fs->fs == NULL){
        return INVALID_BLOCK;
    }

    block_num_t first_dir_block = get_first_dir_block_from_curr_dir_block(m_fs->fs, curr_dir_block);
    if(first_dir_block == INVALID_BLOCK){
        mini_log(ERROR, "create_dir", "Impossibile ottenere il primo blocco della cartella");
        return INVALID_BLOCK;
    }

    if(dir_name_buf == NULL){
        mini_log(ERROR, "create_dir", "Nome della cartella non specificato (dir_name == NULL)");
        return INVALID_BLOCK;
    }

    // Verifica che ci sia spazio per creare una cartella nuova
    if(can_create_new_dir(m_fs->fs, first_dir_block) == false){
        mini_log(WARNING, "create_dir", "Impossibile procedere con la creazione di una nuova cartella, requisiti mancanti");
        return INVALID_BLOCK;
    }

    // Verifica che non esista già una cartella con lo stesso nome in questa cartella
    DIR_ENTRY* already_exists = get_dir_by_name(m_fs->fs, first_dir_block, dir_name_buf);
    if(already_exists != NULL){
        mini_log(WARNING, "create_dir", "Esiste già una cartella con lo stesso nome");
        return INVALID_BLOCK;
    }

    // Ottieni la dir_entry in cui salvare questa nuova cartella
    DIR_ENTRY_POSITION de_p = get_available_dir_entry(m_fs->fs, first_dir_block);
    if(is_dir_entry_position_null(de_p)){
        if(extend_dir(m_fs, first_dir_block, 1)){
            mini_log(ERROR, "create_dir", "Estensione della cartella fallita");    
            return INVALID_BLOCK;
        }
        
        mini_log(LOG, "create_dir", "Cartella estesa per accomodare una nuova file_entry");
    }

    DIR_ENTRY* de = dir_entry_pos_to_dir_entry_pointer(m_fs->fs, de_p);
    
    strncpy(de->name, dir_name_buf, MAX_FILENAME_SIZE);
    de->name[MAX_FILENAME_SIZE] = '\0';
    
    de->is_dir = DIR;
    
    de->file.n_elems = 0;
    de->file.start_block = allocate_block(m_fs->fs);
    
    BLOCK* new_dir_block = block_num_to_block_pointer(m_fs->fs, de->file.start_block);
    DIR_ENTRY* first_dir_entry = (DIR_ENTRY*) new_dir_block;
    
    first_dir_entry->is_dir = DIR_REF_TOP;
    first_dir_entry->internal_dir_ref.ref.block = de_p.block;
    first_dir_entry->internal_dir_ref.ref.offset = de_p.offset;
    
    // DEVO AGGIORNARE la directory in cui si trova questa nuova, dato che ora contiene un elemento in più.
    if(update_dir_elem_added(m_fs->fs, curr_dir_block)){
        mini_log(ERROR, "erase_dir", "Fallito aggiornamento della cartella genitore di quella creata");
    }
    
    mini_log(LOG, "create_dir", "Cartella creata con successo");
    
    return de->file.start_block;
}

int erase_dir(MOUNTED_FS* m_fs, block_num_t dir_block){
    if(m_fs == NULL || m_fs->fs == NULL){
        return -1;
    }

    if(is_block_valid(m_fs->fs, dir_block) == false || is_block_free(m_fs->fs, dir_block)){
        return -1;
    }

    block_num_t fdb = get_first_dir_block_from_curr_dir_block(m_fs->fs, dir_block);
    if(fdb == INVALID_BLOCK){
        return -1;
    }

    if(fdb == ROOT_DIR_STARTING_BLOCK){
        return -1;
    }

    DIR_ENTRY* de_top = (DIR_ENTRY*) block_num_to_block_pointer(m_fs->fs, fdb);
    DIR_ENTRY* de = dir_entry_pos_to_dir_entry_pointer(m_fs->fs, de_top->internal_dir_ref.ref);
    
    // Controlla che la cartella sia vuota
    if(get_dir_n_elems(m_fs->fs, fdb) > 0){
        return -1;
    }

    // Dealloca tutti i blocchi successivi al primo (indicato dalla variabile fdb)
    block_num_t to_be_removed = get_next_block(m_fs->fs, fdb);
    block_num_t nxt;

    while(to_be_removed != LAST_BLOCK){
        nxt = get_next_block(m_fs->fs, to_be_removed);

        free_block(m_fs->fs, to_be_removed);
    }

    // Libera la file entry presente nella cartella che contiene questa cartella
    memset(de, 0, sizeof(DIR_ENTRY));
    de->is_dir = EMPTY;
    
    // (quindi aggiorna il numero di elementi contenuti nella cartella che contiene quella attuale)
    block_num_t parent_dir = get_parent_dir_block(m_fs->fs, dir_block);
    if(update_dir_elem_removed(m_fs->fs, parent_dir)){
        mini_log(ERROR, "erase_dir", "Fallito aggiornamento della cartella genitore di quella eliminata");
    }

    // Libera il blocco indicato da fdb
    free_block(m_fs->fs, fdb);

    // Termina con successo
    mini_log(LOG, "erase_dir", "Cartella eliminata con successo");
    return 0;
}

DIR_EXPLORER* list_dir(MOUNTED_FS* m_fs, block_num_t curr_dir){
    if(m_fs == NULL || m_fs->fs == NULL){
        return NULL;
    }

    block_num_t fdb = get_first_dir_block_from_curr_dir_block(m_fs->fs, curr_dir);
    if(fdb == INVALID_BLOCK){
        return NULL;
    }

    int n_elems = get_dir_n_elems(m_fs->fs, fdb);
    if(n_elems < 0){
        mini_log(ERROR, "list_dir", "Cartella da esplorare corrotta o non esistente (numero di elementi contenuti negativo o errore interno)");
        return NULL;
    }
    
    DIR_EXPLORER* exp = malloc(sizeof(DIR_EXPLORER));
    if(exp == NULL){
        mini_log(ERROR, "list_dir", "Memoria insufficiente per allocare la struttura dati DIR_EXPLORER");
        return NULL;
    }
    
    exp->elems = NULL;

    if(n_elems == 0){
        exp->n_elems = 0;
        return exp;
    }

    // Da ora è implementato il caso in cui la cartella contenga almeno un elemento
    exp->n_elems = n_elems;

    bool error = false;
    
    DIR_ENTRY* de;
    DIR_ENTRY_POSITION de_p;
    de_p.block = fdb;
    de_p.offset = 1;

    DIR_EXPLORER_STACK_ELEM** last = &(exp->elems);

    while(error == false && n_elems > 0){
        de = dir_entry_pos_to_dir_entry_pointer(m_fs->fs, de_p);
        
        if(de == NULL){
            error = true;
            mini_log(ERROR, "list_dir", "Impossibile ottenere un elemento della cartella");
            break;
        }
        
        if(de->is_dir == DIR || de->is_dir == DATA){
            DIR_EXPLORER_STACK_ELEM* new_elem = malloc(sizeof(DIR_EXPLORER_STACK_ELEM));
            if(new_elem == NULL){
                error = true;
                mini_log(ERROR, "list_dir", "Memoria insufficiente per allocare la struttura dati DIR_EXPLORER_STACK_ELEM");
                break;
            }

            new_elem->elem = de;
            new_elem->next = NULL;

            *last = new_elem;
            last = &(new_elem->next);

            --n_elems;
        }
        else if(de->is_dir != EMPTY){
            mini_log(ERROR, "list_dir", "Incontrato un elemento non previsto");
            error = true;
            break;
        }

        if(n_elems > 0){
            // Visto che so di dover ottenere ancora un elemento di questa cartella, passo a controllare la dir_entry successiva
            ++de_p.offset;
            if(de_p.offset >= FILE_ENTRIES_PER_DIR_BLOCK){
                // Allora devo passare al blocco successivo della cartella
                de_p.block = get_next_block(m_fs->fs, de_p.block);
                if(de_p.block == LAST_BLOCK){
                    // Dovrebbe esserci ancora un elemento da trovare ma i blocchi da controllare sono finiti, quindi errore (fs corrotto?)
                    error = true;
                    mini_log(ERROR, "list_dir", "Raggiunta la fine della cartella, ma non sono stati trovati tutti gli elementi attesi");
                    break;
                }

                de_p.offset = 1;
            }
        }
    }

    if(error == false){
        mini_log(LOG, "list_dir", "Contenuto della cartella ottenuto con successo");
    }

    return exp;
}

void delete_list_dir(DIR_EXPLORER* exp){
    if(exp == NULL){
        return;
    }
    
    DIR_EXPLORER_STACK_ELEM* curr = exp->elems;
    DIR_EXPLORER_STACK_ELEM* nxt = exp->elems;
    while(curr != NULL){
        nxt = curr->next;
        
        free(curr);

        curr = nxt;
    }

    free(exp);

    mini_log(LOG, "delete_list_dir", "Lista di elementi della cartella eliminata");
}

DIR_ENTRY* get_dir_by_name(const FAT_FS* fs, block_num_t curr_dir, const char* dir_name_buf){
    
    if(fs == NULL){
        mini_log(ERROR, "get_dir_by_name", "fs nullo");
        return NULL;
    }
    
    block_num_t fdb = get_first_dir_block_from_curr_dir_block(fs, curr_dir);
    if(fdb == INVALID_BLOCK){
        mini_log(ERROR, "get_dir_by_name", "Imposibile ottenere il primo blocco della cartella");
        return NULL;
    }

    int n_elems = get_dir_n_elems(fs, fdb);
    if(n_elems < 0){
        mini_log(ERROR, "get_dir_by_name", "Cartella corrotta");
    }
    
    bool error = false;
    
    DIR_ENTRY* de;
    DIR_ENTRY_POSITION de_p;
    de_p.block = fdb;
    de_p.offset = 1;
    
    while(error == false && n_elems > 0){
        de = dir_entry_pos_to_dir_entry_pointer(fs, de_p);
        
        if(de == NULL){
            error = true;
            break;
        }
        
        if(de->is_dir == DIR && strncmp(de->name, dir_name_buf, MAX_FILENAME_SIZE) == 0){
            return de;
        }
        
        if(de->is_dir == DATA || de->is_dir == DIR)
            --n_elems;

        if(n_elems > 0){
            // Ci sono ancora elementi da controllare
            ++de_p.offset;
            if(de_p.offset >= FILE_ENTRIES_PER_DIR_BLOCK){
                // Allora devo passare al blocco successivo della cartella
                de_p.block = get_next_block(fs, de_p.block);
                if(de_p.block == LAST_BLOCK){
                    // Dovrebbe esserci ancora un elemento da trovare ma i blocchi da controllare sono finiti
                    error = true;
                    break;
                }

                de_p.offset = 1;
            }
        }
    }

    if(error == true){
        mini_log(ERROR, "get_dir_by_name", "Errore nella ricerca");
    }

    return NULL;
}


DIR_ENTRY* get_file_by_name(const FAT_FS* fs, block_num_t curr_dir, const char* file_name_buf, const char* extension_buf){
    if(fs == NULL){
        mini_log(ERROR, "get_file_by_name", "fs nullo");
        return NULL;
    }

    block_num_t fdb = get_first_dir_block_from_curr_dir_block(fs, curr_dir);
    if(fdb == INVALID_BLOCK){
        mini_log(ERROR, "get_file_by_name", "Imposibile ottenere il primo blocco della cartella");
        return NULL;
    }

    int n_elems = get_dir_n_elems(fs, fdb);
    if(n_elems < 0){
        mini_log(ERROR, "get_file_by_name", "Cartella corrotta");
    }

    bool error = false;

    DIR_ENTRY* de;
    DIR_ENTRY_POSITION de_p;
    de_p.block = fdb;
    de_p.offset = 1;

    while(error == false && n_elems > 0){
        de = dir_entry_pos_to_dir_entry_pointer(fs, de_p);
        
        if(de == NULL){
            error = true;
            break;
        }
        
        if(de->is_dir == DATA && strncmp(de->name, file_name_buf, MAX_FILENAME_SIZE) == 0 && strncmp(de->file_extension, extension_buf, MAX_FILE_EXTENSION_SIZE) == 0){
            return de;
        }
        
        if(de->is_dir == DATA || de->is_dir == DIR)
            --n_elems;

        if(n_elems > 0){
            // Ci sono ancora elementi da controllare
            ++de_p.offset;
            if(de_p.offset >= FILE_ENTRIES_PER_DIR_BLOCK){
                // Allora devo passare al blocco successivo della cartella
                de_p.block = get_next_block(fs, de_p.block);
                if(de_p.block == LAST_BLOCK){
                    // Dovrebbe esserci ancora un elemento da trovare ma i blocchi da controllare sono finiti
                    error = true;
                    break;
                }

                de_p.offset = 1;
            }
        }
    }

    if(error == true){
        mini_log(ERROR, "get_file_by_name", "Errore nella ricerca");
    }

    return NULL;
}

int get_file_name_extension(const FAT_FS* fs, block_num_t ffb, char* name_buf, char* extension_buf){
    if(fs == NULL){
        return -1;
    }

    if(is_block_valid(fs, ffb) == false || is_block_free(fs, ffb)){
        return -1;
    }

    FIRST_FILE_BLOCK* blk = (FIRST_FILE_BLOCK*) block_num_to_block_pointer(fs, ffb);
    DIR_ENTRY_POSITION de_p = blk->dir_entry_pos;
    DIR_ENTRY* de = dir_entry_pos_to_dir_entry_pointer(fs, de_p);

    if(de->is_dir != DATA || de->file.start_block != ffb){
        return -1;
    }

    strncpy(name_buf, de->name, MAX_FILENAME_SIZE);
    strncpy(extension_buf, de->file_extension, MAX_FILE_EXTENSION_SIZE);

    return 0;
}

int get_directory_name(const FAT_FS* fs, block_num_t dir_block, char* name_buf){
    if(fs == NULL){
        return -1;
    }

    if(is_block_valid(fs, dir_block) == false || is_block_free(fs, dir_block)){
        return -1;
    }

    block_num_t fdb = get_first_dir_block_from_curr_dir_block(fs, dir_block);
    if(fdb == INVALID_BLOCK){
        return -1;
    }

    DIR_ENTRY* de;
    DIR_ENTRY* temp = (DIR_ENTRY*) block_num_to_block_pointer(fs, fdb);
    if(fdb == ROOT_DIR_STARTING_BLOCK){
        de = temp;
    }
    else{
        de = dir_entry_pos_to_dir_entry_pointer(fs, temp->internal_dir_ref.ref);
    }

    if(de->is_dir != DIR && de->is_dir != DIR_REF_ROOT){
        return -1;
    }

    strncpy(name_buf, de->name, MAX_FILENAME_SIZE);

    return 0;
}