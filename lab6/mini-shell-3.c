/* mini-shell-3.c */

/* * ======================================================================================
 * [헤더 파일 포함 구역]
 * 운영체제(커널)에게 "이 기능을 쓰겠습니다"라고 요청하기 위한 도구 상자들입니다.
 * ======================================================================================
 */
#include <stdio.h>      // [표준 입출력] printf(화면출력), fgets(키보드입력), perror(에러메시지)
#include <stdlib.h>     // [유틸리티] malloc(메모리할당), exit(프로세스종료), atoi(문자열->정수변환)
#include <unistd.h>     // [유닉스 표준] fork(복제), execvp(실행), getpid(ID확인), dup2(복사) 등 핵심 시스템 콜
#include <string.h>     // [문자열] strcmp(비교), strncpy(복사), strtok(자르기)
#include <sys/wait.h>   // [대기] waitpid(자식 기다리기), WIFSTOPPED(멈춤확인) 등의 매크로
#include <fcntl.h>      // [파일제어] open(파일열기), O_RDONLY(읽기전용) 등의 상수 정의
#include <ctype.h>      // [문자타입] isspace(공백인지 확인), isalnum(알파벳/숫자 확인)
#include <signal.h>     // [시그널] kill(신호보내기), signal(핸들러등록), SIGTSTP(정지신호), SIGCONT(재개신호)

/* * ======================================================================================
 * [매크로 상수 정의]
 * 코드 중간에 숫자를 하드코딩하지 않고, 여기서 한 번에 관리하여 유지보수성을 높입니다.
 * ======================================================================================
 */
#define MAX_LINE 1024       // 사용자가 입력할 수 있는 명령어의 최대 길이 (예: ls -al ...)
#define MAX_ARGS 64         // 명령어 하나에 붙을 수 있는 옵션의 최대 개수 (예: ls, -a, -l ...)
#define MAX_BLOCK_LINES 32  // if문 블록 안에 저장할 수 있는 최대 줄 수
#define MAX_VARS 64         // 쉘이 기억할 수 있는 변수의 최대 개수
#define MAX_JOBS 64         // 백그라운드나 정지 상태로 관리할 수 있는 작업(Job)의 최대 개수

/* * ======================================================================================
 * [구조체: Job]
 * 백그라운드에서 실행 중이거나(Run), Ctrl+Z로 멈춰있는(Stop) 작업 하나하나의 정보를 담는 그릇입니다.
 * ======================================================================================
 */
typedef struct {
    pid_t pid;              // [Process ID] 운영체제가 부여한 주민등록번호 같은 고유 번호
    char command[MAX_LINE]; // [명령어] 사용자가 입력했던 명령어 문자열 (나중에 'jobs'로 보여줄 때 사용)
    int stopped;            // [상태] 0이면 "실행 중(Running)", 1이면 "멈춤(Stopped)"
} Job;

/* [전역 변수: 작업 리스트] */
Job jobs[MAX_JOBS];     // Job 구조체들을 담을 배열 (작업 목록판)
int job_count = 0;      // 현재 저장된 작업이 몇 개인지 카운트

/* * [전역 변수: Foreground PID]
 * - 역할: 현재 화면(터미널)을 차지하고 사용자의 키보드 입력을 받고 있는 프로세스의 ID입니다.
 * - 왜 전역인가?: 시그널 핸들러(handle_sigtstp) 함수가 이 변수를 참조해야 하는데, 
 * 핸들러 함수는 파라미터를 마음대로 추가할 수 없기 때문입니다.
 * - 값의 의미: -1이면 쉘만 떠 있는 상태, 양수면 특정 프로그램(예: vim)이 실행 중인 상태.
 */
pid_t fg_pid = -1; 

/* [구조체: 환경 변수] A=10 같은 변수를 저장 */
typedef struct {
    char name[64];      // 변수 이름 (예: MY_PATH)
    char value[256];    // 변수 값 (예: /home/user)
} Variable;

