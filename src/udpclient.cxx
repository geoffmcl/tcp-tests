/****************udpclient.cxx********************/
// 2011-03-31 - Some (messy) experiments with SOCKETS
//
// Written by Geoff R. McLane, started March 2011.
//
// Copyright (C) 2011 - ????  Geoff R. McLane
//   - http://geoffair.org - reports@geoffair.info -
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef _MSC_VER
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h> // for PRIu64, ...
#endif // !_MSC_VER

#include "winsockerr.cxx"

/* Default host name of 'localhost', change accordingly */
#define SERVER "127.0.0.1"

/* Server's port number */
#define SERVPORT 3333
#ifndef DEF_PAUSE
#define DEF_PAUSE 160 /* default 160 ms pause */
#endif

static int verbosity = 0;
#define VERB1 (verbosity >= 1)
#define VERB2 (verbosity >= 2)
#define VERB5 (verbosity >= 5)
#define VERB9 (verbosity >= 9)

static int no_send_to_server = 0;   // no sending

static int wait_receive = 0; /* only set if data sent... */
static int wait_cycles = 0;
static int max_wait_cycles = 3;
static int no_blocking = 1;
static int exit_on_recv_err = 0;
/* just some statistics */
static time_t cycle_time;
static uint64_t cycles = 0;
static uint64_t cyc_per_sec = 0;
static uint64_t max_cyc_per_sec = 0;

/* pause the running */
static time_t pause_time;
static int is_paused = 0;

// when to show a message
static time_t last_time = 0;
static time_t nxt_msg_time = 20; /* show working dot each 20 seconds */

static char server[264];
static int port;

// stats kept
static uint64_t sends_done = 0;
static uint64_t sent_bytes = 0;
static uint64_t recvs_done = 0;
static uint64_t recv_bytes = 0;

void show_stats(void)
{
    if (cyc_per_sec)
        printf("Running at %" PRIu64 " cycles per second\n",  cyc_per_sec );
    if (sends_done || recvs_done) {
        printf("Done %" PRIu64 " sends, %" PRIu64 " bytes, %" PRIu64 " recvs, %" PRIu64 " bytes\n",
            sends_done, sent_bytes, recvs_done, recv_bytes );
    }
    if (wait_receive)
        printf("Presently cycling in 'wait_receive'... r to reset...\n");
}

static void show_help(void)
{
    printf("KEY HELP\n");
    printf(" p     = Toggle PAUSE. Currently %s\n", (is_paused ? "On" : "OFF") );
    printf(" r     = Reset wait for receive. Currently %s\n", (wait_receive ? "On" : "Off") );
    printf(" 01259 = Set verbosity level. Now=%d\n", verbosity );
    printf(" ESC   = Begin Exit...\n");
    printf("STATS - info\n");
    show_stats();
}

// static
int check_key(void)
{
    int chr = 0;
#ifdef GOT_KEYBOARD_TEST
    chr = test_for_input();
    if (chr) {
        switch (chr)
        {
        case 0x1b:
            printf("UDP Client - ESC key - commence exit\n");
            return 1;
            break;
        case '?':
            show_help();
            break;
        case 'p':
            pause_time = time(0);
            is_paused = (is_paused ? 0 : 1);
            printf("p - Toggled PAUSE. Currently %s\n",
                (is_paused ? "On" : "OFF") );
            break;
        case 'r':
            if (wait_receive)
                printf("r - reset wait receive...\n");
            else
                printf("r - reset wait receive... but not set\n");
            wait_receive = 0;
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
            printf("Unused key input...\n");
            break;
        }
        chr = 0;
    }
#endif // #ifdef GOT_KEYBOARD_TEST
    return chr;
}

