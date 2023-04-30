#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hashdict.h"

#define BLOCKSIZE 2048

char* count_words(char* buffer, struct dictionary* dic, size_t* lwlen){
    char current_word[256];
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
            dic_add(dic, strdup(current_word), i);
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



int main()
{
    FILE * file;
    struct dictionary* dic = dic_new(0);
    char path[100];

    char ch;
    int characters, words, lines;


    /* Input path of files to merge to third file */
    printf("Enter source file path: ");
    scanf("%s", path);

    /* Open source files in 'r' mode */
    file = fopen(path, "r");


    /* Check if file opened successfully */
    if (file == NULL)
    {
        printf("\nUnable to open file.\n");
        printf("Please check if file exists and you have read privilege.\n");

        exit(EXIT_FAILURE);
    }

    char buffer[BLOCKSIZE];
    long bytesread;

    characters = words = lines = 0;

    /*while(fgets(buffer, BLOCKSIZE-1, file)!=NULL){
        count_words(buffer, dic);
    }*/

    char* last_word = NULL;
    size_t lwlen = 0;
    size_t b4_space;
    int i = 0;
    while((bytesread = fread (buffer, sizeof(char), BLOCKSIZE-1, file))){
        b4_space = 0;
        buffer[bytesread] = '\0';
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
    
    // Print hashtable
    for (int i = 0; i < dic->length; i++) {
        if (dic->table[i] != 0) {
            struct keynode *k = dic->table[i];
            while (k) {
                if(k->value)
                    printf("Word: %s Count: %d\n", k->key, k->value);
                k = k->next;
            }
        }
    }

    dic_delete(dic);

    /* Close files to release resources */
    fclose(file);

    return 0;
}