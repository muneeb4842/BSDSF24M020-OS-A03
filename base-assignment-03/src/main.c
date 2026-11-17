#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while (1) {
        reap_background();                      // report finished background jobs

        cmdline = read_cmd(PROMPT, stdin);
        if (cmdline == NULL) break;             // Ctrl+D

        // "!!" or "!n": swap the line for the saved command
        if (cmdline[0] == '!') {
            char* expanded = history_expand(cmdline);
            free(cmdline);
            if (expanded == NULL) continue;     // error already printed
            printf("%s\n", expanded);           // show what will run
            cmdline = expanded;
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            history_add(cmdline);               // only real commands are saved
            execute(arglist);

            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
