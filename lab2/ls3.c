#include <stdio.h>      // 표준 입출력 함수 (printf, fprintf, perror) 사용을 위해 포함.
#include <sys/types.h>  // 시스템 데이터 타입 (uid_t, gid_t 등) 정의를 위해 포함.
#include <dirent.h>     // 디렉터리 스트림 (DIR, struct dirent) 함수 사용을 위해 포함.
#include <sys/stat.h>   // 파일 상태 정보 (stat, struct stat) 및 모드 관련 매크로 사용을 위해 포함.
#include <string.h>     // 문자열 함수 (strcmp, strcpy) 사용을 위해 포함.
#include <pwd.h>        // 사용자 정보 (struct passwd, getpwuid) 함수 사용을 위해 포함.
#include <grp.h>        // 그룹 정보 (struct group, getgrgid) 함수 사용을 위해 포함.

void do_ls(char[]);             // 지정된 디렉터리의 엔트리를 순회하는 함수 선언.
void dostat(char *);            // 파일 경로를 받아 stat 정보를 가져오는 함수 선언.
void show_file_info(char *, struct stat *); // stat 구조체 정보를 포맷하여 출력하는 함수 선언.
void mode_to_letters(int, char []); // 파일 모드(정수)를 문자열 권한(drwxr-xr-x)으로 변환하는 함수 선언.
char *uid_to_name(uid_t);       // 사용자 ID(UID)를 사용자 이름으로 변환하는 함수 선언.
char *gid_to_name(gid_t );      // 그룹 ID(GID)를 그룹 이름으로 변환하는 함수 선언.

int main(int ac, char *av[]) // 메인 함수: ac (인수 개수), av (인수 문자열 배열).
{
    if (ac == 1) {          // 인수가 1개(실행 파일 이름만)인 경우
        do_ls(".");         // 현재 디렉터리(.)를 대상으로 do_ls 함수 호출.
    } else {                // 인수가 여러 개인 경우 (예: ls dir1 dir2)
        while (--ac) {      // 인수 개수가 0이 될 때까지 반복 (--ac는 반복 전에 ac를 감소시킴).
            printf("%s:\n", *++av); // *++av: 포인터를 다음 인수로 이동하고, 디렉터리 이름을 출력.
            do_ls(*av);     // 해당 디렉터리를 대상으로 do_ls 함수 호출.
        }
    }
    return 0;               // 프로그램 성공적으로 종료.
}

/**
 * do_ls: 디렉터리를 열고 내부의 모든 엔트리를 순회하며 상세 정보를 출력합니다.
 * @param dirname: 순회할 디렉터리 경로.
 */
void do_ls(char dirname[])
{
    DIR *dir_ptr;           // opendir()의 반환 값 (디렉터리 스트림 포인터).
    struct dirent *direntp; // readdir()의 반환 값 (디렉터리 엔트리 정보).

    // opendir(dirname): 디렉터리 스트림을 엽니다. 실패 시 NULL 반환.
    if ((dir_ptr = opendir(dirname)) == NULL) {
        // fprintf(stderr, ...): 디렉터리를 열 수 없을 때 에러 메시지를 표준 에러 스트림에 출력.
        fprintf(stderr, "ls1: cannot open %s\n", dirname);
    } else {
        // readdir(dir_ptr): 디렉터리에서 다음 엔트리를 읽어 NULL이 아닐 때까지 반복.
        while ((direntp = readdir(dir_ptr)) != NULL) {
            char path[1024]; // 파일의 전체 경로를 저장할 버퍼.
            // sprintf(buf, format, ...): 디렉터리 이름과 엔트리 이름을 합쳐 완전한 경로를 생성.
            sprintf(path, "%s/%s", dirname, direntp->d_name);
            dostat(path);   // 완성된 경로를 dostat 함수에 전달하여 파일 상태 정보 요청.
        }
        closedir(dir_ptr);  // closedir(dir_ptr): 디렉터리 스트림을 닫고 자원을 해제.
    }
}

/**
 * dostat: 파일 경로를 받아 stat 시스템 호출을 실행하고 결과를 show_file_info로 전달합니다.
 * @param filename: 파일의 전체 경로.
 */
void dostat(char *filename)
{
    struct stat info; // 파일 메타데이터를 저장할 stat 구조체 선언.

    // stat(pathname, statbuf): 파일의 상태 정보를 info 구조체에 기록합니다. 실패 시 -1 반환.
    if (stat(filename, &info) == -1) {
        // stat 실패 시 (예: 권한 문제, 파일 삭제 등), 시스템 에러 메시지 출력.
        perror(filename);
    } else {
        // stat 성공 시, 파일 이름과 stat 정보를 출력 함수에 전달.
        show_file_info(filename, &info);
    }
}

/**
 * show_file_info: stat 구조체에서 필요한 필드들을 추출하여 ls -l 형식으로 출력합니다.
 * @param filename: 파일 이름 (출력의 마지막 열).
 * @param info_p: stat 구조체 포인터 (파일 메타데이터).
 */
