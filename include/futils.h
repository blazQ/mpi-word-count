#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#define MAX_FILES 256

/**************************************
 * List implementation
 * ************************************/
typedef struct file_node File_node;

typedef struct file_list File_list;

File_list* get_file_list(long* total_lines, char* dir_path, char* executable_name);

void free_file_list(File_list *list);

void print_file_list(File_list *list);

/***************************************
 * Vector implementation
 * *************************************/

struct file_info{
    char*	file_name; 
    size_t	file_size; 
};

typedef struct file_info File_info;

struct file_vector{
    size_t		size;
    File_info	files[];
};

typedef struct file_vector File_vector;

void get_file_vec(File_vector **vector, size_t* total_lines, char* dir_path, char* executable_name);

File_info* get_file_at(File_vector **vector, size_t position);

size_t get_file_vec_size(File_vector **vector);

void print_file_vec(File_vector **vector);

#endif  
