#include <stdio.h>
#include <unistd.h>  // sleep() 함수를 사용하기 위해 필요합니다.
#include <curses.h>  // Ncurses 라이브러리 헤더 파일입니다.

int main()
{
    int i; 
    
    // Ncurses 모드를 시작하고 터미널을 설정합니다.
    initscr(); 
    // 화면 전체를 지웁니다.
    clear();   
    
    // 화면의 총 행 수(LINES)만큼 반복합니다. (i는 0부터 LINES-1까지)
    for (i = 0; i < LINES; i++) { 
        
        // 1. 출력 위치 지정: 커서를 (i행, i+1열) 위치로 이동합니다.
        //    (move(y, x)에서 y=i, x=i+1)
        move(i, i + 1);          
        
        // 2. 홀수 줄에서는 강조(standout) 모드를 고,
        if (i % 2 == 1)
            standout();
        
        // 3. 문자열을 현재 커서 위치에 출력합니다.
        addstr("Hello, world!"); 
        
        // 4. 홀수 줄 출력을 마쳤다면 강조 모드를 해제합니다.
        if (i % 2 == 1)
            standend(); 
        
        // 5. 화면 갱신: 출력 버퍼의 내용을 터미널 화면에 반영합니다.
        refresh(); 
        
        // 6. 1초 대기: 애니메이션 속도 조절을 위해 잠시 멈춥니다.
        sleep(1);  
        
        // 7. 커서 위치 복원: 문자열을 지우기 위해 커서를 출력 시작 위치(i, i+1)로 다시 이동시킵니다.
        //    (이전 addstr()로 인해 커서는 i행의 오른쪽 끝으로 이동된 상태입니다.)
        move(i, i + 1); 
        
        // 8. 문자열 지우기: 공백으로 덮어써서 이전 문자열을 지웁니다.
        //    (문자열 길이만큼의 공백을 출력해야 완벽하게 지워집니다.)
        addstr("             "); // "Hello, world!" 길이(13) 만큼의 공백
        
        // 9. 화면 갱신: 지운 결과를 화면에 반영합니다.
        refresh();
    } // for 루프 종료
    
    // Ncurses 모드 종료: 터미널 속성을 원래대로 복구합니다.
    endwin();
    
    return 0;
}