void show_cmd_help(char * name)
{
    printf("%s [Options]\n", name);
    printf(" --host <name> (-h) = Host name, or IP address. Def=%s.\n", SERVER);
    printf(" --port <port> (-p) = Port values. Def=%d,\n", SERVPORT);
    printf(" --nosend      (-n) = No send to server.\n");
    printf(" -v[nn]             = Increment/Set verbosity. Def=%d\n", verbosity );
}

void set_next_message( char *bufptr, int buflen )
{
    static int message_counter = 0;
    message_counter++;
    /* setup the message to send, but the whole buffer is sent... */
    memset(bufptr, 0x00, buflen); /* clear to all zero */
    /* characters + terminating NULL */
    sprintf(bufptr, "Hello! A client request message %d!", message_counter);
}

int run_udpclient(SOCKET sd, struct sockaddr_in * serveraddr)
{
    int iret = 0;
    char buffer[100];
    char *bufptr = buffer;
    int buflen = sizeof(buffer);
    int rc;
    socklen_t serveraddrlen = (socklen_t)sizeof(struct sockaddr_in);
    int repeat = 1;

    cycle_time = time(0);
    max_cyc_per_sec = 0;
    cycles = 0;
    printf("UDP Client - Entering repeat loop. Verbosity = %d.\n", verbosity);
    while (repeat)
    {
        cycles++; /* bump the cycles */
        if (time(0) != cycle_time) {
            cycle_time = time(0);
            cyc_per_sec = cycles;
            if (cycles > max_cyc_per_sec)
                max_cyc_per_sec = cycles;
            cycles = 0;
        }
        if ((time(0) - last_time) > nxt_msg_time) {
            show_stats();
            last_time = time(0); /* set for NEXT message time */
        }
        if (check_key())
            goto Client_Done;

        if (no_send_to_server) {
            printf("UDP Client - Skipping send 1...\n" );
            goto Done_Send;
        }
        set_next_message( bufptr, buflen );
        /* Use the sendto() function to send the data */
        /* to the server. */
        /************************************************/
        /* This example does not use flags that control */
        /* the transmission of the data. */
        /************************************************/

        /* send request to server */
        if (VERB5) {
            printf("UDP Client - sendto IP %s, port %u, len %d bytes...\n",
                inet_ntoa(serveraddr->sin_addr),
                ntohs(serveraddr->sin_port),
                buflen);
        }
        rc = sendto(sd, bufptr, buflen, 0, (struct sockaddr *)serveraddr, serveraddrlen);
        if (SERROR(rc)) {
            PERROR("UDP Client - sendto() error");
            iret = 1;
            goto Client_Done;
        }

        // stats kept
        sends_done++;
        sent_bytes += rc;

        if (VERB9)
            printf("UDP Client - sendto() OK! %d sent [%s]\n", rc, bufptr);
Done_Send:

        if (VERB5) {
            printf("UDP Client - Waiting reply from UDP server... %s... %s...\n",
                (no_blocking ? "nonblocking" : "blocking until received"),
#ifdef GOT_KEYBOARD_TEST
                "ESC key to exit"
#else
                "Ctrl+c to exit"
#endif
            );
        }

        wait_receive = 1;
        wait_cycles = 0;
        while (wait_receive) {

            if (check_key())
                goto Client_Done;

            // always wait a little before checking for a reply
            clock_wait(m_delay_time);

            wait_cycles++;
            /* Use the recvfrom() function to receive the */
            /* data back from the server. */
            /************************************************/
            /* This example does not use flags that control */
            /* the reception of the data. */
            /************************************************/
            /* Read server reply. */
            /* Note: serveraddr is reset on the recvfrom() function. */
            serveraddrlen = sizeof(serveraddr);
            if (VERB5) {
                printf("Doing: rc = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)serveraddr, &serveraddrlen);\n");
                if (VERB9) {
                    printf("Socket %d, buflen %d,  IP=%s, port %d\n", sd, buflen,
                        inet_ntoa(serveraddr->sin_addr),
                        ntohs(serveraddr->sin_port) );
                }

            }
            rc = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)serveraddr, &serveraddrlen);
            if (SERROR(rc)) {
                // PERROR("UDP Client - recvfrom error!");
                if (no_blocking)
                {
#ifdef _MSC_VER
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK) {
                        clock_wait(m_delay_time);
                        continue;
                    } else {
                        if (err == WSAEINVAL) {
                            PERROR("UDP Client - recvfrom() error");
                            printf("UDP Client - resetting socket_in struct...\n" );
                            memset(serveraddr,0,sizeof(struct sockaddr_in));
                            serveraddrlen = (int)sizeof(struct sockaddr_in);
                            serveraddr->sin_family = AF_INET;
                            serveraddr->sin_addr.s_addr = inet_addr(server);
                            serveraddr->sin_port = htons(port);
                            rc = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)serveraddr, &serveraddrlen);
                            if (SERROR(rc)) {
                               err = WSAGetLastError();
                               if (err == WSAEINVAL) {
                                    printf("UDP Client - Got repeat of INVALID PARAMS! Aborting...\n" );
                                    iret = 1;
                                    goto Client_Done;
                               }
                            }
                        }
                    }
