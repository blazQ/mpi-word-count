#include <ctype.h>
#include <stdio.h>
#include "hashdict.h"
#include "chnkcnt.h"
#include "mpi.h"

char* count_words(char* buffer, struct dictionary* dic, size_t* lwlen){
    char current_word[WORD_MAX];
    size_t i = 0;
    while(*buffer){
        i = 0;
        while(isalnum(*buffer)){
            current_word[i] = tolower(*buffer);
            i++;
            buffer++;
        }

        current_word[i] = '\0';

        if(i != 0 && dic_find(dic, current_word, i))
            *dic->value = *dic->value + 1;
        
        else if(i != 0){
            dic_add(dic, current_word, i);
            *dic->value = 1;
        }
        if(!*buffer)
            break;

        buffer++;
    }
    // Go back! If the buffer ended with a character there's a high chance the last word is truncated
    // So we need to return it. In this way the caller can use his knowledge of the next buffer
    // to understand if the last word is truncated (it will be then merged with the first character of the next buffer)
    buffer--;
    char* return_word = NULL;

    if(isalnum(*buffer)){
        return_word = strdup(current_word);
        *lwlen = i;
    }

    return return_word;
}

char* recover_missing_word(char* buffer, char* previous_portion, size_t lwlen, size_t* b4_space){
        char* spacer = buffer;

        while(isalnum(*spacer)){
            *spacer = tolower(*spacer);
            *b4_space = *b4_space + 1;
            spacer++;
        }

        char* missing_word = malloc(sizeof(*missing_word) * (lwlen + *b4_space + 1));
        memcpy(missing_word, previous_portion, lwlen);
        memcpy(missing_word+lwlen, buffer, *b4_space);
        missing_word[lwlen + *b4_space] = '\0';

        return missing_word;
}

char* get_if_first_word(char* buffer){
    char first_word_buf[256];
    size_t len = 0;
    while(*buffer && isalnum(*buffer)){
        first_word_buf[len] = tolower(*buffer);
        len++;
        buffer++;
    }
    first_word_buf[len] = '\0';
    return len ? strdup(first_word_buf) : NULL;
}

char* count_words_chunk(char* file_name, long start, long end, struct dictionary* dic, char** first_word){
    FILE * file;
    file = fopen(file_name, "r");

    /* Check if file opened successfully */
    if (file == NULL)
    {
        fprintf(stderr, "\nUnable to open file.\n");
        fprintf(stderr, "Please check if file exists and you have read privilege.\n");

        exit(EXIT_FAILURE);
    }
    long chnk_sz = end - start;     /* Size of the current chunk */
    size_t bytes2read = BLOCKSIZE > chnk_sz ? chnk_sz: BLOCKSIZE; /* How many bytes we have to read for each iteration */
    size_t bytesread;               /* How many characters we actually read for each iteration */
    long total_bytes_read = 0;      /* How many characters we've read so far */
    char buffer[bytes2read+1];      /* Where we store what we read in each iteration */

    fseek(file, start, SEEK_SET);   /* Positioning the cursor at the start of the interval */

    char* last_word = NULL;         /* The last word in the previous block */
    size_t lwlen = 0;               /* The length of the previous word, in order to avoid calling strlen needlessly */
    size_t b4_space;                /* How many characters before the first space in the current block */
    int i = 0;

    while(total_bytes_read < chnk_sz && (bytesread = fread (buffer, sizeof(char), bytes2read, file))){
        b4_space = 0;
        /* We are updating total_bytes inside the iteration because it could happen that, for example, we have read
            n-x bytes, where n is basically the chunk size, up to this point. We then read y bytes, with y > x.
            We have surpassed the limit of the chunk, but interrupting the cycle would make us lose x bytes. */
        total_bytes_read += bytesread;
        if(total_bytes_read > chnk_sz){
            bytesread -= (total_bytes_read - chnk_sz);
            total_bytes_read = chnk_sz;
        }
        buffer[bytesread] = '\0';

        if(i==0){
            *first_word = get_if_first_word(buffer);
        }
        i++;

        /* Basically if the previous block ended with a word, there's a chance that word could continue at the beginning
            of this block. In that case we merge the two words and remove the occurence of the partial word discoverd earlier. */
        if(last_word && isalnum(buffer[0])){
            char* missing_word = recover_missing_word(buffer, last_word, lwlen, &b4_space);

            if(dic_find(dic, missing_word, lwlen+b4_space))
                *dic->value = *dic->value + 1;
        
            else {
                dic_add(dic, missing_word, lwlen+b4_space);
                *dic->value = 1;
            }

            dic_find(dic, last_word, lwlen);
            if(*dic->value > 0)
                *dic->value = *dic->value - 1;
            else
                *dic->value = 0;

            free(last_word);
        }

        last_word = count_words(buffer+b4_space, dic, &lwlen);
    }
    
    fclose(file);
    return isalnum(buffer[bytesread-1]) ? last_word : NULL;
}

void sync_with_prev(char* last_word, int rank, struct dictionary* dic){
    MPI_Status status;
    char fw_recv[256];
    long lw_len = last_word ? strlen(last_word): -1;
    long fw_len;
    int response = 1;

    MPI_Recv(&fw_len, 1, MPI_LONG, rank+1, 0, MPI_COMM_WORLD, &status);
    if(fw_len){
        MPI_Recv(fw_recv, fw_len+1, MPI_CHAR, rank+1, 0, MPI_COMM_WORLD, &status);
        if(last_word){
            char* missing_word = malloc(sizeof(*missing_word)*(lw_len+fw_len+1));
            memcpy(missing_word, last_word, lw_len);
            memcpy(missing_word+lw_len, fw_recv, fw_len);
            missing_word[fw_len+lw_len] = '\0';

            // Make it a function so it's less verbose. It's practically everywhere @todo
            if(dic_find(dic, missing_word, fw_len+lw_len))
                *dic->value = *dic->value + 1;
            else {
                dic_add(dic, missing_word, fw_len+lw_len);
                *dic->value = 1;
            }

            // Eliminating last word
            dic_find(dic, last_word, lw_len);
            *dic->value = *dic->value -1;

            response = 0;
            // Signaling next one
            MPI_Send(&response, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);

            free(missing_word);
        }
        else {
            MPI_Send(&response, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
        }
    }
    else {
        MPI_Send(&response, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
    }
}

void sync_with_next(char* fw_word, int rank, struct dictionary* dic){
    MPI_Status status;
    long fw_len = fw_word ? strlen(fw_word): 0;
    int response = 1;

    MPI_Send(&fw_len, 1, MPI_LONG, rank-1, 0, MPI_COMM_WORLD); // Send to the previous in line your first word.
    if(fw_len)
        MPI_Send(fw_word, fw_len+1, MPI_CHAR, rank-1, 0, MPI_COMM_WORLD);

    MPI_Recv(&response, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, &status);

    if(!response){
        dic_find(dic, fw_word, fw_len);
        *dic->value = *dic->value -1;
    }
}

