#include "shell.h"

/* ---------- Reading and splitting ---------- */

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

    char** arglist = (char**) calloc(MAXARGS + 1, sizeof(char*));
    if (arglist == NULL) return NULL;

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;
        if (*cp == '\0') break;

        start = cp;
        while (*cp != '\0' && *cp != ' ' && *cp != '\t') cp++;
        len = cp - start;
        if (len >= ARGLEN) len = ARGLEN - 1;

        arglist[argnum] = (char*) malloc(ARGLEN);
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) {
        free(arglist);
        return NULL;
    }

    return arglist;
}

/* ---------- History ---------- */

static char* history[HISTORY_SIZE];
static int hist_count = 0;   // how many commands are stored (max 20)
static int hist_total = 0;   // how many commands were ever added

// Number of the oldest command still stored
static int first_number(void) {
    return hist_total - hist_count + 1;
}

// Saves a command. When full, the oldest one is dropped.
void history_add(const char* cmd) {
    if (hist_count == HISTORY_SIZE) {
        free(history[0]);
        memmove(history, history + 1, sizeof(char*) * (HISTORY_SIZE - 1));
        hist_count--;
    }
    history[hist_count++] = strdup(cmd);
    hist_total++;
}

// Prints the stored commands with their numbers
void history_show(void) {
    int first = first_number();
    for (int i = 0; i < hist_count; i++) {
        printf("%4d  %s\n", first + i, history[i]);
    }
}

// Handles "!!" and "!n". Returns a new string (caller frees) or NULL on error.
char* history_expand(const char* cmdline) {
    if (hist_count == 0) {
        fprintf(stderr, "history: no commands yet\n");
        return NULL;
    }

    int n;
    if (strcmp(cmdline, "!!") == 0) {
        n = hist_total;                       // last command
    } else {
        char* end;
        n = (int) strtol(cmdline + 1, &end, 10);
        if (end == cmdline + 1 || *end != '\0') {
            fprintf(stderr, "history: use !! or !<number>\n");
            return NULL;
        }
    }

    int idx = n - first_number();
    if (idx < 0 || idx >= hist_count) {
        fprintf(stderr, "history: no command number %d\n", n);
        return NULL;
    }
    return strdup(history[idx]);
}
