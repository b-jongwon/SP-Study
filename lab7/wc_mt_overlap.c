/* ==========================================================================
 * [헤더 파일 포함]
 * - stdio.h: 입출력 함수 (printf, fopen, fclose 등)
 * - stdlib.h: 메모리 할당/해제 (malloc, free), 프로세스 종료 (exit), 변환 (atoi)
 * - pthread.h: POSIX 스레드 라이브러리 (스레드 생성, 뮤텍스, 조건변수 등 핵심!)
 * - string.h: 문자열 처리 (사실 이 코드에선 크게 안 쓰임, 습관적으로 포함된 듯)
 * - ctype.h: 문자 타입 검사 (isalnum - 알파벳/숫자인지 확인)
 * - sys/time.h: 시간 측정 (gettimeofday - 성능 테스트용)
 * ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

/* [상수 정의 (매크로)] */
#define CHUNK_SIZE (64*1024)   // 64KB. 파일에서 한 번에 읽어올 데이터의 크기. (I/O 효율성 때문)
#define BUFFER_CAPACITY 64     // 생산자와 소비자가 공유하는 큐(버퍼)의 최대 크기 (슬롯 개수)
#define MAX_CONSUMERS 32       // 최대 생성 가능한 소비자 스레드 개수 제한

/* * [구조체: Chunk]
 * 파일의 일부분(조각)을 담아서 소비자에게 전달하기 위한 택배 상자 같은 존재입니다.
 */
typedef struct {
    char* data;             // 실제 텍스트 데이터가 담긴 힙 메모리 주소 (malloc으로 할당됨)
    size_t size;            // 이 조각의 데이터 크기 (바이트 단위)
    int starts_inside_word; // (중요) 이 조각이 단어 중간부터 시작하는지 여부.
                            // 예: 이전 청크가 "Ap"로 끝났고 이번이 "ple"로 시작하면 1.
                            // 다만, 아래 생산자 로직을 보면 단어를 끊지 않고 가져오도록 구현되어 있어 
                            // 실제로는 항상 0일 가능성이 높음 (방어적 코딩).
} Chunk;

/* ==========================================================================
 * [전역 변수 - 공유 자원]
 * 모든 스레드가 이 변수들을 공유하므로, 접근 시 반드시 동기화(Lock)가 필요합니다.
 * ========================================================================== */
Chunk buffer[BUFFER_CAPACITY]; // 원형 큐(Circular Queue)로 사용될 버퍼 배열
int in = 0;                    // 생산자가 데이터를 넣을 인덱스 (Head)
int out = 0;                   // 소비자가 데이터를 꺼낼 인덱스 (Tail)
int count = 0;                 // 현재 버퍼에 차 있는 데이터 개수

int total_word_count = 0;      // [최종 결과] 모든 스레드가 합산한 총 단어 수
int num_consumers = 1;         // 실행 시 입력받을 소비자 스레드 개수
int is_done = 0;               // 생산자가 "나 일 다 끝났어(파일 다 읽음)"라고 알리는 플래그

/* * [동기화 객체]
 * - mutex: 공유 자원(위의 전역 변수들)을 한 번에 하나의 스레드만 건드리게 하는 자물쇠
 * - not_empty: "버퍼가 비어있지 않음"을 알리는 신호 (소비자가 대기할 때 사용)
 * - not_full: "버퍼가 가득 차지 않음"을 알리는 신호 (생산자가 대기할 때 사용)
 */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

/* [함수: 단어 문자 판별] 알파벳이나 숫자인가? */
int is_word_char(char c) {
    return isalnum(c); // ctype.h 함수 사용
}

/* * [함수: 청크 내 단어 수 세기]
 * 소비자가 실행하는 핵심 로직입니다. 텍스트 덩어리를 받아 단어 몇 개인지 셉니다.
 */
