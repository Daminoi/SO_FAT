#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#include "fatFS.h"
#include "trashBash.h"
#include "fsFunctions.h"
#include "fsUtils.h"
#include "minilogger.h"

int tb_create_new_fs_on_file(const char* filename, unsigned int n_blocks){
    if(filename == NULL){
        return -1;
    }

    return create_fs_on_file(filename, n_blocks);
}

int tb_mount_fs_from_file(const char* filename, TRASHBASH_PATH* path){
    if(filename == NULL){
        return -1;
    }

    path->m_fs = mount_fs_from_file(filename);

    if(path->m_fs == NULL){
        return -1;
    }
    else{
        path->path[0] = ROOT_DIR_STARTING_BLOCK;
        path->depth = 0;
        return 0;
    }
}

int tb_unmount_fs(TRASHBASH_PATH* path){
    if(path == NULL || path->m_fs == NULL){
        return -1;
    }

    int retval = unmount_fs(path->m_fs);
    path->m_fs = NULL;

    return retval;
}

int tb_pwd_impl(TRASHBASH_PATH* path){
    if(path == NULL || path->m_fs == NULL){
        return -1;
    }

    char dir_name_buf[MAX_FILENAME_SIZE + 1];
    dir_name_buf[MAX_FILENAME_SIZE] = '\0';

    get_directory_name(path->m_fs->fs, path->path[0], dir_name_buf);
    printf("%s", dir_name_buf);

    for(int i=1; i <= path->depth; ++i){
        get_directory_name(path->m_fs->fs, path->path[i], dir_name_buf);
        printf("/%s", dir_name_buf);
    }

    return 0;
}

int tb_pwd(TRASHBASH_PATH* path){
    int retval = tb_pwd_impl(path);
    
    printf("\n");

    return retval;
}

int tb_print_prompt(TRASHBASH_PATH* path){
    if(path == NULL || path->m_fs == NULL){
        // TODO
        // Prima di aver montato un fs, mostra delle indicazioni
        printf("§ ");

        return 0;
    }
    else{
        int retv = tb_pwd_impl(path);

        printf("$ ");

        return retv;
    }
}

int tb_list(TRASHBASH_PATH* path){
    if(path == NULL || path->m_fs == NULL){
        return -1;
    }

    char name_buf[MAX_FILENAME_SIZE + 1];
    char extension_buf[MAX_FILE_EXTENSION_SIZE + 1];
    name_buf[MAX_FILENAME_SIZE] = '\0';
    extension_buf[MAX_FILE_EXTENSION_SIZE] = '\0';

    DIR_EXPLORER* list_to_explore = list_dir(path->m_fs, path->path[path->depth]);
    if(list_to_explore == NULL){
        return -1;
    }

    DIR_EXPLORER_STACK_ELEM* curr = list_to_explore->elems;
    if(list_to_explore->n_elems > 1 || list_to_explore->n_elems == 0){
        printf("Ci sono %d elementi in questa cartella.\n", list_to_explore->n_elems);
    }
    else if(list_to_explore->n_elems == 1){
        printf("C'è un elemento in questa cartella.\n");
    }
    else{
        printf("Cartella corrotta (numero di elementi contenuti negativo?)\n");
        return -1;
    }

    while(curr != NULL){
        if(curr->elem->is_dir == DATA){
            get_file_name_extension(path->m_fs->fs, curr->elem->file.start_block, name_buf, extension_buf);
            printf("%s.%s FILE\n", name_buf, extension_buf);
        }
        else if(curr->elem->is_dir == DIR){
            get_directory_name(path->m_fs->fs, curr->elem->file.start_block, name_buf);
            printf("%s DIR\n", name_buf);
        }
        else{
            printf("Elemento IMPREVISTO (né un file, né una cartella)!");
        }

        curr = curr->next;
    }

    delete_list_dir(list_to_explore);

    return 0;
}

