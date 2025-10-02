#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <termios.h>

#define MAX_INPUT 1024
#define RED   "\033[31m"
#define RESET "\033[0m"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s \"target sentence\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *target = argv[1];
    int target_len = strlen(target);
    char input[MAX_INPUT];
    int index = 0;
    char ch;
    struct timeval start, end;
    struct termios old_attr, new_attr; 

    
    printf("Type the following sentence: \n%s\n", target);
    printf("\nStart typing: ");
    fflush(stdout);

   
    tcgetattr(STDIN_FILENO, &old_attr);
    new_attr = old_attr;

   
    new_attr.c_lflag &= ~(ICANON | ECHO);
    
   
    tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);


   
    gettimeofday(&start, NULL);
    
 
    while (index < target_len && index < MAX_INPUT - 1) {
        ch = getchar();

        if (ch == '\n' || ch == '\r') {
            break; 
        }

       
        if (ch == 127 || ch == 8) {
            if (index > 0) {
                index--;
                printf("\b \b"); 
                fflush(stdout);
            }
            continue; 
        }

       
        input[index] = ch;

       
        if (ch == target[index]) {
            
            printf("%c", ch);
        } else {
           
            printf(RED "%c" RESET, ch);
        }
        
        fflush(stdout);
        index++; 
    }
    
   
    gettimeofday(&end, NULL);
    
   
    tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);
    
    
    input[index] = '\0';


 
    double elapsed = (double)(end.tv_sec - start.tv_sec) +
                     (double)(end.tv_usec - start.tv_usec) / 1e6;

    
    int correct = 0;
  
    int compare_len = (index < target_len) ? index : target_len; 
    
    for (int i = 0; i < compare_len; i++) {
        if (input[i] == target[i]) {
            correct++;
        }
    }
    
   
    double accuracy = (target_len > 0) ? 100.0 * correct / target_len : 0.0;
   
    double speed = (elapsed > 0) ? (double)index / elapsed : 0.0; 

    
    printf("\n\n=== Result ===\n");
    printf("Time taken: %.2f seconds\n", elapsed);
    printf("Typing speed: %.2f chars/sec\n", speed);
    printf("Accuracy: %.2f%%\n", accuracy);
    
    return 0;
}