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

bool is_full_path(char *cmd) {
    // Check if the command starts with '/' or './' or '../'
    if (cmd[0] == '/' || (cmd[0] == '.' && cmd[1] == '/') || (cmd[0] == '.' && cmd[1] == '.' && cmd[2] == '/')) {
        return true;
    }
    return false;
}

/* Convert cmd name to its full path */
char* get_full_path(char *cmd){
    char *path = getenv("PATH"); // get the PATH environment variable
    if (path == NULL) {
        fprintf(stderr, "PATH not set\n");
        return NULL;
    }

    char *full_path = malloc(4096); // allocate memory for full path
    if (full_path == NULL) {
        perror("malloc failed");
        return NULL;
    }

    char *path_copy = strdup(path); // duplicate the PATH string
    if (path_copy == NULL) {
        perror("strdup failed");
        free(full_path);
        return NULL;
    }

    // dir stores the word before the 1st ':'
    char *dir = strtok(path_copy, ":"); 
    // The first call to strtok() with path_copy set up 
    // internal state about where it left off in the string 
    
    while (dir != NULL) {
        //create the full path: "dir/cmd\0"
        char *try_path = malloc(strlen(dir) + strlen(cmd)+2);
        if (try_path == NULL) {
            perror("malloc failed");
            free(full_path);
            free(path_copy);
            return NULL;
        }

        strcpy(try_path, dir); // copy the directory
        strcat(try_path, "/"); // add a '/'
        strcat(try_path, cmd); // add the command

        //check if potential path is executable by the user
        if (access(try_path, X_OK) == 0) {
            // If the file is executable, return the full path
            strcpy(full_path, try_path);
            free(try_path);
            free(path_copy);
            return full_path;
        }

        free(try_path);

        // calling strtok() again with NULL continues tokenizing from where strtok() left off
        dir = strtok(NULL, ":"); 
    }
    free(path_copy);
    return NULL;
}

