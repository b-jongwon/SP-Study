/* mini-shell-2.c */
/*
 * [헤더 파일 포함]
 * 시스템 콜(운영체제 함수)을 사용하기 위해 필요한 라이브러리들입니다.
 */
#include <stdio.h>      // printf, fgets, perror (표준 입출력)
#include <stdlib.h>     // malloc, free, exit, getenv, setenv (일반 유틸리티)
#include <unistd.h>     // fork, execvp, close, dup2, getpid, sleep (유닉스 표준 시스템 콜)
#include <string.h>     // strcmp, strncpy, strtok, strchr (문자열 처리)
#include <sys/wait.h>   // waitpid, WIFEXITED, WUNTRACED 매크로 (프로세스 웨이팅)
#include <fcntl.h>      // open, O_RDONLY, O_CREAT 등 (파일 제어 옵션)
#include <ctype.h>      // isspace, isalnum (문자 타입 검사)
#include <signal.h>     // signal, kill, SIGTSTP, SIG_DFL (시그널 처리 핵심 헤더)

/* * [상수 정의] 
 * 매직 넘버(하드코딩된 숫자)를 피하고 유지보수를 쉽게 하기 위함입니다.
 */
#define MAX_LINE 1024       // 사용자가 입력할 수 있는 명령줄의 최대 길이
#define MAX_ARGS 64         // 명령어 하나에 붙을 수 있는 인자(옵션)의 최대 개수
#define MAX_BLOCK_LINES 32  // if문 블록 안에 들어갈 수 있는 최대 줄 수
#define MAX_VARS 64         // 저장할 수 있는 커스텀 변수의 최대 개수
#define MAX_JOBS 64         // [Job Control] 관리할 수 있는 백그라운드/정지 작업의 최대 수

/* * [구조체: Job] 
 * 백그라운드에서 실행 중이거나, Ctrl+Z로 멈춰있는 작업의 정보를 담습니다.
 */
typedef struct {
    pid_t pid;              // 프로세스 ID (운영체제가 부여하는 고유 번호, int와 비슷함)
    char command[MAX_LINE]; // 사용자가 입력했던 명령어 (나중에 jobs로 보여주기 위함)
    int stopped;            // 상태 플래그 (0: 실행 중/Running, 1: 멈춤/Stopped)
} Job;

/* * [전역 변수: Job 관리] 
 * 함수들(main, handler, execute) 사이에서 공유되어야 하므로 전역으로 선언합니다.
 */
Job jobs[MAX_JOBS];     // 작업들을 저장할 배열
int job_count = 0;      // 현재 저장된 작업의 개수

/* * [전역 변수: 포그라운드 PID]
 * 현재 사용자의 입력을 독점하고 있는(화면 앞) 프로세스의 ID입니다.
 * -1이면 쉘만 떠 있고 아무것도 실행 안 하는 상태입니다.
 * 시그널 핸들러(handle_sigtstp)가 이 변수를 보고 누구를 멈출지 결정합니다.
 */
pid_t fg_pid = -1;

/* * [구조체: 변수]
 * 쉘 내부 변수(예: A=10)를 저장하기 위한 구조체입니다.
 */
typedef struct {
    char name[64];      // 변수 이름 (예: "HOME")
    char value[256];    // 변수 값 (예: "/home/user")
} Variable;

/* 변수 저장소 (지역 변수용, 전역 변수용) */
Variable local_vars[MAX_VARS];
int local_var_count = 0;
Variable global_vars[MAX_VARS];
int global_var_count = 0;

/*
 * [함수: 작업 상태 출력]
 * 'jobs' 명령어를 쳤을 때 호출됩니다.
 * 예: [1] Running 1234 sleep 100 &
 */
void print_job_status(int index) {
    // 삼항 연산자: stopped가 1이면 "Stopped", 0이면 "Running" 문자열 선택
    const char *status = jobs[index].stopped ? "Stopped" : "Running";
    
    // %-8s: 8칸을 확보하고 왼쪽 정렬 (출력 줄 맞춤용)
    printf("[%d] %-8s %d %s\n", index + 1, status, jobs[index].pid, jobs[index].command);
}

