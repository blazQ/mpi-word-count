#ifndef WORKLOAD_H
#define WORKLOAD_H

#include "futils.h"

#define REGULAR 0
#define FIRST 1
#define LAST 2
#define UNIQUE 3

/********************************************
 * File Chunks (for workload initialization)
 * Data structure used for multiple chunk follows
 * the same approach as what was used with files.
 * In short, it's a variable length array to 
 * preserve cache locality.
 * ******************************************/

typedef struct chunk{
    char*           file_name;
    long            start;
    long            end;
    int             special_position;
} File_chunk;

void print_chunk(File_chunk* chunk);

typedef struct chunk_vector{
    int             owner;
    size_t          size;
    File_chunk      chunks[];
} Chunk_vector;

int chunk_push_back(Chunk_vector **vector, char* file_name, long start, long end, int special_position);

File_chunk* get_chunk_at(Chunk_vector **vector, size_t position);

size_t get_chunk_vec_size(Chunk_vector **vector);

void set_owner(Chunk_vector **vector, int owner);

void print_chunk_vec(Chunk_vector **vector);

void get_workload(Chunk_vector** chunks_proc, int wsize, File_vector** files, size_t total_size, size_t nofiles);

#endif
