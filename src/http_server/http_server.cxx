// http_server.cpp : Defines the entry point for the console application.
//

#include "http_server.hxx"

/*
AUTHOR: Abhijeet Rastogi (http://www.google.com/profiles/abhijeet.1989)

This is a very simple HTTP server. Default port is 10000 and ROOT for the server is your current working directory..

You can provide command line arguments like:- $./a.aout -p [port] -r [path]

for ex. 
$./a.out -p 50000 -r /home/
to start a server at port 50000 with root directory as "/home"

$./a.out -r /home/shadyabhi
starts the server at port 10000 with ROOT as /home/shadyabhi

*/

#include<sys/types.h>
#include<sys/stat.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#ifdef _MSC_VER
#include <WinSock2.h>
#include <Ws2tcpip.h>   // for inet_ntop, etc
#pragma comment( lib, "ws2_32" ) /* link with ws2_32.DLL */
#else // !_MSC_VER
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netdb.h>
#endif // _MSC_VER
#include<signal.h>
#include<fcntl.h>

#ifndef SPRTF
#define SPRTF printf
#endif

/* some definitions used */
#ifdef _MSC_VER
/* ========== for windows ========= */
#define SERROR(a) (a == SOCKET_ERROR)
#define SCLOSE closesocket
#define PERROR(a) win_wsa_perror(a)
#define SREAD recv
#define SWRITE(a,b,c) send(a,b,c,0)
#ifndef EINTR
#define EINTR WSAEINTR
#endif
#define PRId64 "I64d"
#define PRIu64 "I64u"
// get a message from the system for this error value
char *get_errmsg_text( int err )
{
    LPSTR ptr = 0;
    DWORD fm = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&ptr, 0, NULL );
    if (ptr) {
        size_t len = strlen(ptr);
        while(len--) {
            if (ptr[len] > ' ') break;
            ptr[len] = 0;
        }
        if (len) return ptr;
        LocalFree(ptr);
    }
    return NULL;
}

void win_wsa_perror( char *msg )
{
    int err = WSAGetLastError();
    LPSTR ptr = get_errmsg_text(err);
    if (ptr) {
        SPRTF("%s = %s (%d)\n", msg, ptr, err);
        LocalFree(ptr);
    } else {
        SPRTF("%s %d\n", msg, err);
    }
}

/* ================================ */
#else /* !_MSC_VER */
/* =========== unix/linux ========= */
#define SERROR(a) (a < 0)
typedef int SOCKET;
#define SCLOSE close
#define PERROR(a) perror(a)
#define SREAD read
#define SWRITE write
/* ================================ */
#endif /* _MSC_VER y/n */


/* Define to the version of this package. */
#define PACKAGE_VERSION "0.0.1"

#define CONNMAX 1000
#define BYTES 1024

char *ROOT;
int listenfd, clients[CONNMAX];
void error(char *);
void startServer(char *);
void respond(int);

int parse_cmds( int argc, char **argv )
{
    int iret = 0;
#ifdef _MSC_VER

#else
    //Parsing the command line arguments
    char c;    
    while ((c = getopt (argc, argv, "p:r:")) != -1)
        switch (c)
        {
            case 'r':
                ROOT = malloc(strlen(optarg));
                strcpy(ROOT,optarg);
                break;
            case 'p':
                strcpy(PORT,optarg);
                break;
            case '?':
                fprintf(stderr,"Wrong arguments given!!!\n");
                exit(1);
            default:
                exit(1);
        }
#endif
    return iret;
}    

static void net_exit ( void )
{
#ifdef _MSC_VER
	/* Clean up windows networking */
	if ( WSACleanup() == SOCKET_ERROR ) {
		if ( WSAGetLastError() == WSAEINPROGRESS ) {
			WSACancelBlockingCall();
			WSACleanup();
		}
	}
#endif
}


int net_init ()
{
#ifdef _MSC_VER
	/* Start up the windows networking */
	WORD version_wanted = MAKEWORD(2,2);
	WSADATA wsaData;
	if ( WSAStartup(version_wanted, &wsaData) != 0 ) {
		SPRTF("Couldn't initialize Winsock 2.2\n");
		return 1;
	}
#endif
    atexit( net_exit ) ;
	return 0;
}

