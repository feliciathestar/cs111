/*
 * Word count application with one process per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
 */

/*
 * Copyright © 2019 University of California, Berkeley
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
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h> 

#include "word_count.h"
#include "word_helpers.h"

//process multiple input files, with each file being handled by a separate process

/*
 * Reads formatted word counts from a stream and merges them into the main list
 */
void merge_counts(word_count_list_t *wclist, FILE *count_stream) {
    char *word; // word to be counted
    int count; // count of the word
    int rv; // return value of fscanf
    
    while ((rv = fscanf(count_stream, "%8d\t%ms\n", &count, &word)) == 2) {
        add_word_with_count(wclist, word, count);
    }
    if ((rv == EOF) && (feof(count_stream) == 0)) {
        perror("could not read counts");
    } else if (rv != EOF) {
        fprintf(stderr, "read ill-formed count (matched %d)\n", rv);
    }
}

/*
 * main - handle command line, spawning one process per file.
 */
int main(int argc, char *argv[]) {
    word_count_list_t word_counts;
    init_words(&word_counts);                
    
    if (argc <= 1) {
        /* Process stdin in a single process. */
        count_words(&word_counts, stdin);
    } else {
        int num_files = argc - 1;
        pid_t *pids = malloc(num_files * sizeof(pid_t));// array of process ids
        int *read_fds = malloc(num_files * sizeof(int)); // array of read file descriptors or each child
        if (pids == NULL || read_fds == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        // for each input file, create a pipe
        for (int i = 0; i < num_files; i++) {
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe creation failed");
                free(pids);
                free(read_fds);
                exit(EXIT_FAILURE);
            }

            pids[i] = fork(); // create a new process
            if (pids[i] < 0){
                perror("fork failed");
                free(pids);
                free(read_fds);
                exit(EXIT_FAILURE);
            }else if (pids[i] == 0) {
                // child process
                free(pids); // Child doesn't need these arrays; only parent
                free(read_fds); // they do not need to know about other processes
                close(pipefd[0]); // close read end of pipe
                
                // create a new word_count_list_t for the child process
                word_count_list_t child_word_counts;
                init_words(&child_word_counts);
                
                // open the input file
                FILE *fp = fopen(argv[i+1], "r"); // use i+1 because argv[0] is the program name
                if (fp == NULL) {
                    perror("fopen");
                    close(pipefd[1]); // Close write end before exiting
                    exit(EXIT_FAILURE);
                }
                count_words(&child_word_counts, fp);  // CORRECT
                fclose(fp);

                // write the word counts to the pipe
                FILE *pipe_stream = fdopen(pipefd[1], "w");
                if (!pipe_stream) {
                    perror("fdopen pipe write stream failed");
                    close(pipefd[1]);
                    exit(1);
                }
                fprint_words(&child_word_counts, pipe_stream);  // CORRECT
                

                // safe close of the write end of the pipe
                if (fclose(pipe_stream) != 0) { 
                    perror("fclose pipe_stream");
                    // fd is likely closed anyway, but exit to signal error
                    exit(EXIT_FAILURE);
                }
                exit(EXIT_SUCCESS);

            } else {
                // parent process
                close(pipefd[1]); // close write end of pipe
                read_fds[i] = pipefd[0]; // store the read end of the pipe
            }
        }

        // wait for all children and merge their results
        for (int i = 0 ; i < num_files; i++) {
            // Merge counts from the ith child
            FILE *pipe_stream = fdopen(read_fds[i], "r");
            if (pipe_stream == NULL) {
                perror("fopen pipe read stream failed");
                free(pids);
                free(read_fds);
                exit(EXIT_FAILURE);
            }else{
                merge_counts(&word_counts, pipe_stream);
                if (fclose(pipe_stream) != 0) { // fclose also closes read_fds[i]
                    perror("fclose read pipe failed");
                }
            }

            // Wait for the child process to finish
            int status;
            if (waitpid(pids[i], &status, 0) == -1) {
                perror("waitpid failed");
                free(pids);
                free(read_fds);
                exit(EXIT_FAILURE);
            } else { // waitpid succeeded, now check how the child exited
                if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS) {
                    // Child exited with an error
                    fprintf(stderr, "Child process %d exited with status %d\n", pids[i], WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    // Child was terminated by a signal
                    fprintf(stderr, "Child process %d terminated by signal %d\n", pids[i], WTERMSIG(status));
                }
           }
        }
        free(pids);
        free(read_fds);
    }

    /* Output final result of all process' work. */
    wordcount_sort(&word_counts, less_count);
    fprint_words(&word_counts, stdout);
    return 0;
}
