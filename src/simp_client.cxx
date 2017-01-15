// simp_client.cxx
// 2011-03-31 - Some (messy) experiments with SOCKETS
//
// Written by Geoff R. McLane, started March 2011.
//
// Copyright (C) 2011 - ????  Geoff R. McLane  - http://geoffair.org
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id$

#include "config.h" /* always the FIRST include */

/* common includes */
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <memory.h>
#include <time.h>
#include <limits.h>

#ifdef _MSC_VER
////////////////////////////////////////////////////////////////
#include <io.h>
#include <conio.h>
////////////////////////////////////////////////////////////////
#else // !_MSC_VER
////////////////////////////////////////////////////////////////
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h> /* for PRIu64, ... */
#include <termios.h>
////////////////////////////////////////////////////////////////
#endif // _MSC_VER

#include "simp_common.hxx"
#include "winsockerr.cxx"

using namespace std;

#define TRY_USING_SELECT
#undef ADD_UDP_SUPPORT

static char * modName = (char *)"CLIENT:";
static char hostName[MAXSTR];
static int out_count = 0;
static int wait_4_reply = 0;
static int is_paused = 0;
static time_t pause_time;
static time_t msg_wait_time = 30; // show paused, every 30 seconds
static int sleep_secs = DEF_SLEEP;
static SOCKET highest;
static int receive_only = 0;    // -r Only receive - no SEND
static int once_only = 0;       // -1 Only 1 LOOP and exit
static int verbosity = 0;

#define VERB1 (verbosity >= 1)
#define VERB2 (verbosity >= 2)
#define VERB5 (verbosity >= 5)
#define VERB9 (verbosity >= 9)

static char m_data[MAXSTR+16];
static char m_answer[MAXSTR+16];

static int socket_type = DEF_SOCK_TYPE;

static int add_nonblocking = 1;
static int max_recv_errors = 2;
static int max_send_errors = 2;

static INCLIENT m_client;

static time_t cycle_time;
uint64_t cycles = 0;
uint64_t cyc_per_sec = 0;
uint64_t max_cyc_per_sec = 0;

// stats kept
static clock_t begin_clock, curr_clock, last_clock;
static clock_t min_ms_wait = 160; // slow to this rate on client 160 = 6Hz
static clock_t ms_wait_chg = 10;  // i and d increment/decrements

static uint64_t sends_done = 0;
static uint64_t sent_bytes = 0;
static uint64_t recvs_done = 0;
static uint64_t recv_bytes = 0;
static uint64_t t_sends_done = 0;
static uint64_t t_sent_bytes = 0;
static uint64_t t_recvs_done = 0;
static uint64_t t_recv_bytes = 0;
static time_t last_time, begin_time;
static bool not_connected = true;
static int secs_to_sleep = 5;
static int cycle_count = 0;    // cycles while waiting for connection
static struct sockaddr_in serv_addr;
static struct hostent *hostptr;
static int portNumber;
static struct in_addr * pin;
static char * IP;
static PINCLIENT pic = &m_client;
static SOCKET sockfd = 0;

char * get_cps_stg(void)
{
    static char _s_cps[128];
    ostringstream ss;
    ss << cyc_per_sec;
    strcpy(_s_cps, ss.str().c_str());
    return _s_cps;
}

#ifdef TRY_USING_SELECT
static fd_set ListenSocket;
static double curr_timeout = 0.01; // seconds to timeout
static struct timeval timeout;
void set_time_out(void)
{
    int usecs = (int)(curr_timeout * 1000000.0);
    timeout.tv_sec = usecs / 1000000;
    timeout.tv_usec = usecs % 1000000;
}

char * get_to_stg(void)
{
    static char to[32];
    set_time_out();
    sprintf(to, "%u.%06u secs", (unsigned int)timeout.tv_sec, (unsigned int)timeout.tv_usec);
    return to;
}
#endif // TRY_USING_SELECT

