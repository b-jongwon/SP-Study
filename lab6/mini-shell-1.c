/* mini-shell-1.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>  // open(), O_RDONLY 등의 상수를 위해 필요
#include <ctype.h>  // isspace(), isalnum() 등의 문자 확인 함수

#define MAX_LINE 1024        // 한 줄의 최대 길이
#define MAX_ARGS 64          // 명령어 인자의 최대 개수
#define MAX_BLOCK_LINES 32   // if 블록 내부의 최대 줄 수
#define MAX_VARS 64          // 저장 가능한 변수의 최대 개수

// 변수(이름과 값)를 저장할 구조체 정의
typedef struct {
    char name[64];
    char value[256];
} Variable;

// 쉘 내부에서만 사용하는 지역 변수 저장소
Variable local_vars[MAX_VARS];
int local_var_count = 0;

// export 명령어로 설정되어 자식 프로세스에도 상속될 전역 변수 저장소
Variable global_vars[MAX_VARS];
int global_var_count = 0;

// [함수] 변수 값 가져오기
// 우선순위: 지역 변수 -> 전역 변수 -> 시스템 환경 변수(getenv)
const char* get_var_value(const char* name) {
    // 1. 지역 변수 검색
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_vars[i].name, name) == 0)
            return local_vars[i].value;
    }
    // 2. 전역 변수 검색
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_vars[i].name, name) == 0)
            return global_vars[i].value;
    }
    // 3. 시스템 환경 변수 검색 (예: PATH, HOME 등)
    const char* env = getenv(name);
    return env ? env : ""; // 없으면 빈 문자열 반환
}

// [함수] 지역 변수 설정 (VAR=val 형태)
void set_local_var(const char* name, const char* value) {
    // 이미 있는 변수면 값만 업데이트
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_vars[i].name, name) == 0) {
            strncpy(local_vars[i].value, value, sizeof(local_vars[i].value));
            return;
        }
    }
    // 새 변수라면 배열에 추가
    if (local_var_count < MAX_VARS) {
        strncpy(local_vars[local_var_count].name, name, sizeof(local_vars[local_var_count].name));
        strncpy(local_vars[local_var_count].value, value, sizeof(local_vars[local_var_count].value));
        local_var_count++;
    } else {
        fprintf(stderr, "Error: too many local variables (max %d)\n", MAX_VARS);
    }
}

// [함수] 전역 변수 설정 (export VAR=val 형태)
// 쉘 내부 배열에도 저장하고, 실제 환경 변수(setenv)로도 등록함
void set_global_var(const char* name, const char* value) {
    if (value == NULL) value = "";
    
    // setenv: 현재 프로세스의 환경 변수를 설정 (자식 프로세스 생성 시 상속됨)
    // 세 번째 인자 1은 덮어쓰기 허용을 의미
    setenv(name, value, 1);

    // 내부 관리용 배열에도 저장 (로직은 set_local_var와 동일)
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
    } else {
        fprintf(stderr, "Error: too many global variables (max %d)\n", MAX_VARS);
    }
}

// [함수] 입력 라인의 $VAR 패턴을 실제 값으로 치환 (Variable Expansion)
void expand_variables(char* line) {
    char buffer[MAX_LINE]; // 치환된 결과를 임시 저장할 버퍼
    int bi = 0; // buffer 인덱스

    for (int i = 0; line[i] != '\0'; ) {
        // '$' 기호를 발견하면 변수 치환 시작
        if (line[i] == '$') {
            i++; // '$' 건너뜀
            char varname[64];
            int vi = 0;
            // 변수명 읽기 (알파벳, 숫자, 언더바가 아닐 때까지)
            while (isalnum(line[i]) || line[i] == '_') {
                varname[vi++] = line[i++];
            }
            varname[vi] = '\0';
            
            // 변수 값을 가져와서 버퍼에 복사
            const char* val = get_var_value(varname);
            for (int j = 0; val[j] != '\0'; j++) {
                buffer[bi++] = val[j];
            }
        } else {
            // 일반 문자는 그대로 버퍼에 복사
            buffer[bi++] = line[i++];
        }
    }
    buffer[bi] = '\0'; // 문자열 끝 처리
    strncpy(line, buffer, MAX_LINE); // 원본 라인을 치환된 문자열로 교체
}

// [함수] 명령어 파싱: 문자열을 공백 기준으로 잘라 인자 배열로 만듦
void parse_command(char *line, char **args) {
    expand_variables(line); // 먼저 변수($VAR) 치환 수행

    int i = 0;
    // strtok: 공백, 탭, 엔터를 구분자로 토큰 분리
    char *token = strtok(line, " \t\n");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL; // 인자 리스트의 끝은 항상 NULL이어야 함 (execvp 요구사항)

    if (token != NULL)
        fprintf(stderr, "Warning: too many arguments (max %d); some were ignored\n", MAX_ARGS - 1);
}

// [함수] 외부 명령어 실행 (핵심 기능: Fork, Exec, Redirection)
int execute_external_command(char **args) {
    pid_t pid = fork(); // 1. 프로세스 복제

    if (pid < 0) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // ============= [자식 프로세스 영역] =============
        int input_fd = -1, output_fd = -1;
        char *clean_args[MAX_ARGS]; // 리다이렉션 기호를 제외한 순수 명령어 저장용
        int j = 0;
        
        // 인자를 순회하며 리다이렉션(<, >) 처리
        for (int i = 0; args[i] != NULL; i++) {
            // Input Redirection (< filename)
            if (strcmp(args[i], "<") == 0 && args[i + 1]) {
                input_fd = open(args[i + 1], O_RDONLY); // 파일 읽기 모드로 열기
                if (input_fd < 0) {
                    perror("open for input");
                    exit(1);
                }
                // 표준 입력(0)을 파일 디스크립터로 교체
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
                i++; // 파일명 인자 건너뛰기
            } 
            // Output Redirection (> filename)
            else if (strcmp(args[i], ">") == 0 && args[i + 1]) {
                // 파일 쓰기 모드로 열기 (없으면 생성, 있으면 내용 삭제)
                output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd < 0) {
                    perror("open for output");
                    exit(1);
                }
                // 표준 출력(1)을 파일 디스크립터로 교체
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
                i++; // 파일명 인자 건너뛰기
            } 
            else {
                // 리다이렉션이 아니면 실행할 명령어로 저장
                clean_args[j++] = args[i];
            }
        }
        clean_args[j] = NULL;
        
        // 명령어 실행 (현재 프로세스 메모리를 새 프로그램으로 덮어씀)
        execvp(clean_args[0], clean_args);
        
        // execvp가 실패하면 아래 코드가 실행됨
        fprintf(stderr, "%s: command not found\n", clean_args[0]);
        exit(1);
    } else {
        // ============= [부모 프로세스 영역] =============
        int status;
        waitpid(pid, &status, 0); // 자식이 종료될 때까지 대기
        
        // 자식의 종료 코드(Exit Code) 반환
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
}

// [함수] 빈 줄인지 확인하는 유틸리티
int is_blank_line(const char *line) {
    while (*line) {
        if (!isspace(*line)) return 0; // 공백이 아닌 문자가 있으면 0
        line++;
    }
    return 1; // 끝까지 공백만 있으면 1
}

// 전방 선언: handle_if_block에서 재귀적으로 호출하기 위함
void execute_command(char **args);

// [함수] if-then-fi 블록 처리
// 조건문을 확인하고, 'fi'가 나올 때까지 명령어를 저장했다가 조건이 참이면 실행
void handle_if_block(char *if_line, FILE *input) {
    char cond_line[MAX_LINE];
    char tmp[MAX_LINE];
    
    // 1. "if " 다음의 조건 명령어 추출
    strncpy(tmp, if_line, MAX_LINE);
    tmp[MAX_LINE - 1] = '\0';
    
    if (strlen(if_line) > 3) {
        strncpy(cond_line, if_line + 3, MAX_LINE); // "if " 3글자 제외하고 복사
    } else {
        cond_line[0] = '\0';
    }

    // 2. 다음 줄이 'then'인지 확인
    char line[MAX_LINE];
    if (!fgets(line, sizeof(line), input)) {
        fprintf(stderr, "Syntax error: expected 'then'\n");
        return;
    }
    
    char *trimmed = strtok(line, " \t\n");
    if (!trimmed || strcmp(trimmed, "then") != 0) {
        fprintf(stderr, "Syntax error: expected 'then'\n");
        return;
    }

    // 3. 'fi'가 나올 때까지 내부 블록 명령어들을 메모리에 저장 (버퍼링)
    char block[MAX_BLOCK_LINES][MAX_LINE];
    int count = 0;
    int found_fi = 0;

    while (fgets(line, sizeof(line), input)) {
        if (is_blank_line(line)) continue;
        
        // 첫 번째 토큰이 'fi'인지 확인을 위해 복사본 사용
        char first_token_buf[MAX_LINE];
        strncpy(first_token_buf, line, MAX_LINE);
        char *first_token = strtok(first_token_buf, " \t\n");

        if (first_token && strcmp(first_token, "fi") == 0) {
            found_fi = 1;
            break;
        }

        if (count >= MAX_BLOCK_LINES) {
            fprintf(stderr, "Error: too many lines in if block\n");
            return;
        }
        // 실행하지 않고 저장만 해둠
        strncpy(block[count], line, MAX_LINE);
        block[count][MAX_LINE - 1] = '\0';
        count++;
    }

    if (!found_fi) {
        fprintf(stderr, "Syntax error: missing 'fi'\n");
        return;
    }

    // 4. 조건 명령어 실행 및 결과 확인
    char *args[MAX_ARGS];
    parse_command(cond_line, args);
    int cond_result = execute_external_command(args); // 조건 실행

    // 5. 조건이 참(Exit Code 0)이면 저장해둔 블록 내 명령어들 실행
    if (cond_result == 0) { 
        for (int i = 0; i < count; i++) {
             // 저장된 라인을 다시 파싱하고 실행 (재귀적 구조)
            parse_command(block[i], args);
            execute_command(args);
        }
    }
}

// [함수] 명령어 종류에 따라 내장 명령어 또는 외부 명령어로 분기
void execute_command(char **args) {
    if (args[0] == NULL) return;

    // 1. 내장 명령어: set (모든 변수 출력)
    if (strcmp(args[0], "set") == 0) {
        for (int i = 0; i < local_var_count; i++) {
            printf("%s=%s\n", local_vars[i].name, local_vars[i].value);
        }
        for (int i = 0; i < global_var_count; i++) {
            printf("export %s=%s\n", global_vars[i].name, global_vars[i].value);
        }
        return;
    }

    // 2. 지역 변수 할당 처리 (VAR=val 형태 감지)
    // '=' 문자가 포함되어 있고, 맨 앞 글자는 아닐 때
    if (strchr(args[0], '=') && args[0][0] != '=') {
        char *eq = strchr(args[0], '=');
        *eq = '\0'; // '='을 기준으로 문자열 분리
        set_local_var(args[0], eq + 1); // 이름, 값 등록
        return;
    }

    // 3. 내장 명령어: exit
    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }

    // 4. 내장 명령어: export (전역 변수 설정)
    if (strcmp(args[0], "export") == 0) {
        for (int i = 1; args[i]; i++) { // export A=10 B=20 처럼 여러 개 가능
            char *eq = strchr(args[i], '=');
            if (eq) {
                *eq = '\0';
                set_global_var(args[i], eq + 1);
            } else {
                // export VAR 처럼 값 없이 이름만 있는 경우 기존 값 찾아 등록
                const char* val = get_var_value(args[i]);
                set_global_var(args[i], val);
            }
        }
        return;
    }

    // 5. 그 외에는 외부 명령어(ls, pwd 등)로 간주하고 실행
    execute_external_command(args);
}

// [함수] 한 줄 처리 로직 (인터랙티브 모드와 파일 모드 공용)
void process_line(char *line, FILE *input) {
    char *args[MAX_ARGS];
    if (is_blank_line(line)) return;

    // 'if'로 시작하는지 검사
    if (strncmp(line, "if ", 3) == 0) {
        handle_if_block(line, input);
    } else {
        // 일반 명령어 처리
        parse_command(line, args);
        execute_command(args);
    }
}

// [메인 함수] 쉘 진입점
int main(int argc, char *argv[]) {
    char line[MAX_LINE];

    // 모드 1: 스크립트 파일 실행 모드 (./shell script.sh)
    if (argc == 2) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) {
            perror("fopen");
            return 1;
        }
        // 파일 끝까지 한 줄씩 읽어서 실행
        while (fgets(line, sizeof(line), fp))
            process_line(line, fp);
        fclose(fp);
        return 0;
    }

    // 모드 2: 인터랙티브 모드 (사용자 입력 대기)
    while (1) {
        printf("mini-shell> "); // 프롬프트 출력
        fflush(stdout); // 버퍼 비우기 (출력 즉시 표시)
        
        // EOF(Ctrl+D) 입력 시 종료
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;
            
        process_line(line, stdin);
    }
    return 0;
}