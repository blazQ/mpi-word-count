#ifndef CHNKCNT_H
#define CHNKCNT_H

#include "hashdict.h"
#include "mpi.h"

#define BLOCKSIZE 2048
#define WORD_MAX 256

typedef struct{
	char* first_word;
	char* last_word;
	int chunk_type;
} sync_info;

char* count_words(char* buffer, struct dictionary* dic, size_t* lwlen);

char* recover_missing_word(char* buffer, char* previous_portion, size_t lwlen, size_t* b4_space);

char* count_words_chunk(char* file_name, long start, long end, struct dictionary* dic, char** first_word);

void sync_with_next(char* last_word, int rank, struct dictionary* dic, MPI_Comm comm);

void sync_with_prev(char* fw_word, int rank, struct dictionary* dic, MPI_Comm comm);

#endif