#ifndef DEF_MESSAGE_LOG
#define DEF_MESSAGE_LOG "fgio_client.log"
#endif

static char *msg_log = (char *)DEF_MESSAGE_LOG;
static FILE *msg_file = NULL;

void write_msg_log(const char *msg, int len)
{
    if (msg_file == NULL) {
        msg_file = fopen(msg_log,"ab");
        if (!msg_file) {
            printf("ERROR: Failed to OPEN/append %s log file!\n", msg_log);
            msg_file = (FILE *)-1;
        }
    }
    if (msg_file && (msg_file != (FILE *)-1)) {
        int wtn = (int)fwrite(msg,1,len,msg_file);
        if (wtn != len) {
            fclose(msg_file);
            msg_file = (FILE *)-1;
            printf("ERROR: Failed to WRITE %d != %d to %s log file!\n", wtn, len, msg_log);
        }
    }
}

void show_stats(void)
{
    time_t curr_time = time(NULL);
    // get elapsed
    time_t elapsed1 = curr_time - last_time;
    time_t elapsed2 = curr_time - begin_time;
    // update totals
    t_sends_done += sends_done;
    t_sent_bytes += sent_bytes;
    t_recvs_done += recvs_done;
    t_recv_bytes += recv_bytes;
    // show it
    printf("Stats: %4" PRIu64 " sends, %6" PRIu64 " bytes, %4" PRIu64 " recvs, %6" PRIu64 " bytes",
        sends_done, sent_bytes, recvs_done, recv_bytes );
    printf(" in %d secs,\n", (int)elapsed1);
    printf("Total: %4" PRIu64 " sends, %6" PRIu64 " bytes, %4" PRIu64 " recvs, %6" PRIu64 " bytes",
        t_sends_done, t_sent_bytes, t_recvs_done, t_recv_bytes );
    printf(" in %d secs.\n", (int)elapsed2 );
    // clear running totals
    sends_done = 0;
    sent_bytes = 0;
    recvs_done = 0;
    recv_bytes = 0;
    // update time
    last_time = curr_time;
}

void show_help(void)
{
    printf(" ?   = This help...\n");
#if 0 // for server
    printf(" e   = Toggle ECHO. Currently %s\n",
        (do_echo ? "On" : "OFF") );
#endif // 0
    printf(" i   = Increment throttle. Add %d ms to wait - slower. Presently %d\n", (int)ms_wait_chg, (int)min_ms_wait );
    printf(" d   = Decrement throttle. Sub %d ms from wait - faster. Presently %d\n", (int)ms_wait_chg, (int)min_ms_wait );
    printf(" p   = Toggle PAUSE. Currently %s\n", (is_paused ? "On" : "OFF") );
    printf(" s   = Show STATS\n");
    printf(" 01259 = Set verbosity level. Now=%d\n", verbosity );
    printf("ESC    = EXIT application...\n");
    printf("STATS\n");
    if (cyc_per_sec > 0) {
        printf("Running at %" PRIu64 " cycles per second\n",  cyc_per_sec );
    }
    if (min_ms_wait) {
        printf("Throttled to %d millisecs, %d Hz\n", (int)min_ms_wait, (int)(1000 / min_ms_wait) );
    } else {
        printf("No throttle applied\n");
    }
    if (sends_done || recvs_done)
        show_stats();
}


