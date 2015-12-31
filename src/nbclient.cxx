// nbclient.cxx
// 2011-03-31 - Some (messy) experiments with SOCKETS
// In this case setting NON-BLOCKING
// ==================================================
//
// Written by Geoff R. McLane, started March 2011.
//
// Copyright (C) 2011 - ????  Geoff R. McLane
//  - http://geoffair.org - reports@geoffair.info -
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
#if (defined(_MSC_VER) && !defined(_CONFIG_H_MSVC_))
#error "ERROR: Copy config.h-msvc to config.h for Windows compile!"
#endif

#define TRY_NB_SERVER // just a small deviation to talk to the nbserver

// #include <iostream>

#include <sys/types.h>
#ifdef _MSC_VER
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <memory.h>
#include <io.h>
#include <conio.h>
#include <time.h>
#else // !_MSC_VER
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h> /* for inet_ntoa, ... */
#include <fcntl.h>
#include <memory.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#endif // _MSC_VER

using namespace std;

#include "winsockerr.cxx"

static char * modName = (char *)"NB Client(tcp)";

static char hostName[MAXSTR];
static int out_count = 0;
static int wait_4_reply = 0;
static int is_paused = 0;
static time_t pause_time;
static time_t msg_wait_time = 30; // show paused, every 30 seconds
static time_t last_read = 0;
static time_t read_msg_time = 10; // show waiting read
static int nul_read_cnt = 0;
static int write_after = 3;
static char m_data[MAXSTR+16];
static char m_answer[MAXSTR+16];

static int socket_type = DEF_SOCK_TYPE;

static int add_nonblocking = 1;
static int max_recv_errors = 2;
static int max_send_errors = 2;

static INCLIENT hostServ;

void show_help(void)
{
    printf("KEY  = ACTION\n");
    printf(" ?   = This help...\n");
#if 0 // for server
    printf(" e   = Toggle ECHO. Currently %s\n",
        (do_echo ? "On" : "OFF") );
#endif // 0
    printf(" d    = Decrease delay by 10 ms. Currently %d\n", m_delay_time );
    printf(" i    = Increase delay by 10 ms. Currently %d\n", m_delay_time );
    printf(" p   = Toggle PAUSE. Currently %s\n",
        (is_paused ? "On" : "OFF") );
    printf("ESC  = EXIT application...\n");
}


int check_key(void)
{
    int chr = test_for_input();
    if (chr) {
        switch (chr)
        {
        case '?':
            show_help();
            break;
#if 0 // for server only
        case 'e':
            do_echo = (do_echo ? 0 : 1);
            printf("e - Toggled ECHO. Currently %s\n",
                (do_echo ? "On" : "OFF") );
            break;
#endif // 0
        case 'd':
            m_delay_time -= 10;
            if (m_delay_time < 0) m_delay_time = 0;
            printf("d - Decrease delay to %d\n", m_delay_time);
            break;
        case 'i':
            m_delay_time += 10;
            printf("i - Increased delay to %d\n", m_delay_time);
            break;
        case 'p':
            pause_time = time(0);
            is_paused = (is_paused ? 0 : 1);
            printf("p - Toggled PAUSE. Currently %s\n",
                (is_paused ? "On" : "OFF") );
            break;
        case 0x1b:
            printf("ESC - Commence exit...\n");
            return 1;
            break;
        default:
            printf("Unused key input...\n");
            break;
        }
    }
    return 0;
}


#ifdef _MSC_VER
int error_is_fatal(int err)
{
    if (err == WSAECONNABORTED)
        return 1;
    if (err == WSAECONNRESET)
        return 1;
    // what other FATAL errors ... or maybe ALL are FATAL
    return 0;
}

int error_is_not_fatal( int wsa_errno )
{
    switch (wsa_errno) {
    case WSAEWOULDBLOCK: // always == NET_EAGAIN?
    case WSAEALREADY:
    case WSAEINPROGRESS:
      return 1;
    }
    return 0;
}

#endif /* _MSC_VER */

void in_exit(void)
{
    printf("NB Client(tcp) - IN EXIT\n");
}

void clean_data(char * dest, char * src, int len)
{
    int i, off, c;
    off = 0;
    for (i = 0; i < len; i++) {
        c = src[i];
        if (c & 0x80) { // got high bit on
            dest[off++] = '@';
            c &= ~0x80;
        }
        if (c < ' ') {
            dest[off++] = '^';
            c += '@';
        }
        dest[off++] = c;
    }
    dest[off] = 0;
}

