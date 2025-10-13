/*
 * [프로그램 목적 및 핵심 개념]
 * ncurses 라이브러리를 사용하여 고전 게임 '스네이크'를 구현한 코드입니다.
 * 사용자는 뱀을 조종하여 먹이를 먹고, 뱀의 길이를 늘려 점수를 얻습니다.
 * 벽이나 자기 자신의 몸에 부딪히면 게임이 종료됩니다.
 *
 * [주요 학습 포인트]
 * 1. 게임 루프(Game Loop): '입력 처리 -> 상태 업데이트 -> 렌더링(화면 그리기)'이라는
 * 모든 게임의 기본 구조를 이해할 수 있습니다.
 * 2. 데이터 구조 활용: '뱀'이라는 동적인 개체를 구조체 배열(Position snake[])로 표현하고
 * 관리하는 방법을 배웁니다. 열거형(enum)을 통해 방향을 명시적으로 관리합니다.
 * 3. ncurses 심화 활용: 화면을 그리고(mvaddch), 사용자 입력을 실시간으로 받으며(timeout, getch),
 * 게임의 상태(점수, 일시정지)를 화면에 표시하는 방법을 익힙니다.
 * 4. 시그널 핸들링 응용: Ctrl+C(SIGINT)로 안전하게 종료하는 기능과
 * Ctrl+Z(SIGTSTP)로 게임을 일시정지/재개하는 실용적인 기능을 구현합니다.
 */

// ======================= 헤더 파일 포함 (Header Inclusion) =======================
#include <ncurses.h> // 터미널 제어를 위한 ncurses 라이브러리 (macOS에서는 <curses.h>)
#include <stdlib.h>  // 난수 생성(rand, srand) 및 프로그램 종료(exit)를 위해 포함
#include <time.h>    // srand의 시드 값으로 현재 시간을 사용하기 위해 포함 (time)
#include <unistd.h>  // usleep 함수로 게임 속도를 조절하기 위해 포함
#include <signal.h>  // SIGINT, SIGTSTP 시그널을 처리하기 위해 포함

// ======================= 전역 상수 정의 (Global Constants) =======================
#define WIDTH 40        // 게임 영역의 너비
#define HEIGHT 20       // 게임 영역의 높이
#define MAX_LEN 300     // 뱀의 최대 길이 (배열 크기)

// ======================= 사용자 정의 타입 (Custom Types) =======================
// [문법: 구조체] x와 y 좌표를 하나의 단위로 묶기 위해 Position 구조체를 정의합니다.
// 이렇게 하면 뱀의 각 부분이나 먹이의 위치를 (x, y) 쌍으로 쉽게 관리할 수 있습니다.
typedef struct {
    int x, y;
} Position;

// [문법: 열거형] 뱀의 이동 방향을 나타내는 상수를 정의합니다.
// 숫자로 방향을 나타내는 것(예: 0=UP, 1=DOWN)보다 코드의 가독성이 월등히 좋아집니다.
typedef enum { UP, DOWN, LEFT, RIGHT } Direction;

// ======================= 전역 변수 선언 (Global Variables) =======================
// 게임의 핵심 상태(State) 데이터들입니다. 여러 함수에서 공유해야 하므로 전역으로 선언합니다.
Position snake[MAX_LEN];  // 뱀의 각 몸통 부분의 위치를 저장하는 배열
int snake_length = 5;     // 뱀의 현재 길이
Direction dir = RIGHT;    // 뱀의 현재 이동 방향
Position food;            // 먹이의 현재 위치
int game_over = 0;        // 게임 종료 여부를 나타내는 플래그 (0: 진행중, 1: 종료)
int score = 0;            // 현재 점수
int paused = 0;           // 게임 일시정지 여부를 나타내는 플래그 (0: 진행중, 1: 일시정지)

// ======================= 함수 프로토타입 선언 (Function Prototypes) =======================
// main 함수보다 뒤에 정의될 함수들을 main에서 사용하기 위해 미리 선언(forward declaration)합니다.
void draw_border();
void draw_food();
void draw_snake();
void draw_status();
void move_snake();
void handle_input();

// ======================= 함수 정의 (Function Definitions) =======================

/**
 * @brief 게임 종료 시 ncurses 모드를 해제하는 정리 함수입니다.
 */
void end_game_cleanup() {
    endwin(); // [ncurses] ncurses 모드를 종료하고 터미널을 원래 상태로 복원합니다.
}

/**
 * @brief Ctrl+C (SIGINT) 시그널을 처리하는 핸들러입니다.
 */
