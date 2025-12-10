/*
 * [헤더 파일 포함]
 * - pthread.h: 스레드 생성/종료/동기화를 위한 POSIX 표준 라이브러리
 * - sys/time.h: 마이크로초(us) 단위의 정밀한 시간 측정을 위한 gettimeofday 함수 포함
 * - ctype.h: isalnum() 등 문자 판별 함수 포함
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>

#define MAX_THREADS 16 // 최대 스레드 개수 제한 (안전장치)

/* * [함수: 시간 차이 계산]
 * 시작 시간(start)과 끝 시간(end)을 받아서 밀리초(ms) 단위로 변환해 반환합니다.
 * tv_sec: 초 단위 / tv_usec: 마이크로초(1/1,000,000초) 단위
 */
double time_diff_ms(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_usec - start.tv_usec) / 1000.0;
}

/* * [구조체: 스레드 인자 (Thread Argument)]
 * pthread_create는 인자를 딱 1개(void*)만 받을 수 있습니다.
 * 그래서 스레드에게 "너는 어디서부터 어디까지 읽어라"라고 알려줄 정보들을 
 * 이 구조체 하나에 묶어서(Packing) 보냅니다.
 */
typedef struct {
    char *buffer;   // [공유 데이터] 파일 내용 전체가 담긴 거대한 메모리 주소 (모든 스레드가 공유함)
    long start;     // [구역 설정] 이 스레드가 검사를 시작할 배열 인덱스
    long end;       // [구역 설정] 이 스레드가 검사를 멈출 배열 인덱스
    int count;      // [결과 저장] 이 스레드가 찾은 단어 개수를 여기에 적어서 돌려줌
} ThreadArg;

/* [함수: 단어 판별] 알파벳이나 숫자면 참(True) */
int is_word_char(char c) {
    return isalnum(c);
}

/* * [스레드 작업 함수: 단어 세기]
 * 각 스레드는 전체 버퍼 중, 자신에게 할당된 [start ~ end) 구간만 훑습니다.
 */
void* count_words(void* arg) {
    // void* 로 받은 짐보따리를 다시 내 구조체 모양으로 캐스팅해서 풂
    ThreadArg* t_arg = (ThreadArg*) arg;
    
    int in_word = 0; // 상태 플래그 (0: 단어 밖, 1: 단어 안)
    int count = 0;   // 로컬 카운트

    // 지정된 범위(start ~ end)만 반복
    for (long i = t_arg->start; i < t_arg->end; i++) {
        // [공유 메모리 접근] t_arg->buffer는 힙 영역에 있는 거대 배열
        if (is_word_char(t_arg->buffer[i])) {
            if (!in_word) { // 공백이었다가 문자가 나오면? -> 단어 시작!
                count++;
                in_word = 1;
            }
        } else {
            in_word = 0; // 공백이나 특수문자가 나오면 -> 단어 끝
        }
    }

    // 결과 저장 (메인 스레드가 나중에 읽어갈 것임)
    t_arg->count = count;
    return NULL;
}

