#include <stdio.h>      // 표준 입출력 함수 (fprintf, snprintf, perror, exit) 사용을 위해 포함.
#include <stdlib.h>     // 일반 유틸리티 함수 (exit, EXIT_FAILURE) 사용을 위해 포함.
#include <dirent.h>     // 디렉터리 스트림 (DIR, struct dirent) 함수 (opendir, readdir) 사용을 위해 포함.
#include <unistd.h>     // POSIX 시스템 호출 (write, close) 사용을 위해 포함.
#include <fcntl.h>      // 파일 제어 함수 (open) 및 플래그 (O_WRONLY) 사용을 위해 포함.
#include <string.h>     // 문자열 함수 (strcmp) 사용을 위해 포함.

#define PTS_PATH "/dev/pts/"                  // 가상 터미널 장치 파일들이 위치한 경로 정의.
#define MAX_PATH 266                          // 파일 경로를 저장할 버퍼의 최대 크기 정의.
#define MAX_FORMATTED_MESSAGE_LENGTH 1024     // 포맷된 메시지를 저장할 버퍼의 최대 크기 정의.

int main(int argc, char *argv[])              // 메인 함수: argc (인수 개수), argv (인수 문자열 배열).
{
    if(argc < 2)                              // 인수가 2개 미만인 경우 (프로그램 이름 + 메시지)
    {
        // 표준 에러 스트림에 올바른 사용법을 출력하고 프로그램 종료.
        fprintf(stderr , "Usage: %s <message>\n",argv[0]);
        exit(EXIT_FAILURE);                   // 실패 상태를 나타내는 매크로로 종료 (일반적으로 1).
    }

    char *message = argv[1];                  // 사용자가 명령줄 인수로 제공한 메시지 문자열 포인터.
    char formatted_message[MAX_FORMATTED_MESSAGE_LENGTH]; // 브로드캐스트용 메시지를 포맷하여 저장할 버퍼.

    
    // snprintf: 버퍼 오버플로우를 방지하며 메시지를 포맷하고, 문자열 길이를 반환.
    int len = snprintf(formatted_message, MAX_FORMATTED_MESSAGE_LENGTH, 
                       "[Broadcast] %s\n", message); // [Broadcast] 접두사와 줄바꿈 문자를 추가하여 메시지 포맷.
    
    // snprintf의 반환 값(len)을 size_t 타입으로 변환하여 write 함수에 전달할 준비.
    size_t message_len = (size_t)len;

    
    DIR *dirp;                                // opendir()의 반환 값, 디렉터리 스트림 포인터.
    struct dirent *dp;                        // readdir()의 반환 값, 디렉터리 엔트리 정보 포인터.

    // opendir(pathname): PTS_PATH 경로를 열고 디렉터리 스트림 포인터를 얻음.
    dirp = opendir(PTS_PATH);
    if (dirp == NULL) {                       // 디렉터리 열기 실패 시 (예: 권한 문제)
        
        perror("Error opening /dev/pts");      // 시스템 에러 메시지(errno) 출력.
        exit(EXIT_FAILURE);                   // 프로그램 실패 종료.
    }

    // readdir(dirp): 디렉터리 스트림에서 다음 엔트리를 읽어 NULL이 아닐 때까지 반복.
    while ((dp = readdir(dirp)) != NULL) {
        char full_path[MAX_PATH];             // 장치 파일의 전체 경로를 저장할 버퍼.
        int fd;                               // 장치 파일을 열 때 사용할 파일 디스크립터.
        
        
        // strcmp(s1, s2): 문자열 비교 (같으면 0 반환).
        // "."와 ".." 엔트리 건너뛰기: 이들은 실제 터미널 장치 파일이 아님.
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;                         // 루프의 다음 반복으로 이동.
        }

        
        // d_name[0] < '0' || d_name[0] > '9': 파일 이름의 첫 글자가 숫자인지 확인 (PTS 장치 파일 이름은 보통 숫자임).
        // 다른 특수 파일(예: tty, ptmx)을 걸러내기 위한 휴리스틱 필터링.
        if (dp->d_name[0] < '0' || dp->d_name[0] > '9') {
             continue;                        // 숫자로 시작하지 않으면 다음 엔트리로 이동.
        }

      
        // snprintf(buf, size, format, ...): 전체 장치 파일 경로를 조합하여 full_path에 저장.
        snprintf(full_path, MAX_PATH, "%s%s", PTS_PATH, dp->d_name); // 예: /dev/pts/0, /dev/pts/1

      
        // open(pathname, O_WRONLY): 장치 파일을 쓰기 전용(O_WRONLY)으로 엽니다.
        // 장치 파일에 쓰는 행위는 해당 장치(터미널)에 출력하는 것을 의미합니다.
        fd = open(full_path, O_WRONLY);

      
        if (fd < 0) {                         // open 실패 시 (예: 터미널이 닫혔거나 쓰기 권한이 없는 경우)
            continue;                         // 해당 터미널에 대한 쓰기를 건너뛰고 다음 엔트리로 이동.
        }

      
        // write(fd, buf, count): 포맷된 메시지를 해당 파일 디스크립터(터미널)에 씁니다.
        // 터미널에 메시지가 즉시 출력되는 결과를 가져옵니다.
        write(fd, formatted_message, message_len);

      
        // close(fd): 쓰기 작업을 마친 후 파일 디스크립터를 닫고 자원을 해제.
        close(fd);
    }

    closedir(dirp);                           // 모든 순회가 끝난 후 디렉터리 스트림을 닫음.

    return 0;                                 // 프로그램 성공적으로 종료.
}
