#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "fatFS.h"
#include "fsUtils.h"
#include "trashBash.h"

#define TEST_STRING_MAX_LENGTH_BYTES          524288
#define GENERATED_TEST_STRING_LENGTH_BYTES    7235
#define TEST_FS_BLOCKS                        20000        // 20000 * 512 ~ 10 MB circa 

// Generatore rubato di stringhe per il testing
void rand_str(char *dest, size_t length) {
    char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!?";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}

int main(){

    printf("\n\tTESTER-> fatFS versione %d\n\n", CURRENT_FS_VERSION);

    const int expected_tests = 5; // DA AGGIORNARE IN BASE AL NUMERO DI TEST SCRITTI
    
    int completed_tests = 0;
    int performed_tests = 0;

    int failed_tests = 0;


    printf("\n\tTESTER-> Avvio test\n\n");

    if(GENERATED_TEST_STRING_LENGTH_BYTES > TEST_STRING_MAX_LENGTH_BYTES){
        printf("\n\tTESTER-> Controlla bene i parametri di test!\n\n");
        return -1;
    }

    printf("\n\tTESTER-> Parametri:\n\tDimensione del fs in blocchi: %d\n\tLunghezza della stringa di prova: %d\n\n", TEST_FS_BLOCKS, GENERATED_TEST_STRING_LENGTH_BYTES);

    MOUNTED_FS* mounted_fs_testing;

    if(!failed_tests){
        ++performed_tests;
        if(create_fs_on_file("fat.myfat", TEST_FS_BLOCKS) != 0){
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
        printf("\n\tTESTER-> Test creazione file\n");
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

        printf("\n\tTESTER-> Test scrittura e lettura da file\n");
        
        char test_string[TEST_STRING_MAX_LENGTH_BYTES + 1];
        char test_output[TEST_STRING_MAX_LENGTH_BYTES + 1];
        memset(test_string, 0, TEST_STRING_MAX_LENGTH_BYTES + 1);
        memset(test_output, 0, TEST_STRING_MAX_LENGTH_BYTES + 1);
        unsigned long int input_length;

        /* Stringa di test
        strncpy(test_string, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nullam pellentesque vehicula neque, eu ornare orci interdum eu. Morbi et augue sit amet tellus faucibus venenatis. Curabitur dignissim enim ante, non sodales erat ornare eget. Morbi dapibus elementum felis et lobortis. Ut a lorem fringilla, sollicitudin velit eget, pharetra libero. Praesent nec odio in velit ultricies rutrum a eget quam. Nunc sed volutpat urna. In condimentum, ipsum id aliquet pulvinar, est augue consectetur ipsum, vel malesuada est tellus ut augue. Sed et erat et nunc facilisis finibus nec a velit. In mauris erat, mattis nec efficitur sit amet, porta nec enim. Maecenas iaculis felis sed venenatis iaculis. Integer consequat pretium est, non malesuada urna imperdiet et. Nullam eu velit orci.Phasellus maximus, justo eu posuere ultricies, nulla purus blandit mi, eget luctus justo ex sit amet nunc. Mauris pulvinar elit eu vehicula fermentum. Morbi molestie facilisis nunc sit amet volutpat. Vivamus rutrum nibh vel ex malesuada cursus mauris", 1024);
        strncpy(test_string+1023, "Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; Donec vel pharetra dolor, et molestie elit. Suspendisse sit amet nisl sed neque venenatis efficitur eget sit amet dolor. Vestibulum consequat luctus vulputate. Curabitur et ligula felis. Phasellus posuere orci ut massa sagittis, sit amet imperdiet libero dictum. Curabitur sit amet orci at orci aliquet facilisis sit amet quis libero. Donec rutrum in mi vel facilisis. Aenean in placerat tortor. Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos. Ut quis gravida ipsum. Morbi quis fermentum ante.Nullam maximus orci non mi faucibus, eget vestibulum ipsum aliquam. Curabitur pellentesque placerat nibh. Sed ut commodo justo, a molestie orci. Curabitur condimentum vestibulum ante ut pellentesque. Fusce ac sagittis justo, a iaculis mi. Integer sit amet sem vitae diam volutpat aliquet. Nulla sit amet justo venenatis, dignissim augue eu, sagittis leo. Cras sed commodo nulla, nec tincidunt morbi", 1024);
        */

        rand_str(test_string, GENERATED_TEST_STRING_LENGTH_BYTES);
        input_length = strnlen(test_string, TEST_STRING_MAX_LENGTH_BYTES);


        printf("\nStringa di test di %lu caratteri:\n%s\n\n", input_length, test_string);
        
        
        long int fun_res = write_file(test_file, test_string, input_length);
        printf("\nScritti %ld di %lu caratteri della stringa sul file\n", fun_res, input_length);
        printf("Nuova dimensione del file: %u bytes\n\n", get_file_size(test_file->m_fs->fs, test_file->first_file_block));

        printf("\nSposto la testina di lettura del file all'inizio per leggerne l'intero contenuto\n\n");
        file_seek(test_file, 0, FILE_SEEK_START);

        fun_res = read_file(test_file, test_output, input_length);
        printf("\nLetti %ld di %lu caratteri della stringa dal file\n", fun_res, input_length);
        
        printf("Dopo scrittura e lettura, è stata ottenuta una stringa di %lu caratteri:\n%s\n\n", strnlen(test_output, TEST_STRING_MAX_LENGTH_BYTES), test_output);

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