#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utmp.h>

#define NRECS   16
#define NULLUT  ((struct utmp *) NULL)
#define UTSIZE  (sizeof(struct utmp))

static char utmpbuf[NRECS * UTSIZE];    /* storage      */
static int num_recs;                    /* num stored   */
static int cur_rec;                     /* next to go   */
static int fd_utmp = -1;                /* read from    */

int utmp_open(char *filename)
{
    fd_utmp = open(filename, O_RDONLY); /* open it      */
    cur_rec = num_recs = 0;             /* no recs yet  */
    return fd_utmp;                     /* report       */
}

void utmp_close()
{
    if (fd_utmp != -1)                  /* don't close if not */
        close(fd_utmp);                 /* open         */
}

int utmp_reload()
{
    int amt_read;
    amt_read = read(fd_utmp, utmpbuf, NRECS * UTSIZE);
    num_recs = amt_read/UTSIZE;
    cur_rec = 0;
    return num_recs;
}

struct utmp *utmp_next()
{
    struct utmp *recp;
    if (fd_utmp == -1)              /* error ?      */
        return NULLUT;
    if (cur_rec == num_recs && utmp_reload() == 0)
        return NULLUT;
    recp = (struct utmp *) &utmpbuf[cur_rec * UTSIZE];
    cur_rec++;
    return recp;
}
