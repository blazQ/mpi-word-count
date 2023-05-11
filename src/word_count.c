#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include "mpi.h"
#include "futils.h"
#include "workload.h"
#include "chnkcnt.h"
#include "hashdict.h"
#include "histogram.h"

#define MASTER 0

typedef enum {
	DEFAULT_MODE,
	DIRECTORY_MODE,
	FILE_FLAG,
	FAILURE = -1
} Mode;

void usage_print(char* program_name);

Mode mode_init(int argc, char* argv[]);

int main(int argc, char* argv[]){
	int rank, wsize;
	long *localszs = NULL; 
	double start, end;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &wsize);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	Mode mode = mode_init(argc, argv);

	if(mode == FAILURE){
		if(MASTER == rank)
			usage_print(argv[0]);
		exit(EXIT_FAILURE);
	}

	// Creating histogram datatype for partial result transfer
	MPI_Datatype histogram_element_dt;
	MPI_Type_create_histogram(&histogram_element_dt);

	// Starting line for benchmarking
	MPI_Barrier(MPI_COMM_WORLD);
	start = MPI_Wtime();

	// Obtaining all the files in the selected directory
	size_t total_size;
	File_vector *file_list = NULL;
	char* output_file = NULL;

	// Depending on the mode
	if(mode == DEFAULT_MODE)	// Reading from cwd
		get_file_vec(&file_list, &total_size, ".", argv[0]);

	else if(mode == (DEFAULT_MODE + FILE_FLAG)){	// Reading from cwd with file output
		get_file_vec(&file_list, &total_size, ".", argv[0]);
		output_file = argv[2];
	}
	
	else if(mode == DIRECTORY_MODE) // Reading from selected directory
		get_file_vec(&file_list, &total_size, argv[2], argv[0]);
	
	else {
		get_file_vec(&file_list, &total_size, argv[3], argv[0]);
		output_file = argv[4];
	}


	// Printing the list of files 
	if(MASTER == rank){
		print_file_vec(&file_list);
		fprintf(stderr, "\n\tTotal Size: %8ld bytes\n", total_size);
	}

	// Dividing workloads
	Chunk_vector **chunks_proc = malloc(sizeof(*chunks_proc) * wsize);
	for(int i = 0; i < wsize; i++)
		chunks_proc[i] = NULL;

	// Computing workload for all wsize processes
	get_workload(chunks_proc, wsize, &file_list, total_size, file_list->size);

	// Freeing heap memory
	free(file_list);

	// Printing the workload for each processor
	if(MASTER == rank){
		for(int i = 0; i < wsize; i++)
			print_chunk_vec(&chunks_proc[i]);
	}

	// Counting words
	sync_info* special_chunks = malloc(sizeof(*special_chunks) * 2);
	special_chunks[0].chunk_type = -1;
	special_chunks[1].chunk_type = -1;

	struct dictionary* dic = dic_new(0);
	for(size_t i = 0, j = 0; i < chunks_proc[rank]->size; i++){
		File_chunk curr_chunk = chunks_proc[rank]->chunks[i];
		char* first_word = NULL;
		char* last_word = count_words_chunk(curr_chunk.file_name, curr_chunk.start, curr_chunk.end, dic, &first_word);
		if(curr_chunk.special_position != UNIQUE){
			special_chunks[j].last_word = last_word ? strdup(last_word): NULL;
			special_chunks[j].first_word = first_word ? strdup(first_word): NULL;
			special_chunks[j].chunk_type = curr_chunk.special_position;
			j++;
		}

		// Freeing heap memory
		if(first_word)
			free(first_word);
		if(last_word)
			free(last_word);
	}

	for(int i = 0; i < 2; i++){
		if(special_chunks[i].chunk_type==FIRST){
			sync_with_next(special_chunks[i].last_word, rank, dic, MPI_COMM_WORLD);
		}
		else if(special_chunks[i].chunk_type == LAST){
			sync_with_prev(special_chunks[i].first_word, rank, dic, MPI_COMM_WORLD);
		}
		else if(special_chunks[i].chunk_type == REGULAR){
			sync_with_prev(special_chunks[i].first_word, rank, dic, MPI_COMM_WORLD);
			sync_with_next(special_chunks[i].last_word, rank, dic, MPI_COMM_WORLD);
		}
	}


	// Freeing heap memory
	for(int i = 0; i < wsize; i++)
		free(chunks_proc[i]);
	free(chunks_proc);

	free(special_chunks);

	// Creating local histograms
	histogram_element *local_elements = malloc(sizeof(*local_elements) * dic->count);
	long snd_sz = get_local_histogram(local_elements, dic);

	if(MASTER == rank)
		localszs = malloc(sizeof(*localszs)*wsize);
	
	// Gathering the number of words to receive from each process
	MPI_Gather(&snd_sz, 1, MPI_LONG, localszs, 1, MPI_LONG, MASTER, MPI_COMM_WORLD);

	if(MASTER == rank){
		// Allocating space for wsize histogram_element[]
		histogram_element **process_histograms = malloc(sizeof(*process_histograms)*wsize);
		for(int i = 1; i < wsize; i++)
			process_histograms[i] = malloc(sizeof(histogram_element) * localszs[i]);

		// Receiving local histograms
		MPI_Status status;
		for(int i = 1; i < wsize; i++){
			MPI_Recv(process_histograms[i], localszs[i], histogram_element_dt, i, 0, MPI_COMM_WORLD, &status);
		}

		// Merge histograms in one big histogram 
		merge_dict(dic, process_histograms, localszs, wsize);

		FILE *output_file_pointer;
		if(mode == DEFAULT_MODE || mode == DIRECTORY_MODE){
			output_file_pointer = stdout;
		}
		else {
			output_file_pointer = fopen(output_file, "w+");
		}

		// Make it a function so it's less verbose? @todo
		// Printing to output_file
		fprintf(output_file_pointer, "Word, Count\n");
		for (int i = 0; i < dic->length; i++) {
	        if (dic->table[i] != 0) {
	            struct keynode *k = dic->table[i];
	            while (k) {
	                if(k->value){
	                    fprintf(output_file_pointer, "%.*s, %d\n", k->len, k->key, k->value);
	                }
	                k = k->next;
	            }
	        }
    	}

		// Freeing heap memory
		free(process_histograms);
		free(localszs);
	}
	else {
		// Sending histograms to master 
		MPI_Send(local_elements, snd_sz, histogram_element_dt, MASTER, 0, MPI_COMM_WORLD);
	}

	MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();

    // Freeing heap memory
	dic_delete(dic);
	free(local_elements);

    if(MASTER == rank)
    	fprintf(stderr, "\n\tTime elapsed: %f\n", end-start);

    MPI_Type_free(&histogram_element_dt);
    MPI_Finalize();

	return 0;
}

void usage_print(char* exec_name){
	fprintf(stderr, "Usage: %s [-d] [-f]  <directory> <output_file>\n", exec_name);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -d : Specify directory\n");
	fprintf(stderr, "  -f : Specify file\n");
	fprintf(stderr, "  -d -f : Specify directory and file\n");
	fprintf(stderr, "If you launch the executable without arguments it will scan the cwd.\n");
}

Mode mode_init(int argc, char* argv[]){
	int opt;
	Mode exec_mode = DEFAULT_MODE;

	while((opt = getopt(argc, argv, "df")) != -1) {
		switch(opt){
			case 'd': exec_mode += DIRECTORY_MODE; break;
			case 'f': exec_mode += FILE_FLAG; break;
		}
	}

	if((argc > 5) || (exec_mode == DIRECTORY_MODE && argc != 3) || (exec_mode == FILE_FLAG && argc != 3) || (exec_mode == (DIRECTORY_MODE+FILE_FLAG) && argc != 5)) {
		exec_mode = FAILURE;
	}

	return exec_mode;
}

