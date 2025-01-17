#include <stdio.h>
#include "fatFS.h"

int main(){
    printf("\n\tfatFS versione %d\n\n", CURRENT_FS_VERSION);

    int completed_tests = 0;

    printf("\n\tAvvio test\n");

    printf("\n\t\"Formattazione\" di un file con fatFS\n\n");

    if(create_fs_on_file("param_non_utilizzato.txt", 6666) != 0){
        printf("\n\tTest creazione file system: ERRORE\n");
    }
    else{
        printf("\n\tTest creazione file system: SUCCESSO\n");
        ++completed_tests;
    }

    // Qui ci sarà l'opzione per eseguire i test o per avviare trashbash e provare il programma finito
}