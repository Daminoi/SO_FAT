#ifndef TRASHBASH_H
#define TRASHBASH_H

#include "fatFS.h"

#define USER_INPUT_BUF_SIZE 128
#define PARAM_BUF_SIZE 128

#define TB_NO_COMMAND                   -2
#define TB_UNKNOWN_COMMAND              -1

#define TB_QUIT                         0
#define TB_QUIT_STRING                  "quit"
#define TB_HELP                         1
#define TB_HELP_STRING                  "help"

#define TB_MOUNT_FS_FROM_FILE           2
#define TB_MOUNT_FS_FROM_FILE_STRING    "mfff" 
#define TB_UNMOUNT                      3
#define TB_UNMOUNT_STRING               "umount"
#define TB_CREATE_FS_ON_FILE            4
#define TB_CREATE_FS_ON_FILE_STRING     "cfof"

#define TB_PWD                          5
#define TB_PWD_STRING                   "pwd"
#define TB_LIST                         6
#define TB_LIST_STRING                  "ls"
#define TB_TREE                         7
#define TB_TREE_STRING                  "tree"
#define TB_CHANGE_DIR                   8
#define TB_CHANGE_DIR_STRING            "cd"

#define TB_CREATE_FILE                  9
#define TB_CREATE_FILE_STRING           "mkfile"
#define TB_CREATE_DIR                   10
#define TB_CREATE_DIR_STRING            "mkdir"
#define TB_DELETE_FILE                  11
#define TB_DELETE_FILE_STRING           "rmf"
#define TB_DELETE_DIR                   12
#define TB_DELETE_DIR_STRING            "rmd"

#define TB_CLS                          13
#define TB_CLS_STRING                   "clear"

#define TB_SAVE_FILE_TO_FS              14
#define TB_SAVE_FILE_TO_FS_STRING       "sftf"
#define TB_EXPORT_FILE_FROM_FS          15
#define TB_EXPORT_FILE_FROM_FS_STRING   "efff"

#define DELIMS " \t"

#define MAX_PATH_LENGTH 64

typedef struct TRASHBASH_PATH{
    block_num_t path[MAX_PATH_LENGTH];
    int depth;
    MOUNTED_FS* m_fs;
}TRASHBASH_PATH;

void trash_bash();

// Restituisce il numero (come definito sopra) del comando scritto dall'utente
int tb_parse_input(char* user_input_in_buffer, char* first_param_out_buffer, char* second_param_out_buffer, int* more_params_found);

int tb_create_new_fs_on_file(const char* filename);

int tb_mount_fs_from_file(const char* filename, TRASHBASH_PATH* path);
int tb_unmount_fs(TRASHBASH_PATH* path);

int tb_pwd(TRASHBASH_PATH* path);
int tb_print_prompt(TRASHBASH_PATH* path);

int tb_list(TRASHBASH_PATH* path);
int tb_tree(TRASHBASH_PATH* path);

int tb_change_dir(TRASHBASH_PATH* path, char* goto_dir_name);

int tb_create_file(TRASHBASH_PATH* path, char* file_name_buffer, char* file_extension_buffer, FILE_HANDLE** new_file);
int tb_create_dir(TRASHBASH_PATH* path, char* dir_name_buffer, block_num_t* new_dir);

int tb_delete_file(TRASHBASH_PATH* path, char* file_name_buffer, char* file_extension_buffer);
int tb_delete_dir(TRASHBASH_PATH* path, char* dir_name_buffer);

int tb_save_file_to_fs(TRASHBASH_PATH* path, char* file_content_buffer, unsigned int file_content_size, char* file_name_buffer, char* file_extension_buffer);

#endif /* TRASHBASH_H */