bool needs_redirection (struct tokens *tokens){
    // Check if the command has a redirection operator
    for (int i = 0; i < tokens_get_length(tokens); i++) {
        char *token = tokens_get_token(tokens, i);
        if (strcmp(token, ">") == 0 || strcmp(token, "<") == 0) {
            return true;
        }
    }
    return false;
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
            //While the shell’s process group does not own the terminal, we keep waiting
            // if a background process group is running, we suspend it using SIGTTIN 
        }

        /* Ignore interactive and job-control signals*/
        struct sigaction act; // act defines how you want the program to handle certain signals
        memset(&act, 0, sizeof(act)); // zeros out the structure — good practice for avoiding garbage values
        act.sa_handler = SIG_IGN; // a predefined constant meaning "ignore."

        sigaction(SIGINT, &act, NULL); // Don’t kill your shell, just subprocesses.
        sigaction(SIGQUIT, &act, NULL); // only kill subprocesses too
        sigaction(SIGTSTP, &act, NULL); // so the shell does not pause
        sigaction(SIGTTIN, &act, NULL); // Shell might read stdin even if it’s not foreground — don’t let it suspend.
        sigaction(SIGTTOU, &act, NULL); // Shell might write stdout even if it’s not foreground 

        /* Put ourselves in our own process group */
        shell_pgid = getpid();
        if (setpgid(0, shell_pgid) < 0){
            perror("Couldn't put the shell in its own process group");
            exit(EXIT_FAILURE);
        }

        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save the current terminal attributes to shell_tmodes, so it can be restored later.*/
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
        if (tokens == NULL) {
            fprintf(stderr, "empty cmd line input");
            continue;
        }

        /* Find which built-in function to run. */
        int fundex = lookup(tokens_get_token(tokens, 0)); // look for the 1st token

        if (fundex >= 0) {
            cmd_table[fundex].fun(tokens);
        } else {
            /* Resolve the command to its full path */ 
            char *cmd = tokens_get_token(tokens, 0);
            char *cmd_path;  // Store path separately
            if (is_full_path(cmd)) {
                cmd_path = strdup(cmd);
            } else {
                // Otherwise, get the full path using get_full_path()
                cmd_path = get_full_path(cmd);
                if (cmd == NULL) {
                    fprintf(stderr, "Command not found: %s\n", tokens_get_token(tokens, 0));
                    tokens_destroy(tokens);
                    continue;
                }
            }

            /* Redirect streams between process and file */
            if (needs_redirection(tokens)) {
                int fd;
                char *redir_token = tokens_get_token(tokens, 1);
                if (strcmp(redir_token, ">") == 0) {
                    fd = open(tokens_get_token(tokens, 2), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                } else if (strcmp(redir_token, "<") == 0) {
                    fd = open(tokens_get_token(tokens, 2), O_RDONLY);
                } else {
                    fprintf(stderr, "Invalid redirection operator\n");
                    free(cmd_path);
                    tokens_destroy(tokens);
                    continue;
                }

                if (fd < 0) {
                    perror("open failed");
                    free(cmd_path);
                    tokens_destroy(tokens);
                    continue;
                }

                dup2(fd, strcmp(redir_token, ">") == 0 ? STDOUT_FILENO : STDIN_FILENO);
                close(fd);
            }

            pid_t pid = fork();
            if (pid == 0) {
                /* Put chile process in its own process group */
                if (setpgid(0, 0) < 0) {
                    perror("setpgid failed");
                    exit(EXIT_FAILURE);
                }
                // If we're in interactive mode, take control of the terminal
                if (shell_is_interactive) {
                    tcsetpgrp(shell_terminal, getpid());
                }
                // set the signal handlers back to default
                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);

                /* Execute command */
                size_t num_tokens = tokens_get_length(tokens);
                size_t argv_size = needs_redirection(tokens) ? num_tokens - 2 : num_tokens;

                // Allocate ptrs to each token word
                char **argv = malloc((argv_size + 1) * sizeof(char *));
                if (!argv) {
                    perror("malloc failed");
                    free(cmd_path);
                    exit(EXIT_FAILURE);
                }

                // Copy command name and arguments, skipping redirection tokens
                argv[0] = tokens_get_token(tokens, 0);  // Command name
                size_t argv_idx = 1;
                
                for (size_t i = 1; i < num_tokens; i++) {
                    char *token = tokens_get_token(tokens, i);
                    // Skip redirection operator and filename
                    if (strcmp(token, ">") == 0 || strcmp(token, "<") == 0) {
                        i++;  // Skip the next token (filename)
                        continue;
                    }
                    argv[argv_idx++] = token;
                }
                argv[argv_size] = NULL;

                execv(cmd_path, argv);
                perror("execv failed");
                free(argv);
                free(cmd_path);
                exit(EXIT_FAILURE);
            } else if (pid < 0) {
                perror("fork failed");
                free(cmd_path);
                exit(EXIT_FAILURE); // exit parent process
            } else { // Parent process
                // Put child in its own process group
                if (setpgid(pid, pid) < 0) {
                    // Ignore race condition errors (EACCES occurs when the child is already running smth)
                    if (errno != EACCES) { 
                        perror("setpgid failed");
                    }
                }
                
                // If in interactive mode, give terminal control to child
                if (shell_is_interactive) {
                    tcsetpgrp(shell_terminal, pid);
                }

                int status;
                waitpid(pid, &status, 0); // child should be done after this
                free(cmd_path);

                /* Set the terminal back to the shell's process group */
                if (shell_is_interactive) {
                    // When child is done, take back terminal control
                    tcsetpgrp(shell_terminal, shell_pgid);
                    
                    // Restore the shell's terminal modes
                    tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
                }
            }
        }

        if (shell_is_interactive) {
            /* Only print shell prompts when standard input is not a tty. */
            fprintf(stdout, "%d: ", ++line_num);
        }

        /* Clean up memory. */
        tokens_destroy(tokens);
    }

    return 0;
}
