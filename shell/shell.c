#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function
 * parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Stores terminal settings */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);


/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* A structure holding a command, its function, and documentation */
typedef struct fun_desc {
    cmd_fun_t *fun; // function pointer
    char *cmd;    // command string
    char *doc;  // documentation string
} fun_desc_t;

/* stores all built-in commands, mapping command names to their funcs and help text.*/
fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print the current working directory"},
    {cmd_cd, "cd", "change the current working directory"},
};

/* Prints a helpful description for the given command */
/* determining the number of elements in an array at compile time */
int cmd_help(unused struct tokens *tokens) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++) {
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
    }
    return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
    exit(0);
}

int cmd_pwd (unused struct tokens *tokens){
    char cwd[4096]; // buffer to store the current working directory
    /* getcwd() gets the current working directory */
    /* getcwd() returns a pointer to the current working directory */
    if (getcwd(cwd, sizeof(cwd)) != NULL){
        printf("%s\n", cwd);
        return 0;
    } else {
        perror("getcwd() error");
        return 1;
    }
}

/* Changes the current directory to dir */
int cmd_cd (struct tokens *tokens){
    char *dir = tokens_get_token (tokens, 1); // get the second token
    if (dir == NULL){
        fprintf (stderr, "cd: missing dir argument\n");
        return 1;
    }
    if (chdir(dir) != 0) {
        /* chdir() changes the current working directory */
        perror("chdir failed"); 
        return 1;
    }
    return 0;
}

/* Looks up the built-in command, if it exists return its index in cmd_table[] */
int lookup(char *cmd) {
    if (cmd != NULL) {
        for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++) {
            if (strcmp(cmd_table[i].cmd, cmd) == 0) {
                return i;
            }
        }
    }
    return -1;
}

/*  
    Sets up the shell's terminal environment, 
    handling foreground/background processes and terminal control. 
*/
void init_shell() {
    /* Our shell is connected to standard input. */
    shell_terminal = STDIN_FILENO; // Assigns file descriptor 0 to shell_terminal

    /* Check if we are running interactively, otherwise reading cmd from file*/
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {
        /* If the shell is not currently in the foreground, we must pause the
         * shell until it becomes a foreground process. We use SIGTTIN to pause
         * the shell. When the shell gets moved to the foreground, we'll receive
         * a SIGCONT. */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp())) {
            kill(-shell_pgid, SIGTTIN);
        }

        /* Saves the shell's process id */
        shell_pgid = getpid();

        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save the current termios to a variable, so it can be restored later.
         */
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}

int main(unused int argc, unused char *argv[]) {
    init_shell();

    static char line[4096];
    int line_num = 0; //tracks the prompt number for the shell's UI

    /* Only print shell prompts when standard input is a tty */
    if (shell_is_interactive) {
        fprintf(stdout, "%d: ", line_num);
    }

    while (fgets(line, 4096, stdin)) {
        /* Split our line into words. */
        struct tokens *tokens = tokenize(line);

        /* Find which built-in function to run. */
        int fundex = lookup(tokens_get_token(tokens, 0)); // look for the 1st token

        if (fundex >= 0) {
            cmd_table[fundex].fun(tokens);
        } else {
            pid_t pid = fork(); // create a new process
            if (pid == 0){
                size_t num_tokens = tokens_get_length(tokens);

                // Allocate ptrs to each token word
                char **argv = malloc ((num_tokens +1) * sizeof(char *));
                if (!argv) {
                    perror("malloc failed");
                    exit(EXIT_FAILURE);
                }

                // Copy each token ptr into the argv array
                for (size_t i = 0; i < num_tokens; i++) {
                    argv[i] = tokens_get_token(tokens, i);
                }
                argv[num_tokens] = NULL; // last element must be NULL

                // execute command using execv()
                execv(argv[0], argv);

                // If execv() fails, print error message
                perror("execv failed");
                free(argv); // free the allocated memory
                exit(EXIT_FAILURE); // exit child process

            } else if (pid < 0) {
                perror("fork failed");
                exit(EXIT_FAILURE); // exit parent process
            } else {
                // Parent process waits for child to finish
                int status;
                waitpid(pid, &status, 0);
            }
        }
        // fprintf(stdout, "This shell doesn't know how to run programs.\n");

        if (shell_is_interactive) {
            /* Only print shell prompts when standard input is not a tty. */
            fprintf(stdout, "%d: ", ++line_num);
        }

        /* Clean up memory. */
        tokens_destroy(tokens);
    }

    return 0;
}
