#include <stdio.h>      // 표준 입출력 함수 (printf, perror, exit)를 사용하기 위해 포함.
#include <unistd.h>     // POSIX 함수 (read, close 등)를 사용하기 위해 포함.
#include <sys/types.h>  // 시스템 데이터 타입 정의 (utmp_open/close/next의 외부 함수 사용을 가정).
#include <utmp.h>       // utmp 레코드 구조체, 상수 (UTMP_FILE, USER_PROCESS)를 사용하기 위해 포함.
#include <fcntl.h>      // 파일 제어 함수 (utmp_open 내부에서 사용됨)를 위한 정의 포함.
#include <time.h>       // 시간 관련 함수 (time, ctime)를 사용하기 위해 포함.
#include <stdlib.h>     // 일반 유틸리티 함수 (exit)를 사용하기 위해 포함.

#define SHOWHOST        // 호스트 이름 출력을 활성화하기 위한 전처리기 매크로 정의 (컴파일 시 조건부 컴파일에 사용).

int utmp_open(char *);      // utmp 파일을 여는 함수 (외부/다른 파일에 정의됨) 선언.
void utmp_close();          // utmp 파일을 닫는 함수 선언.
void show_info(struct utmp *); // utmp 레코드 정보를 출력하는 함수 선언.
void showtime(time_t);      // 시간을 보기 좋은 형식으로 변환하여 출력하는 함수 선언.

int main()
{
    struct utmp *utbufp     /* holds pointer to next rec     */ // [버퍼 포인터]: utmp_next()가 반환하는 다음 레코드의 주소를 저장할 포인터.
        , *utmp_next();     /* returns pointer to next       */ // utmp_next 함수를 선언하여 포인터 형태로 레코드를 반환함을 명시.

    // utmp_open(filename): utmp 파일을 열고 파일 디스크립터(FD)를 초기화합니다.
    if ( utmp_open (UTMP_FILE) == -1){  // 시스템 상수 UTMP_FILE (예: "/var/run/utmp")을 열고, 실패하면 (-1 반환)
        perror (UTMP_FILE);             // 실패한 이유(errno)와 함께 에러 메시지 출력.
        exit(1);                        // 오류 상태(1)로 프로그램 종료.
    }

    // utmp_next(): 버퍼링된 utmp 레코드 중 다음 레코드의 포인터를 반환합니다.
    while ((utbufp = utmp_next()) != ((struct utmp*) NULL)) // utmp_next()가 NULL이 아닌 유효한 레코드를 반환하는 동안 반복.  () 는 타입캐스팅.
        show_info(utbufp);                                  // 획득한 레코드 포인터를 show_info 함수에 전달하여 정보 출력.

    utmp_close();                       // 모든 작업이 끝난 후 utmp 파일 디스크립터를 닫고 자원을 해제합니다.
    return 0;                           // 프로그램 성공적으로 종료.
}

/**
 * show_info: utmp 레코드를 검사하고 유효한 정보를 표준 출력에 출력합니다.
 * @param utbufp: 출력할 utmp 레코드 구조체에 대한 포인터.
 */
void show_info(struct utmp *utbufp)
{
    // ut_type: 레코드의 유형 (로그인, 사용자 프로세스, 부팅 등)을 나타내는 필드.
    if (utbufp->ut_type != USER_PROCESS ) // 현재 레코드가 **실제 사용자 로그인 세션**이 아니면
        return;                         // 즉시 함수를 종료하고 다음 레코드로 넘어갑니다. (필터링)

    // printf("%-8.8s", str): 최대 8문자까지 왼쪽 정렬로 문자열을 출력합니다.
    printf("%-8.8s", utbufp->ut_name);  // ut_name (사용자 이름)을 8문자 고정 폭으로 출력.
    printf(" ");
    printf("%-8.8s", utbufp->ut_line);  // ut_line (터미널 장치, 예: tty1, pts/0)을 8문자 고정 폭으로 출력.
    printf(" ");
    showtime(utbufp->ut_time);          // ut_time (로그인 시간, time_t 형식)을 showtime 함수로 전달하여 출력.

// #ifdef: 컴파일 시 SHOWHOST 매크로가 정의되어 있을 때만 아래 코드를 포함시킵니다.
#ifdef SHOWHOST
    if (utbufp->ut_host[0] != '\0')     // ut_host (접속한 원격 호스트 이름)의 첫 글자가 널 문자가 아니면 (원격 접속인 경우)
        printf(" (%s)", utbufp->ut_host); // 호스트 이름을 괄호와 함께 출력합니다.
#endif
    printf("\n");                       // 레코드 출력 후 줄 바꿈.
}

/**
 * showtime: time_t 형식의 시간을 읽기 쉬운 형식으로 변환하여 출력합니다.
 * @param timeval: time_t 형식의 시간 값 (초 단위).
 */
void showtime(time_t timeval)
{
    char *ctime();                      // time_t 값을 문자열로 변환하는 표준 C 라이브러리 함수 ctime() 선언.
    char    *cp;                        // ctime()의 반환 값 (시간 문자열)을 저장할 포인터.

    // ctime(time_t *): timeval이 가리키는 time_t 값을 "Wed Jun 30 21:49:08 1993\n" 같은 문자열로 변환하여 포인터 반환.
    cp = ctime(&timeval);               // timeval 주소를 ctime에 전달하여 시간 문자열 포인터를 얻습니다.

    // "%12.12s": 12문자 고정 폭으로 출력하고, 최대 12문자까지만 출력합니다.
    // cp+4: ctime 결과 문자열에서 "Day " 부분 (예: "Wed ")을 건너뛰고 월/일/시간만 출력하기 위해 포인터를 4칸 이동시킵니다.
    printf("%12.12s", cp+4);
}
