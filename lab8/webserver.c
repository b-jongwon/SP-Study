/* simple-web-server.c */

/* * ======================================================================================
 * [헤더 파일 포함]
 * 리눅스 시스템 프로그래밍과 네트워크를 위한 필수 헤더들입니다.
 * ======================================================================================
 */
#include <stdio.h>      // 표준 입출력 (printf, sscanf, snprintf)
#include <stdlib.h>     // 표준 라이브러리 (exit)
#include <string.h>     // 문자열 처리 (strcmp, strcpy, strlen)
#include <unistd.h>     // 유닉스 표준 시스템 콜 (read, write, close)
#include <fcntl.h>      // 파일 제어 (open, O_RDONLY)
#include <netinet/in.h> // 인터넷 주소 체계 (struct sockaddr_in, htons, INADDR_ANY)
#include <sys/socket.h> // 소켓 핵심 함수 (socket, bind, listen, accept)
#include <sys/types.h>  // 시스템 데이터 타입
#include <sys/stat.h>   // 파일 상태 정보
#include <limits.h>     // 시스템 제한 상수 (PATH_MAX: 경로 최대 길이)

/* * [상수 정의]
 * - PORT 8080: 1024번 이하 포트는 관리자(root) 권한이 필요하므로, 보통 연습용은 8080을 씁니다.
 * - BUF_SIZE 4096: 4KB. 보통 OS의 메모리 페이지 크기와 같아 I/O 효율이 좋습니다.
 */
#define PORT 8080
#define BUF_SIZE 4096
#define PATH_MAX 4096

/* * ======================================================================================
 * [함수: HTTP 요청 처리]
 * 클라이언트(웹 브라우저)와 연결된 소켓(client_fd)을 통해 요청을 읽고 응답을 보냅니다.
 * ======================================================================================
 */
void handle_request(int client_fd) {
    char buffer[BUF_SIZE];
    
    // 1. 요청 읽기 (Read Request)
    // 클라이언트가 보낸 데이터(HTTP Request Packet)를 버퍼로 읽어옵니다.
    // read는 블로킹 함수라 데이터가 올 때까지 대기할 수 있습니다.
    int bytes = read(client_fd, buffer, BUF_SIZE - 1);

    if (bytes <= 0) { // 읽은 바이트가 0이면 연결 종료(EOF), 음수면 에러
        close(client_fd);
        return;
    }

    buffer[bytes] = '\0'; // C언어 문자열의 끝을 알리는 Null Terminator 추가
    printf("Received request:\n%s\n", buffer); // 디버깅용 출력

    // 2. HTTP 파싱 (Parsing)
    // HTTP 요청의 첫 줄은 보통 "GET /index.html HTTP/1.1" 형태입니다.
    char method[8], path[256];
    // sscanf: 문자열에서 형식을 지정해 데이터를 추출. 첫 번째 단어(Method)와 두 번째 단어(Path)만 가져옴.
    sscanf(buffer, "%s %s", method, path);

    // 3. 메소드 검사 (GET 방식만 허용)
    if (strcmp(method, "GET") != 0) {
        // 405 Method Not Allowed: GET 이외의 요청(POST, PUT 등)은 거절
        const char* error = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        write(client_fd, error, strlen(error));
        close(client_fd);
        return;
    }

    // 4. 기본 경로 처리
    // "http://localhost:8080/" 처럼 경로 없이 들어오면 index.html을 보여줌
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }

    // 5. 로컬 파일 경로 완성
    // 웹 루트 디렉토리("./www")와 요청 경로(path)를 합침
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "./www%s", path);

    /* * ==================================================================================
     * [보안 핵심: Directory Traversal 방지]
     * 해커가 "GET /../../etc/passwd" 같은 요청을 보내 서버의 중요 파일을 훔치려 할 수 있습니다.
     * realpath() 함수를 사용하여 상대 경로(..)를 모두 절대 경로로 변환한 뒤 검사해야 합니다.
     * ==================================================================================
     */
    char resolved_path[PATH_MAX]; // 요청한 파일의 실제 절대 경로
    char www_root[PATH_MAX];      // 웹 서버 루트 폴더("./www")의 절대 경로
    
    // 1. 우리 서버의 루트 폴더("./www")가 실제 디스크 어디에 있는지 절대 경로를 구함
    realpath("./www", www_root);
    
    // 2. 요청한 파일의 절대 경로를 구함 (파일이 존재하지 않으면 NULL 반환)
    if (realpath(full_path, resolved_path) != NULL) {
        
        // 3. 요청한 파일의 경로가 웹 루트 경로로 시작하는지 확인 (Prefix Check)
        // 만약 resolved_path가 /etc/passwd 라면 www_root(/home/user/project/www)와 다르므로 차단됨.
        if (strncmp(resolved_path, www_root, strlen(www_root)) != 0) {
            // 403 Forbidden: 접근 권한 없음
            const char* forbidden = 
                "HTTP/1.1 403 Forbidden\r\n\r\n"
                "<h1>403 Forbidden</h1>\n";
            write(client_fd, forbidden, strlen(forbidden));
            close(client_fd);
            return;
        }
    } 
    // realpath가 NULL이면 파일이 없다는 뜻이므로 아래 open에서 처리됨

    

    // 6. 파일 열기 (File Open)
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        // 파일 열기 실패 -> 404 Not Found
        const char* not_found =
            "HTTP/1.1 404 Not Found\r\n\r\n"
            "<h1>404 Not Found</h1>\n";
        write(client_fd, not_found, strlen(not_found));
        close(client_fd);
        return;
    }

    // 7. HTTP 헤더 전송 (200 OK)
    // "\r\n\r\n"은 헤더의 끝을 알리는 필수 마커입니다.
    const char* header = "HTTP/1.1 200 OK\r\n\r\n";
    write(client_fd, header, strlen(header));

    // 8. 파일 내용 전송 (File Transfer)
    // 파일을 조금씩 읽어서(read) 소켓에 씁니다(write).
    char file_buf[BUF_SIZE];
    int n;
    while ((n = read(file_fd, file_buf, BUF_SIZE)) > 0) {
        write(client_fd, file_buf, n); // 소켓으로 데이터 발사
    }

    // 9. 정리 (Clean up)
    close(file_fd);   // 파일 닫기
    close(client_fd); // 클라이언트 연결 끊기 (HTTP/1.0 스타일 - 비지속 연결)
}

