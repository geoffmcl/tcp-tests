/*******************udpserver.cxx*****************/
// 2011-03-31 - Some (messy) experiments with SOCKETS
//
// Written by Geoff R. McLane, started March 2011.
//
// Copyright (C) 2011 - ????  Geoff R. McLane  
//    - http://geoffair.org - reports@geoffair.info -
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
#include <ctype.h> /* for toupper(), ... */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h> // for PRIu64, ...
#endif // !_MSC_VER

#include "winsockerr.cxx"

/* Server's default port number, listen at 3333 */
#define SERVPORT 3333
#ifndef DEF_PAUSE
#define DEF_PAUSE 160 /* default 160 ms pause */
#endif
#ifndef MY_MX_DATA
#define MY_MX_DATA 2048
#endif

static char hostName[264];
static int use_use_ip = 0;
static int do_an_echo = 1;
static int no_blocking = 1;
static int verbosity = 0;
static char *out_file = 0;
static FILE *pout_file = 0;

#define VERB1 (verbosity >= 1)
#define VERB2 (verbosity >= 2)
#define VERB5 (verbosity >= 5)
#define VERB9 (verbosity >= 9)

static char data_buffer[MY_MX_DATA];
static char hex_buffer[MY_MX_DATA];
static char info_buffer[MY_MX_DATA];

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
}

static void show_help(void)
{
    printf("KEY HELP\n");
    printf(" p     = Toggle PAUSE. Currently %s\n", (is_paused ? "On" : "OFF") );
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
            printf("UDP Server - ESC key - commence exit\n");
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

int run_udpserver(SOCKET sd)
{
    int iret = 0;
    int repeat = 1;
    char * bufptr = data_buffer;
    int buflen = sizeof(data_buffer);
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    int i, rlen, slen, qlen, wlen;
    char *infoptr = info_buffer;

    printf("UDP Server - Doing recvfrom()... %s... %s...\n",
        (no_blocking ? "nonblocking" : "blocking until received"),
#ifdef GOT_KEYBOARD_TEST
        "ESC key to exit"
#else
        "Ctrl+c to exit"
#endif
        );

    cycles = 0;
    cycle_time = time(0);
    max_cyc_per_sec = 0;
    printf("UDP Server - Entering repeat loop. Verbosity = %d, echo = %s, blocking = %s.\n", verbosity,
        do_an_echo ? "Yes" : "No",
        no_blocking ? "No" : " Yes" );
    while (repeat) { /* repeat */
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
            break;

        /* Use the recvfrom() function to receive the data */
        rlen = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)&clientaddr, &clientaddrlen);
        if ( SERROR(rlen) ) {
            if (no_blocking)
            {
#ifdef _MSC_VER
                if (WSAGetLastError() == WSAEWOULDBLOCK) {
                    clock_wait(m_delay_time);
                    continue;
                }
#else /* !_MSC_VER */
                if (errno == EAGAIN) {
                    clock_wait(m_delay_time);
                    continue;
                }
#endif /* _MSC_VER y/n */
            }
            PERROR("UDP Server - recvfrom() error");
            iret = 1;
            break;
        }

        // stats kept
        recvs_done++;
        recv_bytes += rlen;

        qlen = sprintf(infoptr, "%s: UDP Server recvfrom() IP %s, port %d, len %d\n",
            get_datetime_str(),
            inet_ntoa(clientaddr.sin_addr), 
            ntohs(clientaddr.sin_port),
            rlen );

        if (VERB1)
            printf("%s", infoptr);

        // assume a simple ascii text message
        if (VERB9) 
            printf("[%s]\n", bufptr);

        if (out_file && pout_file && (pout_file != (FILE *)-1)) {
            wlen = fwrite(infoptr,1,qlen,pout_file);
            if (wlen != qlen) {
                printf("ERROR: Write to out file %s FAILED! Aborting...\n", out_file);
                fclose(pout_file);
                pout_file = (FILE *)-1;
                iret = 1;
                break;
            }
            wlen = fwrite(bufptr,1,rlen,pout_file);
            if (wlen != rlen) {
                printf("ERROR: Write to out file %s FAILED! Aborting...\n", out_file);
                fclose(pout_file);
                pout_file = (FILE *)-1;
                iret = 1;
                break;
            }
        }

        if (do_an_echo) {
            /* Send a reply by using the sendto() function. */
            /* In this example, the system echoes the received */
            /* data back to the client. */
            /* Send a reply, just echo, in upper case the request */
            for (i = 0; i < rlen; i++) {
                bufptr[i] = toupper(bufptr[i]);
            }
            if (VERB9)
                printf("UDP Server - echo back in uppercase to UDP client, using sendto()...\n");

            slen = sendto(sd, bufptr, rlen, 0, (struct sockaddr *)&clientaddr, clientaddrlen);
            if( SERROR(slen) ) {
                PERROR("UDP server - sendto() error");
                iret = 1;
                break;
            }

            // stats kept
            sends_done++;
            sent_bytes += slen;

            if (VERB5)
                printf("UDP Server - sendto() sent %d bytes [%s]\n", slen, bufptr);
        }
        // repeat--;
    } /* while 'repeat', or ESC key if keyboard */

    return iret;
}

