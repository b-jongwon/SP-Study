/*
 * [프로그램 목적 및 핵심 개념]
 * 이 프로그램은 사용자가 입력하는 텍스트를 주기적으로 파일에 자동 저장하고,
 * Ctrl+C (SIGINT) 시그널을 받았을 때 최종적으로 저장 후 안전하게 종료하는 간단한 텍스트 편집기입니다.
 *
 * [주요 학습 포인트]
 * 1. 시그널 핸들링: 운영체제의 비동기적 이벤트(SIGALRM, SIGINT)를 처리하는 방법을 배웁니다.
 * - SIGALRM: 주기적인 작업(자동 저장)을 위해 사용됩니다.
 * - SIGINT: 사용자의 종료 요청(Ctrl+C)을 가로채 안전한 마무리 작업을 수행합니다.
 * 2. 동적 메모리 할당: 사용자의 입력을 영구적으로 저장하기 위해 strdup() 함수로 메모리를 할당하고,
 * 프로그램 종료 시 free()로 해제하여 메모리 누수(memory leak)를 방지합니다.
 * 3. 파일 입출력: C언어의 표준 파일 I/O 함수를 사용하여 데이터를 파일에 쓰고 저장합니다.
 */

// ======================= 헤더 파일 포함 (Header Inclusion) =======================
#include <stdio.h>      // 표준 입출력 함수(printf, fgets, fopen 등)를 사용하기 위해 포함합니다.
#include <stdlib.h>     // 동적 메모리 할당(strdup, free, exit) 및 일반 유틸리티 함수를 위해 포함합니다.
#include <signal.h>     // 시그널 처리(signal, SIGINT, SIGALRM)를 위해 포함합니다.
#include <string.h>     // 문자열 처리 함수(strdup)를 위해 포함합니다. strdup은 비표준이지만 널리 쓰입니다.
#include <unistd.h>     // POSIX 운영체제 API(alarm)를 사용하기 위해 포함합니다.

// ======================= 전역 상수 정의 (Global Constants) =======================
#define MAX_LINES 100                   // 저장할 수 있는 최대 라인 수를 100으로 제한합니다.
#define MAX_LINE_LEN 256                // 한 라인에 입력할 수 있는 최대 길이를 256 바이트로 제한합니다.
#define AUTOSAVE_FILE "autosave.txt"    // 자동 저장될 파일의 이름을 상수로 정의하여 관리 용이성을 높입니다.

// ======================= 전역 변수 선언 (Global Variables) =======================
char *lines[MAX_LINES];                 // 사용자가 입력한 각 라인(문자열)의 주소를 저장할 포인터 배열입니다.
                                        // char* 타입은 문자열(의 시작 주소)을 가리키는 포인터입니다.
int line_count = 0;                     // 현재까지 입력된 라인의 수를 저장하는 카운터 변수입니다.

// ======================= 함수 정의 (Function Definitions) =======================

/**
 * @brief 현재까지 입력된 모든 라인을 파일에 저장하는 함수입니다.
 * @param is_final_save 0이 아니면(true) 최종 저장임을 나타내며, 종료 메시지를 출력합니다.
 * * [이 함수가 필요한 이유]
 * - 코드의 재사용성: 자동 저장과 최종 저장 로직이 동일하므로 하나의 함수로 만들어 중복을 피합니다.
 * - 기능의 분리: 파일 저장이라는 특정 기능을 main 함수나 시그널 핸들러로부터 분리하여 코드 구조를 명확하게 합니다.
 */
