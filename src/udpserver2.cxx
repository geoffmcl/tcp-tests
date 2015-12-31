/*
 *  This file is provided for use with the unix-socket-faq.  It is public
 *  domain, and may be copied freely.  There is no copyright on it.  The
 *  original work was by Vic Metcalfe (vic@brutus.tlug.org), and any
 *  modifications made to that work were made with the understanding that
 *  the finished work would be in the public domain.
 *
 *  If you have found a bug, please pass it on to me at the above address
 *  acknowledging that there will be no copyright on your work.
 *
 *  update: 2011-03-31 - Ported to also compile in MS WIndows
 */
// udpserver2.cxx
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
#if (defined(_MSC_VER) && !defined(_CONFIG_H_MSVC_))
#error "ERROR: Copy config.h-msvc to config.h for Windows compile!"
#endif

#ifndef _MSC_VER
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#endif

#include "winsockerr.cxx"
#include "sockhelp.c"

#undef SHOW_ENDIAN_MADNESS

char * hexify_per_length(char * dst, char * src, int max)
{
    unsigned char c;
    int i;
    *dst = 0;
    for (i = 0; i < max; i++) {
        c = src[i];
        if (i) strcat(dst," ");
        sprintf(EndBuf(dst),"%02X", (c & 0xff));
    }
    return dst;
}

char * hexify_u_short( char * cp )
{
    static char _s_us_buf[16];
    char * hb = _s_us_buf;
    return hexify_per_length(hb,cp,sizeof(u_short));
}

#define MX_H_BUF 256

// htons(client->sin_port), // converts a u_short from host to
// TCP/IP network byte order (which is big-endian).
void show_client( char * src, int recvd, struct sockaddr_in * client )
{
    static char _s_buf[MX_H_BUF];
    // ntohs(u_short) - converts a u_short from TCP/IP network byte order
    // to host byte order (which is little-endian on Intel processors).
    u_short port = ntohs(client->sin_port);
    char * IP = inet_ntoa(client->sin_addr);
    char * dst = _s_buf;
    int max = recvd;
    int * pi = (int *)src;
    int value = *pi;
    int IsBE = (IsBigEndian ? 1 : 0);
        
    if (max >= MX_H_BUF) max = MX_H_BUF - 1;
    
    printf("UDP Server2 - Recd %d bytes", recvd );
#ifdef SHOW_ENDIAN_MADNESS
    printf(" [%s]%u", hexify_per_length(dst,src,max), value );
    value = bswap_32(value); /* swap/invert an int 4 bytes */
    printf(" after bswap_32 [%s]%u int=%d long=%d\n", hexify_per_length(dst,(char *)&value,sizeof(int)),value,sizeof(int),sizeof(long) );
#endif    
    if (IP)
        printf(", from IP [%s]", IP);
    printf(", on port %u", (port & 0xffff) );
#ifdef SHOW_ENDIAN_MADNESS
    printf(" (nw %s",
        hexify_u_short((char *)&client->sin_port) );
    if (IsBE) // Intel - show the host (this machines) hex as well
        printf(", BE %s", hexify_u_short((char *)&port));
    else
        printf(" is LE");
    port = bswap_16(port);
    printf(", np %u)", (port & 0xffff));
    printf(" %s", (IsLittleEndian ? "Little Endian" : "Big Endian") );
#endif // SHOW_ENDIAN_MADNESS
    printf("\n");
}

void UDP_S2_Help(char * name)
{
    printf("%s [Options]\n", name);
    printf("Options:\n");
    printf(" -?        = This help, and exit 0\n");
    printf(" -h <host> = Set HOST. Def is 0.0.0.0 (INADDR_ANY)\n");
    printf(" -p <port> = Set PORT. Def is %d\n", MY_DEF_PORT );
    printf("Establish a UDP Server waiting for a client.\n" );
    printf("Client should send a %d byte message, being the amount of\n", sizeof(int));
    printf(" increment to add to total, and send total back to client.\n");
    printf(" Cycle, and wait for next client connection.\n");
    printf("Uses simple recvfrom(), and sendto() for the reply.\n");
}

