/*******************udpserver.c*****************/
/* Header files needed to use the sockets API. */
/* File contain Macro, Data Type and Structure */
/* definitions along with Function prototypes. */
/* header files */
/* 2011-04-04: ported to MS Windows - geoff    */
// udpserver.cxx
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef _MSC_VER
#include <ctype.h> /* for toupper(), ... */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif // !_MSC_VER

#include "winsockerr.cxx"

/* Server's port number, listen at 3333 */
#define SERVPORT 3333

/* Run the server without argument */
int main(int argc, char *argv[])
{
    /* Variable and structure definitions. */
    SOCKET sd, rc;
    struct sockaddr_in serveraddr, clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    socklen_t serveraddrlen = sizeof(serveraddr);
    char buffer[100];
    char *bufptr = buffer;
    int buflen = sizeof(buffer);
    int i;
    
    printf("UDP Server - compiled on %s, at %s\n", __DATE__, __TIME__);
    /* The socket() function returns a socket */
    /* descriptor representing an endpoint. */
    /* The statement also identifies that the */
    /* INET (Internet Protocol) address family */
    /* with the UDP transport (SOCK_DGRAM) will */
    /* be used for this socket. */
    /******************************************/
    sock_init(); // need some init for windows

    /* get a socket descriptor */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if( SERROR(sd) ) {
        PERROR("UDP server - socket() error");
        sock_end();
        exit(-1);
    }

    printf("UDP server - socket() is OK, sd = 0x%X (%u) %s\n", sd, sd, get_datetime_str() );

    /* After the socket descriptor is received, */
    /* a bind() is done to assign a unique name */
    /* to the socket. In this example, the user */
    /* set the s_addr to zero. This allows the */
    /* system to connect to any client that uses */
    /* port 3333. */
    /********************************************/

    /* bind to address */
    memset(&serveraddr, 0x00, serveraddrlen);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVPORT);
    
    printf("UDP server - try to bind() IP %s, port %d, len %d\n", inet_ntoa(serveraddr.sin_addr),
        ntohs(serveraddr.sin_port), serveraddrlen );
        
    rc = bind(sd, (struct sockaddr *)&serveraddr, serveraddrlen);
    if( SERROR(rc) ) {
        PERROR("UDP server - bind() error");
        SCLOSE(sd);
        sock_end();
        /* If something wrong with socket(), just exit lol */
        exit(-1);
    }

    printf("UDP Server - bind() OK IP %s, port %d\n", inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port));
    
    printf("UDP Server - Doing recvfrom()... blocking until received... Ctrl+c to abort...\n");
    /* Use the recvfrom() function to receive the */
    /* data. The recvfrom() function waits */
    /* indefinitely for data to arrive. */
    /************************************************/
    /* This example does not use flags that control */
    /* the reception of the data. */
    /************************************************/
    /* Wait on client requests. */
    rc = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)&clientaddr, &clientaddrlen);
    if ( SERROR(rc) ) {
        PERROR("UDP Server - recvfrom() error");
        SCLOSE(sd);
        sock_end();
        exit(-1);
    }

    printf("UDP Server - recvfrom() OK IP %s, port %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
    printf("UDP Server - received message, len %d: [%s]\n", rc, bufptr);

    /* Send a reply by using the sendto() function. */
    /* In this example, the system echoes the received */
    /* data back to the client. */
    /************************************************/
    /* This example does not use flags that control */
    /* the transmission of the data */
    /************************************************/

    /* Send a reply, just echo, in upper case the request */
    for (i = 0; i < buflen; i++)
        bufptr[i] = toupper(bufptr[i]);
    printf("UDP Server - echo back in uppercase to UDP client, using sendto()...\n");
    rc = sendto(sd, bufptr, buflen, 0, (struct sockaddr *)&clientaddr, clientaddrlen);
    if( SERROR(rc) ) {
        PERROR("UDP server - sendto() error");
        SCLOSE(sd);
        sock_end();
        exit(-1);
    }

    printf("UDP Server - sendto() sent %d bytes [%s]\n", rc, bufptr);

    /* When the data has been sent, close() the */
    /* socket descriptor. */
    /********************************************/
    /* close() the socket descriptor. */
    printf("UDP Server - close and exit... %s\n", get_datetime_str());
    SCLOSE(sd);
    sock_end();
    return 0;
}

// eof - udpserver.c
