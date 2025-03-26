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

    /*
    BLOCK* blocks = (BLOCK*) (fat + n_blocks);                  // DA RICONTROLLARE

    new_fs.header = header;
    new_fs.fat = fat;
    new_fs.block_section = blocks;
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

    first_dir_entry->name[0] = '/';
    first_dir_entry->name[1] = '\0';

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
        Passo 6: Poni curr_dir_block a 0.
        Restituisci il puntatore alla struttura.
    */

    mounted_fs->fs = fs;
    mounted_fs->curr_dir_block = 0;

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

    // Passo 3: dealloca le strutture dati allocate dinamicamente (FAT_FS e MOUNTED_FAT_FS)
    free(m_fs->fs);
    free(m_fs);

    mini_log(INFO, "unmount_fs", "File System unmount completato con successo.");

    return 0;
}

FILE_HANDLE* create_file(MOUNTED_FS* m_fs, char* file_name_buf, char* extension_buf){
    if(m_fs == NULL || is_not_null_fs(m_fs->fs) == false){
        mini_log(ERROR, "create_file", "File System non valido");
        return NULL;
    }

    if(!can_create_new_file(m_fs->fs, m_fs->curr_dir_block)){
        mini_log(ERROR, "create_file", "Impossibile creare un nuovo file");
        return NULL;
    }

    DIR_ENTRY_POSITION de_p = get_available_dir_entry(m_fs->fs, m_fs->curr_dir_block);
    if(is_dir_entry_position_null(de_p)){
        // TODO
        mini_log(ERROR, "create_file", "ESTENSIONE DELLE CARTELLE NON ANCORA IMPLEMENTATA");
        return NULL;
    }

    DIR_ENTRY* de = dir_entry_pos_to_dir_entry_pointer(m_fs->fs, de_p);

    strncpy(de->name, file_name_buf, MAX_FILENAME_SIZE);
    strncpy(de->file_extension, extension_buf, MAX_FILE_EXTENSION_SIZE);
    de->name[MAX_FILENAME_SIZE] = '\0';
    de->file_extension[MAX_FILE_EXTENSION_SIZE] = '\0';

    de->is_dir = DATA; // Vuol dire che questa dir_entry rappresenta un file vero e proprio (non una cartella figlia o una struttura interna alla cartella)


    // AGGIORNA la directory in cui si trova questo file (deve avere n_elems += 1) TODO


    de->file.file_size = 0;
    de->file.start_block = allocate_block(m_fs->fs);

    FIRST_FILE_BLOCK* ffb = (FIRST_FILE_BLOCK*) block_num_to_block_pointer(m_fs->fs, de->file.start_block);
    ffb->dir_entry_pos.block = de_p.block;
    ffb->dir_entry_pos.offset = de_p.offset;

    mini_log(LOG, "create_file", "File creato con successo");

    FILE_HANDLE* file_handle = (FILE_HANDLE*) malloc(sizeof(FILE_HANDLE));
    if(file_handle == NULL)
        mini_log(ERROR, "create_file", "Impossibile allocare il file handle (RAM disponibile insufficiente?)");

    file_handle->first_file_block = de->file.start_block;
    file_handle->head_pos = 0;
    file_handle->m_fs = m_fs;

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

    // Aggiorno la cartella che lo conteneva (n_elems -= 1)
    // TODO

    // Se si libera un intero blocco della cartella, allora quel blocco dovrebbe essere deallocato?
    // Lo spazio vuoto di questa dir_entry dovrebbe essere riempito dalla dir_entry più in "profondità" per evitare inefficienze nell'utilizzo dei blocchi allocati?
    // TODO

    // Dealloco il primo blocco del file

    free_block(file->m_fs->fs, file->first_file_block);

    // Dealloco il FILE_HANDLE

    file->m_fs = NULL;
    file->first_file_block = INVALID_BLOCK;
    free(file);

    // Quando sarà implementata la lista di FILE_HANDLE aperti, dovrò rimuoverlo anche da quella
    // TODO
}