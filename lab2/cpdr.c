#include <stdio.h>      // 표준 입출력 함수 (fprintf, perror, exit) 사용.
#include <unistd.h>     // POSIX 시스템 호출 (read, write, close, chmod) 사용.
#include <fcntl.h>      // 파일 제어 함수 (open, creat) 및 플래그 사용.
#include <stdlib.h>     // 일반 유틸리티 함수 (exit) 사용.
#include <sys/stat.h>   // 파일 상태 정보 (stat) 및 디렉터리 생성 (mkdir)에 필요한 함수와 구조체 (struct stat) 사용.
#include <string.h>     // 문자열 함수 (strcmp, sprintf) 사용.
#include <dirent.h>     // 디렉터리 엔트리 관련 함수 (opendir, readdir, closedir) 사용.

#define BUFFERSIZE 4096 // 파일 복사 시 성능 향상을 위한 버퍼 크기 (4KB).
#define COPYMODE 0644   // 새로 생성할 파일의 기본 접근 권한 정의 (rw-r--r--).

void oops(char* s1, char* s2);  // 오류 처리 함수 선언.
void cpdr(char* source, char* dest); // [재귀 함수] 디렉터리를 복사하고 재귀적으로 내부를 처리하는 함수 선언.
void cpfile(char* source, char* dest); // 단일 파일을 복사하는 함수 선언 (이전 요청의 cpl.c 로직과 유사).

int main(int ac, char* av[]) { // 메인 함수: ac (인수 개수), av (인수 문자열 배열).
    struct stat st;     // 원본 파일/디렉터리의 상태 정보를 저장할 stat 구조체 선언.
    
    if (ac != 3) {      // 인수가 3개(실행파일, source, destination)가 아니면
        fprintf(stderr, "usage: %s source destination\n", *av); // 표준 에러 스트림에 사용법 출력.
        exit(1);        // 오류 상태(1)로 프로그램 종료.
    }
    
    // stat(pathname, statbuf): 경로의 파일 상태 정보를 stat 구조체에 기록합니다.
    if (stat(av[1], &st) == -1) { // 소스 경로(av[1])의 상태를 확인하고, 실패하면
        oops("Cannot stat", av[1]); // 오류 처리 (파일/디렉터리가 없거나 접근 권한 문제 등).
    }
    
    cpdr(av[1], av[2]); // [핵심 호출]: 소스(av[1])와 목적지(av[2])를 cpdr 함수로 전달하여 복사 시작.
    
    return 0;           // 프로그램 성공적으로 종료.
}

/**
 * cpdr: 디렉터리를 생성하고, 내부의 파일과 하위 디렉터리를 재귀적으로 복사합니다.
 * @param source: 복사할 원본 디렉터리 경로.
 * @param dest: 복사될 목적지 경로.
 */