/* 변수 저장소 (지역 변수용, 전역 변수용) */
Variable local_vars[MAX_VARS];
int local_var_count = 0;
Variable global_vars[MAX_VARS];
int global_var_count = 0;

/* --- Helper Functions (도우미 함수들) --- */

/* * [함수: 작업 상태 출력]
 * 'jobs' 명령어를 입력했을 때, 배열에 있는 내용을 보기 좋게 출력해줍니다.
 * 예시 출력: [1] Running 1234 sleep 100
 */
void print_job_status(int index) {
    // 삼항 연산자: (조건) ? 참일때값 : 거짓일때값
    const char *status = jobs[index].stopped ? "Stopped" : "Running";
    
    // [문법] %-8s: 문자열을 출력하되 8칸을 확보하고 '왼쪽' 정렬하라. (줄 맞춤 용도)
    printf("[%d] %-8s %d %s\n", index + 1, status, jobs[index].pid, jobs[index].command);
}

/* * [시그널 핸들러: Ctrl+Z (SIGTSTP) 처리]
 * - 상황: 사용자가 키보드에서 Ctrl+Z를 눌렀습니다.
 * - OS 동작: 이 프로세스(쉘)에게 SIGTSTP 시그널을 보냅니다.
 * - 핸들러 동작: 쉘 자신이 멈추는 게 아니라, "현재 실행 중인 자식(fg_pid)"을 멈춰야 합니다.
 */
void handle_sigtstp(int sig) {
    // 현재 포그라운드에서 실행 중인 자식 프로세스가 있다면 (즉, 쉘이 노는 중이 아니라면)
    if (fg_pid > 0) {
        // [시스템 콜] kill: 이름은 kill이지만 실제로는 '신호(Signal) 전송' 함수입니다.
        // 자식 프로세스에게 "너 잠깐 멈춰(SIGTSTP)"라는 신호를 전달(Forwarding)합니다.
        kill(fg_pid, SIGTSTP);
    }
    // 만약 fg_pid가 -1이라면? (아무것도 실행 안 함) -> 그냥 무시합니다. 쉘은 멈추면 안 되니까요.
}

/* * [함수: 변수 값 찾기]
 * 이름(name)을 주면 값(value)을 찾아서 돌려줍니다.
 * 순서: 1.지역변수 -> 2.전역변수 -> 3.시스템 환경변수(getenv)
 */
const char* get_var_value(const char* name) {
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_vars[i].name, name) == 0) return local_vars[i].value;
    }
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_vars[i].name, name) == 0) return global_vars[i].value;
    }
    // [OS API] getenv: 운영체제가 관리하는 환경변수(PATH, HOME 등)를 가져옵니다.
    const char* env = getenv(name);
    return env ? env : ""; // 없으면 NULL 대신 빈 문자열 반환 (안전성 확보)
}

/* [함수: 지역 변수 설정] (배열에 저장하거나 덮어쓰기) */
void set_local_var(const char* name, const char* value) {
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_vars[i].name, name) == 0) {
            strncpy(local_vars[i].value, value, sizeof(local_vars[i].value)); // 이미 있으면 값 갱신
            return;
        }
    }
    // 없으면 새로 추가
    if (local_var_count < MAX_VARS) {
        strncpy(local_vars[local_var_count].name, name, sizeof(local_vars[local_var_count].name));
        strncpy(local_vars[local_var_count].value, value, sizeof(local_vars[local_var_count].value));
        local_var_count++;
    }
}

