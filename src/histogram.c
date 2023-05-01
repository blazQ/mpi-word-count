#include <string.h>
#include "mpi.h"
#include "hashdict.h"
#include "histogram.h"

void merge_dict(struct dictionary* dic, histogram_element** process_histograms, long* localszs, int wsize){
	for(int i = 1; i < wsize; i++){
		for(int j = 0; j < localszs[i]; j++){
			char* cur_word = process_histograms[i][j].word;
			size_t cur_len = strlen(process_histograms[i][j].word);
			int cur_value = process_histograms[i][j].count;

			if(dic_find(dic, cur_word, cur_len))
        		*dic->value = *dic->value + cur_value;
    		else{
        		dic_add(dic, process_histograms[i][j].word, i);
        		*dic->value = cur_value;
    		}
		}
	}
}

long get_local_histogram(histogram_element* local_elements, struct dictionary* dic){
	long j = 0;
	for (int i = 0; i < dic->length; i++) {
	    if (dic->table[i] != 0) {
	        struct keynode *k = dic->table[i];
	        while (k) {
	            if(k->value){
	            	// Making sure they are contiguous
	            	memcpy(local_elements[j].word, k->key, k->len);
	            	local_elements[j].count = k->value;
	            	j++;
	            }
	            k = k->next;
	        }
	    }
	}
	return j;
}

int MPI_Type_create_histogram(MPI_Datatype* histogram_element_dt){
	int count = 2;

	int block_length[2] = {
		1,
		256
	};

	MPI_Aint displacements[2] = {
		offsetof(histogram_element, count),
		offsetof(histogram_element, word)
	};

	MPI_Datatype types[2] = {
		MPI_INT,
		MPI_CHAR
	};

	MPI_Type_create_struct(count, block_length, displacements, types, histogram_element_dt);

	return MPI_Type_commit(histogram_element_dt);
}