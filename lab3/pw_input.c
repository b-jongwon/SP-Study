#include <stdio.h>      // 표준 입출력 함수 (printf, fprintf, fflush, getchar) 사용을 위해 포함.
#include <termios.h>    // [핵심] 터미널 속성 제어 구조체(struct termios)와 함수(tcgetattr, tcsetattr) 사용을 위해 포함.
#include <unistd.h>     // POSIX 시스템 호출 (tcgetattr, tcsetattr, STDIN_FILENO) 사용을 위해 포함.
#include <string.h>     // 문자열 함수 (memset 등) 사용을 위해 포함 (여기서는 직접 사용되지 않았으나 일반적인 습관).

#define MAX_PASSWORD_LENGTH 128 // 비밀번호 문자열의 최대 길이 정의.

int main() {
    char password[MAX_PASSWORD_LENGTH]; // 입력된 비밀번호를 저장할 버퍼.
    int index = 0;                      // password 배열에 저장할 다음 문자의 인덱스 (현재 입력 길이).
    char ch;                            // getchar()로 입력받은 단일 문자를 저장할 변수.

    struct termios old_attr, new_attr;  // [핵심] 터미널 속성을 저장할 termios 구조체 선언.
                                        // old_attr: 기존 속성을 백업 (복구용).
                                        // new_attr: 변경할 속성을 설정.

    
    // tcgetattr(fd, termios_p): 파일 디스크립터(STDIN_FILENO)의 현재 터미널 속성을 old_attr에 저장.
    tcgetattr(STDIN_FILENO, &old_attr); // STDIN_FILENO는 표준 입력 (0)을 의미.
    new_attr = old_attr;                // 기존 속성을 new_attr에 복사하여 이를 기반으로 속성을 변경할 준비.

    
    // c_lflag (Local mode flags): 로컬 터미널 처리 모드를 제어하는 비트 플래그 필드.
    // ~(ICANON | ECHO): ICANON(정규 모드)과 ECHO(에코 기능)에 해당하는 비트들을 모두 0으로 설정.
    // ICANON 비활성화: 입력이 줄바꿈 문자('\n')를 만날 때까지 버퍼링되지 않고 즉시 전달되도록 함 (Uncanonical/Raw mode와 유사).
    // ECHO 비활성화: 키보드 입력 문자가 화면에 자동으로 표시되는 것을 방지.
    new_attr.c_lflag &= ~(ICANON | ECHO);

    
    // tcsetattr(fd, optional_actions, termios_p): 터미널 속성을 새로운 값(new_attr)으로 설정.
    // TCSANOW: 속성을 즉시 변경 (TCSAFLUSH 등은 입력/출력 버퍼를 비우고 변경).
    tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);

    printf("Enter your password: ");    // 사용자에게 메시지 출력.
    fflush(stdout);                     // printf는 버퍼링될 수 있으므로, 메시지가 즉시 화면에 표시되도록 버퍼를 비웁니다.

    
    // 사용자 입력을 처리하는 루프
    while (index < MAX_PASSWORD_LENGTH - 1) {
        ch = getchar();                 // getchar(): 버퍼링 없이 입력된 단일 문자를 읽습니다 (ICANON 비활성화의 효과).
        
        // 입력이 줄바꿈(Enter)이거나 캐리지 리턴인 경우 (입력 종료)
        if (ch == '\n' || ch == '\r') {
            break;                      // 루프를 종료하고 비밀번호 입력을 완료.
        }

        // 백스페이스 (ASCII 127) 또는 Delete 키 (ASCII 8, 터미널 환경에 따라 다름) 처리.
        if (ch == 127 || ch == 8) {
            if (index > 0) {            // 입력된 문자가 하나라도 있을 경우에만 작동.
                index--;                // 비밀번호 문자열에서 마지막 문자의 인덱스를 감소 (삭제 처리).
                // printf("\b \b"): 터미널에서 문자 삭제 효과 구현.
                // '\b' (Backspace): 커서를 한 칸 뒤로 이동.
                // ' ' (Space): 현재 위치의 별표를 공백으로 덮어씀.
                // '\b' (Backspace): 다시 커서를 공백 위치로 이동시켜 다음 입력 대기.
                printf("\b \b");
                fflush(stdout);         // 삭제된 내용이 즉시 화면에 반영되도록 버퍼 비움.
            }
        } else {
            // 일반 문자 입력인 경우
            password[index++] = ch;     // 문자를 비밀번호 버퍼에 저장하고 인덱스 증가.
            printf("*");                // 마스킹을 위해 화면에는 별표를 출력.
            fflush(stdout);             // 별표가 즉시 화면에 표시되도록 버퍼 비움.
        }
    }

    password[index] = '\0';             // 입력된 문자열의 끝을 표시하기 위해 널 문자('\0')를 추가.
    
    
    // tcsetattr(fd, optional_actions, termios_p): 프로그램 종료 전에 터미널 속성을
    // TCSANOW: 이전에 백업해 둔 기존 속성(old_attr)으로 즉시 복원. (매우 중요!)
    tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);

    printf("\nPassword entered: %s\n", password); // 입력받은 비밀번호를 최종적으로 출력 (실제 보안 환경에서는 출력하지 않음).
    
    return 0;
}