int tb_change_dir(TRASHBASH_PATH* path, char* goto_dir_name){
    if(path == NULL || path->m_fs == NULL || goto_dir_name == NULL){
        return -1;
    }

    if(strncmp(goto_dir_name, "..", sizeof("..")) == 0){
        if(path->depth <= 0){
            return -1;
        }
        else{
            --path->depth;
            return 0;
        }
    }

    if(path->depth >= (MAX_PATH_LENGTH - 1)){
        return -1;
    }

    DIR_ENTRY* de = get_dir_by_name(path->m_fs->fs, path->path[path->depth], goto_dir_name);
    if(de == NULL){
        printf("Cartella non esistente\n");
        return -1;
    }
    else{
        path->path[path->depth + 1] = de->file.start_block;
        path->depth++;

        return 0;
    }
}

int tb_create_file(TRASHBASH_PATH* path, char* file_name_buffer, char* file_extension_buffer, FILE_HANDLE** new_file){
    if(path == NULL || path->m_fs == NULL || file_name_buffer == NULL || file_extension_buffer == NULL){
        return -1;
    }

    FILE_HANDLE* file = create_file(path->m_fs, path->path[path->depth], file_name_buffer, file_extension_buffer);

    if(file == NULL){
        *new_file = NULL;
        return -1;
    }
    else{
        *new_file = file;
        return 0;
    }
}

int tb_create_dir(TRASHBASH_PATH* path, char* dir_name_buffer, block_num_t* new_dir){
    if(path == NULL || path->m_fs == NULL || dir_name_buffer == NULL){
        return -1;
    }

    block_num_t dir = create_dir(path->m_fs, path->path[path->depth], dir_name_buffer);
    if(dir == INVALID_BLOCK){
        if(new_dir != NULL){
            *new_dir = INVALID_BLOCK;
        }
        return -1;
    }
    else{
        if(new_dir != NULL){
            *new_dir = dir;
        }
        return 0;
    }
}

int tb_delete_file(TRASHBASH_PATH* path, char* file_name_buffer, char* file_extension_buffer){
    if(path == NULL || path->m_fs == NULL || file_name_buffer == NULL || file_extension_buffer == NULL){
        return -1;
    }

    DIR_ENTRY* de = get_file_by_name(path->m_fs->fs, path->path[path->depth], file_name_buffer, file_extension_buffer);
    if(de == NULL){
        return -1;
    }

    FILE_HANDLE* fh = get_or_create_file_handle(path->m_fs, de->file.start_block);
    if(fh == NULL){
        return -1;
    }

    return erase_file(fh);
}

int tb_delete_dir(TRASHBASH_PATH* path, char* dir_name_buffer){
    if(path == NULL || path->m_fs == NULL || dir_name_buffer == NULL){
        return -1;
    }

    DIR_ENTRY* de = get_dir_by_name(path->m_fs->fs, path->path[path->depth], dir_name_buffer);
    if(de == NULL){
        return -1;
    }

    return erase_dir(path->m_fs, de->file.start_block);
}

void tb_tree_helper(MOUNTED_FS* m_fs, block_num_t first_dir_block, unsigned int blank_offset){
    if(is_block_valid(m_fs->fs, first_dir_block) == false || is_block_free(m_fs->fs, first_dir_block)){
        mini_log(ERROR, "tb_tree_helper", "Blocco invalido o non allocato");
        return;
    }

    char name_buf[MAX_FILENAME_SIZE + 1];
    char extension_buf[MAX_FILE_EXTENSION_SIZE + 1];
    name_buf[MAX_FILENAME_SIZE] = '\0';
    extension_buf[MAX_FILE_EXTENSION_SIZE] = '\0';

    DIR_EXPLORER* list_to_explore = list_dir(m_fs, first_dir_block);
    if(list_to_explore == NULL){
        return;
    }

    if(list_to_explore->n_elems == 0){
        return;
    }
    else if(list_to_explore->n_elems < 0){
        for(int i=0; i < blank_offset; ++i){
            fputc(' ', stdout);
        }

        printf("! Cartella corrotta (numero di elementi contenuti negativo)\n");
        return;
    }
    
    // Almeno un elemento in questa cartella
    DIR_EXPLORER_STACK_ELEM* curr = list_to_explore->elems;

    while(curr != NULL){
        for(int i=0; i < blank_offset; ++i){
            fputc(' ', stdout);
        }

        if(curr->elem->is_dir == DATA){
            get_file_name_extension(m_fs->fs, curr->elem->file.start_block, name_buf, extension_buf);
            printf("- %s.%s\n", name_buf, extension_buf);
        }
        else if(curr->elem->is_dir == DIR){
            get_directory_name(m_fs->fs, curr->elem->file.start_block, name_buf);
            printf("+ %s\n", name_buf);

            tb_tree_helper(m_fs, curr->elem->file.start_block, blank_offset + 2);
        }
        else{
            printf("? Elemento IMPREVISTO (né un file, né una cartella)");
        }

        curr = curr->next;
    }

    delete_list_dir(list_to_explore);

    return;
}

