/*
 * Word count application with one thread per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
 */

/*
 * Copyright (C) 2019 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "word_count.h"
#include "word_helpers.h"

typedef struct {
    char *filename; //  each input FILENAME 
    word_count_list_t *word_counts; // POINTER to the shared word count list
}thread_args_t;

/* thread routine */
void * thread_rountine(void * arg){
    
    // cast arg to thread_args_t
    thread_args_t * args = (thread_args_t *) arg;

    //get file object 
    FILE *file = fopen(args->filename, "r");

    // check if file opened successfully
    if (file == NULL) {
        perror("Error opening file");
        pthread_exit(NULL);
    }

    // avoid race condition because adding words to shared word_counts list
    count_words(args->word_counts, file); 
    fclose(file);
    free(args);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    /* Create the empty data structure. */
    word_count_list_t word_counts;
    init_words(&word_counts);

    if (argc <= 1) {
        /* Process stdin in a single thread. */
        count_words(&word_counts, stdin);
    } else {
        /* 
            Spawn a new thread per input file.
            Pass each thread the filename and the shared word count list.   
            Join all threads after creation.
        */
        // create an array of pthread_t called threads
        pthread_t threads[argc - 1];

        for (int i = 1; i < argc; i++) {
            // allocate memory for thread_args_t
            thread_args_t *args = malloc(sizeof(thread_args_t));
            if (args == NULL) {
                perror("Malloc Error for thread_args_t");
                exit(1);
            }
            args->filename = argv[i]; // assign argv to filename
            args->word_counts = &word_counts; // assign word_counts to word_counts

            // create a thread to create a word_count list for each file
            if (pthread_create(&threads[i-1], NULL, thread_rountine, args) != 0){
                perror("Error creating thread");
                free(args);
                exit(1);
            } 
        }

        // join the threads (i.e. each file's word count list)
        for (int i = 1; i < argc; i++) {
            if (pthread_join(threads[i-1], NULL) != 0){
                perror("Error joining thread:");
                exit(1);
            }
        }
    }

    /* Sort and print final result of all threads' work. */ 
    // use wordcount_sort to because the joined word count list is not sorted
    wordcount_sort(&word_counts, less_count);
    fprint_words(&word_counts, stdout);
    return 0;
}