void cpdr(char* source, char* dest) {
    DIR* dir_ptr;               // 디렉터리 스트림 포인터 (opendir의 반환 값).
    struct dirent* direntp;     // 디렉터리 내 엔트리 정보를 담는 포인터.
    struct stat st;             // 목적지 및 현재 처리 중인 엔트리의 상태 정보를 저장할 stat 구조체.
    
    // 목적지 디렉터리 상태 확인 및 생성 로직
    if (stat(dest, &st) == -1) { // stat(dest) 실패 (목적지 디렉터리가 존재하지 않음)
        // mkdir(pathname, mode): 지정된 권한(0755)으로 새 디렉터리를 생성.
        if (mkdir(dest, 0755) == -1) { // 디렉터리 생성에 실패하면
            oops("Cannot create directory", dest); // 오류 처리.
        }
    } else if (!S_ISDIR(st.st_mode)) { // stat 성공 시, 해당 경로가 디렉터리인지 S_ISDIR 매크로로 확인.
        oops("Destination is not a directory", dest); // 디렉터리가 아니면 오류 처리 (파일을 디렉터리로 덮어쓰지 않도록).
    }
    
    // opendir(dirname): 디렉터리 스트림을 열어 DIR 포인터를 반환.
    if ((dir_ptr = opendir(source)) == NULL) { // 소스 디렉터리를 열어 실패하면
        oops("Cannot open directory", source); // 오류 처리.
    }
    
    // readdir(dirp): 디렉터리 스트림에서 다음 엔트리를 읽어 struct dirent 포인터 반환.
    while ((direntp = readdir(dir_ptr)) != NULL) { // 디렉터리에서 엔트리를 하나씩 읽어 NULL이 아닐 동안 반복.
        // strcmp(s1, s2): 문자열 비교 (같으면 0 반환).
        // "."와 ".." 엔트리 건너뛰기: 이들은 현재 디렉터리와 상위 디렉터리를 나타내며 복사 대상이 아닙니다.
        if (strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0) {
            continue; // 루프의 다음 반복으로 이동.
        }
        
        char source_path[1024]; // 원본 파일/디렉터리의 전체 경로를 저장할 버퍼.
        char dest_path[1024];   // 목적지 파일/디렉터리의 전체 경로를 저장할 버퍼.
        
        // sprintf(buf, format, ...): 포맷에 맞게 문자열을 버퍼에 저장 (경로 조합).
        sprintf(source_path, "%s/%s", source, direntp->d_name); // 예: source/file.txt
        sprintf(dest_path, "%s/%s", dest, direntp->d_name);     // 예: dest/file.txt
        
        if (stat(source_path, &st) == -1) { // 조합된 소스 경로의 상태를 다시 확인하여
            oops("Cannot stat", source_path); // 실패 시 오류 처리.
        }
        
        // S_ISDIR(mode): stat 결과가 디렉터리인지 확인하는 매크로.
        if (S_ISDIR(st.st_mode)) {
            cpdr(source_path, dest_path); // [재귀 호출]: 하위 디렉터리이면 cpdr을 **다시 호출**하여 복사.
        } else {
            cpfile(source_path, dest_path); // 디렉터리가 아니면 cpfile을 호출하여 파일 복사.
        }
        
        // chmod(pathname, mode): 파일/디렉터리의 권한을 변경합니다.
        // 복사된 파일/디렉터리에 원본의 권한(st.st_mode)을 적용합니다.
        if (chmod(dest_path, st.st_mode) == -1) {
            oops("Cannot change mode on", dest_path); // 권한 변경 실패 시 오류 처리.
        }
    }
    closedir(dir_ptr); // closedir(dirp): opendir로 연 디렉터리 스트림을 닫고 자원을 해제합니다.
}

/**
 * cpfile: 두 경로 사이에서 단일 파일을 버퍼링하여 복사합니다. (cpl.c 로직)
 * @param source: 원본 파일 경로.
 * @param dest: 목적지 파일 경로.
 */
void cpfile(char* source, char* dest) {
    int in_fd, out_fd, n_chars; // 입력 FD, 출력 FD, 읽은 바이트 수 변수 선언.
    char buf[BUFFERSIZE];       // 복사 작업을 위한 버퍼 선언.
    
    // open(pathname, O_RDONLY): 읽기 전용으로 파일 열기.
    if ((in_fd = open(source, O_RDONLY)) == -1) { 
        oops("Cannot open ", source);
    }
    // creat(pathname, mode): 파일 생성 또는 덮어쓰기.
    if ((out_fd = creat(dest, COPYMODE)) == -1) {
        oops("Cannot creat", dest);
    }
    
    // read/write를 통한 데이터 블록 복사 루프
    while ((n_chars = read(in_fd, buf, BUFFERSIZE)) > 0) { // 파일 끝(> 0)이 아닐 때까지 읽기.
        if (write(out_fd, buf, n_chars) != n_chars) { // 읽은 양만큼 쓰고, 쓴 양이 다르면 오류.
            oops("Write error to ", dest);
        }
    }
    if (n_chars == -1) { // 루프 종료 후 -1이면 읽기 오류 발생.
        oops("Read error from ", source);
    }
    
    // close(fd): 파일 디스크립터 닫기.
    if (close(in_fd) == -1 || close(out_fd) == -1) {
        oops("Error closing files", "");
    }
}

/**
 * oops: 오류 메시지를 출력하고 프로그램을 종료하는 범용 핸들러.
 * @param s1: 사용자 정의 에러 메시지.
 * @param s2: 시스템 오류(errno)를 해석할 경로/파일명.
 */
void oops(char* s1, char* s2) {
    fprintf(stderr, "Error: %s ", s1); // 표준 에러 스트림에 첫 번째 메시지 출력.
    perror(s2); // perror(s): s와 현재 errno에 해당하는 시스템 에러 설명 출력.
    exit(1);    // 오류 상태(1)로 프로그램 종료.
}
