#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#define PTS_PATH "/dev/pts/"
#define MAX_PATH 256
#define MAX_FORMATTED_MESSAGE_LENGTH 1024 

int main(int argc, char *argv[])
{
    if(argc <2)
    {
        fprintf(stderr , "Usage: %s <message>\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    char *message=argv[1];
    char formatted_message[MAX_FORMATTED_MESSAGE_LENGTH];

   
    int len = snprintf(formatted_message, MAX_FORMATTED_MESSAGE_LENGTH, 
                       "[Broadcast] %s\n", message);
    
 
    size_t message_len = (size_t)len;

    
    DIR *dirp;
    struct dirent *dp;

    dirp = opendir(PTS_PATH);
    if (dirp == NULL) {
       
        perror("Error opening /dev/pts"); 
        exit(EXIT_FAILURE);
    }

    
    while ((dp = readdir(dirp)) != NULL) {
        char full_path[MAX_PATH];
        int fd;
        
        
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }

        
        if (dp->d_name[0] < '0' || dp->d_name[0] > '9') {
             continue; 
        }

      
        snprintf(full_path, MAX_PATH, "%s%s", PTS_PATH, dp->d_name);

      
        fd = open(full_path, O_WRONLY);

       
        if (fd < 0) {
            continue; 
        }

       
        write(fd, formatted_message, message_len);

       
        close(fd);
    }

    closedir(dirp);

    return 0;


}