int http_main(int argc, char* argv[])
{
    struct sockaddr_in clientaddr;
    socklen_t addrlen;
    
    //Default Values PATH = ~/ and PORT=10000
    char PORT[6];
    ROOT = getenv("PWD");
    strcpy(PORT,"10000");

    int slot=0;

    printf("Server started at port no. %s%s%s with root directory as %s%s%s\n","\033[92m",PORT,"\033[0m","\033[92m",ROOT,"\033[0m");
    // Setting all elements to -1: signifies there is no client connected
    int i;
    for (i=0; i<CONNMAX; i++)
        clients[i]=-1;

    if ( net_init() )
        return 1;

    startServer(PORT);

    // ACCEPT connections
    while (1)
    {
        addrlen = sizeof(clientaddr);
        clients[slot] = accept (listenfd, (struct sockaddr *) &clientaddr, &addrlen);

        if (clients[slot]<0)
            PERROR ("accept() error");
        else
        {
#ifdef _MSC_VER
            respond(slot);
#else
            if ( fork()==0 )
            {
                respond(slot);
                exit(0);
            }
#endif
        }

        while (clients[slot]!=-1) slot = (slot+1)%CONNMAX;
    }

    return 0;
}


//start server
void startServer(char *port)
{
    struct addrinfo hints, *res, *p;

    // getaddrinfo for host
    memset (&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo( NULL, port, &hints, &res) != 0)
    {
        perror ("getaddrinfo() error");
        exit(1);
    }
    // socket and bind
    for (p = res; p!=NULL; p=p->ai_next)
    {
        listenfd = socket (p->ai_family, p->ai_socktype, 0);
        if (listenfd == -1) continue;
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;
    }
    if (p==NULL)
    {
        perror ("socket() or bind()");
        exit(1);
    }

    freeaddrinfo(res);

    // listen for incoming connections
    if ( listen (listenfd, 1000000) != 0 )
    {
        perror("listen() error");
        exit(1);
    }
}

#ifdef _MSC_VER
#define SD_FLAG SD_BOTH
#else
#define SD_FLAG SHUT_RDWR
#endif

//client connection
void respond(int n)
{
    char mesg[99999], *reqline[3], data_to_send[BYTES], path[99999];
    int rcvd, bytes_read;
    FILE *fd;

    memset( (void*)mesg, (int)'\0', 99999 );

    rcvd=recv(clients[n], mesg, 99999, 0);

    if (rcvd<0)    // receive error
        fprintf(stderr,("recv() error\n"));
    else if (rcvd==0)    // receive socket closed
        fprintf(stderr,"Client disconnected upexpectedly.\n");
    else    // message received
    {
        printf("%s", mesg);
        reqline[0] = strtok (mesg, " \t\n");
        if ( strncmp(reqline[0], "GET\0", 4)==0 )
        {
            reqline[1] = strtok (NULL, " \t");
            reqline[2] = strtok (NULL, " \t\n");
            if ( strncmp( reqline[2], "HTTP/1.0", 8)!=0 && strncmp( reqline[2], "HTTP/1.1", 8)!=0 )
            {
                SWRITE(clients[n], "HTTP/1.0 400 Bad Request\n", 25);
            }
            else
            {
                if ( strncmp(reqline[1], "/\0", 2)==0 )
                    reqline[1] = "/index.html";        //Because if no file is specified, index.html will be opened by default (like it happens in APACHE...

                strcpy(path, ROOT);
                strcpy(&path[strlen(ROOT)], reqline[1]);
                printf("file: %s\n", path);

                if ( (fd= fopen(path, "r")))    //FILE FOUND
                {
                    send(clients[n], "HTTP/1.0 200 OK\n\n", 17, 0);
                    while ( (bytes_read= fread(data_to_send, BYTES, 1, fd))>0 )
                        SWRITE (clients[n], data_to_send, bytes_read);
                }
                else    SWRITE(clients[n], "HTTP/1.0 404 Not Found\n", 23); //FILE NOT FOUND
            }
        }
    }

    //Closing SOCKET
    shutdown (clients[n], SD_FLAG );         //All further send and recieve operations are DISABLED...
    SCLOSE(clients[n]);
    clients[n]=-1;
}


int main(int argc, char**argv)
{
	return 0;
}

// eof - http_server.cxx