int tb_tree(TRASHBASH_PATH* path){
    if(path == NULL || path->m_fs == NULL){
        return -1;
    }

    char start_dir_name[MAX_FILENAME_SIZE + 1];
    get_directory_name(path->m_fs->fs, path->path[path->depth], start_dir_name);

    printf("+ %s\n", start_dir_name);

    tb_tree_helper(path->m_fs, path->path[path->depth], 2);

    return 0;
}

int tb_parse_input(char* user_input_in_buffer, char* first_param_out_buffer, char* second_param_out_buffer, int* more_params_found){
    char* cmd_str;
    first_param_out_buffer[0] = '\0';
    second_param_out_buffer[0] = '\0';

    cmd_str = strtok(user_input_in_buffer, DELIMS);
    if(strlen(user_input_in_buffer) == 0 || cmd_str == NULL){
        return TB_NO_COMMAND;
    }

    int cmd;

    if(strcmp(cmd_str, TB_HELP_STRING) == 0){
        cmd = TB_HELP;
    }
    else if(strcmp(cmd_str, TB_CLS_STRING) == 0){
        cmd = TB_CLS;
    }
    else if(strcmp(cmd_str, TB_QUIT_STRING) == 0){
        cmd = TB_QUIT;
    }
    else if(strcmp(cmd_str, TB_MOUNT_FS_FROM_FILE_STRING) == 0){
        cmd = TB_MOUNT_FS_FROM_FILE;
    }
    else if(strcmp(cmd_str, TB_UNMOUNT_STRING) == 0){
        cmd = TB_UNMOUNT;
    }
    else if(strcmp(cmd_str, TB_CREATE_FS_ON_FILE_STRING) == 0){
        cmd = TB_CREATE_FS_ON_FILE;
    }
    else if(strcmp(cmd_str, TB_CHANGE_DIR_STRING) == 0){
        cmd = TB_CHANGE_DIR;
    }
    else if(strcmp(cmd_str, TB_PWD_STRING) == 0){
        cmd = TB_PWD;
    }
    else if(strcmp(cmd_str, TB_LIST_STRING) == 0){
        cmd = TB_LIST;
    }
    else if(strcmp(cmd_str, TB_TREE_STRING) == 0){
        cmd = TB_TREE;
    }
    else if(strcmp(cmd_str, TB_CREATE_FILE_STRING) == 0){
        cmd = TB_CREATE_FILE;
    }
    else if(strcmp(cmd_str, TB_CREATE_DIR_STRING) == 0){
        cmd = TB_CREATE_DIR;
    }
    else if(strcmp(cmd_str, TB_DELETE_FILE_STRING) == 0){
        cmd = TB_DELETE_FILE;
    }
    else if(strcmp(cmd_str, TB_DELETE_DIR_STRING) == 0){
        cmd = TB_DELETE_DIR;
    }
    else if(strcmp(cmd_str, TB_SAVE_FILE_TO_FS_STRING) == 0){
        cmd = TB_SAVE_FILE_TO_FS;
    }
    else if(strcmp(cmd_str, TB_EXPORT_FILE_FROM_FS_STRING) == 0){
        cmd = TB_EXPORT_FILE_FROM_FS;
    }
    else{
        return TB_UNKNOWN_COMMAND;
    }

    char* first_param = strtok(NULL, DELIMS);
    if(first_param == NULL){
        first_param_out_buffer[0] = '\0';
    }
    else{
        strncpy(first_param_out_buffer, first_param, PARAM_BUF_SIZE-1);
    }

    char* second_param = strtok(NULL, DELIMS);
    if(second_param == NULL){
        second_param_out_buffer[0] = '\0';
    }
    else{
        strncpy(second_param_out_buffer, second_param, PARAM_BUF_SIZE-1);
    }

    if(more_params_found != NULL && strtok(NULL, DELIMS) != NULL){
        *more_params_found = 1; 
    }

    return cmd;
}

