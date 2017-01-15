/*\
 * udp-recv.cxx
 *
 * Copyright (c) 2015 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
\*/
/*\
 * from : https://www.cs.rutgers.edu/~pxk/417/notes/sockets/udp.html

\*/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
#include <Winsock2.h>
typedef int socklen_t;
#else // !_MSC_VER
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h> // for PRIu64, ...
#endif // _MSC_VER y/n

#include <stdio.h>
#include <string.h> // for strdup(), ...
// other includes

#include "winsockerr.cxx"

static const char *module = "udp-recv";

static int no_blocking = 1;
static int add_bind = 1;

static unsigned short Port = 5556; // 27015;
static const char *Host = 0;    // use INADDR_ANY if none

void give_help( char *name )
{
    printf("%s: usage: [options] usr_input\n", module);
    printf("Options:\n");
    printf(" --help  (-h or -?) = This help and exit(0)\n");
    printf(" --Host IP     (-H) = Set host IP address for bind.\n");
    printf("   Default is INADDR_ANY (0.0.0.0)\n");
    printf(" --Port val    (-P) = Set port number to bind to. (def=%d)", Port);

    // TODO: More help
}

int parse_args( int argc, char **argv )
{
    int i,i2,c;
    char *arg, *sarg;
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
            // TODO: Other arguments
            case 'H':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    Host = strdup(sarg);
                }
                else {
                    printf("%s: Error: Expected host name/ip to follow '%s'\n", module, arg);
                    return 1;
                }
                break;
            case 'P':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    Port = atoi(sarg);
                }
                else {
                    printf("%s: Error: Expected port number to follow '%s'\n", module, arg);
                    return 1;
                }
                break;
            default:
                printf("%s: Unknown argument '%s'. Try -? for help...\n", module, arg);
                return 1;
            }
        } else {
            // bear argument
            printf("%s: Unknown bear argument '%s'. Try -? for help...\n", module, arg);
            return 1;
        }
    }
    return 0;
}

/* Default host name of 'localhost', change accordingly */
#define SERVER "127.0.0.1"

/* Server's port number */
#define SERVPORT 3333

#define MX_UDP_MSG 512

int check_key(void)
{
    int chr = test_for_input();
    if (chr) {
        return 1;
    }
    return 0;
}


int do_test()
{
    static char buffer[MX_UDP_MSG];
    int wait_recv = 1;
    double waited_ms = 0.0;
    double total_wait_secs = 0.0;
    int show_time_secs = 10;
    struct sockaddr_in svr;
    char *bufptr = buffer;
    int buflen = MX_UDP_MSG;
    struct sockaddr_in * serveraddr = &svr;
    socklen_t serveraddrlen = (socklen_t)sizeof(struct sockaddr_in);

    int status, iret = 0;
    /* The socket() function returns a socket */
    /* get a socket descriptor */
    SOCKET sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (SERROR(sd)) {
        PERROR("UDP Client - socket() error");
        iret = 1;
        goto Exit;
    }
    printf("UDP Client - begin %s - socket() is OK! value 0x%X (%u)\n",
        get_datetime_str(), (unsigned int)sd, (unsigned int)sd);

    /* set non blocking, if desired */
    if (no_blocking) {
        status = setnonblocking(sd);
        printf("UDP Client - set non-blocking - status = %d\n", status);
    }
    if (add_bind)
    {
        //-----------------------------------------------
        // Bind the socket to any address and the specified port.
        sockaddr_in RecvAddr;

        RecvAddr.sin_family = AF_INET;
        RecvAddr.sin_port = htons(Port);
        if (Host) {
            RecvAddr.sin_addr.s_addr = inet_addr(Host);
        }
        else {
            RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        }

        status = bind(sd, (SOCKADDR *)& RecvAddr, sizeof(RecvAddr));
        if (status != 0) {
            PERROR("UDP Client - bind failed!\n");
            return 1;
        }

        printf("UDP Client - IP %s, port %d, struct len %d\n", inet_ntoa(RecvAddr.sin_addr),
            ntohs(RecvAddr.sin_port), (int) sizeof(RecvAddr));

    }
    
    while (wait_recv) 
    {

        status = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)serveraddr, &serveraddrlen);
        if (SERROR(status))
        {
#ifdef _MSC_VER
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                if (check_key_sleep_ms(m_delay_time,(char *)module))
                    break;
                // clock_wait(m_delay_time);
                waited_ms += (double)m_delay_time;
                if ((int)(waited_ms / 1000.0) >= show_time_secs) {
                    total_wait_secs += (waited_ms / 1000.0);
                    waited_ms = 0.0;
                    printf("UDP Client - Waited %lf secs... any key to exit...\n", total_wait_secs);
                }
                continue;
            }
            else {
                if (err == WSAEINVAL) {
                    PERROR("UDP Client - recvfrom() error");
                    iret = 1;
                    break;
                }
            }
#else

            if (errno == EAGAIN) {
                if (check_key_sleep_ms(m_delay_time, (char *)module))
                    break;
                // clock_wait(m_delay_time);
                continue;
            }
            else {
                PERROR("UDP Client - recvfrom() error");
                iret = 1;
                break;
        }
#endif
        }
    }



Exit:
    /* close() the socket descriptor. */
    printf("UDP Client - close and exit %d... %s\n", iret, get_datetime_str());
    if (sd && !SERROR(sd))
        SCLOSE(sd);

    return iret;
}

// main() OS entry
int main( int argc, char **argv )
{
    int iret = 0;
    iret = parse_args(argc,argv);
    if (iret) {
        if (iret == 2)
            iret = 0;
        return iret;
    }

    sock_init();
    iret = do_test(); // actions of app
    sock_end();

    return iret;
}


// eof = udp-recv.cxx
