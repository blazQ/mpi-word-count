#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include "workload.h"
#include "futils.h"

void print_chunk(File_chunk* chunk){
    char* type;
    if(chunk->special_position == FIRST)
        type = "FIRST";
    else if(chunk->special_position == LAST)
        type = "LAST";
    else if(chunk->special_position == UNIQUE)
        type = "UNIQUE";
    else
        type = "REGULAR";

    fprintf(stderr, "\t%-20s\t\tstart: \t%8ld\tend: \t%8ld\ttype: \t%-8s\t\n", chunk->file_name, chunk->start, chunk->end, type);
}

File_chunk* get_chunk_at(Chunk_vector **vector, size_t position){
    return position < vector[0]->size ? &vector[0]->chunks[position]: NULL;
}

size_t get_chunk_vec_size(Chunk_vector **vector){
    return vector[0]->size;
}

void set_owner(Chunk_vector **vector, int owner){
    vector[0]->owner = owner;
}

int chunk_push_back(Chunk_vector **vector, char* file_name, long start, long end, int special_position){
    size_t x = *vector ? vector[0]->size : 0 , y = x + 1;
    if((x & y ) == 0){
        void *temp = realloc(*vector, sizeof **vector + (x + y) * sizeof vector[0]->chunks[0]);

        if (!temp) {return 1; }
        *vector = temp;
    }

    vector[0]->chunks[x].file_name = file_name;
    vector[0]->chunks[x].start = start;
    vector[0]->chunks[x].end = end;
    vector[0]->chunks[x].special_position = special_position;
    vector[0]->size = y;
    return 0;
}

void print_chunk_vec(Chunk_vector **vector){
        fprintf(stderr, "\n\tChunk vector for process %d contains %zu chunk(s):\t\n", vector[0]->owner, vector[0]->size);
        fprintf(stderr, "\t----------------------------------------------------\t\n");
        for(size_t i = 0; i < vector[0]->size; i++){
            print_chunk(&vector[0]->chunks[i]);
        }
}

void get_workload(Chunk_vector** chunks_proc, int wsize, File_vector** file_list, size_t total_size, size_t nofiles){
    // Calculating how many bytes each process needs to analyze
    int remainder = total_size % wsize;
    size_t size_process = total_size / wsize;

    // Array of remaining capacities for each process 
    size_t remaining_capacities[wsize];
    // Initially all processes have the same remaining capacity, except for those who handle the remainder
    for(int i = 0; i < wsize; i++){
        remaining_capacities[i] = size_process;
        if(i < remainder) remaining_capacities[i]++;
    }

    /*for(int i = 0; i < wsize; i++){
        fprintf(stderr, "\tProcess %d: %8ld bytes\n", i, remaining_capacities[i]);
    }*/

    size_t cur_proc = 0;
    for(size_t i = 0; i < nofiles; i++){
        File_info cur_file = file_list[0]->files[i];
        size_t start = 0;
        size_t file_remaining = cur_file.file_size;
        while(file_remaining){
            if(remaining_capacities[cur_proc] >= file_remaining){
                int type;
                if (start == 0)
                    type = UNIQUE;
                else if((start+file_remaining) == cur_file.file_size)
                    type = LAST;
                else
                    type = REGULAR;

                chunk_push_back(&chunks_proc[cur_proc], strdup(cur_file.file_name), start, start+file_remaining, type);
                // Crea chunk completo e aggiungilo alla lista di cur_proc da start a file remaining
                remaining_capacities[cur_proc] -= file_remaining;
                file_remaining = 0;
            }
            else if(remaining_capacities[cur_proc] > 0){
                int type;
                if(start == 0)
                    type = FIRST;
                else if((start+file_remaining) == cur_file.file_size)
                    type = LAST;
                else
                    type = REGULAR;
                chunk_push_back(&chunks_proc[cur_proc], strdup(cur_file.file_name), start, start+remaining_capacities[cur_proc], type);
                file_remaining -= remaining_capacities[cur_proc];
                start = start + remaining_capacities[cur_proc];
                remaining_capacities[cur_proc] = 0;
                cur_proc++;
            }
        }
    }
    for(int i = 0; i < wsize; i++){
        set_owner(&chunks_proc[i], i);
    }
}

