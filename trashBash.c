#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "trashBash.h"
#include "fatFS.h"
#include "fsFunctions.h"
#include "fsUtils.h"
#include "minilogger.h"

int tb_create_new_fs_on_file(const char* filename){
    if(filename == NULL){
        return -1;
    }

    return create_fs_on_file(filename, 100000);
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
        printf("//%s", dir_name_buf);
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
        return -1;
    }
    else{
        tb_pwd_impl(path);

        printf("$ ");

        return 0;
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
        *new_dir = INVALID_BLOCK;
        return -1;
    }
    else{
        *new_dir = dir;
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
            fputc(' ', stdin);
        }

        printf("! Cartella corrotta (numero di elementi contenuti negativo)\n");
        return;
    }
    
    // Almeno un elemento in questa cartella
    DIR_EXPLORER_STACK_ELEM* curr = list_to_explore->elems;

    while(curr != NULL){
        for(int i=0; i < blank_offset; ++i){
            fputc(' ', stdin);
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

    printf("%s\n", start_dir_name);

    tb_tree_helper(path->m_fs, path->path[path->depth], 2);

    return 0;
}

#define DELIMS " \t"

int tb_parse_input(char* user_input_in_buffer, char* first_param_out_buffer, char* second_param_out_buffer, int* more_params_found){
    char* cmd_str;
    first_param_out_buffer[0] = '\0';
    second_param_out_buffer[0] = '\0';

    cmd_str = strtok(user_input_in_buffer, DELIMS);
    if(cmd_str == NULL){
        return TB_NO_COMMAND;
    }

    int cmd;

    if(strcmp(cmd_str, TB_HELP_STRING) == 0){
        cmd = TB_HELP;
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