/* [함수: 전역 변수 설정] (쉘 내부 배열 + OS 환경변수 동시 설정) */
void set_global_var(const char* name, const char* value) {
    if (value == NULL) value = "";
    
    // [OS API] setenv: 현재 프로세스와 자식 프로세스에게 이 환경변수를 물려주도록 설정합니다.
    setenv(name, value, 1); // 1은 "덮어쓰기 허용"

    // 쉘 내부 배열에도 저장 (set_local_var와 로직 동일)
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
 * 문자열 속에 있는 $VAR 패턴을 실제 값으로 교체합니다.
 * 예: "echo $HOME" -> "echo /home/user"
 */
void expand_variables(char* line) {
    char buffer[MAX_LINE];
    int bi = 0;
    for (int i = 0; line[i] != '\0'; ) {
        if (line[i] == '$') { // '$' 발견 시
            i++;
            char varname[64];
            int vi = 0;
            // 알파벳, 숫자, _(언더바)가 아닐 때까지 읽어서 변수명 추출
            while (isalnum(line[i]) || line[i] == '_') {
                varname[vi++] = line[i++];
            }
            varname[vi] = '\0';
            
            // 값 가져와서 버퍼에 복사
            const char* val = get_var_value(varname);
            for (int j = 0; val[j] != '\0'; j++) {
                buffer[bi++] = val[j];
            }
        } else {
            // 일반 문자는 그대로 복사
            buffer[bi++] = line[i++];
        }
    }
    buffer[bi] = '\0'; // 문자열 끝 처리
    strncpy(line, buffer, MAX_LINE); // 원본 교체
}

/* * [함수: 명령어 파싱]
 * 긴 문자열을 공백(스페이스, 탭, 엔터) 기준으로 잘라서 문자열 배열로 만듭니다.
 * 예: "ls -al" -> args[0]="ls", args[1]="-al", args[2]=NULL
 */
void parse_command(char *line, char **args) {
    expand_variables(line); // 변수($)부터 먼저 해석
    int i = 0;
    // [C 문법] strtok: 문자열 토큰화 함수. 원본 문자열의 공백 자리에 NULL(\0)을 채워넣으며 자릅니다.
    char *token = strtok(line, " \t\n"); 
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n"); // NULL을 넣으면 '아까 멈춘 곳'부터 다시 자릅니다.
    }
    args[i] = NULL; // execvp 함수는 인자 배열의 끝이 NULL이어야 함을 요구합니다.
}

/* * ======================================================================================
 * [핵심 함수: 외부 명령어 실행]
 * 1. Fork로 분신을 만듭니다.
 * 2. Child는 execvp로 변신해서 프로그램을 실행합니다.
 * 3. Parent는 Child를 기다리거나(Foreground), 그냥 둡니다(Background).
 * ======================================================================================
 */