int get_host_information(char * in_host_name)
{
    // Declare variables
    char* IP = NULL;
    hostent* Host = NULL;
    unsigned int addr;
    char * host_name = in_host_name;
    if (( host_name == NULL ) || ( host_name[0] == 0 )) {
        host_name = (char *)"localhost";
    }
    // If an alpha name for the host, use gethostbyname()
    if (isalpha(host_name[0])) {   /* host address is a name */
        // if hostname terminated with newline '\n', remove and zero-terminate 
        if (host_name[strlen(host_name)-1] == '\n')
            host_name[strlen(host_name)-1] = '\0'; 
        Host = gethostbyname(host_name);
    } else  {
        // If not, get host by addr (assume IPv4)
        addr = inet_addr(host_name);
        Host = gethostbyaddr((char *)&addr, 4, AF_INET);
    }
#ifdef _MSC_VER
    if ((WSAGetLastError() != 0)||(Host == NULL)) {
        PERROR("ERROR: gethost... FAILED!");
        return -1;
    }
#else // !_MSC_VER
    if ( Host == NULL ) {
        PERROR("ERROR: gethost... FAILED!");
        return -1;
    }
#endif // _MSC_VER y/n
    // The Host structure can now be used to
    // access information about the host
    // Get the local host information
    IP = inet_ntoa (*(struct in_addr *)*Host->h_addr_list);
    if (IP == NULL)
        return -1; /* FAILED */
    // ELSE, have resolved Host name
    localHost = Host;
    localIP = strdup(IP);
    use_use_ip = 1; /* can use the user supplied IP */
    return 0; // success
}

void cmd_help(char * name)
{
    //char * cp = get_to_stg();
    printf("HELP\n");
    printf(" -?    = This help, and exit 0\n");;
    printf(" -h <host> = Set the HOST name, or IP. Def=<none>\n");
    printf(" -o <file> = Write received data to this file. Def=<none>\n");
    printf(" -p <port> = Set the PORT number. Def=%d\n", PORTNUM );
    printf(" -e y|n    = Set/Unset server echo. Def=%s\n",
        do_an_echo ? "Yes" : "No" );
    printf(" -v[nn]    = Increment/Set verbosity. Def=%d\n", verbosity );
    //printf("Current timeout = %s\n", cp);
}

void test_sizes(void)
{
    printf("Sizeof uint32_t    = %d\n", sizeof(uint32_t)   );
    printf("Sizeof uint64_t    = %d\n", sizeof(uint64_t)   );
    printf("Sizeof long long   = %d\n", sizeof(long long)  );
    printf("Sizeof a float     = %d\n", sizeof(float)      );
    printf("Sizeof a double    = %d\n", sizeof(double)     );
    exit(0);
}