void show_file_info(char *filename, struct stat *info_p)
{
    // 외부 함수들의 선언 (ctime은 time.h에 정의되어 있지만, 명시적으로 선언하는 스타일).
    char *uid_to_name() , *ctime() , *gid_to_name(),*filemode();
    void mode_to_letters();

    char modestr[11]; // 파일 모드(권한)를 담을 문자열 (예: drwxr-xr-x\0).
    // mode_to_letters: stat 모드 정수(info_p->st_mode)를 문자열(modestr)로 변환.
    mode_to_letters(info_p->st_mode, modestr);

    printf("%s", modestr); // [1] 파일 권한 문자열 출력.

    // st_nlink: 하드 링크 수 출력 (%4d는 4칸 너비로 우측 정렬).
    printf("%4d ", (int)info_p->st_nlink);

    // st_uid: UID를 이름으로 변환하여 출력 (%-8s는 8칸 너비로 좌측 정렬).
    printf("%-8s ", uid_to_name(info_p->st_uid));

    // st_gid: GID를 그룹 이름으로 변환하여 출력.
    printf("%-8s ", gid_to_name(info_p->st_gid));

    // st_size: 파일 크기 출력 (%8ld는 8칸 너비로 우측 정렬, long 타입 캐스팅 필요).
    printf("%8ld ", (long)info_p->st_size);

    // ctime(&st_mtime): 최종 수정 시간(time_t)을 "Wed Jun 30 21:49:08 1993\n" 문자열로 변환.
    // 4 + ctime(...): 문자열 포인터를 4칸 이동시켜 "Wed " 부분을 건너뛰고 월/일/시간만 출력.
    // %.12s: 최대 12문자만 출력하도록 지정.
    printf("%.12s ", 4 + ctime(&info_p->st_mtime));

    printf("%s\n", filename); // [7] 파일 이름 출력 후 줄 바꿈.
}

/**
 * mode_to_letters: 파일 모드 비트 필드를 rwxrwxrwx 형식의 문자열로 변환합니다.
 * @param mode: stat 구조체에서 추출한 st_mode 정수 값.
 * @param str: 변환된 권한 문자열을 저장할 배열 포인터.
 */
void mode_to_letters(int mode, char str[])
{
    strcpy(str, "----------"); // 기본값으로 10개의 '-' 문자로 초기화 (10번째는 널 문자).

    // 파일 유형 확인 매크로 (stat.h에 정의됨)
    if (S_ISDIR(mode)) str[0] = 'd'; // 디렉터리
    if (S_ISCHR(mode)) str[0] = 'c'; // 문자 특수 파일 (Character device)
    if (S_ISBLK(mode)) str[0] = 'b'; // 블록 특수 파일 (Block device)

    // 파일 권한 비트 마스크 확인 (비트 AND 연산)
    // 소유자 (User) 권한
    if (mode & S_IRUSR) str[1] = 'r'; // 읽기(Read) 권한 비트가 설정되어 있으면 'r'
    if (mode & S_IWUSR) str[2] = 'w'; // 쓰기(Write) 권한 비트가 설정되어 있으면 'w'
    if (mode & S_IXUSR) str[3] = 'x'; // 실행(Execute) 권한 비트가 설정되어 있으면 'x'

    // 그룹 (Group) 권한
    if (mode & S_IRGRP) str[4] = 'r';
    if (mode & S_IWGRP) str[5] = 'w';
    if (mode & S_IXGRP) str[6] = 'x';

    // 기타 사용자 (Other) 권한
    if (mode & S_IROTH) str[7] = 'r';
    if (mode & S_IWOTH) str[8] = 'w';
    if (mode & S_IXOTH) str[9] = 'x';
}

/**
 * uid_to_name: 사용자 ID(UID)를 사용자 이름 문자열로 변환합니다.
 * @param uid: 파일 소유자의 UID.
 * @return: 사용자 이름 문자열 포인터 또는 변환 실패 시 UID 정수 문자열.
 */
char *uid_to_name(uid_t uid)
{
    struct passwd *getpwuid(), *pw_ptr; // getpwuid 함수 선언 및 passwd 구조체 포인터 선언.
    static char numstr[10];             // 정적 버퍼: getpwuid 실패 시 UID 숫자를 문자열로 저장할 공간.

    // getpwuid(uid): UID에 해당하는 사용자 정보를 /etc/passwd 파일에서 찾아 struct passwd*를 반환.
    if ((pw_ptr = getpwuid(uid)) == NULL) {
        // 실패 시: UID 정수 값을 문자열로 변환하여 반환.
        sprintf(numstr, "%d", uid);
        return numstr;
    } else {
        // 성공 시: passwd 구조체의 사용자 이름 필드(pw_name)를 반환.
        return pw_ptr->pw_name;
    }
}

/**
 * gid_to_name: 그룹 ID(GID)를 그룹 이름 문자열로 변환합니다.
 * @param gid: 파일 소유 그룹의 GID.
 * @return: 그룹 이름 문자열 포인터 또는 변환 실패 시 GID 정수 문자열.
 */
char *gid_to_name(gid_t gid)
{
    struct group *getgrgid(), *grp_ptr; // getgrgid 함수 선언 및 group 구조체 포인터 선언.
    static char numstr[10];             // 정적 버퍼: getgrgid 실패 시 GID 숫자를 문자열로 저장할 공간.

    // getgrgid(gid): GID에 해당하는 그룹 정보를 /etc/group 파일에서 찾아 struct group*를 반환.
    if ((grp_ptr = getgrgid(gid)) == NULL) {
        // 실패 시: GID 정수 값을 문자열로 변환하여 반환.
        sprintf(numstr, "%d", gid);
        return numstr;
    } else {
        // 성공 시: group 구조체의 그룹 이름 필드(gr_name)를 반환.
        return grp_ptr->gr_name;
    }
}
