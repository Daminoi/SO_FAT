#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "minilogger.h"
#include "fatFS.h"

/* Semplice, per ora, controllo di validità del fs (TODO) */
bool is_not_null_fs(const FAT_FS* fs){
    
    if(fs == NULL) return false;
    /*
    if(fs->header == NULL) return false;
    if(fs->fat == NULL) return false;
    if(fs->block_section == NULL) return false;
    */
    return true; 
}

FAT_FS_HEADER* _get_header(const FAT_FS* fs){
    return (FAT_FS_HEADER*) fs->start_location;
}

// Restituisce il puntatore all'inizio della sezione della fat
FAT_ENTRY* _get_fat(const FAT_FS* fs){
    if(is_not_null_fs(fs)){
        return (FAT_ENTRY*) (fs->start_location + fs->fat_offset);
    }
    else{
        mini_log(ERROR, "_get_fat", "fs nullo");
        return NULL;
    }
}

// Restituisce il puntatore all'inizio della sezione dei blocchi del fs
BLOCK* _get_block_section(const FAT_FS* fs){
    if(is_not_null_fs(fs)){
        return (BLOCK*) (fs->start_location + fs->block_section_offset);
    }
    else{
        mini_log(ERROR, "_get_block_section", "fs nullo");
        return NULL;
    }
}

/* Pone tutti i byte del blocco a 0, non lo dealloca */
void clear_block(const FAT_FS* fs, const block_num_t block_to_clear){
    if(is_not_null_fs(fs) && block_to_clear >= 0 && block_to_clear < _get_header(fs)->n_blocks){
        memset(&(_get_block_section(fs)[block_to_clear]), 0, BLOCK_SIZE);
        printf("EXTRA (clear_block) Azzerato blocco %d\n", block_to_clear);
        //mini_log(LOG, "clear_block", "Blocco azzerato");
    }
    else{
        printf("EXTRA (clear_block) FALLITA pulizia blocco %d\n", block_to_clear);
        //mini_log(ERROR, "clear_block", "Impossibile azzerare un blocco");
    }
}

unsigned int get_n_free_blocks(const FAT_FS* fs){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "get_n_free_blocks", "Fs nullo!");
        return 0;
    }
    else return _get_header(fs)->n_free_blocks;
}

unsigned int get_n_blocks(const FAT_FS* fs){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "get_n_blocks", "Fs nullo!");
        return 0;
    }
    else return _get_header(fs)->n_blocks;
}

// Restituisce true se il blocco è libero (fat[block].block_status == FREE_BLOCK)
bool _is_free_block(const FAT_FS* fs, block_num_t block){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "_is_free_block", "Fs nullo!");
        return false;
    }
    else return _get_fat(fs)[block].block_status == FREE_BLOCK;
}

bool _has_next_block(const FAT_FS* fs, block_num_t curr_block){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "_has_next_block", "Fs nullo!");
        return false;
    }
    else return _get_fat(fs)[curr_block].next_block >= 0;
}

block_num_t _get_next_block(const FAT_FS* fs, block_num_t curr_block){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "_get_next_block", "Fs nullo!");
        return INVALID_BLOCK;
    }
    else return _get_fat(fs)[curr_block].next_block;
}

bool _is_valid_block(const FAT_FS* fs, block_num_t block){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "_is_valid_block", "Fs nullo!");
        return false;
    }

    return block >= 0 && block < get_n_blocks(fs);
}





// Funzioni complesse

// Restituisce il numero del blocco allocato
block_num_t _allocate_block(const FAT_FS* fs){
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

    FAT_FS_HEADER* header = _get_header(fs);
    FAT_ENTRY* fat = _get_fat(fs);

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


// Dealloca un blocco, il blocco deve essere l'ultimo del file a cui appartiene (fat[block_to_free] == LAST_BLOCK)
bool _free_block(const FAT_FS* fs, const block_num_t block_to_free){
    if(is_not_null_fs(fs) == false){
        mini_log(ERROR, "free_block", "Impossibile operare su un fs nullo o non valido.");
        return false;
    }

    if(_is_valid_block(fs, block_to_free) == false){
        mini_log(ERROR, "free_block", "Blocco da liberare non esistente!");
        return false;
    }

    if(block_to_free == ROOT_DIR_STARTING_BLOCK){
        mini_log(ERROR, "free_block", "Tentativo di liberare il blocco 0 (inizio root directory)!");
        return false;
    }

    FAT_FS_HEADER* header = _get_header(fs);
    FAT_ENTRY* fat = _get_fat(fs);
    
    if(fat[block_to_free].block_status == FREE_BLOCK){
        mini_log(ERROR, "free_block", "Non si può deallocare un blocco già libero!");
        return false;
    }
    
    if(fat[block_to_free].next_block != LAST_BLOCK){
        mini_log(ERROR, "free_block", "Non può essere deallocato un blocco che non sia l'ultimo del file! (next_block != LAST_BLOCK)!");
        return false;
    }

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
    FAT_FS_HEADER* header = _get_header(&new_fs);


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

    FAT_ENTRY* fat = _get_fat(&new_fs);
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
    BLOCK* first_root_dir_block = _get_block_section(&new_fs);
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