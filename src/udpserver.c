/*******************udpserver.c*****************/
/* Header files needed to use the sockets API. */
/* File contain Macro, Data Type and Structure */
/* definitions along with Function prototypes. */
/* header files */
#ifdef _MSC_VER
#include <Winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef _MSC_VER
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif // !_MSC_VER

/* Server's port number, listen at 3333 */
#define SERVPORT 3333

/* Run the server without argument */
int main(int argc, char *argv[])
{
    /* Variable and structure definitions. */
    SOCKET sd, rc;
    struct sockaddr_in serveraddr, clientaddr;
    int clientaddrlen = sizeof(clientaddr);
    int serveraddrlen = sizeof(serveraddr);
    char buffer[100];
    char *bufptr = buffer;
    int buflen = sizeof(buffer);

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
    if( SERROR(sd) )
    {
        PERROR("UDP server - socket() error");
        sock_end();
        exit(-1);
    }

    printf("UDP server - socket() is OK\n");
    printf("UDP server - try to bind...\n");

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

    rc = bind(sd, (struct sockaddr *)&serveraddr, serveraddrlen);
    if( SERROR(rc) )
    {
        PERROR("UDP server - bind() error");
        SCLOSE(sd);
        sock_end();
        /* If something wrong with socket(), just exit lol */
        exit(-1);
    }

    printf("UDP server - bind() is OK\n");
    printf("UDP server - Using IP %s and port %d\n", inet_ntoa(serveraddr.sin_addr), SERVPORT);
    printf("UDP server - Listening...\n");

    /* Use the recvfrom() function to receive the */
    /* data. The recvfrom() function waits */
    /* indefinitely for data to arrive. */
    /************************************************/
    /* This example does not use flags that control */
    /* the reception of the data. */
    /************************************************/

    /* Wait on client requests. */
    rc = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)&clientaddr, &clientaddrlen);
    if ( SERROR(rc) )
    {
        PERROR("UDP Server - recvfrom() error");
        SCLOSE(sd);
        sock_end();
        exit(-1);
    }

    printf("UDP Server - recvfrom() is OK...\n");
    printf("UDP Server received the following:\n \"%s\" message\n", bufptr);
    printf("from port %d and address %s.\n", ntohs(clientaddr.sin_port),
        inet_ntoa(clientaddr.sin_addr));

    /* Send a reply by using the sendto() function. */
    /* In this example, the system echoes the received */
    /* data back to the client. */
    /************************************************/
    /* This example does not use flags that control */
    /* the transmission of the data */
    /************************************************/

    /* Send a reply, just echo the request */
    printf("UDP Server - replying to the stupid UDP client...\n");
    rc = sendto(sd, bufptr, buflen, 0, 
        (struct sockaddr *)&clientaddr, clientaddrlen);
    if( SERROR(rc) )
    {
        PERROR("UDP server - sendto() error");
        SCLOSE(sd);
        sock_end();
        exit(-1);
    }

    printf("UDP Server - sendto() is OK...\n");

    /* When the data has been sent, close() the */
    /* socket descriptor. */
    /********************************************/
    /* close() the socket descriptor. */
    SCLOSE(sd);
    sock_end();
    // exit(0);
    return 0;
}

// eof - udpserver.c
