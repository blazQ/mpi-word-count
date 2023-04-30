#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include "futils.h"

/*********************************************************************************
 * The following library encompassess all functions related to discovering the
 * files present inside a single directory.
 * Two main approaches are developed: Using a linked list and a variable length
 * array.
 * A linked list has no memory limit, since there can be an unlimited number
 * of files in a directory. It represent the most "clean" "in theory" approach.
 * An array has immensely faster indexing, but
 * needs reallocation when the number of files is above the current size.
 * Scroll down to file_push_back to see details about the array implementation.
 * *******************************************************************************/

struct file_node {
    char                *file_name;
    long                file_size;
    struct file_node    *next;
};

struct file_list {
    File_node           *head;
};

File_node* create_node(char* file_name, long file_size){
    File_node *new_node = malloc(sizeof(File_node));
    if(!new_node)
        return NULL;
    new_node->file_name = strdup(file_name);
    new_node->file_size = file_size;
    new_node->next = NULL;
    return new_node;
}

File_list* make_file_list(){
    File_list *list = malloc(sizeof(File_list));
    if(!list){
        return NULL;
    }
    list->head = NULL;
    return list;
}

void add_file_node(File_list* list, char* file_name, long file_size){
    if(!list->head)
        list->head = create_node(file_name, file_size);
    else{
        File_node *new_node = create_node(file_name, file_size);
        new_node->next = list->head;
        list->head = new_node;
    }
}

void print_file_list(File_list *list){
    File_node *current = list->head;
    if(!list->head)
        return;

    while(current){
        fprintf(stderr, "File_name: %s, Size: %ld bytes\n", current->file_name, current->file_size); 
        current = current->next;
    };
}

void free_file_list(File_list *list){
    File_node *current = list->head;
    File_node *next    = current;
    while(current){
        next = current->next;
        free(current->file_name);
        free(current);
        current = next;
    }
    free(list);
}

long get_file_size(char *filename) {
    struct stat file_status;
    if (stat(filename, &file_status) < 0) {
        return -1;
    }

    return file_status.st_size;
}

File_list* get_file_list(long* total_size, char* dir_path, char* executable_name){
    struct dirent *de;
    *total_size = 0; /* In bytes */
    DIR *dr = opendir(dir_path); /* Current directory */

    if (dr == NULL)
    {
        printf("Could not open current directory" );
        return NULL;
    }

    File_list* list = make_file_list();

    while ((de = readdir(dr)) != NULL)
        /* Only regular files and if it's not the executable */
        if(de->d_type == DT_REG && strcmp(de->d_name, executable_name)){ 
            long current_size = get_file_size(de->d_name);
            add_file_node(list, de->d_name, current_size);
            *total_size+=current_size;
        }

    closedir(dr);
    return list;
}

/**********************************************************************************
 * This implementation uses a vector to preserve cache locality. This vector is 
 * basically a Variable Length Array that starts with size o.
 * After the first push_back the size becomes 1, then it becomes 3.
 * Then it becomes 7, then 15 and so on. (2^n)-1. It provides a solution that scales
 * efficiently with the number of files and preserves cache locality more than
 * the linked list implementation.
 * It is a bit less efficient when the files are few, but in that case the
 * difference is not life-changing compared to what we are saving when the number
 * of files grows.
 * Also, I can simply free it with 1 call of free(vector) and it's easier to pass 
 * with custom MPI Datatype, since it's contiguous.
 * *******************************************************************************/

File_info* get_file_at(File_vector **vector, size_t position){
    return position < vector[0]->size ? &vector[0]->files[position]: NULL;
}

size_t get_file_vec_size(File_vector **vector){
    return vector[0]->size;
}

int file_push_back(File_vector **vector, char* file_name, long file_size){
    size_t x = *vector ? vector[0]->size : 0 , y = x + 1;

    if((x & y ) == 0){
        void *temp = realloc(*vector, sizeof **vector + (x + y) * sizeof vector[0]->files[0]);

        if (!temp) {return 1; }
        *vector = temp;
    }

    vector[0]->files[x].file_name = file_name;
    vector[0]->files[x].file_size = file_size;
    vector[0]->size = y;
    return 0;
}

void get_file_vec(File_vector **vector, size_t* total_size, char* dir_path, char* executable_name){
    struct dirent *de;
    *total_size = 0; /* In bytes */
    DIR *dr = opendir(dir_path); /* Current directory */

    if (dr == NULL)
    {
        printf("Could not open current directory" );
        exit(1);
    }

    int i = 0;
    while ((de = readdir(dr)) != NULL && i < MAX_FILES)
        /* Only regular files and if it's not the executable */
        if(de->d_type == DT_REG && strcmp(de->d_name, executable_name)){ 
            long current_size = get_file_size(de->d_name);
            file_push_back(vector, strdup(de->d_name), current_size);
            *total_size+=current_size;
            i++;
        }

    closedir(dr);
}

void print_file_vec(File_vector **vector){
        fprintf(stderr, "\n\tFile vector contains %zu files:\t\n", vector[0]->size);
        fprintf(stderr, "\t-------------------------------\t\n");
        for(size_t i = 0; i < vector[0]->size; i++){
            File_info info = vector[0]->files[i];
            fprintf(stderr, "\t%-20s\t\tsize: \t%8ld\n", info.file_name, info.file_size);
        }
}