void tb_print_version_info(){
    printf("TrashBash (ver: %s)\n\n", __TIMESTAMP__);
    return;
}

void print_help(){
    printf("Lista di comandi disponibili:\n\n");

    printf("> %s [nome_nuovo_file_su_computer] [numero_di_blocchi] : crea un nuovo fs_fat sul file 'nome_nuovo_file_su_computer' (nel file system vero di questo computer) che sarà poi possibile montare. Il nuovo fs avrà il numero di blocchi specificato al secondo parametro\n", TB_CREATE_FS_ON_FILE_STRING);
    printf("> %s [nome_file_su_computer_con_fs] : monta il fs_fat presente sul file 'nomefile_su_computer_con_fs'\n", TB_MOUNT_FS_FROM_FILE_STRING);
    printf("> %s                                : esegue l'unmount del fs attualmente montato\n", TB_UNMOUNT_STRING);

    printf("> %s                                : pulisce il terminale\n", TB_CLS_STRING);
    printf("> %s                                : chiude la TrashBash\n", TB_QUIT_STRING);
    printf("> %s                                : stampa il percorso attuale\n", TB_PWD_STRING);
    printf("> %s                                : stampa il contenuto della cartella attuale\n", TB_LIST_STRING);
    printf("> %s                                : stampa l'albero del fs\n", TB_TREE_STRING);
    printf("> %s [nome_cartella]                : cambia la cartella attuale a quella di nome 'nome_cartella' (che deve essere contenuta nella cartella attuale, non sono supportati percorsi relativi o percorsi assoluti), se 'nome_cartella' è '..' allora sale di un livello nel fs\n", TB_CHANGE_DIR_STRING);

    printf("> %s [nome_nuovo_file]              : crea un nuovo file di nome 'nome_nuovo_file' nella cartella attuale, non deve essere già presente un file con quel nome\n", TB_CREATE_FILE_STRING);
    printf("> %s [nome_file_da_cancellare]      : elimina il file di nome 'nome_file_da_cancellare' presente nella cartella attuale\n", TB_DELETE_FILE_STRING);
    printf("> %s [nome_nuova_cartella]          : crea una nuova cartella di nome 'nome_nuova_cartella' nella cartella attuale, non deve essere già presente una cartella con quel nome\n", TB_CREATE_DIR_STRING);
    printf("> %s [nome_cartella_da_cancellare]  : elimina la cartella di nome 'nome_cartella_da_cancellare' presente nella cartella attuale\n", TB_DELETE_DIR_STRING);

    printf("> %s [nome_file_su_computer] [nome_file_su_questo_FAT_FS] : copia il file [nome_file_su_computer] sul fs del computer nel file system montato\n", TB_SAVE_FILE_TO_FS_STRING);
    printf("> %s [nome_file_da_esportare]  : copia questo file presente sul fs montato nel fs del computer\n", TB_EXPORT_FILE_FROM_FS_STRING);
    
    printf("\n");
    return;
}

bool contains_invalid_chars(char* str_to_check){
    if(str_to_check == NULL){
        return false;
    }

    char* curr_char = str_to_check;
    
    while(*curr_char != '\0'){
        if(isalnum(*curr_char) == false && *curr_char != '.'){
            return true;
        }

        ++curr_char;
    }

    return false;
}

