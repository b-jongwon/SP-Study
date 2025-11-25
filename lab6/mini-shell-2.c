/* mini-shell-2.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h> // 추가됨

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_BLOCK_LINES 32
#define MAX_VARS 64
#define MAX_JOBS 64 // 추가됨

// --- Job Control Structures (Page 19) ---
typedef struct {
    pid_t pid;
    char command[MAX_LINE];
    int stopped; // 0: running, 1: stopped
} Job;

Job jobs[MAX_JOBS];
int job_count = 0;
pid_t fg_pid = -1;

// --- 기존 변수 ---
typedef struct {
    char name[64];
    char value[256];
} Variable;

Variable local_vars[MAX_VARS];
int local_var_count = 0;
Variable global_vars[MAX_VARS];
int global_var_count = 0;

// --- Job Helper Function (Page 19) ---
void print_job_status(int index) {
    const char *status = jobs[index].stopped ? "Stopped" : "Running";
    printf("[%d] %-8s %d %s\n", index + 1, status, jobs[index].pid, jobs[index].command);
}

// --- Signal Handler (Page 20) ---
void handle_sigtstp(int sig) {
    if (fg_pid > 0) {
        kill(fg_pid, SIGTSTP);
        // 여기서 바로 jobs에 추가하지 않고 waitpid에서 상태 변화 감지 후 처리
    }
}

// --- 기존 함수들 (get_var_value ~ set_global_var ~ expand_variables ~ parse_command) ---
// (mini-shell-1.c와 동일하므로 생략하지 않고 과제 제출용으로 전체 포함)

const char* get_var_value(const char* name) {
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_vars[i].name, name) == 0) return local_vars[i].value;
    }
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_vars[i].name, name) == 0) return global_vars[i].value;
    }
    const char* env = getenv(name);
    return env ? env : "";
}

void set_local_var(const char* name, const char* value) {
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_vars[i].name, name) == 0) {
            strncpy(local_vars[i].value, value, sizeof(local_vars[i].value));
            return;
        }
    }
    if (local_var_count < MAX_VARS) {
        strncpy(local_vars[local_var_count].name, name, sizeof(local_vars[local_var_count].name));
        strncpy(local_vars[local_var_count].value, value, sizeof(local_vars[local_var_count].value));
        local_var_count++;
    }
}

void set_global_var(const char* name, const char* value) {
    if (value == NULL) value = "";
    setenv(name, value, 1);
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_vars[i].name, name) == 0) {
            strncpy(global_vars[i].value, value, sizeof(global_vars[i].value));
            return;
        }
    }
    if (global_var_count < MAX_VARS) {
        strncpy(global_vars[global_var_count].name, name, sizeof(global_vars[global_var_count].name));
        strncpy(global_vars[global_var_count].value, value, sizeof(global_vars[global_var_count].value));
        global_var_count++;
    }
}

void expand_variables(char* line) {
    char buffer[MAX_LINE];
    int bi = 0;
    for (int i = 0; line[i] != '\0'; ) {
        if (line[i] == '$') {
            i++;
            char varname[64];
            int vi = 0;
            while (isalnum(line[i]) || line[i] == '_') {
                varname[vi++] = line[i++];
            }
            varname[vi] = '\0';
            const char* val = get_var_value(varname);
            for (int j = 0; val[j] != '\0'; j++) {
                buffer[bi++] = val[j];
            }
        } else {
            buffer[bi++] = line[i++];
        }
    }
    buffer[bi] = '\0';
    strncpy(line, buffer, MAX_LINE);
}

void parse_command(char *line, char **args) {
    expand_variables(line);
    int i = 0;
    char *token = strtok(line, " \t\n");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
}

// --- execute_external_command 수정됨 (Background 및 Signal 처리) ---
int execute_external_command(char **args) {
    int is_bg = 0;
    int k = 0;
    // Check for & at the end
    while (args[k] != NULL) k++;
    if (k > 0 && strcmp(args[k-1], "&") == 0) {
        is_bg = 1;
        args[k-1] = NULL; // Remove &
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Child process
        // Restore default signal handlers in child
        signal(SIGTSTP, SIG_DFL);

        int input_fd = -1, output_fd = -1;
        char *clean_args[MAX_ARGS];
        int j = 0;
        
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "<") == 0 && args[i + 1]) {
                input_fd = open(args[i + 1], O_RDONLY);
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
                i++;
            } else if (strcmp(args[i], ">") == 0 && args[i + 1]) {
                output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
                i++;
            } else {
                clean_args[j++] = args[i];
            }
        }
        clean_args[j] = NULL;
        
        execvp(clean_args[0], clean_args);
        fprintf(stderr, "%s: command not found\n", clean_args[0]);
        exit(1);
    } else {
        // Parent process
        if (is_bg) {
            // Background Execution
            if (job_count < MAX_JOBS) {
                jobs[job_count].pid = pid;
                strncpy(jobs[job_count].command, args[0], MAX_LINE); // Store command name
                jobs[job_count].stopped = 0;
                printf("[background pid %d]\n", pid);
                job_count++;
            }
            return 0;
        } else {
            // Foreground Execution
            fg_pid = pid;
            int status;
            // WUNTRACED to detect stopped children
            waitpid(pid, &status, WUNTRACED);
            fg_pid = -1;

            if (WIFSTOPPED(status)) {
                // Handle Ctrl+Z (Stopped)
                if (job_count < MAX_JOBS) {
                    jobs[job_count].pid = pid;
                    strncpy(jobs[job_count].command, args[0], MAX_LINE);
                    jobs[job_count].stopped = 1;
                    printf("\n[Stopped] pid %d\n", pid);
                    job_count++;
                }
            }
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
    }
}

int is_blank_line(const char *line) {
    while (*line) {
        if (!isspace(*line)) return 0;
        line++;
    }
    return 1;
}

void execute_command(char **args);

void handle_if_block(char *if_line, FILE *input) {
    // (mini-shell-1.c와 동일)
    char cond_line[MAX_LINE], tmp[MAX_LINE], line[MAX_LINE];
    strncpy(tmp, if_line, MAX_LINE); tmp[MAX_LINE-1] = '\0';
    if (strlen(if_line) > 3) strncpy(cond_line, if_line + 3, MAX_LINE);
    else cond_line[0] = '\0';

    if (!fgets(line, sizeof(line), input)) return;
    char *trimmed = strtok(line, " \t\n");
    if (!trimmed || strcmp(trimmed, "then") != 0) { fprintf(stderr, "Syntax error: expected 'then'\n"); return; }

    char block[MAX_BLOCK_LINES][MAX_LINE];
    int count = 0, found_fi = 0;
    while (fgets(line, sizeof(line), input)) {
        if (is_blank_line(line)) continue;
        char ft_buf[MAX_LINE]; strncpy(ft_buf, line, MAX_LINE);
        char *ft = strtok(ft_buf, " \t\n");
        if (ft && strcmp(ft, "fi") == 0) { found_fi = 1; break; }
        strncpy(block[count++], line, MAX_LINE);
    }
    if (!found_fi) { fprintf(stderr, "Syntax error: missing 'fi'\n"); return; }

    char *cargs[MAX_ARGS];
    parse_command(cond_line, cargs);
    if (execute_external_command(cargs) == 0) {
        for (int i = 0; i < count; i++) {
            parse_command(block[i], cargs);
            execute_command(cargs);
        }
    }
}

// --- execute_command 수정됨 (jobs 추가) ---
void execute_command(char **args) {
    if (args[0] == NULL) return;

    if (strcmp(args[0], "set") == 0) {
        for (int i = 0; i < local_var_count; i++) printf("%s=%s\n", local_vars[i].name, local_vars[i].value);
        for (int i = 0; i < global_var_count; i++) printf("export %s=%s\n", global_vars[i].name, global_vars[i].value);
        return;
    }

    if (strchr(args[0], '=') && args[0][0] != '=') {
        char *eq = strchr(args[0], '='); *eq = '\0';
        set_local_var(args[0], eq + 1);
        return;
    }

    if (strcmp(args[0], "exit") == 0) exit(0);

    if (strcmp(args[0], "export") == 0) {
        for (int i = 1; args[i]; i++) {
            char *eq = strchr(args[i], '=');
            if (eq) { *eq = '\0'; set_global_var(args[i], eq + 1); }
            else set_global_var(args[i], get_var_value(args[i]));
        }
        return;
    }

    // --- New Built-in: jobs ---
    if (strcmp(args[0], "jobs") == 0) {
        for (int i = 0; i < job_count; i++) {
            print_job_status(i);
        }
        return;
    }

    execute_external_command(args);
}

void process_line(char *line, FILE *input) {
    char *args[MAX_ARGS];
    if (is_blank_line(line)) return;
    if (strncmp(line, "if ", 3) == 0) handle_if_block(line, input);
    else { parse_command(line, args); execute_command(args); }
}

int main(int argc, char *argv[]) {
    // --- Register Signal Handler ---
    signal(SIGTSTP, handle_sigtstp);

    char line[MAX_LINE];
    if (argc == 2) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { perror("fopen"); return 1; }
        while (fgets(line, sizeof(line), fp)) process_line(line, fp);
        fclose(fp);
        return 0;
    }
    while (1) {
        printf("mini-shell> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        process_line(line, stdin);
    }
    return 0;
}