int count_words_in_chunk(char* buf, size_t size, int starts_inside_word) {
    int count = 0;      // 단어 수
    int in_word = 0;    // 현재 '단어 내부'를 지나가고 있는지 상태 플래그 (State Machine 기법)
    size_t i = 0;

    // 만약 이전 청크에서 단어가 끊겨서 넘어왔다면, 이번 청크의 시작 부분은 
    // 새로운 단어가 아니라 이전 단어의 꼬리이므로 건너뛰어야 함.
    if (starts_inside_word) {
        while (i < size && is_word_char(buf[i])) i++;
    }

    // 데이터 끝까지 순회
    for (; i < size; i++) {
        if (is_word_char(buf[i])) { // 현재 문자가 단어의 일부라면
            if (!in_word) {         // 방금 전까지 공백이었다면? -> "새 단어 시작!"
                count++;            // 카운트 증가
                in_word = 1;        // 상태를 '단어 안'으로 변경
            }
        } else {                    // 현재 문자가 공백/특수문자라면
            in_word = 0;            // 상태를 '단어 밖'으로 변경
        }
    }
    return count;
}

/* * [스레드 함수: 생산자 (Producer)]
 * 파일을 읽어서 Chunk로 만들고 버퍼에 넣는 역할 (딱 1개의 스레드만 생성됨)
 */
void* producer(void* arg) {
    char* filename = (char*)arg; // void* 매개변수를 문자열 포인터로 캐스팅
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen"); // 파일 열기 실패 시 에러 출력
        exit(1);
    }

    int prev_ends_in_word = 0; // 이전 청크가 단어 중간에서 끝났는지 추적

    while (1) {
        // [메모리 할당] 청크 데이터를 담을 공간. 
        // +256을 하는 이유: 아래에서 단어가 잘리는 걸 방지하기 위해 
        // CHUNK_SIZE보다 조금 더 읽을 수도 있기 때문에 여유 공간(Padding)을 둠.
        char* buf = malloc(CHUNK_SIZE + 256);
        if (!buf) {
            perror("malloc");
            exit(1);
        }

        // 파일에서 CHUNK_SIZE만큼 읽음
        size_t size = fread(buf, 1, CHUNK_SIZE, fp);
        if (size == 0) { // 파일 끝(EOF) 도달
            free(buf);   // 빈 버퍼는 해제
            break;       // 루프 종료
        }

        // [중요 로직: 단어 경계 처리]
        // 읽어온 데이터의 마지막이 단어 문자라면(예: "appl"), 단어가 짤린 것일 수 있음.
        // 따라서 공백이 나올 때까지 파일에서 1바이트씩 더 읽어서 buf에 붙임.
        // 이렇게 하면 항상 청크는 온전한 단어로 끝나게 됨.
        int c;
        while (size > 0 && is_word_char(buf[size - 1]) && (c = fgetc(fp)) != EOF) {
            buf[size++] = (char)c; // 버퍼 뒤에 붙이고 size 증가
        }

        // 구조체 생성 및 데이터 설정
        Chunk chunk = {
            .data = buf,
            .size = size,
            .starts_inside_word = prev_ends_in_word // 위 로직 덕분에 사실상 항상 0일 것임
        };

        // 현재 청크의 마지막이 단어 문자인지 확인 (다음 청크를 위해)
        // 위 while문에서 공백까지 읽었으므로 대부분 0이 됨.
        prev_ends_in_word = is_word_char(buf[size - 1]);

        /* ----- 임계 영역 (Critical Section) 시작 ----- */
        pthread_mutex_lock(&mutex); // 자물쇠 잠금

        // 버퍼가 꽉 찼다면? 빈 공간이 생길 때까지 대기(Sleep)
        // while을 쓰는 이유: 깨어났는데 그새 다른 스레드가 채웠을 수도 있어서 재확인 필수 (Spurious Wakeup)
        while (count == BUFFER_CAPACITY) {
            pthread_cond_wait(&not_full, &mutex); // 자물쇠를 잠시 풀고 not_full 신호를 기다림
        }

        // 데이터 넣기 (원형 큐 로직)
        buffer[in] = chunk;
        in = (in + 1) % BUFFER_CAPACITY;
        count++; // 데이터 개수 증가

        // "버퍼에 데이터 있다!"라고 소비자들에게 신호 보냄
        pthread_cond_signal(&not_empty); 
        
        pthread_mutex_unlock(&mutex); // 자물쇠 반납
        /* ----- 임계 영역 끝 ----- */
    }
    
    fclose(fp); // 파일 닫기

    /* [종료 처리] 생산 완료 알림 */
    pthread_mutex_lock(&mutex);
    is_done = 1; // "나 끝났음" 플래그 설정
    // 대기 중인 모든 소비자 스레드를 다 깨움 (broadcast). 
    // 왜? 자고 있는 소비자들이 일어나서 is_done을 확인하고 퇴근해야 하니까.
    pthread_cond_broadcast(&not_empty); 
    pthread_mutex_unlock(&mutex);

    return NULL;
}

