#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "fatFS.h"
#include "fsUtils.h"
#include "trashBash.h"

int main(){

    printf("\n\tTESTER-> fatFS versione %d\n\n", CURRENT_FS_VERSION);

    const int expected_tests = 5; // DA AGGIORNARE IN BASE AL NUMERO DI TEST SCRITTI
    
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

    
    FILE_HANDLE* test_file;
    
    if(!failed_tests){
        printf("\n\tTESTER-> Test scrittura e lettura da file\n");
        ++performed_tests;
        
        test_file = create_file(mounted_fs_testing, ROOT_DIR_STARTING_BLOCK, "prova", "txt");
        
        if(test_file == NULL){
            printf("\n\tTESTER-> Test creazione file: ERRORE\n\n");
            ++failed_tests;
        }
        else{
            printf("\n\tTESTER-> Test creazione file: SUCCESSO\n\n");
            ++completed_tests;
        }
    }
    
    if(!failed_tests){
        ++performed_tests;
        
        char test_string[1024];
        char test_output[1024];
        memset(test_string, 0, 1024);
        memset(test_output, 0, 1024);
        unsigned long int input_length;
        
        strncpy(test_string, "Prova scrittura e lettura testo, questa stringa dovrebbe rimanere uguale anche dopo averla scritta sul fs e averla riletta(questo testo incluso)lamelamangiocibofruttoananasrumgrottamaregrassboomsuperfoodincaliforniawithcapperiradicchioinsalsadiottonelimoneconpannaincercaditestingconstringalungachedeverimanereintattaefinisceconunaX", 1024);
        input_length = strnlen(test_string, 1024);


        printf("\nStringa di test di %lu caratteri:\n%s\n\n", input_length, test_string);
        
        
        long int fun_res = write_file(test_file, test_string, input_length);
        printf("\nScritti %ld di %lu caratteri della stringa sul file\n", fun_res, input_length);
        printf("Nuova dimensione del file: %u bytes\n\n", get_file_size(test_file->m_fs->fs, test_file->first_file_block));

        printf("\nSposto la testina di lettura del file all'inizio per leggerne l'intero contenuto\n\n");
        file_seek(test_file, 0, FILE_SEEK_START);

        fun_res = read_file(test_file, test_output, input_length);
        printf("\nLetti %ld di %lu caratteri della stringa dal file\n", fun_res, input_length);
        
        printf("Dopo scrittura e lettura, ottenuta stringa di %lu caratteri:\n%s\n\n", strnlen(test_output, 1024), test_output);

        if(strcmp(test_string, test_output) != 0){
            printf("\n\tTESTER-> Test scrittura e lettura file: ERRORE\n\n");
            ++failed_tests;
        }
        else{
            printf("\n\tTESTER-> Test scrittura e lettura file: SUCCESSO\n\n");
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