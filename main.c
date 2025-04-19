#include <stdio.h>
#include <stdbool.h>

#include "fatFS.h"
#include "trashBash.h"

int main(){

    printf("\n\tTESTER-> fatFS versione %d\n\n", CURRENT_FS_VERSION);

    const int expected_tests = 3; // DA AGGIORNARE IN BASE AL NUMERO DI TEST SCRITTI
    
    int completed_tests = 0;
    int performed_tests = 0;

    int failed_tests = 0;


    printf("\n\tTESTER-> Avvio test\n\n");

    MOUNTED_FS* mounted_fs_testing;

    if(!failed_tests){
        ++performed_tests;
        if(create_fs_on_file("fat.myfat", 2001) != 0){
            printf("\n\tTESTER-> Test creazione file system su file: ERRORE\n\n");
            ++failed_tests;
        }
        else{
            printf("\n\tTESTER-> Test creazione file system su file: SUCCESSO\n\n");
            ++completed_tests;
        }
    }

    if(!failed_tests){
        ++performed_tests;

        mounted_fs_testing = mount_fs_from_file("fat.myfat");
        if(mounted_fs_testing == NULL){
            printf("\n\tTESTER-> Test caricamento del file system appena creato (dal file): ERRORE\n\n");
            ++failed_tests;
        }
        else{
            printf("\n\tTESTER-> Test caricamento del file system appena creato (dal file): SUCCESSO\n\n");
            ++completed_tests;
        }
    }

    if(!failed_tests){
        ++performed_tests;

        if(unmount_fs(mounted_fs_testing)){
            printf("\n\tTESTER-> Test unmount: ERRORE\n\n");
            ++failed_tests;
        }
        else{
            printf("\n\tTESTER-> Test unmount: SUCCESSO\n\n");
            ++completed_tests;
        }
    }

    printf("\n\n\tTESTER-> Procedura completata:\n\t\ttest effettuati %d di %d\n\t\ttest completati %d, test falliti %d\n\n", performed_tests, expected_tests, completed_tests, failed_tests);

    printf("Avvio TrashBash\n");
    trash_bash();
}