/* * ======================================================================================
 * [메인 함수]
 * 서버 소켓을 생성하고, 연결을 기다리는(Listen) 무한 루프를 돕니다.
 * ======================================================================================
 */
int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr; // IPv4 주소 구조체
    socklen_t client_len = sizeof(client_addr);

    /* 1. 소켓 생성 (Socket Creation)
     * - AF_INET: IPv4 인터넷 프로토콜 사용
     * - SOCK_STREAM: TCP 프로토콜 사용 (연결 지향형, 신뢰성 보장)
     */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    /* 2. 소켓 옵션 설정 (SO_REUSEADDR) - ★면접 단골 질문★
     * 서버를 껐다 바로 다시 킬 때, "Address already in use" 에러를 방지합니다.
     * TCP는 연결 종료 후 잠시 'TIME_WAIT' 상태로 포트를 점유하는데, 이를 무시하고 재사용하게 해줍니다.
     */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 3. 주소 구조체 초기화 */
    server_addr.sin_family = AF_INET;
    // htons (Host TO Network Short): 내 컴퓨터의 바이트 순서(Little Endian)를 네트워크 표준(Big Endian)으로 변환
    server_addr.sin_port = htons(PORT); 
    // INADDR_ANY: 내 컴퓨터에 랜카드가 여러 개일 때, 어느 IP로 들어오든 다 처리하겠다 (0.0.0.0)
    server_addr.sin_addr.s_addr = INADDR_ANY;

    /* 4. 바인딩 (Binding)
     * 소켓(전화기)에 전화번호(IP, Port)를 할당하는 과정입니다.
     */
    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    

    /* 5. 리슨 (Listening)
     * 연결 요청을 받을 준비를 합니다. 
     * 5는 'Backlog Queue' 크기로, 동시에 연결 요청이 몰릴 때 대기시킬 수 있는 최대 수입니다.
     */
    listen(server_fd, 5);
    printf("Simple Web Server running at http://localhost:%d\n", PORT);

    /* 6. 연결 수락 루프 (Accept Loop) */
    while (1) {
        // accept: 클라이언트가 연결할 때까지 여기서 프로그램이 멈춰있습니다(Blocking).
        // 연결되면 새로운 소켓 파일 디스크립터(client_fd)를 반환합니다.
        // server_fd는 '연결 대기용', client_fd는 '실제 통신용'입니다.
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        
        // 요청 처리 (여기서는 함수 호출로 처리하므로, 처리가 끝날 때까지 다른 연결 못 받음 -> 싱글 스레드의 한계)
        handle_request(client_fd);
    }

    close(server_fd); // 서버 소켓 닫기 (사실 무한루프라 여기까지 안 옴)
    return 0;
}