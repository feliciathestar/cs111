/*
 * You may NOT modify this file. Any changes you make to this file will not
 * be used when grading your submission.
 */

// Headers for pthread library and standard functions
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Default number of threads to create
#define NUM_THREADS 4

// Global variable shared between all threads
int common = 162;

// Pointer to shared string in heap memory
char *somethingshared;

void *threadfun(void *threadid) {
    // a long integer can fit inside a pointer variable, so it just casts the number directly.
    // the process is reversed here to get the original integer value back
    long tid;
    tid = (long) threadid;
    
    // Print thread info: ID, stack address, shared variable location and value
    printf("Thread #%lx stack: %lx common: %lx (%d) tptr: %lx\n", tid,
           (unsigned long) &tid, (unsigned long) &common, common++,
           (unsigned long) threadid);
           
    // Print shared string and offset by thread ID
    printf("%lx: %s\n", (unsigned long) somethingshared, somethingshared + tid);
    
    // Terminate this thread (not the whole program)
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int rc;                     // Return code for pthread_create
    long t;                     // Loop counter/thread ID
    int nthreads = NUM_THREADS; // Default thread count
    
    // Allocate heap memory and copy string into it
    char *targs = strcpy(malloc(100), "I am on the heap.");

    // Override thread count if provided as argument
    if (argc > 1) {
        nthreads = atoi(argv[1]);
    }

    // Array to store thread IDs
    pthread_t threads[nthreads];

    // Print main thread info and the heap string
    printf("Main stack: %lx, common: %lx (%d)\n", (unsigned long) &t,
           (unsigned long) &common, common);
    puts(targs);
    
    // Make global pointer point to heap string
    somethingshared = targs;
    
    // Create threads, passing thread number as identifier
    for (t = 0; t < nthreads; t++) {
        printf("main: creating thread %ld\n", t);
        rc = pthread_create(&threads[t], NULL, threadfun, (void *) t);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    /* Wait for all threads to complete before exiting */
    pthread_exit(NULL);
}