void handle_sigint(int sig) {
    int ch;
    
    paused = 1; // [로직] 종료 확인 중 게임이 진행되지 않도록 즉시 일시정지 상태로 만듭니다.
    
    clear();
    draw_border();
    mvprintw(HEIGHT / 2, WIDTH / 2 - 19, "Are you sure you want to quit? (y/n):");
    refresh();
    
    // [ncurses: 중요] getch의 동작 모드를 일시적으로 변경합니다.
    // timeout(-1)은 사용자가 키를 누를 때까지 무한정 기다리는 '블로킹(blocking)' 모드입니다.
    timeout(-1);
    ch = getch(); // 사용자의 y/n 입력을 받습니다.
    // 다시 원래의 '논블로킹(non-blocking)' 모드로 되돌려 게임 루프가 정상 동작하게 합니다.
    timeout(100); 
    
    if (ch == 'y' || ch == 'Y') {
        end_game_cleanup(); // ncurses 모드를 정리합니다.
        // ncurses 모드 종료 후에는 표준 printf로 최종 점수를 출력할 수 있습니다.
        printf("Terminated by user. Final Score: %d\n", score);
        exit(0); // 프로그램을 정상 종료합니다.
    } else {
        paused = 0; // 'n'을 누르면 일시정지를 해제하고 게임으로 복귀합니다.
    }
}

/**
 * @brief Ctrl+Z (SIGTSTP) 시그널을 처리하는 핸들러입니다.
 */
void handle_sigtstp(int sig) {
    // [문법: 논리 NOT 연산자] paused 변수의 값을 반전시킵니다 (0 -> 1, 1 -> 0).
    // 이를 통해 일시정지/재개 기능을 토글(toggle)합니다.
    paused = !paused;
}

// --- 그리기(Drawing) 관련 함수들 ---
void draw_border() { /* 이전 코드와 동일하여 상세 주석 생략 */ 
    for (int i = 0; i <= WIDTH + 1; i++) {
        mvaddch(0, i, '#');
        mvaddch(HEIGHT + 1, i, '#');
    }
    for (int i = 0; i <= HEIGHT + 1; i++) {
        mvaddch(i, 0, '#');
        mvaddch(i, WIDTH + 1, '#');
    }
}

void draw_snake() {
    // snake 배열을 순회하며 각 몸통 부분 'O'를 화면에 그립니다.
    for (int i = 0; i < snake_length; i++) {
        mvaddch(snake[i].y, snake[i].x, 'O');
    }
}

void draw_food() {
    mvaddch(food.y, food.x, '@'); // 먹이 '@'를 화면에 그립니다.
}

void draw_status() {
    mvprintw(HEIGHT + 2, 1, "Score: %d, Length: %d", score, snake_length);
    
    // [문법: 조건문] 게임이 일시정지 상태일 때만 PAUSED 메시지를 화면 중앙에 표시합니다.
    if (paused) {
        mvprintw(HEIGHT / 2 - 1, WIDTH / 2 - 6, "== PAUSED ==");
        mvprintw(HEIGHT / 2, WIDTH / 2 - 19, "Press 'p' or Ctrl+Z to resume");
    }
}

// --- 게임 로직 관련 함수들 ---

/**
 * @brief 뱀을 이동시키고 충돌 및 먹이 섭취를 검사하는 핵심 로직 함수입니다.
 */
void move_snake() {
    // [로직: 뱀 몸통 이동] 꼬리부터 머리 바로 뒤까지, 각 몸통 조각을 바로 앞 조각의 위치로 이동시킵니다.
    // 이 작업을 통해 뱀이 머리를 따라가는 것처럼 보이게 됩니다.
    for (int i = snake_length - 1; i > 0; i--) {
        snake[i] = snake[i - 1]; // 구조체는 이렇게 통째로 복사(대입)가 가능합니다.
    }

    // [로직: 뱀 머리 이동] 현재 방향(dir)에 따라 머리(snake[0])의 좌표를 한 칸 업데이트합니다.
    switch (dir) {
        case UP:    snake[0].y--; break;
        case DOWN:  snake[0].y++; break;
        case LEFT:  snake[0].x--; break;
        case RIGHT: snake[0].x++; break;
    }

    // [로직: 벽 충돌 검사] 뱀의 머리가 경계선을 벗어났는지 확인합니다.
    if (snake[0].x <= 0 || snake[0].x >= WIDTH + 1 ||
        snake[0].y <= 0 || snake[0].y >= HEIGHT + 1) {
        game_over = 1; // 게임 오버 플래그를 설정합니다.
        return;        // 즉시 함수를 종료하여 아래의 다른 검사를 생략합니다.
    }

    // [로직: 먹이 섭취 검사] 뱀의 머리가 먹이와 같은 위치에 있는지 확인합니다.
    if (snake[0].x == food.x && snake[0].y == food.y) {
        if (snake_length < MAX_LEN) // 뱀의 길이가 배열 최대 크기를 넘지 않도록 방지
            snake_length++;         // 뱀 길이를 1 증가시킵니다.
        score++;                    // 점수를 1 증가시킵니다.
        
        // [로직: 새 먹이 생성] 새로운 먹이를 게임 영역 내의 임의의 위치에 생성합니다.
        // rand() % N은 0부터 N-1까지의 난수를 생성합니다. +1을 하여 1부터 N까지의 범위로 조정합니다.
        food.x = rand() % (WIDTH - 2) + 1;
        food.y = rand() % (HEIGHT - 2) + 1;
    }

    // [로직: 자기 몸 충돌 검사] 뱀의 머리(snake[0])가 몸통(snake[1]부터 끝까지)과 겹치는지 확인합니다.
    for (int i = 1; i < snake_length; i++) {
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
            game_over = 1; // 게임 오버 플래그를 설정합니다.
            return;
        }
    }
}

