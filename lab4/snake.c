#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

// 게임 영역 크기 (테두리 제외)
#define WIDTH 40
#define HEIGHT 20
#define MAX_LEN 300 

// 뱀의 위치 구조체 정의
typedef struct {
    int x, y;
} Position;

// 뱀의 이동 방향 정의
typedef enum { UP, DOWN, LEFT, RIGHT } Direction;

// 전역 변수 초기화
Position snake[MAX_LEN];
int snake_length = 5;
Direction dir = RIGHT;
Position food;
int game_over = 0;
int score = 0;
int paused = 0;

// ******************* 함수 선언 *******************
// (main 함수에서 사용되는 함수들을 먼저 선언합니다.)
void draw_border();
void draw_food();
void draw_snake();
void draw_status();
void move_snake();
void handle_input();


/**
 * 게임 종료 시 정리 (스켈레톤 코드의 end_game() 역할)
 * SIGINT 핸들러와 main 함수 끝에서 모두 사용됩니다.
 */
void end_game_cleanup() {
    endwin(); 
}

/**
 * SIGINT (Ctrl+C) 핸들러: 종료 확인 및 처리 (요구사항)
 */
void handle_sigint(int sig) {
    int ch;
    
    // 일시정지 상태에서만 질문을 처리하기 위해 paused 플래그를 임시로 설정
    paused = 1; 
    
    clear();
    draw_border();
    // 요구되는 포맷: "Are you sure you want to quit? (y/n):"
    mvprintw(HEIGHT / 2, WIDTH / 2 - 19, "Are you sure you want to quit? (y/n):");
    refresh();
    
    // 사용자 입력 받기 (timeout을 -1로 변경하여 blocking 모드로 전환)
    timeout(-1); 
    ch = getch();
    timeout(100); // init_game()에 맞게 다시 non-blocking으로 복구
    
    if (ch == 'y' || ch == 'Y') {
        end_game_cleanup(); 
        // 요구되는 포맷: "Terminated by user. Final Score: ..."
        printf("Terminated by user. Final Score: %d\n", score);
        exit(0);
    } else {
        paused = 0; // 'n' 입력 시 게임 재개
    }
}

/**
 * SIGTSTP (Ctrl+Z) 핸들러: 일시 정지/재개 토글
 */
void handle_sigtstp(int sig) {
    paused = !paused; // 일시정지 상태 토글
}

/**
 * Draw the border around the game area using '#'.
 */
void draw_border() {
    // 상단 및 하단 경계
    for (int i = 0; i <= WIDTH + 1; i++) {
        mvaddch(0, i, '#');
        mvaddch(HEIGHT + 1, i, '#');
    }
    
    // 좌측 및 우측 경계
    for (int i = 0; i <= HEIGHT + 1; i++) {
        mvaddch(i, 0, '#');
        mvaddch(i, WIDTH + 1, '#');
    }
}

/**
 * Draw the snake on the screen using 'O'.
 */
void draw_snake() {
    // 뱀의 모든 세그먼트 그리기
    for (int i = 0; i < snake_length; i++) {
        mvaddch(snake[i].y, snake[i].x, 'O');
    }
}

/**
 * Draw the food on the screen using '@'.
 */
void draw_food() {
    mvaddch(food.y, food.x, '@'); // 먹이 (@) 그리기
}

/**
 * Display the game status.
 */
void draw_status() {
    // 게임 영역 바로 아래에 상태 표시 (요구되는 포맷)
    mvprintw(HEIGHT + 2, 1, "Score: %d, Length: %d", score, snake_length); 
    
    // 일시 정지 상태 표시 (요구사항)
    if (paused) {
        // 일시 정지 메시지를 화면 중앙에 표시
        mvprintw(HEIGHT / 2 - 1, WIDTH / 2 - 6, "== PAUSED ==");
        mvprintw(HEIGHT / 2, WIDTH / 2 - 19, "Press 'p' or Ctrl+Z to resume");
    }
}

/**
 * 뱀 이동 로직 및 충돌/먹이 검사
 */
void move_snake() {
    // 꼬리부터 머리까지 한 칸씩 앞으로 이동
    for (int i = snake_length - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }

    // 현재 방향에 따라 머리 위치 업데이트
    switch (dir) {
        case UP:    snake[0].y--; break;
        case DOWN:  snake[0].y++; break;
        case LEFT:  snake[0].x--; break;
        case RIGHT: snake[0].x++; break;
    }

    // 벽 충돌 검사
    if (snake[0].x <= 0 || snake[0].x >= WIDTH + 1 ||
        snake[0].y <= 0 || snake[0].y >= HEIGHT + 1) {
        game_over = 1;
        return;
    }

    // 먹이 섭취 검사
    if (snake[0].x == food.x && snake[0].y == food.y) {
        if (snake_length < MAX_LEN)
            snake_length++; // 뱀 길이 증가
        score++; // 점수 증가
        
        // 새로운 먹이 위치 생성
        food.x = rand() % (WIDTH - 2) + 1;
        food.y = rand() % (HEIGHT - 2) + 1;
    }

    // 자신과의 충돌 검사
    for (int i = 1; i < snake_length; i++) {
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
            game_over = 1;
            return;
        }
    }
}

/**
 * Handle keyboard input (WASD for movement, 'p' to pause).
 */
void handle_input() {
    int ch = getch(); // non-blocking mode

    switch (ch) {
        case 'w': if (dir != DOWN) dir = UP; break;    
        case 's': if (dir != UP) dir = DOWN; break;    
        case 'a': if (dir != RIGHT) dir = LEFT; break; 
        case 'd': if (dir != LEFT) dir = RIGHT; break; 
        case 'p': paused = !paused; break;             // 'p' 키로 일시정지 토글
        default: break;
    }
}

/**
 * Initialize the game state and configure terminal settings.
 */
void init_game() {
    initscr();     // ncurses 모드 초기화
    noecho();      // 입력 문자 에코 비활성화
    curs_set(FALSE); // 커서 숨김
    timeout(100);  // getch를 non-blocking (100ms 대기)으로 설정
    srand(time(NULL)); // 난수 생성기 시드 설정

    // 뱀 초기 위치 설정
    for (int i = 0; i < snake_length; i++) {
        snake[i].x = WIDTH / 2 - i;
        snake[i].y = HEIGHT / 2;
    }

    // 먹이 초기 위치 설정
    food.x = rand() % (WIDTH - 2) + 1;
    food.y = rand() % (HEIGHT - 2) + 1;

    // 시그널 핸들러 등록
    signal(SIGINT, handle_sigint);    // Ctrl+C 처리
    signal(SIGTSTP, handle_sigtstp);  // Ctrl+Z 처리
}

/**
 * Main game loop.
 */
int main() {
    init_game(); // 게임 초기화

    while (!game_over) { // 게임 종료 플래그가 켜질 때까지 반복
        clear(); // 화면 지우기

        draw_border(); // 경계 그리기
        draw_food();   // 먹이 그리기
        draw_snake();  // 뱀 그리기
        draw_status(); // 상태 표시 (점수/일시정지)

        handle_input(); // 입력 처리

        // 일시 정지 상태가 아닐 때만 뱀을 이동
        if (!paused) { 
            move_snake(); // 뱀 이동
        }

        refresh(); // 화면 업데이트
        usleep(100000); // 100ms 지연
    }

    // 게임 종료 시 정리 
    end_game_cleanup(); 
    printf("Game Over! Final Score: %d\n", score); // 최종 점수 출력

    return 0; // 프로그램 종료
}