int main(int argc, char* argv[]) {
    // 인자 체크
    if (argc != 3) {
        printf("Usage: %s <filename> <num_threads>\n", argv[0]);
        return 1;
    }

    // [전체 시간 측정 시작]
    struct timeval total_start, total_end;
    gettimeofday(&total_start, NULL);

    char* filename = argv[1];
    int num_threads = atoi(argv[2]); // 스레드 개수 파싱

    if (num_threads <= 0 || num_threads > MAX_THREADS) {
        printf("Thread count must be between 1 and %d\n", MAX_THREADS);
        return 1;
    }

    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    /* * [파일 크기 구하기]
     * 파일을 열자마자 끝(SEEK_END)으로 점프해서 위치(ftell)를 알아내면 그게 파일 크기입니다.
     * 그리고 다시 처음(SEEK_SET)으로 돌아옵니다.
     */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // [I/O 시간 측정 시작]
    struct timeval io_start, io_end;
    gettimeofday(&io_start, NULL);

    /* * [메모리 통째로 할당 (Load All Strategy)]
     * 파일 크기만큼 힙 메모리를 할당합니다.
     * 주의: 파일이 RAM보다 크면(예: 100GB) 이 방식은 컴퓨터를 뻗게 만듭니다. (OOM Kill 발생)
     * 하지만 파일이 적당하다면 가장 빠른 방식 중 하나입니다.
     */
    char* buffer = malloc(size + 1);
    
    // 파일 내용을 한 방에 메모리로 복사 (Disk -> RAM)
    // 이 부분이 프로그램 실행 시간의 대부분을 차지할 가능성이 큽니다 (I/O Bottleneck).
    fread(buffer, 1, size, fp);
    buffer[size] = '\0'; // 문자열 끝 처리 (Null-terminate)

    gettimeofday(&io_end, NULL); // I/O 끝
    fclose(fp);

    // [단어 세기(Computation) 시간 측정 시작]
    struct timeval wc_start, wc_end;
    gettimeofday(&wc_start, NULL);

    pthread_t threads[MAX_THREADS]; // 스레드 ID 배열
    ThreadArg args[MAX_THREADS];    // 각 스레드에게 줄 인자 배열
    
    // [구역 나누기] 전체 크기를 스레드 수로 나눔 (N빵)
    long block = size / num_threads;

    for (int i = 0; i < num_threads; i++) {
        args[i].buffer = buffer; // 모든 스레드가 같은 버퍼(책)를 봅니다.
        
        // [기본 구역 설정]
        args[i].start = i * block;
        // 마지막 스레드는 남은 짜투리까지 다 맡아야 함 (size까지)
        args[i].end = (i == num_threads - 1) ? size : (i + 1) * block;

        /* * [매우 중요: 경계 문제 (Boundary Problem) 해결 로직]
         * 파일: "Hello World"를 2개 스레드가 나눈다고 가정해봅시다.
         * 스레드1: "Hello Wo"
         * 스레드2: "rld"
         * * 그냥 세면 스레드1은 "Hello", "Wo" -> 2개
         * 스레드2는 "rld" -> 1개
         * 총 3개? 틀렸습니다! "World"는 1개여야 합니다.
         * 이 문제를 해결하기 위해 '구역 조정'을 수행합니다.
         */

        // 1. 시작점 조정 (첫 번째 스레드 제외)
        // 내 구역의 시작점이 단어의 중간이라면? (예: "rld"의 'r')
        // 그 단어는 앞 스레드가 처리하도록 나는 건너뛴다.
        if (i != 0) {
            while (args[i].start < size && is_word_char(buffer[args[i].start])) {
                args[i].start++;
            }
        }
        
        // 2. 끝점 조정 (마지막 스레드 제외)
        // 내 구역의 끝점이 단어의 중간이라면? (예: "Wo"의 'o')
        // 그 단어가 끝날 때까지 내 구역을 늘려서 내가 처리한다.
        if (i != num_threads - 1) {
            while (args[i].end < size && is_word_char(buffer[args[i].end])) {
                args[i].end++;
            }
        }
        
        /* * 위 로직 덕분에, 단어가 잘리는 구간에서는 
         * 앞 스레드가 경계를 넘어서 단어를 끝까지 읽고,
         * 뒤 스레드는 그 단어를 건너뛰고 시작하게 되어 중복/누락을 방지합니다.
         */

        // 스레드 생성 (일 시작!)
        pthread_create(&threads[i], NULL, count_words, &args[i]);
    }

    // [결과 취합 (Reduce)]
    int total = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL); // 스레드가 퇴근할 때까지 대기
        total += args[i].count;         // 각자 세온 단어 수를 합산
    }

    gettimeofday(&wc_end, NULL); // 계산 끝
    free(buffer); // 메모리 해제

    gettimeofday(&total_end, NULL); // 전체 끝

    // 시간 계산
    double io_time = time_diff_ms(io_start, io_end);
    double wc_time = time_diff_ms(wc_start, wc_end);
    double total_time = time_diff_ms(total_start, total_end);

    // 결과 출력
    printf("Total words: %d\n", total);
    printf("Elapsed time (total): %.2f ms\n", total_time);
    printf(" I/O time: %.2f ms\n", io_time);          // 파일 읽는 시간
    printf(" Word count time: %.2f ms\n", wc_time);   // 실제 스레드들이 일한 시간

    return 0;
}