/**
 * @brief 사용자의 키 입력을 처리하는 함수입니다.
 */
void handle_input() {
    int ch = getch(); // [ncurses] 키 입력을 받습니다. timeout(100) 설정으로 인해 100ms 대기 후 키가 없으면 ERR을 반환.

    switch (ch) {
        // [로직] 뱀이 자신의 이동 방향과 정반대로 바로 이동하는 것을 방지합니다 (예: 오른쪽으로 가다가 바로 왼쪽으로).
        case 'w': if (dir != DOWN) dir = UP; break;    
        case 's': if (dir != UP) dir = DOWN; break;    
        case 'a': if (dir != RIGHT) dir = LEFT; break; 
        case 'd': if (dir != LEFT) dir = RIGHT; break; 
        case 'p': paused = !paused; break; // 'p' 키로도 일시정지/재개 토글
        default: break; // 그 외의 키는 무시합니다.
    }
}

/**
 * @brief 게임 시작에 필요한 모든 것을 초기화하는 함수입니다.
 */
void init_game() {
    initscr();          // ncurses 모드 시작
    noecho();           // 입력한 키가 화면에 보이지 않게 함
    curs_set(FALSE);    // 커서 숨김
    // [ncurses: 중요] getch()가 100ms(0.1초)만 기다리게 설정합니다.
    // 이 시간 동안 키 입력이 없으면 getch()는 ERR을 반환하고 프로그램은 계속 진행됩니다.
    // 이것이 게임의 '틱(tick)' 또는 프레임 속도를 결정하는 핵심 요소 중 하나입니다.
    timeout(100);
    // [문법: 난수 시드 설정] time(NULL)로 현재 시간을 받아와 난수 생성기의 시드를 설정합니다.
    // 이렇게 해야 게임을 실행할 때마다 다른 순서의 난수(즉, 다른 먹이 위치)가 생성됩니다.
    srand(time(NULL));

    // [로직: 초기 뱀 위치 설정] 뱀을 화면 중앙에 수평으로 배치합니다.
    for (int i = 0; i < snake_length; i++) {
        snake[i].x = WIDTH / 2 - i;
        snake[i].y = HEIGHT / 2;
    }

    // [로직: 초기 먹이 위치 설정]
    food.x = rand() % (WIDTH - 2) + 1;
    food.y = rand() % (HEIGHT - 2) + 1;

    // [문법: 시그널 핸들러 등록]
    signal(SIGINT, handle_sigint);    // Ctrl+C에 대한 핸들러 등록
    signal(SIGTSTP, handle_sigtstp);  // Ctrl+Z에 대한 핸들러 등록
}

// ======================= main 함수: 프로그램의 시작점 =======================
int main() {
    init_game(); // 게임 초기화 함수를 호출합니다.

    // --- 메인 게임 루프 ---
    // game_over 플래그가 1이 될 때까지 무한히 반복합니다.
    while (!game_over) {
        // [1. 화면 정리]
        clear();

        // [2. 렌더링(화면 그리기)]
        draw_border();
        draw_food();
        draw_snake();
        draw_status(); // 게임의 현재 상태를 바탕으로 모든 요소를 그립니다.

        // [3. 사용자 입력 처리]
        handle_input(); // 키 입력을 받아 게임 상태(방향, 일시정지)를 변경합니다.

        // [4. 상태 업데이트]
        // 일시정지 상태가 아닐 때만 게임 로직(뱀 이동, 충돌 검사 등)을 실행합니다.
        if (!paused) { 
            move_snake();
        }

        // [5. 실제 화면에 반영]
        refresh(); // 가상 스크린에 그려진 모든 내용을 실제 터미널 화면에 업데이트합니다.
        
        // [6. 속도 조절]
        // [함수: usleep] 마이크로초(1/1,000,000초) 단위로 프로그램 실행을 잠시 멈춥니다.
        // 100,000 마이크로초 = 100밀리초 = 0.1초. 이 지연 시간이 게임의 속도를 결정합니다.
        usleep(100000); 
    }
 
    // --- 게임 종료 처리 ---
    end_game_cleanup(); // ncurses 모드를 정리합니다.
    printf("Game Over! Final Score: %d\n", score); // 최종 점수를 출력합니다.

    return 0; // 프로그램을 정상적으로 종료합니다.
}
