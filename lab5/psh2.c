#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h> 

#define MAXARGS 20      // 최대 인자 개수
#define ARGLEN 100      // 인자 하나의 최대 길이

// 함수 프로토타입 선언
void execute(char *arglist[]);
char *makestring(char *buf);

int main() {
    char *arglist[MAXARGS + 1]; // 실행할 명령과 인자들을 저장할 포인터 배열 (마지막은 NULL이어야 함)
    int numargs = 0;            // 현재 입력된 인자의 개수
    char argbuf[ARGLEN];        // 사용자 입력을 임시로 저장할 버퍼
    
    // 무한 루프처럼 돌면서 인자를 계속 받음 (numargs가 꽉 찰 때까지)
    while (numargs < MAXARGS) {
        printf("Arg[%d]? ", numargs); // 프롬프트 출력 예: Arg[0]? 
        
        // 사용자로부터 문자열 입력 받음 (fgets는 엔터값인 \n까지 포함해서 읽음)
        // 입력이 있고, 첫 글자가 엔터(\n)가 아닌 경우 -> 유효한 인자로 간주
        if (fgets(argbuf, ARGLEN, stdin) && *argbuf != '\n') {
            // 입력받은 문자열을 동적 할당하여 arglist에 저장하고 개수 증가
            arglist[numargs++] = makestring(argbuf);
        } else {
            // 엔터만 쳤을 때 (실행 명령 신호)
            if (numargs > 0) { 
                arglist[numargs] = NULL; // execvp는 인자 배열의 끝이 NULL이어야 함
                execute(arglist);        // 명령어 실행 함수 호출
                numargs = 0;             // 실행 후 인자 개수 초기화 (다음 명령 받을 준비)
            }
        }
    }
    return 0;
}

// 자식 프로세스를 생성하여 명령어를 실행하는 함수
void execute(char *arglist[]) {
    pid_t pid;
    int exitstatus;

    pid = fork(); // 1. 프로세스 복제 (부모와 똑같은 자식 프로세스 생성)
    
    // fork 실패 시 (-1 반환)
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    } 
    // 자식 프로세스 코드 (pid == 0)
    else if (pid == 0) {
        // execvp: 현재 프로세스의 이미지를 arglist[0] 프로그램으로 교체
        // 예: arglist가 {"ls", "-l", NULL} 이라면 ls 프로그램 실행
        execvp(arglist[0], arglist);
        
        // execvp가 성공하면 이 밑의 코드는 절대 실행되지 않음.
        // 실행되었다는 것은 execvp가 실패했다는 뜻 (예: 존재하지 않는 명령어)
        perror("execvp failed");
        exit(1);
    } 
    // 부모 프로세스 코드 (pid > 0, 자식의 PID를 반환받음)
    else {
        // 자식 프로세스가 종료될 때까지 기다림 (Blocking)
        while (wait(&exitstatus) != pid)
            ;
        
        // 자식이 종료되면 상태 코드 출력 (비트 연산으로 종료 코드와 시그널 추출)
        // exitstatus >> 8: 상위 8비트가 실제 exit code
        // exitstatus & 0x7F: 하위 7비트가 종료시킨 시그널 번호
        printf("Child exited with status %d, signal %d\n",
               exitstatus >> 8, exitstatus & 0x7F); 
    }
}

// 문자열을 힙(Heap) 메모리에 새로 할당해서 복사하는 함수
// (지역 변수 argbuf는 계속 덮어씌워지므로, 따로 저장해둬야 함)
char *makestring(char *buf) {
    // 문자열 끝의 엔터(\n)를 NULL 문자(\0)로 변경하여 문자열을 완성
    buf[strcspn(buf, "\n")] = '\0'; 
    
    // 문자열 길이만큼 메모리 동적 할당 (+1은 NULL 문자 공간)
    char *cp = malloc(strlen(buf) + 1);
    
    if (cp == NULL) { // 메모리 부족 시
        fprintf(stderr, "no memory\n");
        exit(1);
    }
    
    strcpy(cp, buf); // 할당된 메모리에 문자열 복사
    return cp;       // 주소 반환
}