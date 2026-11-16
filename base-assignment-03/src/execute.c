#include "shell.h"

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

    if (strcmp(args[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <path>  change directory\n");
        printf("  exit       quit the shell\n");
        printf("  help       show this message\n");
        return 1;
    }

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
            execvp(arglist[0], arglist);
            perror("Command not found"); // runs only if execvp fails
            exit(1);
        default: // Parent process
            waitpid(cpid, &status, 0);
            return 0;
    }
}
