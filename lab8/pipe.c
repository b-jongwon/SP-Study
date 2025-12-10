/* pipe-sim.c */

/* * ======================================================================================
 * [헤더 파일 포함]
 * - stdio.h: 입출력 (fprintf, perror)
 * - stdlib.h: 메모리/프로세스 관리 (exit)
 * - string.h: 문자열 처리 (strtok, strncpy)
 * - unistd.h: [핵심] 유닉스 표준 시스템 콜 (fork, pipe, dup2, execvp, close)
 * - sys/wait.h: 프로세스 대기 (waitpid)
 * ======================================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 16 // 명령어 하나당 최대 인자 개수 (예: ls, -a, -l ... 최대 16개)

/* * [함수: 명령어 파싱]
 * 문자열 "ls -l -a"를 받아서 ["ls", "-l", "-a", NULL] 형태의 배열로 변환합니다.
 * execvp 함수가 이런 형태(char* argv[])를 요구하기 때문입니다.
 */
void parse_command(char *input, char **argv)
{
    int i = 0;
    // [strtok] 문자열을 공백(" ") 기준으로 조각냅니다 (Tokenizing).
    // 주의: strtok은 원본 문자열(input)을 변경합니다 (공백 자리에 \0을 넣음).
    char *token = strtok(input, " ");
    
    while (token != NULL && i < MAX_ARGS - 1)
    {
        argv[i++] = token; // 배열에 토큰 주소 저장
        token = strtok(NULL, " "); // 다음 토큰 찾기
    }
    argv[i] = NULL; // [중요] exec 계열 함수의 인자 배열 끝은 반드시 NULL이어야 함.
}

int main(int argc, char *argv[])
{
    // 인자 개수 체크 (프로그램 이름 + 명령1 + 명령2 = 최소 3개)
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <command1> <command2>\n", argv[0]);
        fprintf(stderr, "Example: %s \"ls -l\" \"sort\"\n", argv[0]);
        exit(1);
    }

    // [문자열 복사]
    // argv[1], argv[2]는 수정 불가능한 영역에 있을 수도 있고, 안전을 위해 로컬 버퍼에 복사합니다.
    // strtok이 이 버퍼의 내용을 수정하며 토큰을 나눕니다.
    char cmd1_buf[256], cmd2_buf[256];
    strncpy(cmd1_buf, argv[1], sizeof(cmd1_buf));
    strncpy(cmd2_buf, argv[2], sizeof(cmd2_buf));

    // 파싱된 인자들을 담을 포인터 배열
    char *cmd1_argv[MAX_ARGS];
    char *cmd2_argv[MAX_ARGS];

    // 명령어를 실행 가능한 형태로 쪼개기
    parse_command(cmd1_buf, cmd1_argv);
    parse_command(cmd2_buf, cmd2_argv);

    /* * ======================================================================================
     * [핵심: 파이프 생성]
     * pipefd[0]: 읽기 전용 (수도꼭지 출구) -> 뒤쪽 명령어(sort)가 입를 댈 곳
     * pipefd[1]: 쓰기 전용 (수도꼭지 입구) -> 앞쪽 명령어(ls)가 데이터를 쏟아부을 곳
     * ======================================================================================
     */
    int pipefd[2];
    if (pipe(pipefd) == -1) // 파이프 생성 실패 시
    {
        perror("pipe");
        exit(1);
    }
    
    

    /* * ======================================================================================
     * [첫 번째 자식 프로세스: Writer / Producer]
     * 예: "ls -l" 역할을 수행. 결과를 화면이 아닌 파이프로 보냄.
     * ======================================================================================
     */
    pid_t pid1 = fork();
    if (pid1 == 0)
    {
        // 1. 읽기 포트 닫기: 나는 쓰기만 할 거니까 읽는 구멍은 필요 없음. (리소스 낭비 방지 & 안전)
        close(pipefd[0]);

        // 2. [핵심] 리다이렉션 (Redirection)
        // dup2(old, new): pipefd[1](파이프 입구)를 STDOUT_FILENO(1번, 모니터 출력) 자리에 복제.
        // 즉, 이제부터 printf 등을 하면 모니터가 아니라 파이프 안으로 데이터가 들어감.
        dup2(pipefd[1], STDOUT_FILENO);

        // 3. 원본 쓰기 포트 닫기: 이미 1번(STDOUT)으로 복제했으니, 원본 파일 디스크립터는 필요 없음.
        close(pipefd[1]);

        // 4. 명령어 실행 (프로세스 교체)
        // execvp: PATH 환경변수에서 cmd1_argv[0]을 찾아 실행.
        // 성공하면 여기서 프로세스가 ls로 변신하므로 아래 코드는 실행되지 않음.
        execvp(cmd1_argv[0], cmd1_argv);
        
        // 여기까지 왔다면 exec 실패 (오타 등)
        perror("execvp cmd1 failed");
        exit(1);
    }

    /* * ======================================================================================
     * [두 번째 자식 프로세스: Reader / Consumer]
     * 예: "sort" 역할을 수행. 키보드가 아닌 파이프에서 데이터를 읽음.
     * ======================================================================================
     */
    pid_t pid2 = fork();
    if (pid2 == 0)
    {
        // 1. 쓰기 포트 닫기: 나는 읽기만 할 거니까 쓰는 구멍은 필요 없음.
        close(pipefd[1]);

        // 2. [핵심] 리다이렉션
        // dup2: pipefd[0](파이프 출구)를 STDIN_FILENO(0번, 키보드 입력) 자리에 복제.
        // 즉, 이제부터 scanf 등을 하면 키보드가 아니라 파이프에서 데이터를 빨아들임.
        dup2(pipefd[0], STDIN_FILENO);

        // 3. 원본 읽기 포트 닫기
        close(pipefd[0]);

        // 4. 명령어 실행
        execvp(cmd2_argv[0], cmd2_argv);
        perror("execvp cmd2 failed");
        exit(1);
    }

    /* * ======================================================================================
     * [부모 프로세스: 정리 및 대기]
     * ★★★ 매우 중요한 부분 ★★★
     * 부모 프로세스는 파이프를 전혀 쓰지 않습니다. 따라서 반드시 닫아야 합니다.
     * 특히, pipefd[1](쓰기)을 닫지 않으면, 데이터를 읽는 자식(pid2)은
     * "어? 아직 누군가(부모)가 파이프에 연결되어 있네? 데이터가 더 올 수도 있겠다"라고 생각해서
     * 영원히 끝나지 않는 EOF(End of File)를 기다리며 멈춰버립니다 (Deadlock).
     * ======================================================================================
     */
    close(pipefd[0]);
    close(pipefd[1]);

    // 자식들이 끝날 때까지 대기 (좀비 프로세스 방지)
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}