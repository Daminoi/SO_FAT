#ifndef FAT_FS_H
#define FAT_FS_H

#define CURRENT_FS_VERSION 1

#define byte_offset unsigned long int
#define block_num_t int             // Deve essere intero ed avere il valore -1 nel suo dominio

#define MAX_BLOCKS (1LL << ((sizeof(block_num_t) - 1) * 8)) - 1

#define ROOT_DIR_STARTING_BLOCK 0   // Deve rimanere 0 !!

// Valori possibili del campo is_dir di DIR_ENTRY
#define EMPTY 0
#define DATA 1
#define DIR 2

/*
    Ogni cartella ha nel suo primo blocco la prima DIR_ENTRY con valore is_dir = IS_DIR_REF_TOP che 
    contiene il numero del blocco e l'offset dove si trovano le informazioni sulla cartella 
    (quindi una DIR_ENTRY con is_dir = IS_DIR posizionata in un'altra cartella) 
*/
#define DIR_REF_TOP 3

/* 
    Stesso significato della costante precedente, ma usato SOLAMENTE nella cartella ROOT che contiene direttamente nel suo
    primo blocco i dati che descrivono la cartella (quindi solo i campi di "file" della union sono validi).
*/
#define DIR_REF_ROOT 4

/*
    La prima DIR_ENTRY di ogni blocco di una cartella (tranne nel caso che quel blocco sia il primo della cartella)
    punta al primo blocco di quella cartella
*/
#define DIR_REF 5


typedef struct FAT_FS_HEADER{
    int version;

    unsigned int n_blocks;
    unsigned int n_free_blocks;

    block_num_t first_free_block;
    block_num_t last_free_block;
} FAT_FS_HEADER;

#define MAX_FILENAME_SIZE 26
#define MAX_FILE_EXTENSION_SIZE 3

typedef struct DIR_ENTRY_POSITION{
    block_num_t block;  // Il blocco dove si trova quella dir entry (il fs di cui è parte il blocco non viene salvato in questa struttura)
    unsigned int offset;    // ES. 0 è la prima dir entry effettiva di quel blocco, viene esclusa la dir entry utilizzata per la struttura interna delle cartelle 
} DIR_ENTRY_POSITION;

typedef struct DIR_ENTRY{
    
    char name[27];         // 26 byte + 1 byte per il terminatore di stringa
    char is_dir;
    char file_extension[4];     // 3 byte + 1 byte per il terminatore di stringa
    
    union {
        
        struct{
            block_num_t start_block;
            
            union{
                unsigned int file_size;     // dimensione del file, se è una cartella allora è sensato n_elems
                int n_elems;       // valido solo per le cartelle (non sono contati gli elementi necessari al funzionamento interno)
            };
        } file;
        
        struct{
            DIR_ENTRY_POSITION ref; 
            // Se is_dir = DIR_REF, il valore ref.offset DEVE essere 0.
            // Se is_dir = DIR_REF_ROOT, ref non ha senso.
        } internal_dir_ref;
    };
} DIR_ENTRY;                   // 40 Bytes?

#define BLOCK_SIZE 512

typedef struct BLOCK{
    char block_data[BLOCK_SIZE];
} BLOCK;

#define FIRST_BLOCK_DATA_SIZE BLOCK_SIZE - sizeof(block_num_t) - sizeof(unsigned int)

typedef struct FIRST_FILE_BLOCK{
    DIR_ENTRY_POSITION dir_entry_pos;
    char first_block_data[FIRST_BLOCK_DATA_SIZE];
} FIRST_FILE_BLOCK;

typedef struct MOUNTED_FS MOUNTED_FS; // per risolvere il problema delle inclusioni di struct in altre struct

typedef struct FILE_HANDLE{
    MOUNTED_FS* m_fs;
    block_num_t first_file_block;
    unsigned int head_pos;
} FILE_HANDLE;

typedef struct FILE_HANDLE_STACK_ELEM{
    FILE_HANDLE* file_handle;
    struct FILE_HANDLE_STACK_ELEM* next;
} FH_STACK_ELEM;

typedef struct DIR_EXPLORER_STACK_ELEM{
    DIR_ENTRY* elem;
    struct DIR_EXPLORER_STACK_ELEM* next;
}DIR_EXPLORER_STACK_ELEM;

// Se si verificasse un errore nell'esplorazione di una cartella, n_elems non rifletterà il numero di elementi nella lista puntata da elems.
typedef struct DIR_EXPLORER{
    int n_elems;
    DIR_EXPLORER_STACK_ELEM* elems;
}DIR_EXPLORER;

// NOTA: si tratta di divisione intera! 9 / 4 = 2!
#define FILE_ENTRIES_PER_DIR_BLOCK (unsigned int)(BLOCK_SIZE / (sizeof(DIR_ENTRY)))