/* * [스레드 함수: 소비자 (Consumer)]
 * 버퍼에서 Chunk를 꺼내 단어를 세고 결과를 합산 (여러 개의 스레드가 동시에 실행됨)
 */
void* consumer(void* arg) {
    while (1) {
        /* ----- 임계 영역 시작 ----- */
        pthread_mutex_lock(&mutex);

        // 버퍼가 비어있고, 아직 생산자가 일을 안 끝냈다면? -> 대기
        while (count == 0 && !is_done) {
            pthread_cond_wait(&not_empty, &mutex);
        }

        // 버퍼도 비어있고, 생산자도 끝났다면? -> 나도 퇴근(루프 종료)
        if (count == 0 && is_done) {
            pthread_mutex_unlock(&mutex); // 나가기 전에 자물쇠 꼭 풀기!
            break;
        }

        // 데이터 꺼내기 (원형 큐 로직)
        Chunk chunk = buffer[out];
        out = (out + 1) % BUFFER_CAPACITY;
        count--; // 데이터 개수 감소
        
        // "버퍼에 빈 공간 생겼다!"라고 생산자에게 신호 보냄
        pthread_cond_signal(&not_full);
        
        pthread_mutex_unlock(&mutex);
        /* ----- 임계 영역 끝 ----- */

        // [작업 수행] 
        // 자물쇠 밖에서 수행함 (매우 중요!). 
        // 여기서 시간을 써야 병렬 처리의 의미가 있음. 
        // 자물쇠 안에서 이걸 하면 사실상 싱글 스레드랑 다를 게 없음.
        int wc = count_words_in_chunk(chunk.data, chunk.size, chunk.starts_inside_word);

        // 생산자가 malloc한 메모리를 여기서 소비자가 해제 (책임 전가)
        free(chunk.data);

        // [결과 합산]
        // 전역 변수 total_word_count를 건드리므로 다시 자물쇠 필요
        pthread_mutex_lock(&mutex);
        total_word_count += wc;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

/* [메인 함수] */
int main(int argc, char* argv[]) {
    // 인자 확인 (실행 파일명, 대상 파일, 스레드 수)
    if (argc != 3) {
        printf("Usage: %s <filename> <num_consumers>\n", argv[0]);
        return 1;
    }

    // 소비자 스레드 개수 파싱 및 유효성 검사
    num_consumers = atoi(argv[2]);
    if (num_consumers <= 0 || num_consumers > MAX_CONSUMERS) {
        printf("Number of consumers must be between 1 and %d\n", MAX_CONSUMERS);
        return 1;
    }

    // [시간 측정 시작]
    struct timeval start, end;
    gettimeofday(&start, NULL);

    pthread_t prod; // 생산자 스레드 ID
    pthread_t consumers[MAX_CONSUMERS]; // 소비자 스레드 ID 배열

    // 1. 생산자 스레드 생성 (파일 이름을 인자로 넘김)
    pthread_create(&prod, NULL, producer, argv[1]);
    
    // 2. 소비자 스레드들 생성
    for (int i = 0; i < num_consumers; i++) {
        pthread_create(&consumers[i], NULL, consumer, NULL);
    }

    // 3. 스레드 종료 대기 (Join)
    // 메인 스레드는 여기서 블락되어 자식들이 다 끝날 때까지 기다림
    pthread_join(prod, NULL); // 생산자가 끝날 때까지 대기
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumers[i], NULL); // 모든 소비자가 끝날 때까지 대기
    }

    // [시간 측정 종료]
    gettimeofday(&end, NULL);
    // 초(s)와 마이크로초(us) 단위를 밀리초(ms)로 변환하여 계산
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                     (end.tv_usec - start.tv_usec) / 1000.0;

    // 결과 출력
    printf("Total words: %d\n", total_word_count);
    printf("Elapsed time (total): %.2f ms\n", elapsed);

    return 0;
}