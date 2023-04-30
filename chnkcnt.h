#ifndef CHNKCNT_H
#define CHNKCNT_H

#include "hashdict.h"

#define BLOCKSIZE 2048

char* count_words(char* buffer, struct dictionary* dic, size_t* lwlen);

char* recover_missing_word(char* buffer, char* previous_portion, size_t lwlen, size_t* b4_space);

void count_words_chunk(char* file_name, long start, long end, struct dictionary* dic);

#endif
