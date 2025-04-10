#ifndef TRASHBASH_H
#define TRASHBASH_H

#include "fatFS.h"

#define MAX_PATH_LENGTH 64

typedef struct TRASHBASH_PATH{
    block_num_t path[MAX_PATH_LENGTH];
    int depth;
    MOUNTED_FS* m_fs;
}TRASHBASH_PATH;

int tb_start();

int tb_create_new_fs_on_file(const char* filename);

int tb_mount_fs_from_file(const char* filename, TRASHBASH_PATH* path);
int tb_unmount_fs(TRASHBASH_PATH* path);

int tb_pwd(TRASHBASH_PATH* path);
int tb_print_prompt(TRASHBASH_PATH* path);

int tb_list(TRASHBASH_PATH* path);
int tb_tree(TRASHBASH_PATH* path);

int tb_parse_input(char* user_input_in_buffer, char* command_out_buffer, char* first_param_out_buffer, char* second_param_out_buffer);

int tb_change_dir(TRASHBASH_PATH* path, char* goto_dir_name);

int tb_create_file(TRASHBASH_PATH* path, char* file_name_buffer, char* file_extension_buffer, FILE_HANDLE** new_file);
int tb_create_dir(TRASHBASH_PATH* path, char* dir_name_buffer, block_num_t* new_dir);

int tb_delete_file(TRASHBASH_PATH* path, char* file_name_buffer, char* file_extension_buffer);
int tb_delete_dir(TRASHBASH_PATH* path, char* dir_name_buffer);
#endif /* TRASHBASH_H */