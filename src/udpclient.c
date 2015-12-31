/****************udpclient.c********************/
/* Header files needed to use the sockets API. */
/* File contain Macro, Data Type and Structure */
/* definitions along with Function prototypes. */
/***********************************************/
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
#include <netdb.h>
#endif // !_MSC_VER
 
/* Host name of my system, change accordingly */
/* Put the server hostname that run the UDP server program */
/* This will be used as default UDP server for client connection */
#define SERVER "Dell01"

/* Server's port number */
#define SERVPORT 3333

static hostent* localHost = NULL;
static char* localIP = NULL;

int get_local_host(void)
{
    int iret = 1; // assume success
    // Get the local host information
    localHost = gethostbyname("");
    if (localHost) {
        localIP = inet_ntoa (*(struct in_addr *)*localHost->h_addr_list);
        if (localIP)
            return 1;
    }
    return 0;
}

/* Pass in 1 argument (argv[1]) which is either the */
/* address or host name of the server, or */
/* set the server name in the #define SERVER above. */
int main(int argc, char *argv[])
{

    /* Variable and structure definitions. */
    SOCKET sd, rc;
    struct sockaddr_in serveraddr, clientaddr;
    int serveraddrlen = sizeof(serveraddr);
    char server[255];
    char buffer[100];
    char *bufptr = buffer;
    int buflen = sizeof(buffer);
    struct hostent *hostp;

    memset(buffer, 0x00, sizeof(buffer));

    /* 36 characters + terminating NULL */
    memcpy(buffer, "Hello! A client request message lol!", 37);

    printf("UDP Client - compiled on %s, at %s\n", __DATE__, __TIME__);
    /* The socket() function returns a socket */
    /* descriptor representing an endpoint. */
    /* The statement also identifies that the */
    /* INET (Internet Protocol) address family */
    /* with the UDP transport (SOCK_DGRAM) will */
    /* be used for this socket. */
    /******************************************/

    /* get a socket descriptor */
    sock_init();
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (SERROR(sd) )
    {
        PERROR("UDP Client - socket() error");
        sock_end();
        /* Just exit lol! */
        exit(-1);
    }

    printf("UDP Client - socket() is OK!\n");

    /* If the hostname/IP of the server is supplied */
    /* Or if(argc = 2) */
    if( argc > 1 )
    {
        strcpy(server, argv[1]);
    }
    else
    {
        /*Use default hostname or IP*/
        printf("UDP Client - Usage %s <Server hostname or IP>\n", argv[0]);
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
    if( serveraddr.sin_addr.s_addr == (unsigned long)INADDR_NONE )
    {
        /* Use the gethostbyname() function to retrieve */
        /* the address of the host server if the system */
        /* passed the host name of the server as a parameter. */
        /************************************************/
        /* get server address */
        hostp = gethostbyname(server);
        if(hostp == (struct hostent *)NULL)
        {
            printf("HOST NOT FOUND --> ");
            /* h_errno is usually defined */
            /* in netdb.h */
            printf("h_errno = %d\n", h_errno);
            SCLOSE(sd);
            sock_end();
            exit(-1);
        }
        else
        {
            printf("UDP Client - gethostname() of the server is OK... \n");
            printf("UDP Client - Connected to UDP server %s on port %d.\n", server, SERVPORT);
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

    rc = sendto(sd, bufptr, buflen, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (SERROR(rc))
    {
        PERROR("UDP Client - sendto() error");
        SCLOSE(sd);
        sock_end();
        exit(-1);
    }

    printf("UDP Client - sendto() is OK!\n");
    printf("UDP Client - Waiting a reply from UDP server...\n");

    /* Use the recvfrom() function to receive the */
    /* data back from the server. */
    /************************************************/
    /* This example does not use flags that control */
    /* the reception of the data. */
    /************************************************/

    /* Read server reply. */
    /* Note: serveraddr is reset on the recvfrom() function. */
    rc = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)&serveraddr, &serveraddrlen);
    if (SERROR(rc))
    {
        PERROR("UDP Client - recvfrom() error");
        SCLOSE(sd);
        sock_end();
        exit(-1);
    }

    printf("UDP client - received the following: \"%s\" message\n", bufptr);
    printf(" from port %d, address %s\n", ntohs(serveraddr.sin_port), inet_ntoa(serveraddr.sin_addr));

    /* When the data has been received, close() */
    /* the socket descriptor. */
    /********************************************/
    /* close() the socket descriptor. */
    SCLOSE(sd);
    sock_end();
    // exit(0);
    return 0;
}

// eof - udpclient.c
