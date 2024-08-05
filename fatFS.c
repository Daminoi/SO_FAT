#include "fatFS.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

int create_new_fat_fs_on_file(const char* const file_name){
    void* mmapped_fat_fs; 

    // Calcolo dello spazio necessario per l'intero fs, in byte
    unsigned long file_size = 0;

    int n_blocks = 2000 + 1; // Approssimativamente 1 MB (il blocco in più è per la root directory)

    // Dimensione delle informazioni sul fs
    file_size += sizeof(FAT_FS_HEADER); 

    // Dimensione della FAT
    file_size += n_blocks * sizeof(FAT_TABLE_ENTRY);
    
    // Dimensione dello spazio effettivamente dedicato a file e cartelle
    file_size += n_blocks * BLOCK_SIZE;


    // Creo un file di dimensione (almeno) pari a file_size
    FILE* fat_fs_file;

    if( (fat_fs_file = fopen("fat.myfat", "wb+")) == NULL ){
        mini_log(ERROR, NULL, -1, "Impossibile creare un nuovo file, potrebbe già essere aperto da un altro processo.");
        return -1;
    }

    if( ftruncate(fileno(fat_fs_file), file_size) == -1){
        mini_log(ERROR, NULL, -1, "Impossibile impostare la dimensione necessaria per il file.");
        return -1;
    }


    // Secondo passo, mmap del file
    mmapped_fat_fs = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(fat_fs_file), 0);

    if(mmapped_fat_fs == MAP_FAILED){
        mini_log(ERROR, NULL, -1, "Impossibile la mappatura del file su RAM?");
        return -1;
    }
    

    /* 
        Terzo passo, formatta correttamente il file mmappato,
        inizializza la FAT e la cartella di root
    */

}