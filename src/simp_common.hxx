// simp_common.hxx
#ifndef _SIMP_COMMON_HXX_
#define _SIMP_COMMON_HXX_

/* common includes */
#include <stdio.h>

/* includes if NOT windows */
#ifndef _MSC_VER
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <termios.h>
#include <netdb.h>  /* for hostent structure, ... */
#include <netinet/in.h>
#include <arpa/inet.h> /* for inet_ntoa(), ... */
#include <string.h> // for strlen()
#endif /* !_MSC_VER */

#define DEF_SLEEP 1  /* default SLEEP (in seconds) */
#define DEF_SOCK_TYPE SOCK_STREAM
#define MAXSTR 1024
#define PORTNUM 5050
#define HOST "localhost"

/* some definitions used */
#ifdef _MSC_VER
/* ========== for windows ========= */
#define SERROR(a) (a == SOCKET_ERROR)
#define SCLOSE closesocket
#define PERROR(a) win_wsa_perror(a)
#define SREAD recv
#define SWRITE send
//#ifndef EINTR
//#define EINTR WSAEINTR
//#endif
#define PRId64 "I64d"
#define PRIu64 "I64u"
/* ================================ */
#else /* !_MSC_VER */
/* =========== unix/linux ========= */
#define SERROR(a) (a < 0)
typedef int SOCKET;
#define SCLOSE close
#define PERROR(a) perror(a)
#define SREAD read
#define SWRITE write
/* ================================ */
#endif /* _MSC_VER y/n */

typedef struct tagINCLIENT {
    SOCKET sock;
    struct sockaddr addr;
    int addrlen, read_cnt, write_cnt;
    time_t created, last_read;
    int had_rw_error;
    int recv_errors, send_errors;
    char IP[32];
    char h_name[256];
}INCLIENT, * PINCLIENT;

#ifndef GOT_DATETIME_STG
#define GOT_DATETIME_STG
#include <time.h>

static char * set_datetime_stg(char * cp, time_t date)
{
    struct tm *tmr;
    tmr = localtime(&date);
    sprintf (cp, "%02d.%02d.%04d %02d:%02d:%02d ",
        tmr->tm_mday,
        tmr->tm_mon+1,
        tmr->tm_year+1900,
        tmr->tm_hour,
        tmr->tm_min,
        tmr->tm_sec);
    return cp;
}

static char * get_datetime_str( void )
{
    static char _s_ds_buf[64];
    char * cp = _s_ds_buf;
    time_t  date = time(0);
    return set_datetime_stg(cp,date);
}

#endif /* #ifndef GOT_DATETIME_STG */

/* ***************************************** */
/* keyboard check */
#ifndef GOT_KEYBOARD_TEST
#define GOT_KEYBOARD_TEST

#ifdef _MSC_VER
#include <conio.h>
int test_for_input(void)
{
    int chr = _kbhit();
    if (chr)
        chr = _getch();
    return chr;
}
#else
int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}
int test_for_input(void)
{
    int chr = kbhit();
    if (chr)
        chr = getchar();
    return chr;
}
#endif
#endif /* #ifndef GOT_KEYBOARD_TEST */
/* ============================================= */

#define EndBuf(a) ( a + strlen(a) )

#ifndef ENDIAN_TEST
#define ENDIAN_TEST
/* simple ENDIAN test */
static const int _s_EndianTest = 1;
#define IsLittleEndian (*((char *) &_s_EndianTest ) != 0)
#define IsBigEndian    (*((char *) &_s_EndianTest ) == 0)
#endif

#ifndef DEF_PAUSE
#define DEF_PAUSE 160 /* default 160 ms pause */
#endif

#ifndef ADDED_CLOCK_WAIT
#define ADDED_CLOCK_WAIT
#ifdef _MSC_VER
#include <time.h>  /* for clock() */
#else // _MSC_VER
#include <sched.h> /* for sched_yield(); */
#endif // _MSC_VER

static void clock_wait(int in_ms)
{
    clock_t ms = in_ms;
    clock_t last_clock, curr_clock, msecs;
    msecs = 0;
#ifdef _MSC_VER
    Sleep(0);
#else // _MSC_VER
    sched_yield(); /* #include <sched.h> */
#endif // _MSC_VER
    last_clock = clock();
    while (msecs < ms) {
        curr_clock = clock();
        // get elapsed milliseconds
        msecs = ((curr_clock - last_clock) * 1000) / CLOCKS_PER_SEC;
        if (msecs < ms) {
#ifdef _MSC_VER
            Sleep(0);
#else // _MSC_VER
            sched_yield(); /* #include <sched.h> */
#endif // _MSC_VER
        }
    }
}

int check_key(void);
static int m_delay_time = DEF_PAUSE;
static int check_key_sleep(int secs, char * name)
{
    int msecs = secs * 1000; /* get ms to wait */
    //int max_wait = 5; /* set max miliseconds to sleep */
    if (secs > 1)
        printf("%s - Sleeping for %d seconds...\n", name, secs);
    while (msecs) {
        if (check_key())
            return 1;
        clock_wait(m_delay_time); /* usually 160ms, about 1/6 of a second - 6Hz */
        if (msecs > m_delay_time)
            msecs -= m_delay_time;
        else
            msecs = 0;
    }
    return 0;
}

static int check_key_sleep_ms(int msecs, char * name)
{
    // printf("%s - Sleeping for %d ms...\n", name, msecs);
    while (msecs) {
        if (check_key())
            return 1;
        clock_wait(m_delay_time); /* usually 160ms, about 1/6 of a second - 6Hz */
        if (msecs > m_delay_time)
            msecs -= m_delay_time;
        else
            msecs = 0;
    }
    return 0;
}

#endif // ADDED_CLOCK_WAIT

#ifndef ISNUM
#define ISNUM(a) ((a >= '0')&&(a <= '9'))
#endif
#ifndef ADDED_IS_DIGITS
#define ADDED_IS_DIGITS

static int is_digits(char * arg)
{
    size_t len,i;
    len = strlen(arg);
    for (i = 0; i < len; i++) {
        if ( !ISNUM(arg[i]) )
            return 0;
    }
    return 1; /* is all digits */
}

#endif // #ifndef ADDED_IS_DIGITS


#endif /* #ifndef _SIMP_COMMON_HXX_ */
/* eof - simp_common.hxx */
