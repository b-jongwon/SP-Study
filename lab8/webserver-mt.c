/* thread-pool-server.c */

/* * ======================================================================================
 * [헤더 파일 포함]
 * - netinet/in.h, sys/socket.h: 네트워크 소켓 프로그래밍의 핵심 (주소 구조체, 소켓 함수)
 * - pthread.h: 멀티스레딩 (뮤텍스, 조건변수, 스레드 생성)
 * - fcntl.h: 파일 제어 (open 함수 옵션 등)
 * - unistd.h: 유닉스 표준 시스템 콜 (read, write, close)
 * - limits.h: 시스템 제한 상수 (PATH_MAX 등)
 * ======================================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <limits.h>

/* * [매크로 상수 정의]
 * - PORT: 서버가 귀를 기울일 포트 번호 (8080은 보통 개발용 웹서버 포트)
 * - BUF_SIZE: 데이터 송수신 버퍼 크기 (4KB는 메모리 페이지 크기와 유사해 효율적)
 * - MAX_QUEUE: 대기열(Queue)에 쌓아둘 수 있는 최대 클라이언트 요청 수
 * - THREAD_POOL_SIZE: 미리 만들어둘 일꾼(Worker) 스레드의 개수
 */
#define PORT 8080
#define BUF_SIZE 4096
#define MAX_QUEUE 16
#define THREAD_POOL_SIZE 4
#define PATH_MAX 4096

/* * ======================================================================================
 * [공유 자원: 작업 대기열 (Circular Queue)]
 * 메인 스레드(생산자)가 클라이언트 소켓(fd)을 넣고, 워커 스레드(소비자)가 꺼내가는 공간입니다.
 * 모든 스레드가 공유하므로 반드시 동기화가 필요합니다.
 * ======================================================================================
 */
int queue[MAX_QUEUE]; // 소켓 파일 디스크립터(int)를 저장하는 배열
int front = 0;        // 데이터를 꺼낼 위치 (Head)
int rear = 0;         // 데이터를 넣을 위치 (Tail)
int count = 0;        // 현재 대기열에 있는 요청 개수

/* * [동기화 객체 (Synchronization Primitives)]
 * - mutex: 대기열(queue)에 접근할 때 한 번에 한 놈만 건드리게 하는 자물쇠.
 * - cond_nonempty: "큐가 비어있지 않다(일감이 있다)"라고 워커들을 깨우는 신호.
 * - cond_nonfull: "큐가 꽉 차지 않았다(공간 있다)"라고 메인 스레드를 깨우는 신호.
 */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_nonempty = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_nonfull = PTHREAD_COND_INITIALIZER;

/* * ======================================================================================
 * [함수: HTTP 요청 처리 (Business Logic)]
 * 워커 스레드가 실제로 수행하는 일입니다.
 * 1. 요청 읽기 -> 2. 파싱 -> 3. 파일 찾기 -> 4. 응답 보내기
 * ======================================================================================
 */
void handle_request(int client_fd) {
    char buffer[BUF_SIZE];
    
    // 1. 소켓에서 데이터 읽기 (브라우저가 보낸 요청)
    int bytes = read(client_fd, buffer, BUF_SIZE - 1);
    if (bytes <= 0) {
        close(client_fd); // 읽기 실패하거나 연결 끊김
        return;
    }
    buffer[bytes] = '\0'; // 문자열 끝 처리

    // 2. HTTP 요청 라인 파싱 (예: "GET /index.html HTTP/1.1")
    char method[8], path[256];
    // sscanf로 첫 번째 단어(Method)와 두 번째 단어(Path)만 추출
    sscanf(buffer, "%s %s", method, path);

    // 3. 메소드 검사 (GET 방식만 지원)
    if (strcmp(method, "GET") != 0) {
        const char* error = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        write(client_fd, error, strlen(error));
        close(client_fd);
        return;
    }

    // 4. 루트 경로("/") 처리 -> index.html로 매핑
    if (strcmp(path, "/") == 0)
        strcpy(path, "/index.html");

    // 5. 파일 경로 조합 (./www + /index.html)
    char full_path[512];
    // snprintf는 버퍼 오버플로우를 방지하는 안전한 함수입니다.
    snprintf(full_path, sizeof(full_path), "./www%s", path);

    // * [보안 중요!] Directory Traversal 공격 방지 로직 *
    // 해커가 "GET /../../etc/passwd" 같은 요청을 보낼 수 있음.
    // realpath 함수는 상대 경로(..)를 모두 해석해서 절대 경로로 변환해줌.
    char resolved_path[PATH_MAX];
    char www_root[PATH_MAX];
    
    realpath("./www", www_root); // 웹 루트의 절대 경로 구하기

    // 요청한 파일의 절대 경로를 구하고, 그게 웹 루트(www_root)로 시작하는지 검사
    if (realpath(full_path, resolved_path) != NULL) {
        if (strncmp(resolved_path, www_root, strlen(www_root)) != 0) {
            // 웹 루트 밖의 파일(예: 시스템 파일)을 요청했다면 403 Forbidden
            const char* forbidden = "HTTP/1.1 403 Forbidden\r\n\r\n<h1>403 Forbidden</h1>\n";
            write(client_fd, forbidden, strlen(forbidden));
            close(client_fd);
            return;
        }
    }

    // 6. 파일 열기
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        // 파일이 없으면 404 Not Found 전송
        const char* not_found = "HTTP/1.1 404 Not Found\r\n\r\n<h1>404 Not Found</h1>\n";
        write(client_fd, not_found, strlen(not_found));
        close(client_fd);
        return;
    }

    // 7. 정상 응답 헤더 전송 (200 OK)
    const char* header = "HTTP/1.1 200 OK\r\n\r\n";
    write(client_fd, header, strlen(header));

    // 8. 파일 내용 전송 (반복문으로 끝까지 읽어서 씀)
    char file_buf[BUF_SIZE];
    int n;
    while ((n = read(file_fd, file_buf, BUF_SIZE)) > 0) {
        write(client_fd, file_buf, n);
    }

    // 9. 리소스 정리 (파일 닫기, 소켓 닫기)
    close(file_fd);
    close(client_fd);
}

