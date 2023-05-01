#include <ctype.h>
#include <stdio.h>
#include "hashdict.h"
#include "chnkcnt.h"

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
    // So we need to return it. In this way the caller can use his knowledge of the next buffer and of
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

void count_words_chunk(char* file_name, long start, long end, struct dictionary* dic){
    FILE * file;
    file = fopen(file_name, "r");

    /* Check if file opened successfully */
    if (file == NULL)
    {
        fprintf(stderr, "\nUnable to open file.\n");
        fprintf(stderr, "Please check if file exists and you have read privilege.\n");

        exit(EXIT_FAILURE);
    }

    size_t bytes2read = BLOCKSIZE > (end-start) ? (end-start): BLOCKSIZE; /* How many bytes we have to read for each iteration */
    size_t bytesread;               /* How many characters we actually read for each iteration */
    long total_bytes_read = 0;    /* How many characters we've read so far */
    char buffer[bytes2read+1];      /* Where we store what we read in each iteration */

    fseek(file, start, SEEK_SET);   /* Positioning the cursor at the start of the interval */

    /* What we want to do is this:
        Read ONLY the portion of the file defined by the chunk.
        DO NOT read more bytes than what you have to.
        We could easily achieve this with a simple getc, reading character by character until we've read end-start characters.
        But it's slower than reading the chunk in fixed blocks with fread. So we write
        some code to accomodate eventual problems (like, for example, words torn between blocks)
    */

    char* last_word = NULL;         /* What was the last word in the previous block */
    size_t lwlen = 0;               /* The length of the previous word, in order to avoid calling strlen needlessly */
    size_t b4_space;                /* How many characters before the first space in the current block */

    /* Guard condition can be read as follows:
        While you can still read something and the total bytes read are not above the chunk size... */
    while((bytesread = fread (buffer, sizeof(char), bytes2read, file)) && total_bytes_read < (end - start)){
        b4_space = 0;
        /* We are pdating total_bytes inside the iteration because it could happen that, for example, we have read
            n-x bytes, where n is basically the chunk size, up to this point. We then read y bytes, with y > x.
            We have surpassed the limit of the chunk, but interrupting the cycle would make us lose x bytes. */
        total_bytes_read += bytesread;
        if(total_bytes_read > (end - start)){
            bytesread -= (total_bytes_read - (end - start));
            total_bytes_read = (end - start);
        }
        buffer[bytesread] = '\0';

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
}
