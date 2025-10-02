#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#define BUFFERSIZE 4096
#define COPYMODE 0644
void oops(char* s1, char* s2);
void cpdr(char* source, char* dest);
void cpfile(char* source, char* dest);

int main(int ac, char* av[]) {
    struct stat st;
    
    if (ac != 3) {
        fprintf(stderr, "usage: %s source destination\n", *av);
        exit(1);
    }
    
    if (stat(av[1], &st) == -1) {
        oops("Cannot stat", av[1]);
    }
    
    cpdr(av[1], av[2]);
    
    return 0;
}

void cpdr(char* source, char* dest) {
    DIR* dir_ptr;
    struct dirent* direntp;
    struct stat st;
    
    if (stat(dest, &st) == -1) {
        if (mkdir(dest, 0755) == -1) {
            oops("Cannot create directory", dest);
        }
    } else if (!S_ISDIR(st.st_mode)) {
        oops("Destination is not a directory", dest);
    }
    
    if ((dir_ptr = opendir(source)) == NULL) {
        oops("Cannot open directory", source);
    }
    
    while ((direntp = readdir(dir_ptr)) != NULL) {
        if (strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0) {
            continue;
        }
        
        char source_path[1024];
        char dest_path[1024];
        
        sprintf(source_path, "%s/%s", source, direntp->d_name);
        sprintf(dest_path, "%s/%s", dest, direntp->d_name);
        
        if (stat(source_path, &st) == -1) {
            oops("Cannot stat", source_path);
        }
        
        if (S_ISDIR(st.st_mode)) {
            cpdr(source_path, dest_path);
        } else {
            cpfile(source_path, dest_path);
        }
        
        if (chmod(dest_path, st.st_mode) == -1) {
            oops("Cannot change mode on", dest_path);
        }
    }
    closedir(dir_ptr);
}

void cpfile(char* source, char* dest) {
    int in_fd, out_fd, n_chars;
    char buf[BUFFERSIZE];
    
    if ((in_fd = open(source, O_RDONLY)) == -1) {
        oops("Cannot open ", source);
    }
    if ((out_fd = creat(dest, COPYMODE)) == -1) {
        oops("Cannot creat", dest);
    }
    
    while ((n_chars = read(in_fd, buf, BUFFERSIZE)) > 0) {
        if (write(out_fd, buf, n_chars) != n_chars) {
            oops("Write error to ", dest);
        }
    }
    if (n_chars == -1) {
        oops("Read error from ", source);
    }
    
    if (close(in_fd) == -1 || close(out_fd) == -1) {
        oops("Error closing files", "");
    }
}

void oops(char* s1, char* s2) {
    fprintf(stderr, "Error: %s ", s1);
    perror(s2);
    exit(1);
}
