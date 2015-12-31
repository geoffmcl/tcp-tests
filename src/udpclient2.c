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

#ifdef _MSC_VER
#include <Winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sockhelp.h"

#include "sockhelp.c"

#define MY_DEF_COUNT 10

void show_help(void)
{
    fprintf(stderr,"Usage: udpclient [-h address] [-p port] [increment] \n");
    fprintf(stderr,"where address is an ip address, or host name, and\n");
    fprintf(stderr,"port is the port numer of the server.\n");
    fprintf(stderr,"Increment is the number to add to the\n");
    fprintf(stderr,"server's count. Using default count %d if none given.\n", MY_DEF_COUNT);
}

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

int main(int argc, char *argv[])
{
    SOCKET sock; /* Our socket that we will talk through */
    struct sockaddr_in client, server; /* Client and server address info */
    int recvd; /* Number of bytes recieved */
    int inc_amount = 0; /* Amount we want to incement the server's number */
    int total; /* New total, returned from server */
    int structlength; /* Length of sockaddr structure */
    int port = 0; /* Port we will talk on */
    struct in_addr *addr = NULL; /* Internet address of server */
    char *err; /* Used to test for valid integer value */
    int i, c, status;

    printf("UDP Client - compiled on %s, at %s\n", __DATE__, __TIME__);
    for (i = 1; i < argc; i++)
    {
        err = argv[i];
        if (*err == '-') {
            while (*err == '-') err++;
            c = *err;
            switch (c)
            {
            case '?':
                show_help();
                exit(EXIT_SUCCESS);
                break;
            case 'h':
                i++;
                if (i < argc) {
                    addr = atoaddr(argv[i]); /* atoaddr is from sockhelp.c */
                    if (addr == NULL) {
                        fprintf(stderr,"ERROR: Invalid network address: %s\n",argv[i]);
                        exit(EXIT_FAILURE);
                    }
                } else {
                    i--;
                    fprintf(stderr,"ERROR: Need additional argument\n");
                    goto Bad_Arg;
                }
                break;
            case 'p':
                i++;
                if (i < argc) {
                    port = atoport(argv[i], "udp"); /* atoport is from sockhelp.c */
                    if (port == -1) {
                        fprintf(stderr,"ERROR: Can't use port: %s\n",argv[i]);
                        exit(EXIT_FAILURE);
                    }
                } else {
                    i--;
                    fprintf(stderr,"ERROR: Need additional argument\n");
                    goto Bad_Arg;

                }
                break;
            default:
Bad_Arg:
                fprintf(stderr,"ERROR: Invalid argument! Try -?\n",argv[i]);
                exit(EXIT_FAILURE);
                break;
            }
        } else {
            // bare argument = count
            inc_amount = strtol(argv[i],&err,0);
            if ((err[0] != 0)||(inc_amount == 0)) {
                fprintf(stderr,"ERROR: %s is not a valid integer number.\n",argv[i]);
                exit(EXIT_FAILURE);
            }
        }
    }

    if (inc_amount == 0) {
        inc_amount = MY_DEF_COUNT;
        printf("UDP Client - No increment given. Using default %d\n", inc_amount);
    }
    inc_amount = htonl(inc_amount); // to network order
    if (port == 0) {
        printf("UDP Client - Using default port %d\n", MY_DEF_PORT);
        port = htons(MY_DEF_PORT);
    }
    sock_init(); /* initialize sockets (if any needed) */
    if (addr == NULL) {
        if (get_local_host()) {
            printf("UDP Client - Using local host IP [%s], name [%s]\n",
                localIP, localHost->h_name);
            // strcpy(server, localIP); // copy in IP address
            addr = atoaddr(localIP); /* atoaddr is from sockhelp.c */
            if (addr == NULL) {
                fprintf(stderr,"ERROR: Unable to get valid network address from: %s\n",localIP);
                sock_end();
                exit(EXIT_FAILURE);
            }
        } else {
            fprintf(stderr,"ERROR: Unable to get local address!\n");
            sock_end();
            exit(EXIT_FAILURE);
        }
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (SERROR(sock)) {
        PERROR("ERROR: socket() FAILED!");
        exit(EXIT_FAILURE);
    }

    /* We need two sockaddr_in structures here.  One is bound to the socket
     we want to talk on, and the other is used to indicate who we want to
     talk to. */

    /* Set up the server info */
    memset((char *) &server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = addr->s_addr;
    server.sin_port = port;

    /* Set up another structure for our local socket */
    memset((char *) &client, 0, sizeof(client));
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = htonl(INADDR_ANY);
    client.sin_port = htons(0);
    /* bind the client to the port given */
    status = bind(sock, (struct sockaddr *) &client, sizeof(client));
    if (SERROR(status)) {
        PERROR("ERROR: bind() FAILED!");
        SCLOSE(sock);
        sock_end();
        exit(EXIT_FAILURE);
    }

    /* Send request for increment to server.  Note that all amounts are
     transmitted in network byte order, so that the client and server
     can run on different architectures. */
    status = sendto(sock, (char *)&inc_amount, sizeof(inc_amount), 0,
        (struct sockaddr *) &server, sizeof(server));
    if (SERROR(status)) {
        PERROR("ERROR: sendto() FAILED!");
        SCLOSE(sock);
        sock_end();
        exit(EXIT_FAILURE);
    }
    printf("UDP Client - Sent %d bytes. (%d). Await reply...\n",
        status, sizeof(inc_amount) );

    /* Then wait for new total amount. */
    structlength = sizeof(client);
    recvd = recvfrom(sock, (char *)&total, sizeof(total), 0,
        (struct sockaddr *) &client, &structlength);
    if (SERROR(recvd)) {
        PERROR("ERROR: recvfrom() FAILED!");
        SCLOSE(sock);
        sock_end();
        exit(EXIT_FAILURE);
    }
    if (recvd != sizeof(total)) {
        fprintf(stderr,"Got back wrong number of bytes!\n");
        SCLOSE(sock);
        sock_end();
        exit(EXIT_FAILURE);
    }
    total = ntohl(total); /* from network order to local order */
    printf("UDP Client - Current count: %i. Closing client...\n",total);
    SCLOSE(sock);
    sock_end();
    return 0;
}

/* eof -udpclient2.c */
