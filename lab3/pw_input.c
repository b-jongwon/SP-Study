#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#define MAX_PASSWORD_LENGTH 128

int main() {
    char password[MAX_PASSWORD_LENGTH];
    int index = 0;
    char ch;

    struct termios old_attr, new_attr;

    
    tcgetattr(STDIN_FILENO, &old_attr);
    new_attr = old_attr;

    
    new_attr.c_lflag &= ~(ICANON | ECHO);

   
    tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);

    printf("Enter your password: ");
    fflush(stdout);

    
    while (index < MAX_PASSWORD_LENGTH - 1) {
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
        } else {
            password[index++] = ch;
            printf("*");
            fflush(stdout);
        }
    }

    password[index] = '\0'; 
    
   
    tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);

    printf("\nPassword entered: %s\n", password); 
    
    return 0;
}