int execute_external_command(char **args) {
    int is_bg = 0; // 백그라운드 실행 여부 플래그
    int k = 0;
    
    // 1. 명령어 맨 뒤에 '&'가 있는지 확인
    while (args[k] != NULL) k++; // 인자 개수 세기
    if (k > 0 && strcmp(args[k-1], "&") == 0) {
        is_bg = 1;      // "아, 이건 백그라운드 실행이구나"
        args[k-1] = NULL; // 실행할 명령어에서는 '&'를 지워줍니다. (프로그램 인자가 아니므로)
    }

    // [시스템 콜] fork: 프로세스 복제!
    // - 리턴값 > 0: 부모 프로세스 (리턴값은 자식의 PID)
    // - 리턴값 == 0: 자식 프로세스 (자신)
    // - 리턴값 < 0: 에러 (메모리 부족 등)
    pid_t pid = fork(); 
    
    if (pid < 0) {
        perror("fork");
        return -1;
    } 
    // ================= [자식 프로세스 영역] =================
    else if (pid == 0) {
        // [중요] 시그널 핸들러 복구
        // 부모(쉘)는 Ctrl+Z를 무시하거나 직접 처리하지만, 자식(일반 프로그램)은 
        // Ctrl+Z를 받으면 멈추는 게 정상(Default)입니다. 따라서 설정을 '기본(SIG_DFL)'으로 되돌립니다.
        signal(SIGTSTP, SIG_DFL); 

        // I/O 리다이렉션 처리 (<, >)
        int input_fd = -1, output_fd = -1;
        char *clean_args[MAX_ARGS]; // 리다이렉션 기호를 뺀 순수 명령어
        int j = 0;
        
        for (int i = 0; args[i] != NULL; i++) {
            // 입력 리다이렉션: cmd < file (파일 내용을 키보드 입력처럼)
            if (strcmp(args[i], "<") == 0 && args[i + 1]) {
                input_fd = open(args[i + 1], O_RDONLY);
                // [시스템 콜] dup2(old, new): new(표준입력 0번)가 old(파일)를 가리키게 복제
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
                i++; // 파일명 건너뛰기
            } 
            // 출력 리다이렉션: cmd > file (화면 출력을 파일에 저장)
            else if (strcmp(args[i], ">") == 0 && args[i + 1]) {
                // O_WRONLY(쓰기), O_CREAT(없으면 생성), O_TRUNC(있으면 내용삭제)
                output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(output_fd, STDOUT_FILENO); // 표준출력(1번)을 파일로 돌림
                close(output_fd);
                i++;
            } else {
                clean_args[j++] = args[i];
            }
        }
        clean_args[j] = NULL;
        
        // [시스템 콜] execvp: 현재 프로세스의 메모리를 싹 비우고, 새 프로그램(ls, vim 등)으로 덮어씁니다.
        // 성공하면 리턴하지 않습니다. (이미 다른 프로그램이 되었으므로)
        execvp(clean_args[0], clean_args); 
        
        // 여기까지 왔다면 execvp가 실패한 것입니다. (오타 등)
        fprintf(stderr, "%s: command not found\n", clean_args[0]);
        exit(1); // 자식 종료 (에러)
    } 
    // ================= [부모 프로세스 (쉘) 영역] =================
    else {
        // [Case 1] 백그라운드 실행 (&)
        if (is_bg) {
            // wait(기다림)을 하지 않습니다! 쉘은 즉시 다음 명령을 받을 준비를 합니다.
            if (job_count < MAX_JOBS) {
                // 작업 리스트에 "이 녀석이 백그라운드에서 뛰고 있다"고 기록합니다.
                jobs[job_count].pid = pid;
                strncpy(jobs[job_count].command, args[0], MAX_LINE);
                jobs[job_count].stopped = 0; // 상태: 실행 중(Running)
                printf("[background pid %d]\n", pid); // 사용자에게 알려줌
                job_count++;
            }
            return 0;
        } 
        // [Case 2] 포그라운드 실행
        else {
            fg_pid = pid; // "지금 이 자식이 화면을 쓰고 있어"라고 전역변수에 기록 (시그널 핸들러용)
            int status;
            
            // [시스템 콜] waitpid
            // pid인 자식이 상태가 변할 때까지 부모(쉘)를 잠재웁니다.
            // WUNTRACED 옵션: 자식이 '종료'된 것뿐만 아니라 '멈춘(Stopped)' 상태도 감지해라!
            waitpid(pid, &status, WUNTRACED);
            
            fg_pid = -1; // 자식이 끝났거나 멈췄으므로, 포그라운드는 다시 비게 됩니다.

            // 자식이 왜 waitpid를 깨웠는지 확인
            if (WIFSTOPPED(status)) {
                // "아, 종료된 게 아니라 Ctrl+Z 맞고 기절(Stopped)했구나"
                if (job_count < MAX_JOBS) {
                    jobs[job_count].pid = pid;
                    strncpy(jobs[job_count].command, args[0], MAX_LINE);
                    jobs[job_count].stopped = 1; // 상태: 멈춤(Stopped)
                    printf("\n[Stopped] pid %d\n", pid);
                    job_count++;
                }
            }
            // 정상 종료나 에러 종료라면 그 종료 코드(exit code)를 반환
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
    }
}

/* [함수: 빈 줄 확인] (엔터만 쳤을 때 무시하기 위함) */
int is_blank_line(const char *line) {
    while (*line) {
        if (!isspace(*line)) return 0; // 공백 아닌 게 하나라도 있으면 0
        line++;
    }
    return 1; // 끝까지 공백만 있으면 1
}

// 전방 선언 (함수 프로토타입)
void execute_command(char **args);

/* * [함수: if-then-fi 블록 처리]
 * if 조건문은 여러 줄에 걸쳐 있으므로, 입력을 버퍼링(저장)했다가 조건이 맞으면 실행합니다.
 */
void handle_if_block(char *if_line, FILE *input) {
    char cond_line[MAX_LINE], tmp[MAX_LINE], line[MAX_LINE];
    // "if " 라는 3글자를 떼어내고 조건 명령어만 추출
    strncpy(tmp, if_line, MAX_LINE); tmp[MAX_LINE-1] = '\0';
    if (strlen(if_line) > 3) strncpy(cond_line, if_line + 3, MAX_LINE);
    else cond_line[0] = '\0';

    // 다음 줄이 'then' 인지 확인
    if (!fgets(line, sizeof(line), input)) return;
    char *trimmed = strtok(line, " \t\n");
    if (!trimmed || strcmp(trimmed, "then") != 0) { fprintf(stderr, "Syntax error: expected 'then'\n"); return; }

    // 'fi'가 나올 때까지 명령어들을 메모리(block 배열)에 저장
    char block[MAX_BLOCK_LINES][MAX_LINE];
    int count = 0, found_fi = 0;
    while (fgets(line, sizeof(line), input)) {
        if (is_blank_line(line)) continue;
        char ft_buf[MAX_LINE]; strncpy(ft_buf, line, MAX_LINE);
        char *ft = strtok(ft_buf, " \t\n");
        if (ft && strcmp(ft, "fi") == 0) { found_fi = 1; break; } // fi 발견!
        strncpy(block[count++], line, MAX_LINE);
    }
    if (!found_fi) { fprintf(stderr, "Syntax error: missing 'fi'\n"); return; }

    // 1. 조건 명령어 실행
    char *cargs[MAX_ARGS];
    parse_command(cond_line, cargs);
    
    // 2. 실행 결과가 성공(0)이면, 저장해둔 블록 명령어들을 순차적으로 실행
    if (execute_external_command(cargs) == 0) {
        for (int i = 0; i < count; i++) {
            parse_command(block[i], cargs);
            execute_command(cargs);
        }
    }
}

/* * [함수: 명령어 분류 및 실행]
 * 사용자가 입력한 명령어가 쉘 내장(Built-in)인지 외부(External)인지 판단하여 처리합니다.
 */
void execute_command(char **args) {
    if (args[0] == NULL) return;

    // 1. set: 모든 지역/전역 변수 출력
    if (strcmp(args[0], "set") == 0) {
        for (int i = 0; i < local_var_count; i++) printf("%s=%s\n", local_vars[i].name, local_vars[i].value);
        for (int i = 0; i < global_var_count; i++) printf("export %s=%s\n", global_vars[i].name, global_vars[i].value);
        return;
    }

    // 2. 변수 할당 (예: A=100)
    // '=' 문자가 포함되어 있고, 맨 앞글자는 아닐 때
    if (strchr(args[0], '=') && args[0][0] != '=') {
        char *eq = strchr(args[0], '='); *eq = '\0'; // '='을 기준으로 문자열 분리
        set_local_var(args[0], eq + 1);
        return;
    }

    // 3. exit: 쉘 종료
    if (strcmp(args[0], "exit") == 0) exit(0);

    // 4. export: 전역 변수 설정 (자식에게 상속됨)
    if (strcmp(args[0], "export") == 0) {
        for (int i = 1; args[i]; i++) {
            char *eq = strchr(args[i], '=');
            if (eq) { *eq = '\0'; set_global_var(args[i], eq + 1); }
            else set_global_var(args[i], get_var_value(args[i]));
        }
        return;
    }

    // 5. jobs: 현재 관리 중인 작업 목록 출력
    if (strcmp(args[0], "jobs") == 0) {
        for (int i = 0; i < job_count; i++) {
            print_job_status(i);
        }
        return;
    }

    // --- [Mini-Shell-3의 핵심 기능] fg 명령어 ---
    // 백그라운드에 있거나 정지된 작업을 포그라운드로 가져와서 다시 실행합니다.
    if (strcmp(args[0], "fg") == 0) {
        int job_idx = -1;
        
        if (args[1] == NULL) {
            // 인자가 없으면 마지막(가장 최근) 작업을 가져옵니다. (예: % fg)
            if (job_count > 0) job_idx = job_count - 1;
        } else {
            // 인자가 있으면 해당 번호의 작업을 가져옵니다. (예: % fg 1)
            // 사용자에게는 1번부터 보여주지만, 배열 인덱스는 0번부터라 -1을 합니다.
            job_idx = atoi(args[1]) - 1;
        }

        // 유효한 작업 번호인지 검사
        if (job_idx >= 0 && job_idx < job_count) {
            Job *job = &jobs[job_idx]; // 해당 작업 구조체 포인터
            printf("Resuming job [%d] %s\n", job_idx + 1, job->command);
            
            // [시스템 콜] kill(pid, SIGCONT)
            // SIGCONT: Stopped 상태인 프로세스를 다시 깨우는(Running) 마법의 신호입니다.
            kill(job->pid, SIGCONT);
            job->stopped = 0; // 상태를 '실행 중'으로 업데이트
            
            // 이제 이 프로세스가 화면(Foreground)을 차지합니다.
            fg_pid = job->pid; 
            int status;
            
            // 다시 끝날 때까지 기다립니다 (Blocking)
            waitpid(fg_pid, &status, WUNTRACED);
            fg_pid = -1; // 대기 끝

            // 만약 사용자가 "아냐 다시 멈춰" 하고 또 Ctrl+Z를 눌렀다면?
            if (WIFSTOPPED(status)) {
                job->stopped = 1; // 다시 Stopped 상태로 변경
                printf("\n[Stopped] pid %d\n", job->pid);
            } else {
                // 프로세스가 완전히 종료된 경우 (Job 리스트에서 삭제해야 함)
                // [알고리즘] 배열 중간의 요소를 삭제하는 방법:
                // 삭제할 위치 뒤에 있는 모든 요소들을 한 칸씩 앞으로 당깁니다 (Shift).
                for (int j = job_idx; j < job_count - 1; j++) {
                    jobs[j] = jobs[j+1];
                }
                job_count--; // 전체 개수 감소
            }
        } else {
            fprintf(stderr, "fg: no such job\n"); // 잘못된 번호 에러
        }
        return;
    }

    // 7. 내장 명령어가 아니면 외부 프로그램 실행
    execute_external_command(args);
}

/* [함수: 한 줄 처리 프로세스] (입력 -> 파싱 -> 실행) */
void process_line(char *line, FILE *input) {
    char *args[MAX_ARGS];
    if (is_blank_line(line)) return;
    
    // if문 처리와 일반 명령 처리를 분기
    if (strncmp(line, "if ", 3) == 0) handle_if_block(line, input);
    else { parse_command(line, args); execute_command(args); }
}

/* [메인 함수] 쉘의 진입점 */
int main(int argc, char *argv[]) {
    // [초기화] 시그널 핸들러 등록
    // 쉘이 켜지자마자 "Ctrl+Z(SIGTSTP)가 오면 handle_sigtstp 함수를 실행해라!"라고 OS에 등록.
    // 이걸 안 하면 Ctrl+Z 누르는 순간 쉘 자체가 백그라운드로 쫓겨나거나 멈춰버립니다.
    signal(SIGTSTP, handle_sigtstp);
    
    char line[MAX_LINE];
    
    // 모드 1: 스크립트 파일 실행 (예: ./shell script.sh)
    if (argc == 2) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { perror("fopen"); return 1; }
        // 파일 끝(EOF)까지 한 줄씩 읽어서 실행
        while (fgets(line, sizeof(line), fp)) process_line(line, fp);
        fclose(fp);
        return 0;
    }
    
    // 모드 2: 대화형 모드 (Interactive Mode)
    // 무한 루프를 돌며 사용자 입력을 기다립니다.
    while (1) {
        printf("mini-shell> "); // 프롬프트 출력
        fflush(stdout); // 버퍼 비우기 (글자 즉시 출력)
        
        // 사용자 입력 대기 (Ctrl+D 입력 시 NULL 반환 -> 루프 종료)
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        
        process_line(line, stdin);
    }
    return 0;
}