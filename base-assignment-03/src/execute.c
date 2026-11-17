#include "shell.h"
#include <fcntl.h>

/* ---------- Background job list ---------- */

#define MAX_JOBS 64
static pid_t job_pid[MAX_JOBS];   // 0 = free slot
static int   job_num[MAX_JOBS];
static int   next_job = 1;

// Remember a background job and print "[n] pid"
static void job_add(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_pid[i] == 0) {
            job_pid[i] = pid;
            job_num[i] = next_job;
            printf("[%d] %d\n", next_job, (int) pid);
            fflush(stdout);
            next_job++;
            return;
        }
    }
    printf("[?] %d\n", (int) pid);        // job list full
    fflush(stdout);
}

// Collect finished background children (no zombies) and report them.
// Called before every prompt.
void reap_background(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_JOBS; i++) {
            if (job_pid[i] == pid) {
                printf("[%d] Done  %d\n", job_num[i], (int) pid);
                fflush(stdout);
                job_pid[i] = 0;
                break;
            }
        }
    }
}

// A background command must not read the keyboard
static void detach_stdin(void) {
    int fd = open("/dev/null", O_RDONLY);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
}

/* ---------- Built-ins ---------- */

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
        printf("Redirection, pipes, background (put spaces around the symbol):\n");
        printf("  cmd > file    write output to file (overwrite)\n");
        printf("  cmd >> file   add output to the end of file\n");
        printf("  cmd < file    read input from file\n");
        printf("  cmd1 | cmd2   send output of cmd1 into cmd2\n");
        printf("  cmd &         run cmd in the background (last word only)\n");
        return 1;
    }

    return 0;
}

/* ---------- Redirection ---------- */

// Finds <, >, >> in args, opens the files, points stdin/stdout at them
// (dup2), and removes those words from args. Used in a CHILD only.
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

// Runs inside a child process: redirections, then built-in or exec.
// Never returns.
static void run_child(char** args) {
    if (setup_redirection(args) < 0) exit(1);

    if (args[0] == NULL) {
        fprintf(stderr, "syntax error: no command\n");
        exit(1);
    }

    // Built-ins inside a pipeline (e.g. "history | grep ls") run here
    if (handle_builtin(args)) {
        exit(0);                       // exit() flushes the output into the pipe
    }

    execvp(args[0], args);
    perror("Command not found");       // runs only if execvp fails
    exit(1);
}

/* ---------- Pipes ---------- */

static void free_cmds(char*** cmds, int n) {
    for (int i = 0; i < n; i++) free(cmds[i]);
}

// Handles  cmd1 | cmd2 | cmd3 ...   (background = 1: do not wait)
static int run_pipeline(char* arglist[], int background) {
    char** cmds[MAXARGS + 1];          // one word-list per command
    int n = 0, j = 0;

    cmds[0] = (char**) calloc(MAXARGS + 1, sizeof(char*));
    if (cmds[0] == NULL) return 0;

    // Split the words at every "|" (the strings are shared, not copied)
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            if (j == 0) {
                fprintf(stderr, "syntax error near '|'\n");
                free_cmds(cmds, n + 1);
                return 0;
            }
            n++;
            j = 0;
            cmds[n] = (char**) calloc(MAXARGS + 1, sizeof(char*));
            if (cmds[n] == NULL) { free_cmds(cmds, n); return 0; }
        } else {
            cmds[n][j++] = arglist[i];
        }
    }
    if (j == 0) {                      // line ended with "|"
        fprintf(stderr, "syntax error near '|'\n");
        free_cmds(cmds, n + 1);
        return 0;
    }

    int ncmds = n + 1;
    int pipefd[MAXARGS][2];
    pid_t pids[MAXARGS + 1];
    int started = 0;

    // Make all the belts first
    for (int i = 0; i < ncmds - 1; i++) {
        if (pipe(pipefd[i]) < 0) {
            perror("pipe failed");
            for (int k = 0; k < i; k++) { close(pipefd[k][0]); close(pipefd[k][1]); }
            free_cmds(cmds, ncmds);
            return 0;
        }
    }

    fflush(stdout);                    // avoid duplicated output in children

    // One child per command
    for (int i = 0; i < ncmds; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            break;
        }
        if (pid == 0) {
            if (i > 0)          dup2(pipefd[i - 1][0], STDIN_FILENO);
            if (i < ncmds - 1)  dup2(pipefd[i][1], STDOUT_FILENO);
            if (i == 0 && background) detach_stdin();
            for (int k = 0; k < ncmds - 1; k++) {   // close every belt end
                close(pipefd[k][0]);
                close(pipefd[k][1]);
            }
            run_child(cmds[i]);
        }
        pids[started++] = pid;
    }

    // Parent must close its copies, or the last command never sees "end of input"
    for (int k = 0; k < ncmds - 1; k++) {
        close(pipefd[k][0]);
        close(pipefd[k][1]);
    }

    if (background) {
        if (started > 0) job_add(pids[started - 1]);   // report the last command
    } else {
        for (int k = 0; k < started; k++) waitpid(pids[k], NULL, 0);
    }

    free_cmds(cmds, ncmds);
    return 0;
}

/* ---------- Running one command line ---------- */

static int execute_line(char* arglist[], int background) {
    // Is there a pipe in this line?
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) return run_pipeline(arglist, background);
    }

    // No pipe: built-ins (like cd) must run in the shell itself
    if (handle_builtin(arglist)) return 0;

    fflush(stdout);
    pid_t cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0: // Child process
            if (background) detach_stdin();
            run_child(arglist);
            exit(1);
        default: // Parent process
            if (background)
                job_add(cpid);                 // do not wait
            else
                waitpid(cpid, NULL, 0);        // wait for this child only
            return 0;
    }
}

int execute(char* arglist[]) {
    int n = 0;
    while (arglist[n] != NULL) n++;

    int background = 0;
    char* amp = NULL;

    // A trailing "&" means: run in the background
    if (n > 0 && strcmp(arglist[n - 1], "&") == 0) {
        if (n == 1) {
            fprintf(stderr, "syntax error: no command before '&'\n");
            return 0;
        }
        background = 1;
        amp = arglist[n - 1];
        arglist[n - 1] = NULL;         // hide "&" while running
    }

    int result = execute_line(arglist, background);

    if (amp != NULL) arglist[n - 1] = amp;   // put it back so main can free it
    return result;
}