int check_key(void)
{
    int chr = test_for_input();
Do_Next_Char:
    if (chr) {
        switch (chr)
        {
        case 0x1b:
            printf("ESC - Commence exit...\n");
            return 1;
            break;
        case '?':
            show_help();
            break;
#if 0 // for server only
        case 'e':
            do_echo = (do_echo ? 0 : 1);
            printf("e - Toggled ECHO. Currently %\n",
                (do_echo ? "On" : "OFF") );
            break;
#endif // 0
        case 'i':
            min_ms_wait += ms_wait_chg;
            printf("i - Incremented throttle to %d - run slower.\n", (int)min_ms_wait );
            break;
        case 'd':
            if (min_ms_wait > ms_wait_chg)
                min_ms_wait -= ms_wait_chg;
            else
                min_ms_wait = 0;
            printf("d - Decremented throttle to %d - run faster.\n", (int)min_ms_wait );
            break;
        case 'p':
            pause_time = time(0);
            is_paused = (is_paused ? 0 : 1);
            printf("p - Toggled PAUSE. Currently %s\n",
                (is_paused ? "On" : "OFF") );
            break;
        case 'r':
            if (wait_4_reply) {
                wait_4_reply = 0;
                printf("r - Reset wait for reply.\n");
            } else
                printf("r - Wait for reply NOT set!\n");
            break;
        case 's':
            show_stats();
            break;
        case '0':
        case '1':
        case '2':
        case '5':
        case '9':
            verbosity = (chr - '0');
            printf("%c - Set verbosity to %d\n", chr, verbosity);
            break;
        default:
            printf("Unused key input... %d (%x)\n", chr, chr);
            break;
        }
    }
    chr = test_for_input();
    if (chr) goto Do_Next_Char;
    return 0;
}


int error_is_fatal(int err)
{
#ifdef _MSC_VER
    if (err == WSAECONNABORTED)
        return 1;
    if (err == WSAECONNRESET)
        return 1;
    // what other FATAL errors ... or maybe ALL are FATAL
#else // !_MSC_VER
    if (err < 0)
        return 1;
#endif
    return 0;
}

int error_is_not_fatal( int wsa_errno )
{
#ifdef _MSC_VER
    switch (wsa_errno) {
        case WSAEWOULDBLOCK: // always == NET_EAGAIN?
        case WSAEALREADY:
        case WSAEINPROGRESS:
        return 1;
    }
#else // !_MSC_VER
    if (errno == EAGAIN)
        return 1;
#endif // _MSC_VER y/n
    return 0;
}

void clean_data(char * dest, char * src, int len)
{
    int i, off, c;
    off = 0;
    for (i = 0; i < len; i++) {
        c = src[i];
        if (c & 0x80) { // got high bit on
            dest[off++] = '@';
            c &= ~0x80; // strip the HI bit
        }
        if (c < ' ') {  // if LESS than a space
            dest[off++] = '^';  // add leader
            c += '@';   // and bump char into range
        }
        dest[off++] = c;    // store the char
    }
    dest[off] = 0;  // zero terminate it all
}

void show_comp_date(char * name)
{
    printf("Running: %s, version " PACKAGE_VERSION ", compiled on %s, at %s\n",
        name, __DATE__, __TIME__ );
}

void cmd_help(char * name)
{
    printf("HELP\n"
        " -?    = This help, and exit 0\n"
        " -h <host> = Set the HOST name, or IP. Def=%s\n", HOST );
    printf(" -p <port> = Set the PORT number. Def=%d\n", PORTNUM );
#ifdef ADD_UDP_SUPPORT
    printf(" -u        = Use udp socket. Def=tcp\n");
#endif // ADD_UDP_SUPPORT
    printf(" -s secs   = Seconds to wait for re-connection. (def=%d)\n", secs_to_sleep);
    printf(" -v[nn]    = Increment/Set verbosity. Def=%d\n", verbosity );
    printf(" -r        = Receive only. No send done.\n");
    printf(" -1        = Only 1 action and exit. No repeating...\n");
}

