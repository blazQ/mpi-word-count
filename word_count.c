#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"
#include "futils.h"
#include "workload.h"
#include "chnkcnt.h"
#include "hashdict.h"

#define MASTER 0

int main(int argc, char* argv[]){
	int rank, wsize; 
	double start, end;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &wsize);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Barrier(MPI_COMM_WORLD);
	start = MPI_Wtime();

	// Obtaining all the files in the cwd
	size_t total_size;
	File_vector *file_list = NULL;
	get_file_vec(&file_list, &total_size, ".", argv[0]);

	// Printing the list of files and workloads
	if(MASTER == rank){
		print_file_vec(&file_list);
		fprintf(stderr, "\n\tTotal Size: %8ld bytes\n", total_size);
	}

	// Dividing workloads
	Chunk_vector **chunks_proc = malloc(sizeof(*chunks_proc) * wsize);
	// Computing workload for all wsize processes (this function is defined in workload.h)
	get_workload(chunks_proc, wsize, &file_list, total_size, file_list->size);

	// Freeing heap memory
	free(file_list);

	// Printing the chunk list for each processor
	if(MASTER == rank){
		for(int i = 0; i < wsize; i++)
			print_chunk_vec(&chunks_proc[i]);
	}

	// Counting words
	struct dictionary* dic = dic_new(0);
	for(size_t i = 0; i < chunks_proc[rank]->size; i++){
		File_chunk curr_chunk = chunks_proc[rank]->chunks[i];
		count_words_chunk(curr_chunk.file_name, curr_chunk.start, curr_chunk.end, dic);
	}

	// Sending histograms to master
	// MPI Send bla bla bla

	if(MASTER == rank){
		// Merge histograms in one big histogram
		// Write to csv file
		for (int i = 0; i < dic->length; i++) {
        if (dic->table[i] != 0) {
            struct keynode *k = dic->table[i];
            while (k) {
                if(k->value)
                    fprintf(stdout, "Word: %s Count: %d\n", k->key, k->value);
                k = k->next;
            }
        }
    }
	}

	for(int i = 0; i < wsize; i++)
		free(chunks_proc[i]);
	free(chunks_proc);

	MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();
    if(MASTER == rank)
    	fprintf(stderr, "\n\tTime elapsed: %f\n", end-start);
    MPI_Finalize();

	return 0;
}