// Restituisce 0 se la stringa in input è un nome valido per una cartella, -1 altrimenti
// Va usata solo dopo aver eseguito contains_invalid_chars(dir_name)
bool is_valid_dir_name(char* dir_name){
    if(dir_name == NULL){
        return false;
    }

    if(strlen(dir_name) > MAX_FILENAME_SIZE){
        printf("La dimensione massima del nome di una cartella è di %d caratteri\n", MAX_FILENAME_SIZE);
        return false;
    }

    // Non devono essserci '.' nel nome di una cartella
    if(strchr(dir_name, '.') == NULL){
        return true;
    }
    else{
        printf("Non devono esservi '.' nel nome di una cartella\n");
        return false;
    }

    return true;
}

// Restituisce 0 se è un nome valido per un file, -1 altrimenti
// Separa anche nome del file ed estensione nei due buffer specificati
int extract_file_name_and_extension(char* complete_filename_in_buffer, char* filename_out_buffer, char* extension_out_buffer){
    char* str = strtok(complete_filename_in_buffer, ".");
    if(strlen(str) > MAX_FILENAME_SIZE){
        return -1;
    }

    strncpy(filename_out_buffer, str, MAX_FILENAME_SIZE);

    str = strtok(NULL, ".");
    if(strlen(str) > MAX_FILE_EXTENSION_SIZE){
        return -1;
    }

    strncpy(extension_out_buffer, str, MAX_FILE_EXTENSION_SIZE);

    if(strtok(NULL, ".") != NULL){
        return -1;
    }

    return 0;
}

void replace_char(char *str, char to_be_replaced, char replacement_char) {
    char* curr_char = str;
    
    while((curr_char = strchr(curr_char, to_be_replaced)) != NULL) {
        *curr_char = replacement_char;
        ++curr_char;
    }
}

bool fs_mounted(TRASHBASH_PATH* path){
    if(path == NULL || path->m_fs == NULL){
        return false;
    }
    
    return true;
}

int tb_save_file_to_fs(TRASHBASH_PATH* path, char* file_content_buffer, unsigned int file_content_size, char* file_name_buffer, char* file_extension_buffer){
    FILE_HANDLE* file_on_fs;
    
    if(tb_create_file(path, file_name_buffer, file_extension_buffer, &file_on_fs)){
        return -1;
    }

    if(write_file(file_on_fs, file_content_buffer, file_content_size) == GENERIC_FILE_ERROR){
        return -1;
    }

    return 0;
}