int run_nbclient(SOCKET sockfd)
{
    int iret = 0;
    int slen,wlen,rlen;
    PINCLIENT pic = &hostServ;
    printf("NB Client(tcp) - Entering forever loop...\n" );
    while (1) { /* FOREVER */
        if (check_key())
            break;
        if (is_paused) {
            if ((time(0) - pause_time) > msg_wait_time) {
                printf("NB Client(tcp) - presently PAUSED!\n");
                pause_time = time(0);
            }
            clock_wait(m_delay_time);
            continue;
        }

        if (wait_4_reply) {
            // waiting a REPLY to last send
            // then NO SENDING
        } else {
            strcpy(m_data, get_datetime_str());
#if defined(TRY_NB_SERVER)
            out_count++;
            sprintf(EndBuf(m_data),": %d - Hello server!\n", out_count);
#else
        	strcat(m_data, " String to send to the server.");
#endif
            slen = (int)strlen(m_data);
            clean_data(m_answer,m_data,slen);
            printf("[%s] %d bytest to send\n", m_answer, slen );
#ifdef _MSC_VER
	        wlen = send(sockfd,m_data,slen,0);
            if (SERROR(wlen)) {
                PERROR("ERROR: sending to socket!");
                pic->send_errors++;
                if ( error_is_fatal(last_wsa_error) ) {
                    // hm, can either try to connect again, OR ABORT
                    printf("CLIENT aborting...\n");
                    iret = 1;
                    break;
                }
                if (pic->send_errors > max_send_errors) {
                    printf("Maximum send errors exceeded! CLIENT aborting...\n");
                    iret = 1;
                    break;
                }
                if (check_key())
                    break;
                clock_wait(m_delay_time);
                if (check_key())
                    break;
                clock_wait(m_delay_time);
                if (check_key())
                    break;
                clock_wait(m_delay_time);
                continue;
            }
#else
	        //wlen = write(sockfd,m_data,slen);
            wlen = send(sockfd,m_data,slen,0);
            if (SERROR(wlen)) {
                PERROR("ERROR: sending to socket!");
                pic->send_errors++;
                if (check_key())
                    break;
                clock_wait(m_delay_time);
                if (check_key())
                    break;
                clock_wait(m_delay_time);
                if (check_key())
                    break;
                clock_wait(m_delay_time);
                continue;
            }
#endif
            printf("NB Client(tcp) - Sent len %d... sleep %d ms...\n",
                wlen, m_delay_time );
            if (check_key()) {
                break;
            }
            clock_wait(m_delay_time);
            if (check_key()) {
                break;
            }
            wait_4_reply = 1;
        }
#ifdef _MSC_VER
	    rlen = recv(sockfd,m_answer,MAXSTR,0);
        if (SERROR(rlen)) {
            win_set_wsa_msg();
            if (add_nonblocking && error_is_not_fatal(last_wsa_error) ) {
                printf("NB Client(tcp) - got [%s]\n", last_wsa_emsg );
                continue;
            }
            PERROR("ERROR: recv from socket!");
#else
	    rlen = read(sockfd,(void *)m_answer,MAXSTR);
        if SERROR(rlen) {
            if (add_nonblocking && (errno == EAGAIN) ) {
                printf("NB Client(tcp) - got EAGAIN\n");
                continue;
            }
            PERROR("ERROR: read from socket!");
#endif
            pic->recv_errors++;
            if (pic->recv_errors > max_recv_errors) {
                printf("NB Client(tcp) - recv errors %d exceeds maximum. Aborting...\n",
                    pic->recv_errors );
                iret = 1;
                break;
            }
            printf("NB Client(tcp) - Error %d. Wait %d secs for next recv try...\n",
                pic->recv_errors, (DEF_SLEEP * 2));
            if (check_key_sleep(DEF_SLEEP * 2,modName))
                break;
            continue;
        }
        if (rlen == 0) {
            if ((last_read == 0) || ((time(0) - last_read) > read_msg_time)) {
                printf("NB Client(tcp) - got NULL read. Server may have closed...\n");
                last_read = time(0);
                nul_read_cnt++;
                if (nul_read_cnt >= write_after) {
                    printf("NB Client(tcp) - assume server closed, but trying write again...\n");
                    wait_4_reply = 0;
                }
                continue;
            }
        }
        pic->recv_errors = 0; // clear recv errors
        last_read = 0;
        if (rlen > 0) {
            wait_4_reply = 0;
            clean_data(m_data,m_answer,rlen);
            printf("[%s] %d bytes read\n", m_data, rlen);
        }   
        if (check_key())
            break;
        clock_wait(m_delay_time); 
    } /* FOREVER */
    return iret;
}

void show_comp_date(char * name)
{
    printf("NB Client(tcp): %s, compiled %s, at %s\n",
        name, __DATE__, __TIME__ );
}

void cmd_help(char * name)
{
    printf("COMMAND HELP\n");
    printf(" -?        = This help, and exit 0\n");
    printf(" -h <host> = Set the HOST name, or IP. Def=%s\n", HOST );
    printf(" -p <port> = Set the PORT number. Def=%d\n", PORTNUM );
    // printf(" -u        = Use udp socket. Def=tcp" );
}

int main( int argc, char *argv[])
{
    int iret = 0;
    int portNumber, status;
    int i, c;
    char * arg;
    char * IP = NULL;
    PINCLIENT pic = &hostServ;
    struct in_addr * pin = NULL;
    
    // atexit(in_exit);
    show_comp_date(argv[0]);
    sock_init();
    /* set DEFAULTS, if no user input */
    strcpy(hostName,HOST);
	portNumber = PORTNUM;
    for (i = 1; i < argc; i++) {
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
            case 'h':
                if ((i + 1) < argc) {
                    i++;
                    strcpy(hostName,argv[i]);
                } else {
                    goto Bad_Arg;
                }
                break;
            case 'p':
                if ((i + 1) < argc) {
                    i++;
                    portNumber = atoi(argv[i]);
                } else {
                    goto Bad_Arg;
                }
                break;
            //case 'u':
            //    socket_type = SOCK_DGRAM;
            //    break;
            default:
Bad_Arg:
                printf("ERROR: Invalid command line! arg [%s] unknown!\n", argv[i] );
                return 1;
                break;
            }
        } else {
            printf("ERROR: Invalid command line! arg [%s] unknown!\n", argv[i] );
            return 1;
        }
	}
	printf("NB Client(tcp) - host = [%s], port %d, type %s\n",
        hostName, portNumber,
        ((socket_type == SOCK_STREAM) ? "tcp" : "udp") );

    struct hostent *hostptr;
    if ((hostptr = gethostbyname(hostName)) == NULL) {
		printf("ERROR locating host : [%s]! Aborting...", hostName );
		exit(1);	
	}
    printf("NB Client(tcp) - name [%s]\n", hostptr->h_name );

    struct sockaddr_in serv_addr;

	bzero((char *)&serv_addr,sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	memcpy((char *)&serv_addr.sin_addr,(char *)hostptr->h_addr,hostptr->h_length);
	serv_addr.sin_port = htons(portNumber);

    int sockfd,newsockfd;
	sockfd = (int)socket(AF_INET,socket_type,0);
    if (SERROR(sockfd)) {
        PERROR("ERROR: creating socket!");
        iret = 1;
        goto NB_Client_End;
    }

    printf("NB Client(tcp) - Got socket [%d]\n", sockfd );
    status = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
    if (SERROR(status)) {
        PERROR("ERROR: socket connect FAILED!");
        iret = 1;
        goto NB_Client_End;
    }
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
        printf("NB Client(tcp) - Adding non-blocking mode...\n");
        status = setnonblocking(sockfd);
        printf("NB Client(tcp) - io ctrl socket(sockfd,...) returned %d\n", status );
    }

    printf("NB Client(tcp) - connection to IP [%s], port %d, name %s\n",
        pic->IP, ntohs(serv_addr.sin_port), pic->h_name );

    show_help();

    iret = run_nbclient(sockfd);

NB_Client_End:

    if (!SERROR(sockfd)) {
        printf("NB Client(tcp) - Close socket...\n");
        SCLOSE(sockfd);
    }
#ifdef _MSC_VER
    printf("NB Client(tcp) - WSACleanup()...\n");
#endif
    sock_end();
    printf("NB Client(tcp) - Exit %d - %s\n", iret, get_datetime_str() );
    return iret;
}

// eof - nbclient.cxx