void save_lines(int is_final_save) {
    // [문법: 파일 열기] fopen(파일명, 모드) 함수는 파일을 열고 파일 포인터를 반환합니다.
    // "w" 모드는 쓰기 모드이며, 파일이 존재하면 내용을 덮어쓰고 없으면 새로 생성합니다.
    FILE *fp = fopen(AUTOSAVE_FILE, "w"); // AUTOSAVE_FILE을 쓰기 모드("w")로 엽니다.

    // [문법: 오류 처리] 파일 열기에 실패하면 fopen은 NULL을 반환합니다.
    // perror 함수는 마지막으로 발생한 시스템 오류에 대한 메시지를 표준 오류(stderr)에 출력합니다.
    if (fp == NULL) {                       // 파일 열기에 실패했는지 확인합니다.
        perror("Error opening autosave file"); // 오류 메시지를 출력합니다.
        return;                             // 함수를 즉시 종료합니다.
    }

    // [문법: 반복문] for 루프를 사용하여 line_count만큼 반복합니다.
    for (int i = 0; i < line_count; i++) {
        // lines[i]가 NULL이 아닌지 확인하여 안정성을 높입니다. (현재 코드 구조상 항상 NULL이 아니지만 좋은 습관입니다.)
        if (lines[i] != NULL) {
            // [문법: 파일 쓰기] fprintf(파일포인터, 형식, 데이터)는 서식이 있는 데이터를 파일에 씁니다.
            fprintf(fp, "%s", lines[i]);    // lines 배열의 i번째 문자열을 파일에 씁니다.
        }
    }

    // [문법: 파일 닫기] fclose(파일포인터)는 열었던 파일을 닫고 버퍼에 남아있는 데이터를 디스크에 완전히 쓰도록 합니다.
    // 파일을 닫지 않으면 데이터 유실이나 시스템 자원 낭비가 발생할 수 있습니다.
    fclose(fp);                             // 파일 작업을 마쳤으므로 파일을 닫습니다.
    
    // [문법: 조건문] is_final_save 플래그 값에 따라 다른 동작을 수행합니다.
    if (is_final_save) { // 이 값이 1(true)이면, 즉 SIGINT(Ctrl+C)로 호출되었으면
        // 프로그램 종료 시 사용자에게 명확한 피드백을 제공합니다.
        printf("\n[!] Exit signal received. Saving final version...\n");
    } else {
        // 자동 저장 시에는 별도의 메시지를 출력하지 않아 사용자 입력을 방해하지 않습니다. (현재는 비어있음)
    }
}

/**
 * @brief SIGALRM 또는 SIGINT 시그널을 처리하는 핸들러 함수입니다.
 * @param sig 수신된 시그널의 번호 (예: SIGALRM, SIGINT)
 * * [이 함수가 필요한 이유]
 * - 비동기 이벤트 처리: 프로그램 실행 중 언제 발생할지 모르는 OS의 시그널에 대응하기 위해 필요합니다.
 * 이를 통해 프로그램이 강제 종료되지 않고, 정해진 정리 작업을 수행할 수 있습니다.
 */
void handle_signal(int sig) {
    // [문법: 조건문] 전달된 시그널(sig)의 종류에 따라 다른 작업을 수행합니다.
    if (sig == SIGALRM) {               // alarm(5)에 의해 5초마다 발생하는 SIGALRM 시그널인 경우
        save_lines(0);                  // 자동 저장을 수행합니다. (0은 최종 저장이 아님을 의미)
        
        // [문법: 알람 재설정] alarm(초) 함수는 지정된 초 후에 SIGALRM 시그널을 다시 보내도록 예약합니다.
        // 이것을 하지 않으면 알람은 한 번만 울리고 맙니다. 주기적인 작업을 위해 핸들러 내에서 재설정해야 합니다.
        alarm(5);                       // 다음 5초 후의 자동 저장을 위해 알람을 다시 설정합니다.

    } else if (sig == SIGINT) {         // 사용자가 Ctrl+C를 눌러 SIGINT 시그널이 발생한 경우
        
        save_lines(1);                  // 최종 저장을 수행합니다. (1은 최종 저장임을 의미)

        // [메모리 해제: 중요한 이유]
        // strdup으로 동적 할당된 메모리는 프로그램이 종료되기 전에 반드시 free()로 해제해야 합니다.
        // 운영체제가 대부분의 메모리를 회수하긴 하지만, 이는 좋은 프로그래밍 습관이며,
        // 더 복잡한 프로그램에서는 메모리 누수를 막기 위해 필수적입니다.
        for (int i = 0; i < line_count; i++) {
            if (lines[i] != NULL) {     // 각 라인에 할당된 메모리가 있는지 확인하고
                free(lines[i]);         // [문법: 메모리 해제] free() 함수로 동적 할당된 메모리를 해제합니다.
            }
        }
        
        // [문법: 프로그램 종료] exit(0)는 프로그램을 즉시 종료시킵니다. 0은 정상 종료를 의미합니다.
        exit(0);
    }
}

