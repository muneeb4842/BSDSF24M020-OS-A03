#include "shell.h"
#include <fcntl.h>

// Returns 1 if the command was a built-in (already handled), 0 otherwise.
int handle_builtin(char** args) {
    if (strcmp(args[0], "exit") == 0) {
        printf("Shell exited.\n");
        exit(0);
    }

    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL)
            fprintf(stderr, "cd: missing path\n");
        else if (chdir(args[1]) != 0)
            perror("cd failed");
        return 1;
    }

    if (strcmp(args[0], "history") == 0) {
        history_show();
        return 1;
    }

    if (strcmp(args[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <path>  change directory\n");
        printf("  history    show the last %d commands\n", HISTORY_SIZE);
        printf("  !!         repeat the last command\n");
        printf("  !<n>       repeat command number n\n");
        printf("  exit       quit the shell\n");
        printf("  help       show this message\n");
        printf("Redirection (put spaces around the symbol):\n");
        printf("  cmd > file    write output to file (overwrite)\n");
        printf("  cmd >> file   add output to the end of file\n");
        printf("  cmd < file    read input from file\n");
        return 1;
    }

    return 0;
}

// Finds <, >, >> in args, opens the files, points stdin/stdout at them
// (dup2), and removes those words from args. Used in the CHILD only.
// Returns 0 on success, -1 on error.
static int setup_redirection(char** args) {
    int j = 0;

    for (int i = 0; args[i] != NULL; i++) {
        int is_in  = (strcmp(args[i], "<")  == 0);
        int is_out = (strcmp(args[i], ">")  == 0);
        int is_app = (strcmp(args[i], ">>") == 0);

        if (is_in || is_out || is_app) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "syntax error: missing file after '%s'\n", args[i]);
                return -1;
            }

            int fd;
            if (is_in)
                fd = open(args[i + 1], O_RDONLY);
            else if (is_out)
                fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            else
                fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);

            if (fd < 0) {
                perror(args[i + 1]);
                return -1;
            }

            dup2(fd, is_in ? STDIN_FILENO : STDOUT_FILENO);
            close(fd);
            i++;                       // skip the file name too
        } else {
            args[j++] = args[i];       // keep normal words
        }
    }

    args[j] = NULL;
    return 0;
}

int execute(char* arglist[]) {
    if (handle_builtin(arglist)) return 0;

    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0: // Child process
            if (setup_redirection(arglist) < 0) exit(1);
            if (arglist[0] == NULL) {
                fprintf(stderr, "syntax error: no command\n");
                exit(1);
            }
            execvp(arglist[0], arglist);
            perror("Command not found"); // runs only if execvp fails
            exit(1);
        default: // Parent process
            waitpid(cpid, &status, 0);
            return 0;
    }
}
