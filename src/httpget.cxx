// httpget.cxx
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h> // for unix atoi(),...
#include <stdio.h>
#include <string.h> // memset
#ifdef _MSC_VER
// for windows
#include <WinSock2.h>
#pragma comment( lib, "ws2_32" ) /* link with ws2_32.DLL */
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#pragma comment (lib, "Winmm.lib") // __imp__timeGetTime@0
#else
// for unix
#include <sys/socket.h>
#include <netdb.h> // for gethostbyname()
#include <arpa/inet.h> // for inet_ntoa()
#include <fcntl.h>
#include <unistd.h>  // open(), close(), ...
#endif
#include <string>
#include <iostream>

using namespace std;

static const char *module = "httpget";

#define MX_GET_BUF 1024*16
////////////////////////////////////
// DEFAULTS
///////////
#ifndef PORTNUM
#define PORTNUM 80
#endif
#ifndef HOST
#define HOST "crossfeed.freeflightsim.org"
#endif
#define DEF_SOCK_TYPE SOCK_STREAM

#if (!defined(SOCKET) && !defined(_MSC_VER))
typedef int SOCKET;
#endif

static int verbosity = 1;

#define VERB1 (verbosity >= 1)
#define VERB2 (verbosity >= 2)
#define VERB5 (verbosity >= 5)
#define VERB9 (verbosity >= 9)

#ifdef _MSC_VER
#define SERROR(a)   (a == SOCKET_ERROR)
static int last_wsa_error;
static char * last_wsa_emsg = 0;
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
    char *pm = (char *)LocalAlloc( LPTR, 256 );
    if (!pm) {
        cerr << "Memory (LocalAlloc) FAILED! Aborting...\n";
        exit(1);
    }
    sprintf(pm,"WSA ERROR %d", err);
    return pm;
}

static void win_set_wsa_msg(void)
{
    last_wsa_error = WSAGetLastError();
    if (last_wsa_emsg) LocalFree(last_wsa_emsg);
    last_wsa_emsg = get_errmsg_text(last_wsa_error);
}
static void win_wsa_perror(char * msg)
{
    win_set_wsa_msg();
    fprintf(stderr,"%s [%s]\n", msg, last_wsa_emsg);
}
#define PERROR(a) win_wsa_perror(a)
#define SCLOSE closesocket
#define SREAD(a,b,c) recv(a,b,c,0)
#define SWRITE send
#else
#define SERROR(a)   (a < 0)
#define PERROR perror
#define SCLOSE close
#define SREAD read
#define SWRITE write
#endif

static char getReq[MX_GET_BUF+2];

int sendRequest1( int sockfd, const char *filename, const char *host )
{
    int len, ret;
    len = sprintf(getReq, "GET /%s HTTP/1.0\r\nHOST: %s\r\n\r\n", filename, host);
    ret = send(sockfd, getReq, len, 0);
    if (SERROR(ret)) {
       return -1;
    }
    return ret;
}

int sendRequest( int sockfd, const char *filename, const char *host )
{
    int len, res;
    len = sprintf(getReq, "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n",
        filename, host);
    res = send(sockfd, getReq, len, 0);
    if (VERB2) {
        printf("%s: Sent: '%s' %s\n", module, getReq,
            (SERROR(res) ? "ERROR" : "ok") );
    }
    if (SERROR(res)) {
        return -1;
    }
    return res;
}

void init_Sockets(void)
{
#if defined(_MSC_VER)
	/* Start up the windows networking */
	WORD version_wanted = MAKEWORD(1,1);
	WSADATA wsaData;
	if ( WSAStartup(version_wanted, &wsaData) != 0 ) {
        win_set_wsa_msg();
        fprintf(stderr, "WSAStartup FAILED! %s", last_wsa_emsg );
		exit(1);
	}
#endif
}

void end_Sockets(void)
{
#ifdef _MSC_VER
    WSACleanup();
#endif /* _MSC_VER */
}


