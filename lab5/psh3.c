#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAXARGS 20
#define ARGLEN 100

void execute(char *arglist[]);
char *makestring(char *buf);

int main() {
    char *arglist[MAXARGS + 1];
    int numargs = 0;
    char argbuf[ARGLEN];

    // [핵심] 부모 쉘(이 프로그램)은 Ctrl+C(SIGINT)를 무시(SIG_IGN)하도록 설정
    // 이유: 쉘에서 실행 중인 프로그램을 끄려다가 쉘 자체가 꺼지면 안 되기 때문
    signal(SIGINT, SIG_IGN);

    while (numargs < MAXARGS) {
        printf("Arg[%d]? ", numargs);

        // 입력 받기 실패 시 (NULL 반환)
        if (fgets(argbuf, ARGLEN, stdin) == NULL) {
            if (feof(stdin)) { // Ctrl+D (EOF) 입력 시 정상 종료
                printf("\n");
                break;
            } else { // 진짜 에러 발생 시
                perror("fgets failed");
                exit(1);
            }
        }

        // 엔터만 입력된 경우 -> 명령어 실행 시점
        if (*argbuf == '\n') {
            if (numargs > 0) {
                arglist[numargs] = NULL; // 인자 리스트 끝 표시
                execute(arglist);        // 실행
                numargs = 0;             // 초기화
            }
        } else {
            // 일반 문자열 입력 시
            char *new_arg = makestring(argbuf);
            
            // [핵심] 쉘 내장 명령어(Built-in Command) 처리
            // 첫 번째 인자가 "exit"이면 쉘 자체를 종료
            if (numargs == 0 && strcmp(new_arg, "exit") == 0) {
                printf("Exiting shell.\n");
                free(new_arg); // 할당된 메모리 해제
                exit(0);       // 프로그램 종료
            }
            
            // 인자 리스트에 추가
            arglist[numargs++] = new_arg;
        }
    }
    return 0;
}

void execute(char *arglist[]) {
    pid_t pid;
    int exitstatus;

    pid = fork(); // 프로세스 생성
    
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    } else if (pid == 0) {
        // [핵심] 자식 프로세스(실행될 명령어)는 Ctrl+C에 반응해야 함
        // 따라서 시그널 처리를 기본 동작(SIG_DFL)으로 복구
        signal(SIGINT, SIG_DFL); 
        
        execvp(arglist[0], arglist); // 프로그램 실행 (성공 시 덮어써짐)
        perror("execvp failed");
        exit(1);
    } else {
        // 부모 프로세스: 자식이 끝날 때까지 대기
        while (wait(&exitstatus) != pid)
            ;
        
        // [핵심] 매크로를 이용한 안전한 종료 상태 확인
        // WIFEXITED: 정상 종료(exit, return) 되었는지 확인
        if (WIFEXITED(exitstatus)) {
            printf("Child exited with status %d, signal 0\n", WEXITSTATUS(exitstatus));
        } 
        // WIFSIGNALED: 시그널에 의해(강제) 종료되었는지 확인
        else if (WIFSIGNALED(exitstatus)) {
            printf("Child exited with status 0, signal %d\n", WTERMSIG(exitstatus));
        }
    }
}

// 문자열 복사 함수 (이전과 동일)
char *makestring(char *buf) {
    buf[strcspn(buf, "\n")] = '\0';
    char *cp = malloc(strlen(buf) + 1);
    if (cp == NULL) {
        fprintf(stderr, "no memory\n");
        exit(1);
    }
    strcpy(cp, buf);
    return cp;
}