void trash_bash(){
    bool quit = false;

    TRASHBASH_PATH path;
    path.depth = -1;
    path.m_fs = NULL;

    char* user_input = (char*) malloc(USER_INPUT_BUF_SIZE);
    if(user_input == NULL){
        printf("\nMemoria insufficiente\n");
        return;
    }

    int cmd;
    char first_param[PARAM_BUF_SIZE];
    char second_param[PARAM_BUF_SIZE];
    int more_params;

    size_t user_input_max_size = USER_INPUT_BUF_SIZE;

    int error;

    char dir_name[MAX_FILENAME_SIZE + 1];
    char file_name[MAX_FILENAME_SIZE + 1];
    char file_extension[MAX_FILE_EXTENSION_SIZE + 1];

    printf("\n\n");
    tb_print_version_info();

    do{
        error = 0;
        tb_print_prompt(&path);

        getline(&user_input, &user_input_max_size, stdin);
        fflush(stdin);
        
        replace_char(user_input, '\n', '\0');

        cmd = tb_parse_input(user_input, first_param, second_param, &more_params);


        switch(cmd){
        case TB_QUIT:
            quit = true;
            break;
        case TB_NO_COMMAND:
            break;
        case TB_HELP:
            print_help();
            break;
        case TB_CLS:
            system("clear");
            break;
        case TB_MOUNT_FS_FROM_FILE:
            // Verifica prerequisiti
            if(fs_mounted(&path)){
                printf("Un fs è già montato, esegui '%s' prima di montarne un altro\n", TB_UNMOUNT_STRING);
                error = -1;
                break;
            }
            if(contains_invalid_chars(first_param)){
                printf("Sono ammessi solo caratteri alfanumerici\n");
                error = -1;
                break;
            }

            // Esecuzione effettiva del comando
            if(tb_mount_fs_from_file(first_param, &path)){
                error = -1;
                break;
            }
            break;
        case TB_UNMOUNT:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }

            // Esecuzione effettiva del comando
            if(tb_unmount_fs(&path)){
                error = -1;
                break;
            }
            break;
        case TB_CREATE_FS_ON_FILE:
            // Verifica prerequisiti
            if(fs_mounted(&path)){
                error = -1;
                break;
            }
            if(contains_invalid_chars(first_param)){
                printf("Sono ammessi solo caratteri alfanumerici\n");
                error = -1;
                break;
            }
            int idx = 0;
            while(second_param[idx] != '\0'){
                if(isdigit(second_param[idx]) == false){
                    printf("Il secondo parametro non è un numero valido\n");
                    error = -1;
                    break;
                }
                ++idx;
            }
            int blocks = atoi(second_param);
            if(blocks < 0){
                printf("Il numero di blocchi non può essere negativo\n");
                error = -1;
                break;
            }

            // Esecuzione effettiva del comando
            if(tb_create_new_fs_on_file(first_param, (unsigned int)blocks)){
                error = -1;
                break;
            }
            break;
        case TB_CHANGE_DIR:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }
            if(strcmp(first_param, "..") != 0){
                if(contains_invalid_chars(first_param)){
                    printf("Sono ammessi solo caratteri alfanumerici\n");
                    error = -1;
                    break;
                }
                if(is_valid_dir_name(first_param) == false){
                    printf("Non è un nome valido per una cartella\n");
                    error = -1;
                    break;
                }
            }
            
            // Esecuzione effettiva del comando
            if(tb_change_dir(&path, first_param)){
                error = -1;
                break;
            }
            break;
        case TB_PWD:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }

            // Esecuzione effettiva del comando
            if(tb_pwd(&path)){
                error = -1;
                break;
            }
            break;
        case TB_LIST:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }

            // Esecuzione effettiva del comando
            if(tb_list(&path)){
                error = -1;
                break;
            }
            break;
        case TB_TREE:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }

            // Esecuzione effettiva del comando
            if(tb_tree(&path)){
                error = -1;
                break;
            }
            break;
        case TB_CREATE_DIR:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }
            if(contains_invalid_chars(first_param)){
                printf("Sono ammessi solo caratteri alfanumerici\n");
                error = -1;
                break;
            }
            if(is_valid_dir_name(first_param) == false){
                printf("'%s' non è un nome valido per una cartella\n", first_param);
                error = -1;
                break;
            }


            // Esecuzione effettiva del comando
            if(tb_create_dir(&path, first_param, NULL)){
                error = -1;
                break;
            }
            break;
        case TB_CREATE_FILE:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }
            if(extract_file_name_and_extension(first_param, file_name, file_extension)){
                printf("Non è un nome valido per un file\n");
                error = -1;
                break;
            }
        
            // Esecuzione effettiva del comando
            FILE_HANDLE* new_file;
            if(tb_create_file(&path, file_name, file_extension, &new_file)){
                error = -1;
                break;
            }
            break;
        case TB_DELETE_DIR:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }
            if(contains_invalid_chars(first_param) || is_valid_dir_name(first_param) == false){
                printf("'%s' non è un nome valido per una cartella\n", first_param);
                error = -1;
                break;
            }

            // Esecuzione effettiva del comando
            if(tb_delete_dir(&path, first_param)){
                error = -1;
                break;
            }
            break;
        case TB_DELETE_FILE:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }
            if(extract_file_name_and_extension(first_param, file_name, file_extension)){
                printf("Non è un nome valido per un file\n");
                error = -1;
                break;
            }

            // Esecuzione effettiva del comando
            if(tb_delete_file(&path, file_name, file_extension)){
                error = -1;
                break;
            }
            break;
        case TB_SAVE_FILE_TO_FS:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }

            if(strnlen(second_param, 40) == 0 || extract_file_name_and_extension(second_param, file_name, file_extension)){
                printf("Non è un nome valido per un file (per il mio fs FS_FAT)\n");
                error = -1;
                break;
            }

            // Apro il file (il cui nome è indicato dal primo parametro) presente sul fs vero del computer
            FILE* real_file;

            if((real_file = fopen(first_param, "rb")) == NULL){
                printf("Impossibile leggere il file locale %s dal computer da salvare sul fs\n", first_param);
                perror("Errore verificato:");
                error = -1;
                break;
            }

            // Ottengo la dimensione del file
            struct stat file_stats;
            if(fstat(fileno(real_file), &file_stats)){
                printf("Impossibile ottenere la dimensione del file da leggere e salvare\n");
                fclose(real_file);
                error = -1;
                break;
            }
            unsigned long file_size = file_stats.st_size;

            // Copio il contenuto del file in un buffer
            char* file_content_buffer = (char*) malloc(file_size+1);
            if(file_content_buffer == NULL){
                printf("Impossibile allocare il buffer su cui copiare il contenuto del file\n");
                fclose(real_file);
                error = -1;
                break;
            }

            fread(file_content_buffer, 1, file_size, real_file);
            fclose(real_file);

            // Copio il contenuto del buffer nel nuovo file nel mio fs
            if(tb_save_file_to_fs(&path, file_content_buffer, file_size, file_name, file_extension)){
                printf("Creazione e salvataggio del contenuto del file locale %s sul file %s.%s del FS_FAT fallita\n", first_param, file_name, file_extension);
                free(file_content_buffer);
                error = -1;
                break;
            }

            free(file_content_buffer);
            break;
        case TB_EXPORT_FILE_FROM_FS:
            // Verifica prerequisiti
            if(fs_mounted(&path) == false){
                printf("Nessun fs è stato montato\n");
                error = -1;
                break;
            }

            if(extract_file_name_and_extension(first_param, file_name, file_extension)){
                printf("Non è un nome valido per un file\n");
                error = -1;
                break;
            }

            // Esecuzione effettiva del comando
            // Ottengo la dimensione del file, creo un file sul fs vero del computer
            DIR_ENTRY* de = get_file_by_name(path.m_fs->fs, path.path[path.depth], file_name, file_extension);
            if(de == NULL){
                printf("File non trovato\n");
                error = -1;
                break;
            }
            
            FILE_HANDLE* my_file = get_or_create_file_handle(path.m_fs, de->file.start_block);
            if(my_file == NULL){
                printf("Impossibile ottenere il file\n");
                error = -1;
                break;
            }

            unsigned int my_file_size = get_file_size(path.m_fs->fs, de->file.start_block);
            
            char complete_name[31];
            memset(complete_name, 0, 31);
            char ext[4];
            strncpy(ext, file_extension, 3);
            ext[3] = '\0';

            snprintf(complete_name, 31, "%s.%s", file_name, ext);
            printf("Il file salvato sul computer si chiamerà %s.%s\n", file_name, ext);

            FILE* new_real_file = fopen(complete_name, "wb");
            if(new_real_file == NULL){
                printf("Impossibile creare un nuovo file %s.%4s sul fs del computer\n", file_name, file_extension);
                error = -1;
                break;
            }

            // Creo un buffer, vi copio il contenuto del file
            char* temp_buffer = (char*) malloc(my_file_size);
            if(temp_buffer == NULL){
                fclose(new_real_file);
                printf("Memoria insufficiente per creare un buffer temporaneo su cui salvare il file\n");
                error = -1;
                break;
            }

            file_seek(my_file, 0, FILE_SEEK_START);

            if(read_file(my_file, temp_buffer, my_file_size) == GENERIC_FILE_ERROR){
                free(temp_buffer);
                fclose(new_real_file);
                error = -1;
                break;
            }

            // Copio il contenuto del buffer sul file nel computer
            fwrite(temp_buffer, 1, my_file_size, new_real_file);
            break;

        case TB_UNKNOWN_COMMAND:
            printf("Comando sconosciuto, usa 'help' per vedere la lista di comandi\n");
            break;
        default:
            printf("Comando non previsto\n");
            break;
        }
        
        if(error == -1){
            printf("Si è verificato un errore, usa 'help' per ricevere informazioni sui comandi\n");
        }

    }while(quit != true);

    // Prima di terminare, esegui unmount del fs se l'utente non lo ha fatto
    if(fs_mounted(&path)){
        tb_unmount_fs(&path);
    }

    free(user_input);
    return;
}