/*
 * [시그널 핸들러: SIGTSTP (Ctrl+Z)]
 * 사용자가 키보드로 Ctrl+Z를 누르면 운영체제는 이 함수를 강제로 호출합니다.
 * 목적: 쉘(부모)은 죽지 않고, 실행 중인 자식 프로세스만 멈추게 하기 위함.
 */
void handle_sigtstp(int sig) {
    // 현재 포그라운드에서 실행 중인 자식 프로세스가 있다면 (fg_pid > 0)
    if (fg_pid > 0) {
        // kill 함수는 이름과 달리 '죽이는' 게 아니라 '시그널을 보내는' 함수입니다.
        // fg_pid 프로세스에게 "잠깐 멈춰(SIGTSTP)"라는 신호를 보냅니다.
        kill(fg_pid, SIGTSTP);
        
        // [주의] 여기서 jobs 배열에 추가하지 않습니다.
        // 시그널을 보내면 자식 프로세스의 상태가 변하고,
        // 그 상태 변화는 부모 프로세스의 waitpid() 함수에서 감지됩니다.
        // 거기서 처리해야 좀비 프로세스 없이 깔끔하게 처리됩니다.
    }
    // 만약 실행 중인 자식이 없다면? 그냥 무시합니다. (쉘 자신은 멈추면 안 되니까)
}

/* * [함수: 변수 값 가져오기]
 * 변수 이름을 주면 값을 찾아 반환합니다.
 * 탐색 순서: 지역변수 -> 전역변수 -> 시스템 환경변수(OS 레벨)
 */
const char* get_var_value(const char* name) {
    // 1. 지역 변수 배열 뒤지기
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_vars[i].name, name) == 0) return local_vars[i].value;
    }
    // 2. 전역 변수 배열 뒤지기
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_vars[i].name, name) == 0) return global_vars[i].value;
    }
    // 3. 시스템 환경 변수(getenv) 뒤지기 (예: PATH, USER 등)
    const char* env = getenv(name);
    return env ? env : ""; // 없으면 빈 문자열("") 반환 (NULL 반환 시 충돌 방지)
}

/* [함수: 지역 변수 설정] (기존 로직과 동일하여 상세 설명 생략) */
void set_local_var(const char* name, const char* value) {
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_vars[i].name, name) == 0) {
            strncpy(local_vars[i].value, value, sizeof(local_vars[i].value));
            return;
        }
    }
    if (local_var_count < MAX_VARS) {
        strncpy(local_vars[local_var_count].name, name, sizeof(local_vars[local_var_count].name));
        strncpy(local_vars[local_var_count].value, value, sizeof(local_vars[local_var_count].value));
        local_var_count++;
    }
}

/* [함수: 전역 변수 설정] (setenv 시스템 콜 사용) */
void set_global_var(const char* name, const char* value) {
    if (value == NULL) value = "";
    // setenv: 현재 프로세스와 앞으로 생성될 자식 프로세스에게 환경 변수를 물려주도록 설정
    setenv(name, value, 1); // 1: 덮어쓰기 허용
    
    // 내부 배열 업데이트 (set_local_var와 로직 동일)
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_vars[i].name, name) == 0) {
            strncpy(global_vars[i].value, value, sizeof(global_vars[i].value));
            return;
        }
    }
    if (global_var_count < MAX_VARS) {
        strncpy(global_vars[global_var_count].name, name, sizeof(global_vars[global_var_count].name));
        strncpy(global_vars[global_var_count].value, value, sizeof(global_vars[global_var_count].value));
        global_var_count++;
    }
}

/* * [함수: 변수 확장]
 * 문자열 속에 있는 $VAR 형태를 찾아서 실제 값으로 바꿔치기 합니다.
 */
void expand_variables(char* line) {
    char buffer[MAX_LINE]; // 변환된 결과를 임시 저장할 공간
    int bi = 0; // buffer의 현재 인덱스
    
    for (int i = 0; line[i] != '\0'; ) {
        if (line[i] == '$') { // '$' 발견!
            i++; // '$' 다음 글자로 이동
            char varname[64];
            int vi = 0;
            // 변수명 추출: 알파벳, 숫자, 언더바(_)일 때까지 계속 읽음
            while (isalnum(line[i]) || line[i] == '_') {
                varname[vi++] = line[i++];
            }
            varname[vi] = '\0'; // 문자열 완성
            
            // 값 가져오기
            const char* val = get_var_value(varname);
            // 가져온 값을 buffer에 복사
            for (int j = 0; val[j] != '\0'; j++) {
                buffer[bi++] = val[j];
            }
        } else {
            // '$'가 아니면 그냥 그대로 복사
            buffer[bi++] = line[i++];
        }
    }
    buffer[bi] = '\0'; // 문자열 끝 표시
    strncpy(line, buffer, MAX_LINE); // 원본 line을 변환된 buffer로 교체
}