/* * ======================================================================================
 * [함수: 생산자 (Producer)]
 * 메인 스레드가 accept()로 받은 클라이언트 소켓을 큐에 넣습니다.
 * ======================================================================================
 */
void enqueue(int client_fd) {
    pthread_mutex_lock(&mutex); // 자물쇠 잠금 (임계 영역 시작)

    // 큐가 꽉 찼다면? 빈 자리가 생길 때까지 대기(Wait)
    // cond_nonfull 신호가 올 때까지 mutex를 풀고 잠듦 -> 신호 오면 깨서 다시 mutex 잡음
    while (count == MAX_QUEUE) {
        pthread_cond_wait(&cond_nonfull, &mutex);
    }

    // 데이터 넣기 (원형 큐 알고리즘)
    queue[rear] = client_fd;
    rear = (rear + 1) % MAX_QUEUE; // 뱅글뱅글 돌게 만듦
    count++;

    // "자, 일감 들어왔다!" 하고 자고 있는 워커 스레드 하나를 깨움
    pthread_cond_signal(&cond_nonempty);
    
    pthread_mutex_unlock(&mutex); // 자물쇠 해제 (임계 영역 끝)
}

/* * [함수: 큐에서 꺼내기 (내부용)]
 * 워커 스레드가 호출합니다. (이미 lock이 걸린 상태에서 호출됨을 가정)
 */
int dequeue() {
    int client_fd = queue[front];
    front = (front + 1) % MAX_QUEUE;
    count--;
    return client_fd;
}

/* * ======================================================================================
 * [함수: 소비자 (Consumer / Worker Thread)]
 * 스레드 풀의 각 스레드가 실행할 함수입니다. 무한 루프를 돌며 일감을 기다립니다.
 * ======================================================================================
 */
void* worker_thread(void* arg) {
    while (1) {
        // 1. 일감 가지러 가기 (임계 영역)
        pthread_mutex_lock(&mutex);

        // 큐가 비어있다면? 일감이 들어올 때까지 대기(Wait)
        // cond_nonempty 신호를 기다림
        while (count == 0) {
            pthread_cond_wait(&cond_nonempty, &mutex);
        }

        // 2. 일감 꺼내기
        int client_fd = dequeue();
        
        // "빈 자리 생겼어!" 하고 메인 스레드에게 알려줌
        pthread_cond_signal(&cond_nonfull);
        
        pthread_mutex_unlock(&mutex); // 자물쇠 해제 (중요: 빨리 놔줘야 다른 스레드가 큐에 접근함)

        // 3. 실제 업무 처리 (병렬 처리 구간)
        // *매우 중요*: handle_request는 자물쇠 밖에서 실행해야 합니다.
        // 안 그러면 한 번에 한 명만 일을 하게 되어 멀티스레드 쓰는 의미가 없어짐.
        handle_request(client_fd);
    }
    return NULL;
}

/* * ======================================================================================
 * [메인 함수]
 * 서버 초기화 및 스레드 풀 생성, 연결 수락 루프
 * ======================================================================================
 */
int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // 1. 소켓 생성 (IPv4, TCP)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // [중요] SO_REUSEADDR 옵션 설정
    // 서버를 껐다 켰을 때 "Address already in use" 에러가 나지 않도록,
    // TIME_WAIT 상태의 포트를 재사용하게 해주는 필수 옵션.
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. 주소 구조체 설정
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);       // Host to Network Short (엔디안 변환)
    server_addr.sin_addr.s_addr = INADDR_ANY; // 내 컴퓨터의 모든 IP로 들어오는 요청 수락

    // 3. 바인딩 (소켓에 주소표 붙이기)
    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 4. 리슨 (연결 대기열 생성, 10은 OS backlog 크기)
    listen(server_fd, 10);
    printf("Thread-Pool Web Server running at http://localhost:%d\n", PORT);

    // 5. 스레드 풀 생성 (일꾼 4명 고용)
    pthread_t threads[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        // worker_thread 함수를 실행하는 스레드를 만듦
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    // 6. 무한 루프: 클라이언트 연결 수락 (생산자 역할)
    while (1) {
        // accept: 클라이언트가 올 때까지 여기서 '블락(대기)' 됩니다.
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        // 연결된 소켓(일감)을 큐에 등록
        // 이 함수 안에서 큐가 꽉 차있으면 빌 때까지 대기함
        enqueue(client_fd);
    }

    close(server_fd);
    return 0;
}