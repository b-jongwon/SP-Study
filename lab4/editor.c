#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINES 100
#define MAX_LINE_LEN 256
#define AUTOSAVE_FILE "autosave.txt"

char *lines[MAX_LINES];
int line_count = 0;

void save_lines(int is_final_save) {
    FILE *fp = fopen(AUTOSAVE_FILE, "w");
    if (fp == NULL) {
        perror("Error opening autosave file");
        return;
    }

    for (int i = 0; i < line_count; i++) {
        if (lines[i] != NULL) {
            fprintf(fp, "%s", lines[i]);
        }
    }

    fclose(fp);
    
    if (is_final_save) {
        printf("\n[!] Exit signal received. Saving final version...\n");
    } else {
    }
}

void handle_signal(int sig) {
    if (sig == SIGALRM) {
        save_lines(0);
        alarm(5);
    } 
    else if (sig == SIGINT) {
        
        save_lines(1); 

        for (int i = 0; i < line_count; i++) {
            if (lines[i] != NULL) {
                free(lines[i]);
            }
        }
        
        exit(0);
    }
}

int main() {
    if (signal(SIGINT, handle_signal) == SIG_ERR) {
        perror("Cannot register SIGINT handler");
        return 1;
    }
    if (signal(SIGALRM, handle_signal) == SIG_ERR) {
        perror("Cannot register SIGALRM handler");
        return 1;
    }

    alarm(5);

    printf("Enter text (Ctrl+C to quit): \n");

    char buffer[MAX_LINE_LEN];
    while (1) {
        if (fgets(buffer, MAX_LINE_LEN, stdin) != NULL) {
            if (line_count < MAX_LINES) {
                lines[line_count] = strdup(buffer); 
                if (lines[line_count] == NULL) {
                    perror("Memory allocation failed");
                    exit(1);
                }
                line_count++;
            } else {
                printf("Reached maximum number of lines.\n");
            }
        }
    }
    
    return 0;
}