int main(int argc, char * argv[])
{
    int iret = 0;
    SOCKET sock = 0;  /* Sock we will talk on */
    struct sockaddr_in client, server; /* Address information for client and server */
    int recvd; /* Number of bytes recieved */
    int thisinc; /* How many should we increment this time? */
    int count = 0; /* Our current count */
    socklen_t structlength; /* Length of sockaddr structure */
    int port = 0; /* The port we will talk on. */
    int i, status;

    printf("UDP Server2 - compiled on %s, at %s\n", __DATE__, __TIME__);
    sock_init();
    for (i = 1; i < argc; i++)
    {
        char * cp = argv[i];
        if (*cp == '-') {
            // option
            while (*cp == '-') cp++;
            switch (*cp)
            {
            case '?':
                UDP_S2_Help(argv[0]);
                goto UDP_Server2_End;
                break;
            case 'p':
                i++;
                if ( i < argc ) {
                    port = atoport(argv[i],(char *)"udp"); /* atoport is from sockhelp.c */
                    if (port < 0) {
                        fprintf(stderr,"UDP Server2 - Bad PORT number! [%s]\n", argv[i]);
                        i--;
                        goto Bad_Option;
                    }
                } else {
                    fprintf(stderr,"UDP Server2 - 'p' must be followed by a PORT number!\n");
                    i--;
                    goto Bad_Option;
                }
                break;
            default:
                goto Bad_Option;
                break;
            }
        } else {
Bad_Option:
            fprintf(stderr,"UDP Server2 - ERROR: Unknown item [%s]. Try -?\n",
                argv[i] );
            iret = 1;
            goto UDP_Server2_End;
        }
    } // process any command line


    if (port == 0) {
        fprintf(stderr,"UDP Server2 - Using default port %d\n", MY_DEF_PORT);
        port = htons(MY_DEF_PORT);
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (SERROR(sock)) {
        PERROR("ERROR: socket() creation FAILED!");
        iret = 1;
        goto UDP_Server2_End;
    }

    /* Really there is no need for both a client and server structure
       here.  Both are initialized to the same values, and the server
       structure is only needed for the bind, so one would have done
       the trick.  Two were used in the hope of making the source code
       clearer to read.  You bind to the server port, accepting
       messages from anywhere.  You recvfrom a client, and get the
       client information in another structure, so that you know who
       to repond to. */
    memset((char *) &client, 0, sizeof(client));
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = htonl(INADDR_ANY);
    client.sin_port = port;

    memset((char *) &server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = port;

    structlength = sizeof(server);
    status = bind(sock, (struct sockaddr *) &server, structlength);
    if (SERROR(status)) {
        PERROR("ERROR: bind() FAILED!");
        iret = 1;
        goto UDP_Server2_End;
     }
     printf("UDP Server2 - On INADDR_ANY, bound to port %d, waiting for UPD client...\n",
         ntohs(port) );

    while (1) {   /* FOREVER */
        /* Get an increment request */
        structlength = sizeof(client);
        recvd = recvfrom(sock, (char *)&thisinc, sizeof(thisinc), 0, 
            (struct sockaddr *) &client, &structlength);
        if (SERROR(recvd)) {
            PERROR("ERROR: recvfrom() FAILED");
            iret = 1;
            goto UDP_Server2_End;
        }
        show_client( (char *)&thisinc, recvd, &client );
        if (recvd == sizeof(thisinc)) {
            thisinc = ntohl(thisinc); /* convert n/w to local */
            count += thisinc; /* bump the total */
            printf("UDP Server2 - Adding %d. Count now at %d.\n",thisinc,count);
            count = htonl(count); /* local to n/w */
            /* Send back the current total */
            status = sendto(sock, (char *)&count, sizeof(count), 0, 
                (struct sockaddr *) &client, structlength);
            if (SERROR(status)) {
                PERROR("ERROR: sendto() FAILED!");
                iret = 1;
                goto UDP_Server2_End;
            }
            count = ntohl(count);
            printf("UDP Server2 - Sent %d bytes... Waiting for next UDP client...\n",status);
        } else {
            printf("UDP Server2 - Received %d bytes, NOT %d. Ignored... Wait next...\n",recvd, sizeof(thisinc));
        }
    } /* FOREVER */

UDP_Server2_End:

    if (sock && !SERROR(sock))
        SCLOSE(sock);
    sock_end();
    return iret;
}

/* eof - udpserver2.c */
