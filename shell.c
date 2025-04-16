#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT 1024
#define MAX_ARGS 128
#define HISTORY_FILE "command_history.txt"

volatile sig_atomic_t running_child = -1;

void sigint_handler(int sig) {
    if (running_child > 0) {
        kill(running_child, SIGINT);
    } else {
        write(STDOUT_FILENO, "\n", 1);
    }
}

void trim_line(char *line) {
    line[strcspn(line, "\n")] = '\0';
}

void log_command(const char *command) {
    FILE *f = fopen(HISTORY_FILE, "a");
    if (f) {
        fprintf(f, "%s\n", command);
        fclose(f);
    }
}

void tokenize(char *line, char *argv[]) {
    int i = 0;
    char *token = strtok(line, " \t");
    while (token != NULL) {
        argv[i++] = token;
        token = strtok(NULL, " \t");
    }
    argv[i] = NULL;
}

void run_simple_command(char *cmd) {
    char *argv[MAX_ARGS];
    tokenize(cmd, argv);
    if (argv[0] == NULL) return;

    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else {
        running_child = pid;
        waitpid(pid, NULL, 0);
        running_child = -1;
    }
}

void handle_redirection(char *cmd) {
    int in_fd = -1, out_fd = -1, append = 0;
    char *input = strchr(cmd, '<');
    char *output = strstr(cmd, ">>");
    if (!output) output = strchr(cmd, '>');

    if (input) {
        *input = '\0';
        input++;
        input = strtok(input, " \t");
        if (input) {
            in_fd = open(input, O_RDONLY);
            if (in_fd < 0) {
                perror("Input redirection");
                return;
            }
        }
    }

    if (output) {
        append = (strstr(cmd, ">>") != NULL);
        *output = '\0';
        output += append ? 2 : 1;
        output = strtok(output, " \t");
        if (output) {
            out_fd = open(output, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
            if (out_fd < 0) {
                perror("Output redirection");
                return;
            }
        }
    }

    char *argv[MAX_ARGS];
    tokenize(cmd, argv);

    pid_t pid = fork();
    if (pid == 0) {
        if (in_fd != -1) dup2(in_fd, STDIN_FILENO);
        if (out_fd != -1) dup2(out_fd, STDOUT_FILENO);
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else {
        running_child = pid;
        waitpid(pid, NULL, 0);
        running_child = -1;
    }

    if (in_fd != -1) close(in_fd);
    if (out_fd != -1) close(out_fd);
}

void execute_pipeline(char *segments[], int count) {
    int pipefd[2], in_fd = 0;

    for (int i = 0; i < count; ++i) {
        pipe(pipefd);
        pid_t pid = fork();
        if (pid == 0) {
            dup2(in_fd, STDIN_FILENO);
            if (i < count - 1)
                dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);

            char *argv[MAX_ARGS];
            tokenize(segments[i], argv);
            execvp(argv[0], argv);
            perror("execvp");
            exit(1);
        } else {
            running_child = pid;
            waitpid(pid, NULL, 0);
            running_child = -1;
            close(pipefd[1]);
            in_fd = pipefd[0];
        }
    }
}

void run_command(char *cmd) {
    if (strchr(cmd, '|')) {
        char *parts[20];
        int count = 0;
        char *segment = strtok(cmd, "|");
        while (segment) {
            parts[count++] = segment;
            segment = strtok(NULL, "|");
        }
        execute_pipeline(parts, count);
    } else if (strchr(cmd, '>') || strchr(cmd, '<')) {
        handle_redirection(cmd);
    } else {
        run_simple_command(cmd);
    }
}

void parse_input(char *input) {
    char *seq_cmds[64];
    int num_seq = 0;
    char *token = strtok(input, ";");
    while (token) {
        seq_cmds[num_seq++] = token;
        token = strtok(NULL, ";");
    }

    for (int i = 0; i < num_seq; ++i) {
        char *and_cmds[64];
        int num_and = 0;
        token = strtok(seq_cmds[i], "&&");
        while (token) {
            and_cmds[num_and++] = token;
            token = strtok(NULL, "&&");
        }

        int ok = 1;
        for (int j = 0; j < num_and && ok; ++j) {
            char *trimmed = and_cmds[j];
            trim_line(trimmed);
            if (strlen(trimmed) == 0) continue;
            log_command(trimmed);

            if (strncmp(trimmed, "cd", 2) == 0 && (trimmed[2] == ' ' || trimmed[2] == '\0')) {
                char *path = trimmed + 2;
                while (*path == ' ') path++;
                if (strlen(path) == 0) path = getenv("HOME");
                if (chdir(path) != 0) {
                    perror("cd");
                    ok = 0;
                }
                continue;
            }

            pid_t pid = fork();
            if (pid == 0) {
                run_command(trimmed);
                exit(0);
            } else {
                running_child = pid;
                int status;
                waitpid(pid, &status, 0);
                running_child = -1;
                if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
                    ok = 0;
            }
        }
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    char input[MAX_INPUT];
    printf("\033[1;36mWelcome to the Group 7 Custom Shell!\033[0m\n"); 
    printf("\033[1;36mType 'CTRL+C' to stop the current command.\033[0m\n"); 
    printf("\033[1;31mType 'exit' to quit.\033[0m\n");                

    while (1) {
        char cwd[1024], hostname[256];
        getcwd(cwd, sizeof(cwd));
        gethostname(hostname, sizeof(hostname));
        char *user = getenv("USER");
        printf("\033[1;32m%s@%s\033[0m:\033[1;33m%s\033[1;35m group7> ", user ? user : "user", hostname, cwd);
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            if (feof(stdin)) break;
            continue;
        }

        trim_line(input);
        if (strcmp(input, "") == 0) continue;
        if (strcmp(input, "exit") == 0) {
            printf("\033[1;31mYou've exited the Group 7 Terminal.\033[0m\n");
            break;
        }
        

        parse_input(input);
    }

    return 0;
}