int main(int argc, char *argv[])
{
    /* Variable and structure definitions. */
    int iret = 0;
    SOCKET sd;
    struct sockaddr_in serveraddr;
    socklen_t serveraddrlen = sizeof(serveraddr);
    int i, status;
    int portNumber;
    // test_sizes();
    printf("UDP Server - compiled on %s, at %s\n", __DATE__, __TIME__);
    sock_init(); // need some init for windows

    portNumber = SERVPORT;
    hostName[0] = 0;
	// Validate the command line
    for (i = 1; i < argc; i++) {
        char *oarg = argv[i];
        char *arg = oarg;
        if (*arg == '-') {
            arg++;
            while(*arg == '-') arg++;
            int c = *arg;
            switch (c) {
            case '?':
                cmd_help(argv[0]);
                return 0;
                break;
            case 'e':
                if ((i + 1) < argc) {
                    i++;
                    arg = argv[i];
                    if (*arg == 'y')
                        do_an_echo = 1;
                    else if (*arg == 'n')
                        do_an_echo = 0;
                    else {
                        printf("ERROR: Only 'y' or 'n' to follow %s, not %s!\n",
                            oarg, arg );
                        goto Bad_Arg;
                    }
                    printf("udpserver: set echo %s\n", do_an_echo ? "On" : "Off");
                } else {
                    goto Bad_Arg;
                }
                break;

            case 'h':
                if ((i + 1) < argc) {
                    i++;
                    strcpy(hostName,argv[i]);
                } else {
                    goto Bad_Arg;
                }
                break;
            case 'o':
                if ((i + 1) < argc) {
                    i++;
                    out_file = strdup(argv[i]);
                } else {
                    printf("ERROR: Output file name must follow %s\n",oarg);
                    goto Bad_Arg;
                }
                printf("SERVER: Set output file to %s\n", out_file);
                break;
            case 'p':
                if ((i + 1) < argc) {
                    i++;
                    portNumber = atoi(argv[i]);
                } else {
                    printf("ERROR: Port number must follow %s\n",oarg);
                    goto Bad_Arg;
                }
                printf("SERVER: Set port number to %d\n", portNumber);
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
                    } else {
                        printf("ERROR: Verbosity must be digits 0-9, or repeasted v's, not %s\n", oarg);
                        goto Bad_Arg;
                    }
                } else
                    verbosity++;
                printf("SERVER: Set verbosity to %d\n", verbosity);
                break;
            default:
Bad_Arg:
                fprintf(stderr,"ERROR: Invalid command line! arg [%s] unknown\n", argv[i] );
                return 1;
                break;
            }
        } else {
            fprintf(stderr,"ERROR: Invalid command line! bare arg [%s] unknown\n", argv[i] );
            return 1;
        }
	}

	// At this point, the command line has been validated.
    if (hostName[0]) { /* if USER supplied name/IP address */
        status = get_host_information(hostName); /* may use supplied HOST */
        if (status == -1) {
            printf("ERROR: Failed on host name %s\n",hostName);
            iret = 1;
            goto Server_Exit;
        }
    }

    /* The socket() function returns a socket */
    /* get a socket descriptor */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if( SERROR(sd) ) {
        PERROR("UDP server - socket() error");
        iret = 1;
        goto Server_Exit;
    }

    printf("UDP server - begin %s - socket() is OK sd=%u (0x%X)\n",
        get_datetime_str(), sd, sd );

    /* set non blocking, if desired */
    if (no_blocking) {
        status = setnonblocking(sd);
        printf("UDP server - set non-blocking - status = %d\n", status );
    }

    /* bind to address */
    memset(&serveraddr, 0x00, serveraddrlen);
    serveraddr.sin_family = AF_INET;
    if (hostName[0] == 0) {
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {

    }
    serveraddr.sin_port = htons(portNumber);
    
    printf("UDP server - try to bind() IP %s, port %d, struct len %d\n", inet_ntoa(serveraddr.sin_addr),
        ntohs(serveraddr.sin_port), serveraddrlen );
        
    status = bind(sd, (struct sockaddr *)&serveraddr, serveraddrlen);
    if( SERROR(status) ) {
        PERROR("UDP server - bind() error");
        iret = 1;
        goto Server_Exit;
    }

    printf("UDP Server - bind() OK IP %s, port %d\n",
        inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port));

    if (out_file) {
        pout_file = fopen(out_file,"wb");
        if (!pout_file) {
            printf("ERROR: Unable to create output file %s! Aborting...\n", out_file);
            iret = 1;
            goto Server_Exit;
        }
        printf("UDP Server - create output file %s.\n", out_file);
    }

    // =============================
    iret = run_udpserver(sd);   // run server in FOREVER loop
    // =============================

Server_Exit:

    if (out_file && pout_file && (pout_file != (FILE *)-1)) {
        fclose(pout_file);
        printf("UDP Server - close output file %s\n", out_file);
    }

    /* close() the socket descriptor. */
    printf("UDP Server - close and exit %d... %s\n", iret, get_datetime_str());
    if (sd && !SERROR(sd))
        SCLOSE(sd);

    sock_end();
    return iret;
}

// eof - udpserver.c
