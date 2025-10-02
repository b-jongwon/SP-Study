/** cpl.c - a first version of cp - uses read and write with tunable buffer size
 *
 *
 * usage: cpl src dest
 *
 **/
#include <stdio.h>      // 표준 입출력 함수 (fprintf, perror, exit)를 사용하기 위해 포함합니다.
#include <unistd.h>     // POSIX 시스템 호출 (read, write, close)을 사용하기 위해 포함합니다.
#include <fcntl.h>      // 파일 제어 함수 (open, creat) 및 플래그 (O_RDONLY)를 사용하기 위해 포함합니다.
#include <stdlib.h>     // 일반 유틸리티 함수 (exit)를 사용하기 위해 포함합니다.
#include <string.h>     // 문자열 함수 (strcmp)를 사용하기 위해 포함합니다.
#define BUFFERSIZE 4096 // 파일 I/O 성능 향상을 위해 설정한 버퍼 크기 (4KB). read/write 시스템 호출 횟수를 줄입니다.
#define COPYMODE 0644   // 새로 생성할 파일의 기본 접근 권한 정의. 8진수: (소유자: rw-, 그룹: r--, 기타: r--).

void oops(char *, char *); // 오류 발생 시 메시지를 출력하고 프로그램을 종료하는 사용자 정의 함수 선언.

int main(int ac, char *av[]) // 메인 함수: ac (인수 개수), av (인수 문자열 배열).
{
    int in_fd, out_fd, n_chars;   // 입력 FD, 출력 FD, read/write된 바이트 수를 저장할 변수 선언.
    char buf[BUFFERSIZE];         // read()로 읽은 데이터를 임시로 저장할 4KB 크기의 버퍼 배열.

    /* check args */
    if (ac != 3) {                                              // 프로그램 실행 시 인수가 3개(실행파일, src, dest)가 아니면
        fprintf(stderr, "usage: %s source destination\n", av[0]); // 표준 에러 스트림(stderr)에 올바른 사용법 출력.
        exit(1);                                                // 오류 상태(1)로 프로그램 종료.
    }
      if (strcmp(av[1],av[2])==0) // 소스 파일 경로(av[1])와 목적지 파일 경로(av[2])가 문자열로 동일한지 비교 (strcmp 결과가 0이면 같음).
      {
          fprintf(stderr,"cp: '%s' and '%s' are the same file\n",av[1],av[2]); // 두 경로가 같으면 충돌 메시지를 출력합니다.
          exit(1);                                              // 오류 상태(1)로 프로그램 종료.
      }
    
    /* open files */
    // open(pathname, flags): 파일을 열어 파일 디스크립터(FD)를 획득합니다.
    if ((in_fd = open(av[1], O_RDONLY)) == -1) // 소스 파일(av[1])을 **읽기 전용** (O_RDONLY)으로 열고, FD 획득에 실패(-1)하면
        oops("Cannot open ", av[1]);         // oops 함수를 호출하여 오류 처리 (파일이 없거나 권한 문제 등).

    // creat(pathname, mode): 새 파일을 생성하거나 이미 존재하면 내용을 지우고(truncate) 쓰기 전용으로 엽니다.
    if ((out_fd = creat(av[2], COPYMODE)) == -1) // 목적 파일(av[2])을 생성하고, 권한(0644)을 부여하며 FD 획득에 실패하면
        oops("Cannot creat", av[2]);         // oops 함수를 호출하여 오류 처리.

    /* copy files */
    // read(fd, buf, count): FD에서 count 바이트만큼 읽어 buf에 저장하고, 읽은 바이트 수를 반환.
    while ((n_chars = read(in_fd, buf, BUFFERSIZE)) > 0) // in_fd에서 최대 4096바이트를 읽어 n_chars에 저장하고, n_chars가 0보다 크면 (EOF가 아니면)
        // write(fd, buf, count): buf에 있는 count 바이트만큼 FD에 씁니다.
        if (write(out_fd, buf, n_chars) != n_chars)   // out_fd에 읽은 n_chars만큼 쓰고, **실제로 쓴 바이트 수**가 다르면 (쓰기 오류)
            oops("Write error to ", av[2]);           // 쓰기 오류를 처리하고 프로그램 종료.
    if (n_chars == -1)                              // while 루프를 빠져나왔을 때, 0 (EOF)이 아닌 -1이면 (read 시스템 호출 오류)
        oops("Read error from ", av[1]);            // 읽기 오류를 처리하고 프로그램 종료.

    /* close files */
    // close(fd): 파일 디스크립터(FD)를 닫고 커널 자원을 해제합니다. 실패 시 -1 반환.
    if (close(in_fd) == -1 || close(out_fd) == -1) // 입력 FD와 출력 FD를 닫고, 둘 중 하나라도 실패하면
        oops("Error closing files", "");         // 파일 닫기 오류를 처리합니다.

    return 0; // 프로그램 성공적으로 종료 (운영체제에 종료 상태 0 반환).
}

void oops(char *s1, char *s2) // 오류 처리 함수 정의
{
    fprintf(stderr, "Error: %s ", s1); // 표준 에러 스트림에 첫 번째 사용자 메시지 출력.
    perror(s2);                        // 전역 변수 errno에 담긴 **시스템 오류 원인**을 s2와 함께 출력 (예: No such file or directory).
    exit(1);                           // 오류 상태(1)로 프로그램 종료.
}