// ======================= main 함수: 프로그램의 시작점 =======================
int main() {
    // [문법: 시그널 핸들러 등록] signal(시그널, 핸들러함수)는 특정 시그널이 발생했을 때 호출될 함수를 지정합니다.
    // SIG_ERR은 signal 함수가 실패했을 때 반환하는 값입니다.
    if (signal(SIGINT, handle_signal) == SIG_ERR) { // Ctrl+C(SIGINT)에 대한 핸들러를 등록합니다.
        perror("Cannot register SIGINT handler");   // 등록 실패 시 오류 메시지를 출력합니다.
        return 1;                                   // 오류 코드를 반환하며 프로그램을 종료합니다.
    }
    if (signal(SIGALRM, handle_signal) == SIG_ERR) { // 알람(SIGALRM)에 대한 핸들러를 등록합니다.
        perror("Cannot register SIGALRM handler");  // 등록 실패 시 오류 메시지를 출력합니다.
        return 1;                                   // 오류 코드를 반환하며 프로그램을 종료합니다.
    }

    // [문법: 최초 알람 설정] 프로그램 시작 후 5초 뒤에 첫 SIGALRM 시그널을 보내도록 설정합니다.
    alarm(5);

    printf("Enter text (Ctrl+C to quit): \n"); // 사용자에게 안내 메시지를 출력합니다.

    char buffer[MAX_LINE_LEN];              // 사용자의 한 줄 입력을 임시로 저장할 버퍼입니다.

    // [문법: 무한 루프] while(1)은 프로그램이 명시적으로 종료되기 전까지 계속 실행되는 루프를 만듭니다.
    // 이 프로그램은 Ctrl+C(SIGINT)를 통해 종료되므로 무한 루프가 적합합니다.
    while (1) {
        // [문법: 안전한 입력] fgets(저장할곳, 최대길이, 입력소스)는 한 줄을 안전하게 읽어옵니다.
        // gets와 달리 버퍼 오버플로우를 방지할 수 있어 보안에 더 좋습니다. EOF(파일 끝)나 오류 시 NULL을 반환합니다.
        if (fgets(buffer, MAX_LINE_LEN, stdin) != NULL) { // 표준 입력(stdin)으로부터 한 줄을 읽어옵니다.
            
            if (line_count < MAX_LINES) {   // 최대 라인 수에 도달하지 않았는지 확인합니다.
                
                // [문법: 동적 메모리 할당 및 복사] strdup(문자열)은 다음 두 작업을 한 번에 수행합니다.
                // 1. 문자열을 저장하기에 충분한 크기의 메모리를 동적으로 할당합니다. (malloc)
                // 2. 원본 문자열(buffer)의 내용을 새로 할당된 메모리에 복사합니다. (strcpy)
                // buffer의 내용은 다음 루프에서 덮어써지기 때문에, 각 라인을 별도의 메모리에 저장해야 합니다.
                lines[line_count] = strdup(buffer); 
                
                if (lines[line_count] == NULL) { // strdup이 메모리 할당에 실패하면 NULL을 반환합니다.
                    perror("Memory allocation failed"); // 실패 시 오류 메시지를 출력하고
                    exit(1);                            // 프로그램을 비정상 종료합니다.
                }
                line_count++;               // 라인 수를 1 증가시킵니다.
            } else {
                printf("Reached maximum number of lines.\n"); // 최대 라인 수에 도달했음을 알립니다.
            }
        }
    }
    
    // 이 코드는 위의 while(1) 무한 루프 때문에 절대 실행되지 않습니다.
    // 프로그램은 오직 handle_signal 함수 내의 exit(0)를 통해서만 종료됩니다.
    return 0;
}