static struct sockaddr_in serv_addr;
static struct hostent *hostptr;
const char *hostName = HOST;
const char *fileName = "flights.json";
static int portNumber = PORTNUM;
static fd_set ListenSocket;
static double curr_timeout = 1.0; // seconds to timeout
static struct timeval timeout;
static SOCKET highest = 0;
static int socket_type = DEF_SOCK_TYPE;
static const char *log_file = "tempget.log";
static FILE *plog = 0;

void set_time_out(void)
{
    int usecs = (int)(curr_timeout * 1000000.0);
    timeout.tv_sec = usecs / 1000000;
    timeout.tv_usec = usecs % 1000000;
}


void write_to_log( char *got, int len )
{
    if (len == 0)
        return;
    if (plog == 0) {
        plog = fopen(log_file,"wb");
        if (!plog) {
            printf("ERROR: Create log [%s] FAILED!\n", log_file);
            plog = (FILE *)-1;
            return;
        }
    }
    if (plog == (FILE *)-1)
        return;
    int res = fwrite(got,1,len,plog);
    if (res != len) {
        printf("ERROR: Write to log [%s] FAILED! req=%d, write=%d\n", log_file, len, res);
        fclose(plog);
        plog = (FILE *)-1;
    }
}

void give_help(char *name)
{
    size_t ii, len = strlen(name);
    int c;
    char *sn = name;
    for (ii = (len - 1); ii >= 0; ii--) {
        c = name[ii];
        if ((c == '/')||(c == '\\')) {
            sn = &name[ii + 1];
            break;
        }
    }
    printf("Usage: %s [options]\n", sn);
    printf("Options:\n");
    printf(" -?, -h, --help = This help, and exit 0\n");;
    printf(" -s <server>    = Set the HTTP server. (def=%s)\n", hostName);
    printf(" -f <file>      = Set the file to GET. (def=%s)\n", fileName);
    printf(" -p <port>      = Set the PORT number. (def=%d)\n", portNumber );
    printf(" -l <file>      = Set output log file. (def=%s)\n", log_file );
    printf(" -v[nn]         = Increment/Set verbosity. (def=%d)\n", verbosity );
    printf("Purpose: A very simple HTTP GET request, with anything fetched written\n");
    printf("to the log file.\n");

}

#ifndef ISNUM
#define ISNUM(a) (( a >= '0' )&&( a <= '9'))
#endif

int parse_command( int argc, char **argv )
{
    int i, i2, c;
    char *arg, *sarg;
    for (i = 1; i < argc; i++) {
        i2 = i + 1;
        arg = argv[i];
        if (!strcmp(arg,"--help") || !strcmp(arg,"-h") || !strcmp(arg,"-?")) {
            give_help(argv[0]);
            return 2;
        } else if (*arg == '-') {
            sarg = &arg[1];
            while (*sarg == '-') sarg++;
            c = *sarg;
            switch (c) 
            {
            case 's':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    hostName = strdup(sarg);
                    if (VERB2) printf("%s: Set URI to %s\n", module, hostName);
                } else {
                    printf("%s: Server (URI) must follow %s!\n", module, arg);
                    goto Bad_Arg;
                }
                break;
            case 'p':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    portNumber = atoi(sarg);
                    if (VERB2) printf("%s: Set PORT to %d\n", module, portNumber);
                } else {
                    printf("%s: PORT must follow %s!\n", module, arg);
                    goto Bad_Arg;
                }
                break;
            case 'l':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    log_file = strdup(sarg);
                    if (VERB2) printf("%s: Set output log file to %s\n", module, log_file);
                } else {
                    printf("%s: LOG file must follow %s!\n", module, arg);
                    goto Bad_Arg;
                }
                break;
            case 'f':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    fileName  = strdup(sarg);
                    if (VERB2) printf("%s: Set fetch file to %s\n", module, fileName);
                } else {
                    printf("%s: file name must follow %s!\n", module, arg);
                    goto Bad_Arg;
                }
                break;
            case 'v':
                while( *sarg == 'v' ) {
                    verbosity++;
                    sarg++;
                }
                c = *sarg;
                if (c) {
                    if (ISNUM(c)) {
                        verbosity = atoi(sarg);
                    }
                }
                if (VERB2) printf("%s: Set verbosity to %d\n", module, verbosity);
                break;
            default:
                goto Bad_Arg;
                break;
            }
        } else {
Bad_Arg:
            printf("%s: Error: Unknown argument [%s]! Use -? for help.\n", module, arg);
            return 1;
        }
    }
    return 0;
}

