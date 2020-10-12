/**************************************************************************/
/* unix_client.cxx                                                        */
/* Generic client example is used with connection-oriented server designs */
/**************************************************************************/
#ifdef _MSC_VER
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h> /* open(), close() */
#include <string.h> /* memset() */
#include <arpa/inet.h> /* inet_addr() */
#endif

#define SPRTF printf
/* some definitions used */
#ifdef _MSC_VER
/* ========== for windows ========= */
#define SERROR(a) (a == SOCKET_ERROR)
#define SCLOSE closesocket
#define PERROR(a) win_wsa_perror(a)
#define SREAD recv
#define SWRITE send
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


#define DEF_SERVER_PORT  3333
#define DEF_SERVER_IP "127.0.0.1"

static const char* module = "unix_client";

static const char* server_ip = DEF_SERVER_IP;
static unsigned short server_port = DEF_SERVER_PORT;

void give_help(char* name)
{
    printf("%s: usage: [options]\n", module);
    printf("Options:\n");
    printf(" --help  (-h or -?) = This help and exit(0)\n");
    printf(" --ip <address> (i) = Set server ip address. (def=%s)\n", server_ip);
    printf(" --port <port) (-p) = Set coomuniation port. (def=%u)\n", server_port);
    printf("\n");
    printf(" Create a 'socket', connect to server on IP:port, and offer to send\n");
    printf(" messages to the server... simple...\n");
}

int parse_args(int argc, char** argv)
{
    int i, i2, c;
    char* arg, * sarg;
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        i2 = i + 1;
        if (*arg == '-') {
            sarg = &arg[1];
            while (*sarg == '-')
                sarg++;
            c = *sarg;
            switch (c) {
            case 'h':
            case '?':
                give_help(argv[0]);
                return 2;
                break;
            case 'i':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    server_ip = strdup(sarg);
                }
                else {
                    printf("%s: Error: Expect 'ip' address to follow '%s'! Aborting...\n", module, arg);
                    return 1;
                }
                break;
            case 'p':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    server_port = atoi(sarg);
                }
                else {
                    printf("%s: Error: Expect 'port' value to follow '%s'! Aborting...\n", module, arg);
                    return 1;
                }
                break;
            default:
                printf("%s: Unknown argument '%s'. Try -? for help...\n", module, arg);
                return 1;
            }
        }
        else {
            // bear argument
            printf("%s: Error: What is this '%s'? Use -? for help.\n", module, arg);
            return 1;
        }
    }
    return 0;
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

int main (int argc, char *argv[])
{
   int    len, rc, slen, i;
   int    sockfd;
   char   send_buf[80];
   char   recv_buf[80];
   struct sockaddr_in   addr;

   rc = parse_args(argc, argv);
   if (rc) {
       if (rc == 2)
           rc = 0;
       return rc;
   }

   if (net_init())
       return 1;
   /*************************************************/
   /* Create an AF_INET stream socket               */
   /*************************************************/
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0)
   {
      perror("socket");
      exit(1);
   }
   /*************************************************/
   /* Initialize the socket address structure       */
   /*************************************************/
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   // addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_addr.s_addr = inet_addr(server_ip);
   addr.sin_port        = htons(server_port);

   /*************************************************/
   /* Connect to the server                         */
   /*************************************************/
   rc = connect(sockfd,
                (struct sockaddr *)&addr,
                sizeof(struct sockaddr_in));
   if (SERROR(rc))
   {
      PERROR("connect");
      SCLOSE(sockfd);
      exit(1);
   }
   printf("Connect completed.\n");

   while (1) {
       /*************************************************/
       /* Enter data buffer that is to be sent          */
       /*************************************************/
       printf("Enter message to be sent:\n");
       send_buf[0] = 0;
       fgets(send_buf,80,stdin);
       slen = (int)strlen(send_buf);
       if (slen) {
           for (i = slen - 1; i >= 0; i--) {
               if (send_buf[i] > ' ')
                   break;
               send_buf[i] = 0;
           }
       }
       slen = (int)strlen(send_buf);
       if (slen == 0) {
           printf("Nothing to send. Aborting...\n");
           break;
       }

       /*************************************************/
       /* Send data buffer to the worker job            */
       /*************************************************/
       len = send(sockfd, send_buf, slen + 1, 0);
       if (len != (int)(slen + 1))
       {
          PERROR("send failed");
          SCLOSE(sockfd);
          exit(1);
       }
       printf("%d bytes sent\n", len);

       /*************************************************/
       /* Receive data buffer from the worker job       */
       /*************************************************/
       len = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
       if (len != (int)strlen(send_buf) + 1)
       {
          PERROR("recv");
          SCLOSE(sockfd);
          exit(1);
       }
       printf("%d bytes received\n", len);
   }

   /*************************************************/
   /* Close down the socket                         */
   /*************************************************/
   SCLOSE(sockfd);
   return 0;
}

/* unix_client.cxx */
