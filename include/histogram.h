#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include "mpi.h"
#include "hashdict.h"
#include "chnkcnt.h"

typedef struct{
    int count;
    char word[WORD_MAX];
    // Possible optimization: add an "actual_word_length" field, to avoid calling strlen later.
    // It would be easily taken from the hashdict "keylen" attribute @todo
} histogram_element;

long get_local_histogram(histogram_element* local_elements, struct dictionary* dic);

void merge_dict(struct dictionary* dic, histogram_element** process_histograms, long* localszs, int wsize);

int MPI_Type_create_histogram(MPI_Datatype* histogram_element_dt);

#endif