int main( int argc, char **argv )
{
    int iret = 0;
    int tries = 0;
    int max_tries = 10;
    int status;
    int total_recv = 0;

    // deal with command line
    iret = parse_command(argc,argv);
    if (iret) {
        if (iret == 2)
            return 0;
        return 1;
    }

    init_Sockets();
    hostptr = gethostbyname(hostName);
	if(hostptr == NULL) {
		printf("%s: ERROR: %s is unavailable!\n", module, hostName);
		return 1;
	}

	in_addr *address = (in_addr *)hostptr->h_addr;
	string ip_address = inet_ntoa(*address);
    cout << module << ": " << hostName << " (" << ip_address << ")\n";
    memset((char *)&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy((char *)&serv_addr.sin_addr,(char *)hostptr->h_addr,hostptr->h_length);
	serv_addr.sin_port = htons(portNumber);
	SOCKET sockfd = (int)socket(AF_INET,socket_type,0);
    if (SERROR(sockfd)) {
        PERROR("ERROR: creating socket!");
        end_Sockets();
        return 1;
    }
    if (sockfd > highest) highest = sockfd;
    if (VERB1) {
        printf("%s: host = %s, port %d, soc.type %s\n",
            module, hostName, portNumber,
            ((socket_type == SOCK_STREAM) ? "tcp" : "udp"));
    }

    status = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
    if (SERROR(status)) {
        PERROR("ERROR: connect socket!");
        iret = 1;
        goto End_Get;
    }
    status = sendRequest( sockfd, fileName, "Wget" );
    if (SERROR(status)) {
        PERROR("ERROR: send socket!");
        iret = 1;
        goto End_Get;
    }
    while (tries < max_tries) {
        tries++;
        FD_ZERO(&ListenSocket);
        FD_SET(sockfd,&ListenSocket); /* set our main listening socket */
        set_time_out();
        status = select( (int)highest+1, &ListenSocket, NULL, NULL, &timeout );
        if (SERROR(status)) {
            PERROR("ERROR: 'select' FAILED!");
            iret = 1;
            goto End_Get;
        }
        if (status == 0) {
            if (total_recv > 0)
                goto End_Get;
            printf("select timeout\n");
            continue;
        }
        if (status > 0) {
            if ( ! FD_ISSET(sockfd,&ListenSocket)) {
                printf("%s: 'select' returned status %d, BUT NO FD_SET!\n",module,status);
                continue;
            }
            status = SREAD(sockfd,getReq,MX_GET_BUF);
            if (SERROR(status)) {
                PERROR("ERROR: 'read' FAILED!");
                iret = 1;
                goto End_Get;
            }
            if (status == 0) {
                printf("%s: Socket CLOSED by remote\n", module);
                goto End_Get;
            }

            total_recv += status;
            printf("%s: Received %d bytes... {\n",module, status);
            getReq[status] = 0;
            write_to_log( getReq, status );
            printf("%s }\n",getReq);
        }
    }

End_Get:
    if (tries >= max_tries)
        printf("%s: Maximum tries exceeded!\n", module);
    SCLOSE(sockfd);
    end_Sockets();
    if (plog && (plog != (FILE *)-1))
        fclose(plog);
    return iret;
}

// eof