#else /* !_MSC_VER */
                    if (errno == EAGAIN) {
                        clock_wait(m_delay_time);
                        continue;
                    }
#endif /* _MSC_VER y/n */
                }
                PERROR("UDP Client - recvfrom() error");
                if (exit_on_recv_err) {
                    iret = 1;
                    goto Client_Done;
                } else {
                    // should wait a little, then continue
                    int wait_cycles = 200;
                    if (no_send_to_server) {
                        printf("UDP Client - Waiting %d seconds repeating receiving...\n",
                            (int)(( wait_cycles * m_delay_time ) / 1000 ));
                    } else {
                        printf("UDP Client - Waiting %d seconds before next send\n",
                            (int)(( wait_cycles * m_delay_time ) / 1000 ));
                    }
                    while (wait_cycles--) {
                        if (check_key())
                        goto Client_Done;
                        // always wait a little before checking for a reply
                        clock_wait(m_delay_time);
                        if (no_send_to_server) {
                            printf("UDP Client - Skipping send...\n" );
                            goto Done_Send2;
                        }
                        
                        set_next_message(bufptr,buflen);
                        rc = sendto(sd, bufptr, buflen, 0, (struct sockaddr *)serveraddr, serveraddrlen);
                        if (SERROR(rc)) {
                            PERROR("UDP Client - sendto() error");
                            iret = 1;
                            goto Client_Done;
                        }
                        // stats kept
                        sends_done++;
                        sent_bytes += rc;
Done_Send2:
                        clock_wait(m_delay_time);
                        rc = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)serveraddr, &serveraddrlen);
                        if (!SERROR(rc)) {
                            printf("UDP Client - Received %d bytes...\n", rc );
                            break;
                        } else if (no_send_to_server) {
                            PERROR("UDP Client - Receive failed!");
                        }
                    }
                    break; // continue;
                }
            }

            // stats kept
            recvs_done++;
            recv_bytes += rc;

            if (VERB5) {
                printf("UDP Client - recvfrom() IP %s, port %d (cyc=%d)\n", 
                   inet_ntoa(serveraddr->sin_addr),
                    ntohs(serveraddr->sin_port),
                    wait_cycles );
            }
            if (VERB9)
                printf("UDP Client - received %d message: [%s]\n", rc, bufptr);

            wait_receive = 0;

        } /* while waiting receive */

    } /* while repeat */

Client_Done:

    return iret;
}

