#include <stdio.h>      // 표준 입출력 (printf, fprintf, fflush, getchar) 사용을 위해 포함.
#include <stdlib.h>     // 일반 유틸리티 (exit, EXIT_FAILURE) 사용을 위해 포함.
#include <string.h>     // 문자열 함수 (strlen) 사용을 위해 포함.
#include <sys/time.h>   // [핵심] 시간 측정을 위한 struct timeval 및 gettimeofday 사용을 위해 포함.
#include <unistd.h>     // POSIX 시스템 호출 (tcgetattr, tcsetattr, STDIN_FILENO) 사용을 위해 포함.
#include <termios.h>    // [핵심] 터미널 속성 제어를 위한 struct termios 사용을 위해 포함.

#define MAX_INPUT 1024                      // 최대 입력 문자열 길이를 정의.
#define RED  "\033[31m"                    // ANSI 이스케이프 코드: 텍스트를 빨간색으로 변경.
#define RESET "\033[0m"                     // ANSI 이스케이프 코드: 텍스트 색상을 기본값으로 복원.

int main(int argc, char *argv[]) {          // 메인 함수: 프로그램 인수 처리.
    if (argc != 2) {                        // 인수가 2개(실행 파일 + 목표 문장)가 아닌 경우
        // 표준 에러에 올바른 사용법을 출력하고 종료.
        fprintf(stderr, "Usage: %s \"target sentence\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *target = argv[1];           // 목표 문장(인수)을 가리키는 포인터.
    int target_len = strlen(target);        // 목표 문장의 길이 계산.
    char input[MAX_INPUT];                  // 사용자가 입력한 문자열을 저장할 버퍼.
    int index = 0;                          // 현재 입력 위치 인덱스.
    char ch;                                // getchar()로 읽은 단일 문자.
    struct timeval start, end;              // 시간 측정을 위한 구조체 변수 (초와 마이크로초 단위).
    struct termios old_attr, new_attr;      // 터미널 속성 제어를 위한 구조체.

    
    // 사용자에게 목표 문장을 출력하고, 입력을 시작하라는 메시지 출력.
    printf("Type the following sentence: \n%s\n", target);
    printf("\nStart typing: ");
    fflush(stdout);                         // 메시지가 즉시 화면에 표시되도록 출력 버퍼 비움.

    
    // tcgetattr(fd, termios_p): 표준 입력(STDIN)의 현재 터미널 속성을 old_attr에 백업.
    tcgetattr(STDIN_FILENO, &old_attr);
    new_attr = old_attr;                    // new_attr에 백업된 속성을 복사하여 변경 준비.

    
    // new_attr.c_lflag &= ~(ICANON | ECHO): 로컬 모드 플래그 변경.
    // ICANON (Canonical Mode): 비활성화 -> 입력 버퍼링 제거, 문자 즉시 전달.
    // ECHO (Echo): 비활성화 -> 키 입력이 자동으로 화면에 표시되는 것을 방지.
    new_attr.c_lflag &= ~(ICANON | ECHO);
    
    
    // tcsetattr(fd, TCSANOW, termios_p): 변경된 속성(ICANON, ECHO 비활성화)을 터미널에 즉시 적용.
    tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);


    
    // gettimeofday(tv, tz): 현재 시간을 start 구조체에 기록 (마이크로초 정밀도).
    // tz는 보통 NULL로 설정됨. 복사 시작 시간 기록.
    gettimeofday(&start, NULL);
    
    
    // 입력 루프: 입력 위치가 목표 문장 길이보다 짧고, 버퍼 한도에 도달하지 않을 때까지 반복.
    while (index < target_len && index < MAX_INPUT - 1) {
        ch = getchar();                     // getchar(): 버퍼링 없이 단일 문자 입력 대기 및 읽기.

        if (ch == '\n' || ch == '\r') {     // Enter 키 또는 Carriage Return 입력 시
            break;                          // 루프 종료 (타이핑 테스트 종료).
        }

        
        // 백스페이스 (ASCII 127) 또는 Delete 키 (ASCII 8) 처리.
        if (ch == 127 || ch == 8) {
            if (index > 0) {                // 입력된 문자가 있을 경우에만
                index--;                    // 입력 위치 인덱스 감소.
                printf("\b \b");            // 콘솔에서 문자 삭제 효과를 수동으로 구현.
                fflush(stdout);             // 삭제 내용 즉시 반영.
            }
            continue;                       // 삭제는 입력으로 간주하지 않고 다음 입력 대기.
        }

        
        input[index] = ch;                  // 입력된 문자를 입력 버퍼에 저장.

        
        // 입력된 문자(ch)와 목표 문장의 해당 위치 문자(target[index])를 비교.
        if (ch == target[index]) {
            
            printf("%c", ch);               // 일치하면 해당 문자를 그대로 출력.
        } else {
            
            // 불일치하면 ANSI 코드를 사용하여 텍스트를 빨간색으로 출력 후 색상 초기화.
            printf(RED "%c" RESET, ch);
        }
        
        fflush(stdout);                     // 출력(문자 또는 *+색상)을 즉시 화면에 반영.
        index++;                            // 다음 입력 위치로 이동.
    }
    
    
    // gettimeofday(tv, tz): 루프 종료 후, 현재 시간을 end 구조체에 기록.
    gettimeofday(&end, NULL);
    
    
    // tcsetattr(fd, TCSANOW, termios_p): 터미널 속성을 원래 상태(old_attr)로 즉시 복원. (필수!)
    tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);
    
    
    input[index] = '\0';                    // 입력된 문자열의 끝에 널 문자 추가.


    // 경과 시간(Elapsed Time) 계산.
    // 초(tv_sec) 단위의 차이 + 마이크로초(tv_usec) 단위의 차이를 초 단위로 변환하여 합산.
    double elapsed = (double)(end.tv_sec - start.tv_sec) +
                     (double)(end.tv_usec - start.tv_usec) / 1e6; // 1e6은 1,000,000 (마이크로초->초 변환).

    
    int correct = 0;                        // 정확히 일치하는 문자 개수 카운터.
    
    // 비교할 길이 결정: 실제 입력 길이(index)와 목표 문장 길이(target_len) 중 더 작은 값으로 설정.
    int compare_len = (index < target_len) ? index : target_len; 
    
    for (int i = 0; i < compare_len; i++) { // 입력 길이만큼 반복하며 정확도 측정.
        if (input[i] == target[i]) {
            correct++;                      // 문자가 일치하면 카운트 증가.
        }
    }
    
    
    // 정확도 계산: (정확한 문자 수 / 목표 문장 길이) * 100.
    double accuracy = (target_len > 0) ? 100.0 * correct / target_len : 0.0;
    
    // 속도 계산: (입력된 총 문자 수 / 걸린 시간) = 초당 문자 수 (CPS).
    double speed = (elapsed > 0) ? (double)index / elapsed : 0.0; 

    
    // 최종 결과 출력.
    printf("\n\n=== Result ===\n");
    printf("Time taken: %.2f seconds\n", elapsed);
    printf("Typing speed: %.2f chars/sec\n", speed);
    printf("Accuracy: %.2f%%\n", accuracy);
    
    return 0;
}
