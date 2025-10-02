#include <stdio.h>          // 표준 입출력 함수 사용을 위해 포함합니다.
#include <unistd.h>         // POSIX 시스템 호출 (read, close)을 사용하기 위해 포함합니다.
#include <fcntl.h>          // 파일 제어 함수 (open)와 플래그 (O_RDONLY)를 사용하기 위해 포함합니다.
#include <sys/types.h>      // 시스템 데이터 타입 (예: size_t, pid_t) 정의를 위해 포함합니다.
#include <utmp.h>           // utmp 레코드 구조체 (struct utmp) 정의를 위해 포함합니다.

#define NRECS 16            // 한 번에 버퍼로 읽어올 utmp 레코드의 개수를 정의합니다. (버퍼링 단위)
#define NULLUT ((struct utmp *) NULL) // utmp 포인터가 NULL임을 명확하게 정의하는 매크로.
#define UTSIZE (sizeof(struct utmp))  // utmp 구조체 하나의 크기를 정의합니다.

static char utmpbuf[NRECS * UTSIZE];    // [정적 버퍼]: utmp 레코드 16개 크기만큼의 데이터를 저장할 버퍼 공간 (4096 / 16 = 256 bytes per struct).
static int num_recs;                    // [버퍼 메타데이터]: 현재 버퍼(utmpbuf)에 저장된 유효한 레코드의 총 개수.
static int cur_rec;                     // [버퍼 포인터]: 다음에 사용자에게 반환해야 할 레코드의 버퍼 내 인덱스.
static int fd_utmp = -1;                // [파일 디스크립터]: 열려 있는 utmp 파일의 FD. -1은 닫혀 있거나 오류 상태를 의미.

/**
 * utmp_open: utmp 파일을 열고 초기화합니다.
 * @param filename: 열고자 하는 utmp 파일의 경로 (일반적으로 "/var/run/utmp").
 * @return: 파일 디스크립터 (성공 시 0 이상), 또는 -1 (실패 시).
 */
int utmp_open(char *filename)
{
    // open(pathname, flags): 파일을 읽기 전용(O_RDONLY)으로 열고 FD를 얻습니다.
    fd_utmp = open(filename, O_RDONLY); /* utmp 파일을 읽기 전용으로 열어 FD를 fd_utmp에 저장합니다. */
    cur_rec = num_recs = 0;             /* 레코드 관련 메타데이터를 0으로 초기화합니다. */
    return fd_utmp;                     /* 획득한 FD를 반환합니다 (실패 시 -1). */
}

/**
 * utmp_close: 열려 있는 utmp 파일을 닫습니다.
 */
void utmp_close()
{
    if (fd_utmp != -1)                  /* FD가 유효한 값(-1이 아님)인지 확인하여 */
        close(fd_utmp);                 /* close(fd): 파일 디스크립터를 닫고 자원을 해제합니다. */
}

/**
 * utmp_reload: utmp 파일에서 다음 레코드 블록을 버퍼로 읽어옵니다. (핵심 버퍼링 로직)
 * @return: 새로 버퍼에 로드된 레코드의 개수.
 */
int utmp_reload()
{
    int amt_read; // 실제로 read 시스템 호출로 읽은 총 바이트 수를 저장할 변수.

    // read(fd, buf, count): FD에서 count 바이트만큼 읽어 buf에 저장하고, 읽은 바이트 수를 반환합니다.
    amt_read = read(fd_utmp, utmpbuf, NRECS * UTSIZE); // utmp 파일에서 (16 * UTSIZE) 바이트를 버퍼로 읽어옵니다.

    // 읽은 총 바이트를 UTSIZE로 나누어 버퍼에 들어있는 유효한 레코드의 개수를 계산합니다.
    num_recs = amt_read/UTSIZE;

    // 버퍼의 시작점부터 다시 읽기 시작하도록 현재 레코드 인덱스를 0으로 재설정합니다.
    cur_rec = 0;

    return num_recs; // 새로 로드된 레코드 개수를 반환합니다 (0이면 파일의 끝(EOF)에 도달).
}

/**
 * utmp_next: 버퍼에서 다음 utmp 레코드를 가져옵니다.
 * @return: struct utmp 레코드에 대한 포인터, 파일 끝이나 오류 시 NULLUT.
 */
struct utmp *utmp_next()
{
    struct utmp *recp; // 반환할 utmp 레코드 포인터.

    if (fd_utmp == -1)              /* FD가 -1이면 (파일이 열리지 않았거나 닫힘) */
        return NULLUT;              /* 오류로 간주하고 NULL을 반환합니다. */

    // [버퍼 소진 검사]: 현재 버퍼의 모든 레코드를 다 썼고 (cur_rec == num_recs),
    // && utmp_reload()를 호출했지만 새로 로드된 레코드가 0개이면 (EOF 도달)
    if (cur_rec == num_recs && utmp_reload() == 0)
        return NULLUT;              /* 파일 끝에 도달했으므로 NULL을 반환합니다. */

    // utmpbuf[cur_rec * UTSIZE]는 버퍼 내 cur_rec 번째 레코드가 시작하는 메모리 주소입니다.
    // 이를 struct utmp* 타입으로 캐스팅하여 포인터 recp에 저장합니다.
    recp = (struct utmp *) &utmpbuf[cur_rec * UTSIZE];

    cur_rec++;                      // 다음 호출을 위해 현재 레코드 인덱스를 증가시킵니다.
    return recp;                    // 레코드 포인터를 반환합니다.
}