int main(int argc, char *argv[])
{

    /* Variable and structure definitions. */
    int iret = 0;
    SOCKET sd, rc;
    struct sockaddr_in serveraddr;
    socklen_t serveraddrlen = sizeof(serveraddr);
    struct hostent *hostp;
    int i, status;
    char * arg;

    printf("UDP Client - compiled on %s, at %s\n", __DATE__, __TIME__);
    
    sock_init(); /* none for unix, but needed for Windows ... */

    sd = 0; /* no SOCKET yet */
    server[0] = 0; /* no server yet */
    port = SERVPORT; /* set default port */

    /* If the hostname/IP of the server is supplied */
    /* Or use current machine, default port */
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        if (*arg == '-') {
            while (*arg == '-') arg++;
            switch (*arg)
            {
            case '?':
                show_cmd_help(argv[0]);
                goto Client_Exit;
                break;
            case 'h':
                i++;
                if (i < argc) {
                    arg = argv[i];
                    strcpy(server,arg);
                    printf("UDP Client - Set ServerName/IP to [%s]\n", arg);
                } else {
                    i--;
                    printf("ERROR: Server name or IP must follow [%s]\n", argv[i]);
                    iret = 1;
                    goto Client_Exit;
                }
                break;
            case 'n':
                no_send_to_server = 1;
                printf("UDP Client - Set no-send-to-server\n");
                break;

            case 'p':
                i++;
                if (i < argc) {
                    arg = argv[i];
                    port = atoi(arg);
                    printf("UDP Client - Set Port to [%d]\n", port);
                } else {
                    i--;
                    printf("ERROR: Server PORT must follow [%s]\n", argv[i]);
                    iret = 1;
                    goto Client_Exit;
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
                        goto Bad_Command;
                } else
                    verbosity++;
                printf("SERVER: Set verbosity to %d\n", verbosity);
                break;
            default:
                goto Bad_Command;
                break;
            }
        } else {
Bad_Command:
            printf("USAGE ERROR: Unknown command [%s]! Aborting...\n", argv[i]);
            iret = 1;
            goto Client_Exit;
        }
    } /* process the command line */

    if (server[0] == 0) {
        /*Use default hostname or IP*/
        if (get_local_host()) {
            printf("UDP Client - Using local host IP [%s], name [%s], port [%d]\n",
                localIP, localHost->h_name,
                port );
            strcpy(server, localIP); // copy in IP address
            /* could also try the host NAME in localHost->h_name */
        } else {
            printf("UDP Client - Using local default IP [%s], prot [%d]\n", SERVER, port);
            strcpy(server, SERVER);
        }
    }

    /* The socket() function returns a socket */
    /* get a socket descriptor */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (SERROR(sd) ) {
        PERROR("UDP Client - socket() error");
        iret = 1;
        goto Client_Exit;
    }

    printf("UDP Client - begin %s - socket() is OK! value 0x%X (%u)\n",
        get_datetime_str(), sd, sd );

    /* set non blocking, if desired */
    if (no_blocking) {
        status = setnonblocking(sd);
        printf("UDP Client - set non-blocking - status = %d\n", status );
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = inet_addr(server);
    if( serveraddr.sin_addr.s_addr == (unsigned long)INADDR_NONE ) {
        /* Use the gethostbyname() function to retrieve */
        /* the address of the host server if the system */
        /* passed the host name of the server as a parameter. */
        /************************************************/
        /* get server address */
        hostp = gethostbyname(server);
        if(hostp == (struct hostent *)NULL) {
            printf("ERROR: HOST NOT FOUND [%s]! ", server);
            /* h_errno is usually defined in netdb.h */
            printf("h_errno = %d\n", h_errno);
            iret = 1;
            goto Client_Exit;
        } else {
            printf("UDP Client - gethostname() of the server is OK... \n");
        }
        memcpy(&serveraddr.sin_addr, hostp->h_addr, sizeof(serveraddr.sin_addr));
    }

    iret = run_udpclient(sd,&serveraddr);

Client_Exit:

    /* close() the socket descriptor. */
    printf("UDP Client - close and exit %d... %s\n", iret, get_datetime_str() );
    if (sd && !SERROR(sd))
        SCLOSE(sd);
    sock_end();
    return iret;
}

// eof - udpclient.c
