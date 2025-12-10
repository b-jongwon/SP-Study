#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define DELAY 5 // 자식 프로세스가 잠들 시간

void child_code(int delay);
void parent_code(int childpid);

int main() {
    int newpid;
    printf("Before: my pid is %d\n", getpid()); // 현재 프로세스 ID 출력

    // fork 실행
    if ((newpid = fork()) == -1) {
        perror("fork failed");
        exit(1);
    } else if (newpid == 0) {
        // 자식 프로세스는 이 함수 실행
        child_code(DELAY);
    } else {
        // 부모 프로세스는 이 함수 실행 (자식 PID 전달)
        parent_code(newpid);
    }
    return 0;
}

// 자식 프로세스가 수행할 코드
void child_code(int delay) {
    printf("Child %d here. Will sleep for %d seconds.\n", getpid(), delay);
    sleep(delay); // 5초간 대기 (부모가 기다리는지 확인하기 위함)
    printf("Child done. About to exit.\n");
    
    // [중요] 종료 코드 17을 반환하며 종료. 
    // 이 17이라는 숫자가 부모에게 어떻게 전달되는지 확인하는 것이 포인트.
    exit(17); 
}

// 부모 프로세스가 수행할 코드
void parent_code(int childpid) {
    int wait_rv;        // wait()의 리턴값 (종료된 자식의 PID)
    int child_status;   // 자식의 종료 상태 정보가 담길 정수 변수
    int high_8, low_7, bit_7;

    // wait(&child_status): 자식이 끝날 때까지 대기하고, 
    // 종료 정보는 child_status 변수 주소에 저장됨
    wait_rv = wait(&child_status);
    printf("Done waiting for %d. wait() returned: %d\n", childpid, wait_rv);

    // [핵심] child_status의 비트 패턴 출력 (총 16비트만 확인)
    printf("Child status (binary): ");
    for (int i = 15; i >= 0; i--) { 
        int mask = 1 << i;
        // 비트 AND 연산으로 해당 자리가 1인지 0인지 확인
        printf("%d", (child_status & mask) ? 1 : 0);
        if (i % 8 == 0) printf(" "); // 8비트마다 공백으로 가독성 확보
    }
    printf("\n");

    /* Unix/Linux에서 wait status 정수(16bit)의 구조:
       [ Exit Code (8bit) ] [ Core Dump Flag (1bit) ] [ Signal Number (7bit) ]
    */

    // 상위 8비트 추출: 자식이 exit()에 넣은 값 (여기선 17)
    high_8 = (child_status >> 8) & 0xFF;   
    
    // 하위 7비트 추출: 자식을 죽인 시그널 번호 (정상 종료시 0)
    low_7 = child_status & 0x7F;           
    
    // 하위 8번째 비트 (0x80 = 1000 0000): 코어 덤프 발생 여부
    bit_7 = (child_status & 0x80) ? 1 : 0; 

    // 최종 분석 결과 출력
    // 정상 종료했다면 exit=17, signal=0 이 나와야 함
    printf("Status: exit=%d, signal=%d, core dumped=%d\n", high_8, low_7, bit_7);
}