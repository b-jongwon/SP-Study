#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <utmp.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>

#define SHOWHOST

int utmp_open(char *);
void utmp_close();
void show_info(struct utmp *);
void showtime(time_t);

int main()
{
    struct utmp     *utbufp    /* holds pointer to next rec    */
	    , *utmp_next();    /* returns pointer to next        */

    if ( utmp_open (UTMP_FILE) == -1){
        perror (UTMP_FILE);
        exit(1);
    }
    while ((utbufp = utmp_next()) != ((struct utmp*) NULL))
        show_info(utbufp);
    utmp_close();
    return 0;
}

void show_info(struct utmp *utbufp)
{
    if (utbufp->ut_type != USER_PROCESS )
        return;

    printf("%-8.8s", utbufp->ut_name);
    printf(" ");
    printf("%-8.8s", utbufp->ut_line);
    printf(" ");
    showtime(utbufp->ut_time);
#ifdef SHOWHOST
    if (utbufp->ut_host[0] != '\0')
        printf(" (%s)", utbufp->ut_host);
#endif
    printf("\n");
}

void showtime(time_t timeval)
{
    char *ctime();
    char    *cp;
    cp = ctime(&timeval);
    printf("%12.12s", cp+4);
}