// int do_client_connect(int sockfd)
int do_client_connect(void)
{
    int iret = 0;
    int status;
    //struct in_addr * pin;
    //char * IP;
    //PINCLIENT pic = &m_client;
    cycle_count = 0;

	bzero((char *)&serv_addr,sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	memcpy((char *)&serv_addr.sin_addr,(char *)hostptr->h_addr,hostptr->h_length);
	serv_addr.sin_port = htons(portNumber);

	sockfd = (int)socket(AF_INET,socket_type,0);
    if (SERROR(sockfd)) {
        PERROR("ERROR: creating socket!");
        iret = 1;
        return 1;
    }

#ifdef TRY_USING_SELECT
    highest = sockfd;
    sleep_secs = 0; // using 'select' so no sleep timeout
#endif // TRY_USING_SELECT
    printf("CLIENT: Got socket [%d]\n", (int)sockfd);

    while (not_connected)
    {
        status = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
        if (SERROR(status)) {
            if ((cycle_count % 100) == 0) {
                PERROR("ERROR: socket connect FAILED!");
                printf("CLIENT: Will sleep for %d seconds, and try again with socket [%d] (%d)\n",
                    secs_to_sleep, (int)sockfd, cycle_count);
            }
            cycle_count++;
            if (check_key_sleep(secs_to_sleep,modName)) {
                iret = 1;
                break;
            }
        } else {
            not_connected = false;
        }
    }
    if (iret == 0) {
        // show details of server connection
        pin = (struct in_addr *)&serv_addr.sin_addr;
        IP = inet_ntoa ( *pin );
        if (IP) {
            strcpy( pic->IP, IP );
            unsigned int ipval = inet_addr(pic->IP);
            struct hostent * rH = gethostbyaddr((char *)&ipval, sizeof(ipval), AF_INET);
            if (rH)
                strcpy( pic->h_name, rH->h_name );
            else
                strcpy( pic->h_name, "not found");
        } else {
            strcpy( pic->IP, "inet_ntoa failed");
        }
        if (add_nonblocking) {
            printf("Adding non-blocking mode...\n");
            status = setnonblocking(sockfd);
            printf("ioctlsocket(sockfd, FIONBIO,...) returned %d\n", status);
        }

        printf("CLIENT: Connected IP %s, port %d, name [%s]\n",
            pic->IP, ntohs(serv_addr.sin_port), pic->h_name );

        show_stats();

    }
    return iret;
}

// int run_client(SOCKET sockfd)
int run_client(void)
{
    int iret = 0;
    clock_t msec;
    int slen, wlen, rlen, status;
    PINCLIENT pic = &m_client;

    printf("CLIENT: Entering forever loop... ESC key to exit...\n");
    cycle_time = time(0);
    begin_clock = clock();
    last_clock = 0;
    while (1) { /* FOREVER - until ESC key */
        if (not_connected) {
            if ( do_client_connect() ) {
                iret = 1;
                break;
            }
        }

        /* potentially reduce Hz */
        if (last_clock && min_ms_wait) {
            curr_clock = clock();
            // get elapsed milliseconds
            msec = ((curr_clock - last_clock) * 1000) / CLOCKS_PER_SEC;
            if (msec < min_ms_wait) {
                sleep(0);
                if (check_key())
                    break;
                continue;
            }
            last_clock = curr_clock;
        } else
            last_clock = clock();

        cycles++; /* bump the cycles */
        if (time(0) != cycle_time) {
            cycle_time = time(0);
            cyc_per_sec = cycles;
            if (cycles > max_cyc_per_sec)
                max_cyc_per_sec = cycles;
            cycles = 0;
        }
        if (check_key())
            break;
        if (is_paused) {
            if ((time(0) - pause_time) > msg_wait_time) {
                printf("CLIENT: presently PAUSED!\n");
                pause_time = time(0);
            }
            sleep(1);
            continue;
        }

        if ( !receive_only && !wait_4_reply ) {
            // NOT waiting a REPLY to last send
            // so SEND again
            strcpy(m_data, get_datetime_str());
            out_count++;
            sprintf(EndBuf(m_data),": %d - Hello server!\n", out_count);
            slen = (int)strlen(m_data);
            if (VERB9) {
                clean_data(m_answer,m_data,slen);
                printf("[%s] %d bytes sending...\n",
                    m_answer, slen );
            }
#ifdef _MSC_VER
	        wlen = send(sockfd,m_data,slen,0);
            if (SERROR(wlen)) {
                PERROR("ERROR: sending to socket!");
                pic->send_errors++;
                if ( error_is_fatal(last_wsa_error) ) {
                    // hm, can either try to connect again, OR ABORT
                    fprintf(stderr, "CLIENT aborting...\n");
                    break;
                }
                if (pic->send_errors > max_send_errors) {
                    fprintf(stderr, "Maximum send errors exceeded! CLIENT aborting...\n");
                    break;
                }
                if (check_key())
                    break;
                sleep(sleep_secs);
                continue;
            }
#else
	        wlen = write(sockfd,m_data,slen);
            if (wlen < 0) {
                PERROR("ERROR: write to socket!");
                if (check_key())
                    break;
                sleep(sleep_secs);
                continue;
            }
#endif
            sends_done++;
            sent_bytes += wlen;
            if (VERB9)
                printf("CLIENT: Wrote/Sent length %d, sleep %d secs\n",wlen, sleep_secs );
            else if (VERB1)
                printf("S");
                
            if (check_key()) {
                break;
            }
            sleep(sleep_secs);
            if (check_key()) {
                break;
            }
            wait_4_reply = 1;
        }
#ifdef TRY_USING_SELECT
        FD_ZERO(&ListenSocket);
        FD_SET(sockfd,&ListenSocket); /* set our main listening socket */
        set_time_out();
        if (VERB9)
            printf("CLIENT: 'select' setup on sock %u, timeout %u.%06u... (%u)\n", (unsigned int)sockfd, (unsigned int)timeout.tv_sec, (unsigned int)timeout.tv_usec, (unsigned int)highest);
        status = select( (int)highest+1, &ListenSocket, NULL, NULL, &timeout );
        if (SERROR(status)) {
            PERROR("ERROR: 'select' FAILED!");
            iret = 1;
            break;
        }
        if (VERB9)
            printf("CLIENT: 'select' returned status %d\n",status);
        if (status > 0) {
            if ( ! FD_ISSET(sockfd,&ListenSocket)) {
                if (VERB9)
                    printf("CLIENT: 'select' returned status %d, BUT NO FD_SET!\n",status);
                continue;
            }
        } else {
        
            continue;
        }
#endif // TRY_USING_SELECT

        rlen = recv(sockfd,m_answer,MAXSTR,0);
        if (SERROR(rlen)) {
#ifdef _MSC_VER
            win_set_wsa_msg();
            if (add_nonblocking && error_is_not_fatal(last_wsa_error) ) {
                if (VERB5)
                    printf("CLIENT: got [%s] %d\n", last_wsa_emsg, last_wsa_error );
                continue;
            }
#else
            if (add_nonblocking && (errno == EAGAIN) ) {
                if (VERB5)
                    printf("CLIENT: got [EAGAIN] %d\n", errno);
                continue;
            }
#endif
            PERROR("ERROR: recv from socket!");
            pic->recv_errors++;
            if (pic->recv_errors > max_recv_errors) {
                // printf("CLIENT: Maximum recv errors exceeded. Aborting...\n");
                printf("CLIENT: Maximum recv errors exceeded. Assume SERVER closed...\n");
                if (sockfd && !SERROR(sockfd)) {
                    printf("Close socket... "); // close this FAILED socket, and geet a NEW ONE FOR CONNECT
                    SCLOSE(sockfd);
                }
                sockfd = 0;
                not_connected = true;
                continue;
            }
            if (check_key_sleep(sleep_secs * 2,modName))
                break;
            continue;
        } else if (rlen == 0) {
            /* treat a ZERO receive as an ERROR */
            pic->recv_errors++;
            if (pic->recv_errors > max_recv_errors) {
                printf("CLIENT: Maximum recv errors exceeded. Aborting...\n");
                iret = 1;
                break;
            }
            if (check_key_sleep(sleep_secs * 2,modName))
                break;
            continue;
        }
        /* GOT CLIENT DATA */
        recvs_done++;
        recv_bytes += rlen;
        write_msg_log(m_answer, rlen);
        pic->recv_errors = 0; // clear recv errors
        wait_4_reply = 0;
        if (VERB2) {
            clean_data(m_data,m_answer,rlen);
            printf("[%s] %d bytes read\n", m_data, rlen );
        } else if (VERB1) {
            printf("R");
        }
        if (once_only)
            break;
    } /* FOREVER */
    return iret;
}

int main( int argc, char *argv[])
{
    int iret = 0;
    int status;
    int i, i2, c;
    char * arg;

    show_comp_date(argv[0]);
    sock_init();
    strcpy(hostName,HOST);
	portNumber = PORTNUM;
    for (i = 1; i < argc; i++) {
        i2 = i + 1;
        arg = argv[i];
        if (*arg == '-') {
            arg++;
            while(*arg == '-') arg++;
            c = *arg;
            switch (c)
            {
            case '?':
                cmd_help(argv[0]);
                return 0;
                break;
            case '1':
                once_only = 1;
                break;
            case 'h':
                if (i2 < argc) {
                    i++;
                    strcpy(hostName,argv[i]);
                } else {
                    goto Bad_Arg;
                }
                break;
            case 'p':
                if (i2 < argc) {
                    i++;
                    if (is_digits(argv[i])) {
                        portNumber = atoi(argv[i]);
                    } else
                        goto Bad_Arg;
                } else {
                    goto Bad_Arg;
                }
                break;
            case 'r':
                receive_only = 1;
                break;
            case 's':
                if (i2 < argc) {
                    i++;
                    if (is_digits(argv[i])) {
                        secs_to_sleep = atoi(argv[i]);
                    } else
                        goto Bad_Arg;
                } else {
                    goto Bad_Arg;
                }
                break;
            case 'v':
                arg++;
                if (*arg) {
                    // expect digits
                    if (is_digits(arg)) {
                        verbosity = atoi(arg);
                    } else if (*arg == 'v') {
                        verbosity++; /* one inc for first */
                        while(*arg == 'v') {
                            verbosity++;
                            arg++;
                        }
                    } else
                        goto Bad_Arg;
                } else
                    verbosity++;
                printf("SERVER: Set verbosity to %d\n", verbosity);
                break;
#ifdef ADD_UDP_SUPPORT
            case 'u':
                socket_type = SOCK_DGRAM;
                break;
#endif // ADD_UDP_SUPPORT
            default:
Bad_Arg:
                fprintf(stderr, "ERROR: Invalid command line! arg [%s] unknown?\n", argv[i] );
                return 1;
                break;
            }
        } else {
            fprintf(stderr, "ERROR: Invalid command line! bare arg [%s] unknown?\n", argv[i] );
            return 1;
        }
	}
    printf("CLIENT: host = [%s], port %d, type %s\n",
        hostName, portNumber, 
        ((socket_type == SOCK_STREAM) ? "tcp" : "udp") );

    if ((hostptr = gethostbyname(hostName)) == NULL) {
		fprintf(stderr, "ERROR locating host : %s! gethostbyname() FAILED\n", hostName );
        iret = 1;
        goto End_App;
	}
    printf("CLIENT: Got host name [%s]\n", hostptr->h_name );
    last_time = begin_time = time(NULL);

    not_connected = true;
    cycle_count = 0;
    if (do_client_connect()) {
        iret = 1;
        goto End_App;
    }

    show_help();

    iret = run_client();

End_App:

    show_stats();

    printf("CLIENT: ");
    if (sockfd && !SERROR(sockfd)) {
        printf("Close socket... ");
        SCLOSE(sockfd);
    }
    sockfd = 0;
#ifdef _MSC_VER
    printf("WSACleanup()... ");
#endif
    sock_end();
    printf("End app %s\n", get_datetime_str() );
    return iret;
}

// eof - simp_client.cxx