#define DIR_BLOCK_WASTED_BYTES (unsigned int)(BLOCK_SIZE - (FILE_ENTRIES_PER_DIR_BLOCK * sizeof(DIR_ENTRY)))

// Se un blocco è l'ultimo di un file, avrà questo valore nel campo next_block della fat
#define LAST_BLOCK -1

// un valore invalido per indicare un errore nel ritorno di un numero di blocco
#define INVALID_BLOCK -2

// viene restituito se si prova a risalire la gerarchia delle cartelle e si prova a ottenere il blocco del padre della cartella root (che ovviamente non esiste)
#define ROOT_DIR_HAS_NO_PARENT -3

// Valori che può assumere il campo block_status nella fat
#define FREE_BLOCK 0
#define ALLOCATED_BLOCK 1

typedef struct FAT_ENTRY{
    int block_status;
    block_num_t next_block;             // se vale -1, allora è l'ultimo blocco di un file o di una cartella
} FAT_ENTRY;

typedef struct FAT_FS{
    char* start_location;               // dove inizia lo spazio allocato dalla mmap per il fs
    //FAT_FS_HEADER* header;            // L'header è formato dai primi sizeof(FAT_FS_HEADER) byte
    byte_offset fat_offset;             // offset in byte dove inizia la fat rispetto all'inizio del fs, viene calcolato quando si carica il fs
    byte_offset block_section_offset;   // offset in byte da cui iniziano i blocchi con i dati e le cartelle, viene calcolato quando si carica il fs
} FAT_FS;

struct MOUNTED_FS{
    FAT_FS* fs;
    FH_STACK_ELEM* open_file_handles;
};

/* file_name per ora non è utilizzato, viene sempre creato un nuovo file fat.myfat */
int create_fs_on_file(const char* const file_name, block_num_t n_blocks);

MOUNTED_FS* mount_fs_from_file(const char* const file_name);
int unmount_fs(MOUNTED_FS* m_fs);

// Funzioni ausiliarie
bool is_file_handle_valid(FILE_HANDLE* file);

// Funzioni fondamentali

// Crea e apre il file
// Directory block dovrebbe essere il numero del primo blocco della directory 
// TODO
FILE_HANDLE* create_file(MOUNTED_FS* m_fs, block_num_t directory_block, char* file_name, char* extension);
int erase_file(FILE_HANDLE* file);

#define FILE_SEEK_SET 0
#define FILE_SEEK_START 1
#define FILE_SEEK_END 2

int file_seek(FILE_HANDLE* file, long int offset, int whence);
// Restituisce la posizione (byte) attuale della testina di lettura nel file
unsigned int file_tell(FILE_HANDLE* file);

// non è una funzione sicura
unsigned int get_file_size(const FAT_FS* fs, block_num_t first_file_block);

/*
    Crea una cartella di nome dir_name nella prima dir_entry disponibile nella cartella di cui curr_dir_block è un blocco (può non essere il primo, la funzione otterrà in automatico il primo blocco)
*/
int create_dir(MOUNTED_FS* m_fs, block_num_t curr_dir_block, char* dir_name_buf);
/*
    dir_block è un blocco della cartella da cancellare (può non essere il primo, la funzione otterrà da sola il primo e poi cancellerà l'intera cartella)
    Nota che la cartella deve essere vuota affinché possa essere cancellata.
    Non può mai essere cancellata la cartella ROOT.
*/
int erase_dir(MOUNTED_FS* m_fs, block_num_t dir_block);


DIR_EXPLORER* list_dir(MOUNTED_FS* m_fs, block_num_t curr_dir);
void delete_list_dir(DIR_EXPLORER* exp);

// ffb DEVE essere il numero del primo blocco del file o la funzione farà accessi in memoria errati
// TODO
void get_file_name_extension(block_num_t ffb, char* name_buf, char* extension_buf);
// TODO
void get_directory_name(block_num_t dir_block, char* name_buf);

// TODO
DIR_ENTRY* get_dir_by_name(const FAT_FS* fs, block_num_t curr_dir, const char* file_name_buf, const char* extension_buf);
// TODO
DIR_ENTRY* get_file_by_name(const FAT_FS* fs, block_num_t curr_dir, const char* dir_name_buf);

// Richieste da completare affinché il progetto sia completo
//int change_dir(MOUNTED_FS* fs, char* dir_name); // NON RIGUARDA IL FS

// Le ultime da vedere
//long int write(FILE_HANDLE* file, void* from_buffer, unsigned int n_bytes);
//long int read(FILE_HANDLE* file, void* dest_buffer, unsigned int n_bytes);


#endif /* FAT_FS_H */