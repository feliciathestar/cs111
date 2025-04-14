/*
 * Implementation of the word_count interface using Pintos lists and pthreads.
 *
 * You may modify this file, and are expected to modify it.
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

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_lp.c"
#endif

#ifndef PTHREADS
#error "PTHREADS must be #define'd when compiling word_count_lp.c"
#endif

#include "word_count.h"

/*put locks around when involving shared variables*/

void init_words(word_count_list_t *wclist) {
    list_init(&wclist->lst);
    pthread_mutex_init(&wclist->lock, NULL); 
}

size_t len_words(word_count_list_t *wclist) {
    return list_size(&wclist->lst);
}

word_count_t *find_word(word_count_list_t *wclist, char *word) {
    struct list_elem *e;
    for (e = list_begin(&wclist->lst); e != list_end(&wclist->lst); e = list_next(e)){
        word_count_t *wc = list_entry(e, word_count_t, elem);
        if (strcmp(wc->word, word) == 0) {
            return wc;
        }
    }
    return NULL;  // Return NULL if word not found
}

word_count_t *add_word(word_count_list_t *wclist, char *word) {
    pthread_mutex_lock(&wclist->lock);
    int count = 1;  // Declare and initialize count
    if (word == NULL) {
        fprintf(stderr, "Error: word is NULL\n");
        pthread_mutex_unlock(&wclist->lock);  // Don't forget to unlock before return
        return NULL;
    }
    if (count < 0) {
        fprintf(stderr, "Error: count is less than 0\n");
        pthread_mutex_unlock(&wclist->lock);
        return NULL;
    }

    word_count_t *wc = find_word(wclist, word);
    if (wc != NULL) { 
        wc->count += count;
    } else {
        wc = malloc(sizeof(word_count_t));
        if (wc == NULL) {
            perror("malloc error");
            pthread_mutex_unlock(&wclist->lock);
            return NULL;
        }
        wc->word = strdup(word);
        if (wc->word == NULL) {
            free(wc);
            perror("strdup error");
            pthread_mutex_unlock(&wclist->lock);
            return NULL;
        }
        wc->count = count;
        list_push_back(&wclist->lst, &wc->elem);
    }

    pthread_mutex_unlock(&wclist->lock);
    return wc;
}

void fprint_words(word_count_list_t *wclist, FILE *outfile) {
    struct list_elem *e;
    word_count_t *wc;
    if (outfile == NULL) {
        fprintf(stderr, "Error: outfile is NULL\n");
        return;
    }
    for (e = list_begin(&wclist->lst); e != list_end(&wclist->lst); e = list_next(e)) {
        wc = list_entry(e, word_count_t, elem); 
        fprintf(outfile, "%8d\t%s\n", wc->count, wc->word);
    }
}

static bool less_list(const struct list_elem *ewc1, const struct list_elem *ewc2, void *aux) {
    if (ewc1 == NULL || ewc2 == NULL) {
        fprintf(stderr, "Error: ewc1 or ewc2 is NULL\n");
        return false;
    }
    if (aux == NULL) {
        fprintf(stderr, "Error: aux is NULL\n");
        return false;
    }
    const word_count_t *wc1 = list_entry(ewc1, word_count_t, elem);
    const word_count_t *wc2 = list_entry(ewc2, word_count_t, elem);
    bool (*less)(const word_count_t *, const word_count_t *) = aux;

    return less(wc1, wc2);
}

void wordcount_sort(word_count_list_t *wclist,
                    bool less(const word_count_t *, const word_count_t *)) {
    if (wclist == NULL) {
        fprintf(stderr, "Error: wclist is NULL\n");
        return;
    }
    if (less == NULL) {
        fprintf(stderr, "Error: less is NULL\n");
        return;
    }

    pthread_mutex_lock(&wclist->lock);
    list_sort(&wclist->lst, less_list, less);
    pthread_mutex_unlock(&wclist->lock);  // Missing unlock here
}
