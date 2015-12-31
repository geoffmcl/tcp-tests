/****************udpclient.c********************/
/* Header files needed to use the sockets API. */
/* File contain Macro, Data Type and Structure */
/* definitions along with Function prototypes. */
/* 2011-04-04: ported to MS Windows - geoff    */
/***********************************************/
// sim_server.cxx
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif // !_MSC_VER

#include "winsockerr.cxx"

/* Host name of my system, change accordingly */
/* Put the server hostname that run the UDP server program */
/* This will be used as default UDP server for client connection */
#define SERVER "127.0.0.1"

/* Server's port number */
#define SERVPORT 3333

/* Pass in 1 argument (argv[1]) which is either the */
/* address or host name of the server, or */
/* set the server name in the #define SERVER above. */
int main(int argc, char *argv[])
{

    /* Variable and structure definitions. */
    SOCKET sd, rc;
    struct sockaddr_in serveraddr, clientaddr;
    socklen_t serveraddrlen = sizeof(serveraddr);
    char server[255];
    char buffer[100];
    char *bufptr = buffer;
    int buflen = sizeof(buffer);
    struct hostent *hostp;

    printf("UDP Client - compiled on %s, at %s\n", __DATE__, __TIME__);
    
    /* setup the message to send, but the whoe buffer is sent... */
    memset(buffer, 0x00, sizeof(buffer)); /* clear to all zero */
    /* 36 characters + terminating NULL */
    memcpy(buffer, "Hello! A client request message lol!", 37);

    /* The socket() function returns a socket */
    /* descriptor representing an endpoint. */
    /* The statement also identifies that the */
    /* INET (Internet Protocol) address family */
    /* with the UDP transport (SOCK_DGRAM) will */
    /* be used for this socket. */
    /******************************************/
    /* get a socket descriptor */
    sock_init(); /* none for unix, but needed for Windows ... */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (SERROR(sd) ) {
        PERROR("UDP Client - socket() error");
        sock_end(); /* Just exit lol! */
        exit(-1);
    }

    printf("UDP Client - socket() is OK! value 0x%X (%u) %s\n", sd, sd, get_datetime_str());

    /* If the hostname/IP of the server is supplied */
    /* Or use current machine */
    if( argc > 1 ) {
        strcpy(server, argv[1]); /* assumed host name, or IP address */
    } else {
        /*Use default hostname or IP*/
        printf("UDP Client - Usage %s [ServerName/IP]\n", argv[0]);
        if (get_local_host()) {
            printf("UDP Client - Using local host IP [%s], name [%s]\n",
                localIP, localHost->h_name);
            strcpy(server, localIP); // copy in IP address
            // could also tyr the host NAME in localHost->h_name
        } else {
            printf("UDP Client - Using local default hostname/IP [%s]\n", SERVER);
            strcpy(server, SERVER);
        }
    }

    memset(&serveraddr, 0x00, sizeof(struct sockaddr_in));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVPORT);
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
            SCLOSE(sd);
            sock_end();
            exit(-1);
        } else {
            printf("UDP Client - gethostname() of the server is OK... \n");
        }
        memcpy(&serveraddr.sin_addr, hostp->h_addr, sizeof(serveraddr.sin_addr));
    }

    /* Use the sendto() function to send the data */
    /* to the server. */
    /************************************************/
    /* This example does not use flags that control */
    /* the transmission of the data. */
    /************************************************/

    /* send request to server */
    printf("UDP Client - sendto IP %s, port %u, len %d bytes...\n",
        inet_ntoa(serveraddr.sin_addr),
        ntohs(serveraddr.sin_port),
        buflen);
        
    rc = sendto(sd, bufptr, buflen, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (SERROR(rc)) {
        PERROR("UDP Client - sendto() error");
        SCLOSE(sd);
        sock_end();
        exit(-1);
    }

    printf("UDP Client - sendto() OK! %d sent [%s]\n", rc, bufptr);
    printf("UDP Client - Waiting reply from UDP server... if any... Ctrl+c to abort...\n");

    /* Use the recvfrom() function to receive the */
    /* data back from the server. */
    /************************************************/
    /* This example does not use flags that control */
    /* the reception of the data. */
    /************************************************/
    /* Read server reply. */
    /* Note: serveraddr is reset on the recvfrom() function. */
    rc = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)&serveraddr, &serveraddrlen);
    if (SERROR(rc)) {
        PERROR("UDP Client - recvfrom() error");
        SCLOSE(sd);
        sock_end();
        exit(-1);
    }

    printf("UDP Client - recvfrom() IP %s, port %d\n", 
        inet_ntoa(serveraddr.sin_addr),
        ntohs(serveraddr.sin_port) );
        
    printf("UDP Client - received %d message: [%s]\n", rc, bufptr);

    /* When the data has been received, close() */
    /* the socket descriptor. */
    /********************************************/
    /* close() the socket descriptor. */
    printf("UDP Client - close and exit... %s\n", get_datetime_str() );
    SCLOSE(sd);
    sock_end();
    // exit(0);
    return 0;
}

// eof - udpclient.c
