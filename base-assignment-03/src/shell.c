#include "shell.h"

// Reads one line from fp. Returns NULL on Ctrl+D (EOF with nothing typed).
char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    fflush(stdout);

    char* cmdline = (char*) malloc(MAX_LEN);
    if (cmdline == NULL) return NULL;

    int c = 0, pos = 0;

    while (pos < MAX_LEN - 1 && (c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    // Line too long: throw away the rest so it isn't run as a new command
    if (pos == MAX_LEN - 1 && c != '\n' && c != EOF) {
        while ((c = getc(fp)) != EOF && c != '\n');
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Ctrl+D
    }

    cmdline[pos] = '\0';
    return cmdline;
}

// Splits a line into words. Returns a NULL-terminated array, or NULL if empty.
char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0') {
        return NULL;
    }

    // calloc sets every pointer to NULL, so unused slots are safe
    char** arglist = (char**) calloc(MAXARGS + 1, sizeof(char*));
    if (arglist == NULL) return NULL;

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;   // skip spaces
        if (*cp == '\0') break;                   // only spaces left

        start = cp;
        while (*cp != '\0' && *cp != ' ' && *cp != '\t') cp++;
        len = cp - start;
        if (len >= ARGLEN) len = ARGLEN - 1;      // protect the buffer

        arglist[argnum] = (char*) malloc(ARGLEN);
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) {   // line had only spaces
        free(arglist);
        return NULL;
    }

    return arglist;      // arglist[argnum] is already NULL
}