/* * [함수: 명령어 파싱]
 * 긴 문자열을 공백 기준으로 잘라서 문자열 배열(args)로 만듭니다.
 * 예: "ls -l" -> args[0]="ls", args[1]="-l", args[2]=NULL
 */
void parse_command(char *line, char **args) {
    expand_variables(line); // 먼저 변수($VAR)부터 다 바꿈
    int i = 0;
    
    // strtok: 문자열을 조각냄 (토큰화). 원본 문자열의 공백 자리에 \0을 넣음.
    char *token = strtok(line, " \t\n"); 
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n"); // 다음 조각 찾기
    }
    args[i] = NULL; // execvp 함수는 인자 배열의 끝이 NULL이어야 함을 요구함
}

/*
 * [핵심 함수: 외부 명령어 실행]
 * fork()를 떠서 실제 프로그램을 실행하고, 백그라운드/포그라운드 처리를 담당합니다.
 */
int execute_external_command(char **args) {
    int is_bg = 0; // 백그라운드(&) 실행인지 여부 (1=True)
    int k = 0;
    
    // 1. 명령어 맨 뒤에 '&'가 있는지 검사
    while (args[k] != NULL) k++; // 인자 개수 세기
    if (k > 0 && strcmp(args[k-1], "&") == 0) {
        is_bg = 1;      // 백그라운드 모드 활성화
        args[k-1] = NULL; // 실행할 명령어 인자에서는 '&' 제거 (ls & -> ls 실행)
    }

    // 2. 프로세스 복제 (Fork)
    // 부모 프로세스(쉘)가 자기를 똑같이 복사해서 자식 프로세스를 만듭니다.
    pid_t pid = fork();
    
    if (pid < 0) { // Fork 실패 (메모리 부족 등)
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // ======================================
        // 여기는 [자식 프로세스]의 세상입니다.
        // ======================================
        
        // [중요] 시그널 핸들링 복구
        // 부모(쉘)는 Ctrl+Z를 무시하거나 직접 처리하지만, 
        // 자식(실행될 프로그램)은 Ctrl+Z를 받으면 멈춰야(Default 동작) 합니다.
        // 따라서 SIG_DFL(Default)로 설정을 되돌립니다.
        signal(SIGTSTP, SIG_DFL);

        // I/O 리다이렉션 처리 (<, >)
        int input_fd = -1, output_fd = -1;
        char *clean_args[MAX_ARGS]; // 리다이렉션 기호 뺀 진짜 명령어 담을 곳
        int j = 0;
        
        for (int i = 0; args[i] != NULL; i++) {
            // 입력 리다이렉션 (< file)
            if (strcmp(args[i], "<") == 0 && args[i + 1]) {
                input_fd = open(args[i + 1], O_RDONLY); // 파일 열기
                // dup2(old, new): 표준 입력(0번)을 파일(input_fd)로 교체
                // 이제 scanf 등은 키보드가 아니라 파일에서 읽게 됨
                dup2(input_fd, STDIN_FILENO); 
                close(input_fd);
                i++; // 파일명 건너뛰기
            } 
            // 출력 리다이렉션 (> file)
            else if (strcmp(args[i], ">") == 0 && args[i + 1]) {
                // 쓰기 전용, 없으면 생성(CREAT), 있으면 내용 싹 지움(TRUNC)
                output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                // 표준 출력(1번)을 파일로 교체. 이제 printf는 모니터가 아니라 파일에 씀
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
                i++;
            } else {
                clean_args[j++] = args[i]; // 순수 명령어 인자만 담기
            }
        }
        clean_args[j] = NULL;
        
        // [핵심] execvp: 현재 프로세스의 메모리를 싹 비우고, 새 프로그램(ls 등)을 로딩
        execvp(clean_args[0], clean_args);
        
        // 만약 execvp가 리턴되면? 에러가 났다는 뜻입니다. (예: 오타로 없는 명령어 입력)
        fprintf(stderr, "%s: command not found\n", clean_args[0]);
        exit(1); // 자식 프로세스 종료 (에러 코드 1)
        
    } else {
        // ======================================
        // 여기는 [부모 프로세스(쉘)]의 세상입니다.
        // ======================================
        
        if (is_bg) {
            // [Case 1: 백그라운드 실행 (&)]
            // 자식이 끝날 때까지 기다리지 않습니다(No wait).
            
            if (job_count < MAX_JOBS) {
                // Job 배열에 등록만 해둡니다.
                jobs[job_count].pid = pid; 
                strncpy(jobs[job_count].command, args[0], MAX_LINE);
                jobs[job_count].stopped = 0; // 실행 중 상태
                printf("[background pid %d]\n", pid); // 사용자에게 PID 알려줌
                job_count++;
            }
            return 0; // 즉시 리턴해서 다음 명령어 입력받으러 감
            
        } else {
            // [Case 2: 포그라운드 실행]
            // 자식이 끝날 때까지, 혹은 멈출 때까지 기다려야 합니다.
            
            fg_pid = pid; // 시그널 핸들러가 알 수 있게 전역변수에 기록
            int status;
            
            // waitpid 옵션 설명:
            // WUNTRACED: 자식이 종료된 것뿐만 아니라 '멈춘(Stopped)' 상태도 감지하라는 뜻.
            // 이게 없으면 Ctrl+Z로 자식이 멈춰도 부모는 계속 기다리는 무한 대기에 빠짐.
            waitpid(pid, &status, WUNTRACED);
            
            fg_pid = -1; // 기다림 끝났으니 초기화

            // 자식이 왜 waitpid를 깨웠는지 검사
            if (WIFSTOPPED(status)) {
                // "아, 자식이 종료된 게 아니라 Ctrl+Z 맞고 기절(Stop)했구나"
                if (job_count < MAX_JOBS) {
                    jobs[job_count].pid = pid;
                    strncpy(jobs[job_count].command, args[0], MAX_LINE);
                    jobs[job_count].stopped = 1; // 상태를 '멈춤'으로 기록
                    printf("\n[Stopped] pid %d\n", pid);
                    job_count++;
                }
            }
            // 정상 종료나 에러 종료라면 종료 코드 반환
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
    }
}

/* [함수: 빈 줄 체크] (엔터만 쳤을 때 무시하기 위함) */
int is_blank_line(const char *line) {
    while (*line) {
        if (!isspace(*line)) return 0; // 공백 아닌 게 하나라도 있으면 0
        line++;
    }
    return 1; // 전부 공백이면 1
}

/* 함수 원형 선언 (상호 참조를 위해 필요) */
void execute_command(char **args);

/* [함수: if-then-fi 블록 처리] (복잡한 제어문 로직) */
void handle_if_block(char *if_line, FILE *input) {
    char cond_line[MAX_LINE], tmp[MAX_LINE], line[MAX_LINE];
    
    // "if " 문자열 제거 로직
    strncpy(tmp, if_line, MAX_LINE); tmp[MAX_LINE-1] = '\0';
    if (strlen(if_line) > 3) strncpy(cond_line, if_line + 3, MAX_LINE);
    else cond_line[0] = '\0';

    // 다음 줄 읽어서 'then' 확인
    if (!fgets(line, sizeof(line), input)) return;
    char *trimmed = strtok(line, " \t\n");
    if (!trimmed || strcmp(trimmed, "then") != 0) { 
        fprintf(stderr, "Syntax error: expected 'then'\n"); 
        return; 
    }

    // 'fi'가 나올 때까지 명령어들을 배열(block)에 저장 (실행 안 하고 모으기)
    char block[MAX_BLOCK_LINES][MAX_LINE];
    int count = 0, found_fi = 0;
    while (fgets(line, sizeof(line), input)) {
        if (is_blank_line(line)) continue;
        char ft_buf[MAX_LINE]; strncpy(ft_buf, line, MAX_LINE);
        char *ft = strtok(ft_buf, " \t\n");
        if (ft && strcmp(ft, "fi") == 0) { found_fi = 1; break; }
        strncpy(block[count++], line, MAX_LINE);
    }
    if (!found_fi) { fprintf(stderr, "Syntax error: missing 'fi'\n"); return; }

    // 저장 끝났으니 조건 실행 (재귀 호출 방지 위해 parse -> execute_external 사용)
    char *cargs[MAX_ARGS];
    parse_command(cond_line, cargs);
    
    // 조건 명령어 실행 결과가 0 (성공, true)이면 저장해둔 블록 실행
    if (execute_external_command(cargs) == 0) {
        for (int i = 0; i < count; i++) {
            parse_command(block[i], cargs);
            execute_command(cargs); // 내부 명령어(if 중첩 등)도 처리 가능하도록
        }
    }
}

/* * [함수: 명령어 분배기]
 * 사용자가 입력한 명령어가 쉘 내장 명령어(Built-in)인지 외부 명령어인지 판단하고 실행합니다.
 */
void execute_command(char **args) {
    if (args[0] == NULL) return; // 빈 명령어

    // 1. set: 모든 변수 출력
    if (strcmp(args[0], "set") == 0) {
        for (int i = 0; i < local_var_count; i++) printf("%s=%s\n", local_vars[i].name, local_vars[i].value);
        for (int i = 0; i < global_var_count; i++) printf("export %s=%s\n", global_vars[i].name, global_vars[i].value);
        return; // 내장 명령어는 fork 없이 여기서 끝냄
    }

    // 2. 지역변수 할당 (예: MY=100)
    // '='이 포함되어 있고, 맨 앞글자는 아닐 때
    if (strchr(args[0], '=') && args[0][0] != '=') {
        char *eq = strchr(args[0], '='); *eq = '\0'; // '='을 기준으로 자름
        set_local_var(args[0], eq + 1);
        return;
    }

    // 3. exit: 쉘 종료
    if (strcmp(args[0], "exit") == 0) exit(0);

    // 4. export: 전역 변수 설정
    if (strcmp(args[0], "export") == 0) {
        for (int i = 1; args[i]; i++) {
            char *eq = strchr(args[i], '=');
            if (eq) { *eq = '\0'; set_global_var(args[i], eq + 1); }
            else set_global_var(args[i], get_var_value(args[i])); // 값 없이 이름만 오면 기존 값 사용
        }
        return;
    }

    // 5. [신규 기능] jobs: 작업 목록 출력
    if (strcmp(args[0], "jobs") == 0) {
        for (int i = 0; i < job_count; i++) {
            print_job_status(i);
        }
        return;
    }

    // 6. 위 경우가 아니면 외부 프로그램(ls, vim 등) 실행
    execute_external_command(args);
}

/* [함수: 한 줄 처리 프로세스] */
void process_line(char *line, FILE *input) {
    char *args[MAX_ARGS];
    if (is_blank_line(line)) return;
    
    // if문 처리와 일반 명령 처리를 분기
    if (strncmp(line, "if ", 3) == 0) handle_if_block(line, input);
    else { parse_command(line, args); execute_command(args); }
}

/* [메인 함수] */
int main(int argc, char *argv[]) {
    // [매우 중요] 시그널 핸들러 등록
    // 쉘이 시작하자마자 "Ctrl+Z가 들어오면 내가 정한 함수(handle_sigtstp)를 실행해!"라고 OS에 등록
    // 이걸 안 하면 쉘에서 Ctrl+Z 누를 때 쉘 자체가 정지되어 버림
    signal(SIGTSTP, handle_sigtstp);

    char line[MAX_LINE];
    
    // 모드 1: 스크립트 파일 실행 (예: ./shell script.sh)
    if (argc == 2) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { perror("fopen"); return 1; }
        while (fgets(line, sizeof(line), fp)) process_line(line, fp);
        fclose(fp);
        return 0;
    }
    
    // 모드 2: 대화형 쉘 (Interactive Shell)
    while (1) {
        printf("mini-shell> "); // 프롬프트 출력
        fflush(stdout); // 버퍼 비우기 (글자 즉시 출력)
        
        // 사용자 입력 대기 (Ctrl+D 누르면 NULL 반환하여 종료)
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        
        process_line(line, stdin);
    }
    return 0;
}