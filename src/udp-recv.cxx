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
typedef sockaddr SOCKADDR;
#endif // _MSC_VER y/n

#include <stdio.h>
#include <string.h> // for strdup(), ...
// other includes

#include "winsockerr.cxx"

static const char *module = "udp-recv";

static int no_blocking = 1;
static int add_bind = 1;
static int write_msg_header = 0;    // maybe not such a good idea!
static int echo_received_data = 1;  // echo back all received data
static int append_to_log = 0;
static unsigned short Port = 5556; // 27015;
static const char *Host = 0;    // use INADDR_ANY if none

#ifndef DEF_MESSAGE_LOG
#define DEF_MESSAGE_LOG "temp-udprecv.log"
#endif 

static char log_msg[1024];
static const char *msg_log = DEF_MESSAGE_LOG;
static FILE *msg_file = NULL;
static const char *def_txt = "The quick brown fox jumps over the lazy dog!\n";


void give_help( char *name )
{
    printf("%s: usage: [options] usr_input\n", module);
    printf("Options:\n");
    printf(" --help  (-h or -?) = This help and exit(0)\n");
    printf(" --Host IP     (-H) = Set host IP address for bind.\n");
    printf("   Default is INADDR_ANY (0.0.0.0)\n");
    printf(" --Port val    (-P) = Set port number to bind to. (def=%d)\n", Port);
    printf(" --echo yes|no (-e) = Enable/Disable echo of received. (def=%s)\n",
        (echo_received_data ? "yes" : "no"));
    printf(" --log <file>  (-l) = Set data received log. (def=%s)\n", msg_log);
    printf(" --append      (-a) = Set to append to log. (def=%s)\n", (append_to_log ? "yes" : "no"));

    // TODO: More help
}

static int setboolopt(const char *sarg, int *pdo_echo)
{
    int iret = 1;   // assume error
    size_t len = strlen(sarg);
    if (strcmp(sarg, "yes") == 0) {
        *pdo_echo = 1;
        iret = 0;   // all ok
    }
    else if (strcmp(sarg, "no") == 0) {
        *pdo_echo = 0;
        iret = 0;   // all ok
    }
    else if (strcmp(sarg, "on") == 0) {
        *pdo_echo = 1;
        iret = 0;   // all ok
    }
    else if (strcmp(sarg, "off") == 0) {
        *pdo_echo = 0;
        iret = 0;   // all ok
    }
    else if (strcmp(sarg, "1") == 0) {
        *pdo_echo = 1;
        iret = 0;   // all ok
    }
    else if (strcmp(sarg, "0") == 0) {
        *pdo_echo = 0;
        iret = 0;   // all ok
    }
    return iret;
}


static int parse_args( int argc, char **argv )
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
            case 'e':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    if (setboolopt(sarg, &echo_received_data)) {
                        fprintf(stderr, "ERROR: Arg %s must be followed by yes/no, on/off, 1/0! Not %s\n", arg, sarg);
                        return 1;
                    }
                    //if (VERB1)
                        printf("%s: Set 'echo' %s\n", module, (echo_received_data ? "on" : "off"));
                }
                else {
                    fprintf(stderr, "ERROR: Arg %s must be followed by yes/no, on/off, 1/0!!\n", arg);
                    return 1;
                }
                break;
            case 'l':
                if (i2 < argc) {
                    i++;
                    msg_log = strdup(argv[i]);
                    //if (VERB1)
                        printf("%s: Set message log to '%s'\n", module, msg_log);
                }
                else {
                    fprintf(stderr, "ERROR: Arg %s must be followed by a file name!\n", arg);
                    return 1;
                }
                break;
            case 'a':
                append_to_log = 1;
                printf("%s: Set to append to message log...\n", module);
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

static void write_msg_log(const char *msg, int len)
{
    int wtn;
    if (msg_file == NULL) {
        const char *mode = "wb";
        if (append_to_log)
            mode = "ab";
        msg_file = fopen(msg_log, mode);
        if (msg_file) {
            char *cp = log_msg;
            sprintf(cp, "%s: %s %s log file %s\n", module, (append_to_log ? "append to" : "open"), msg_log, get_datetime_str());
            if (write_msg_header) {
                wtn = (int)fwrite(cp, 1, strlen(cp), msg_file);
                if (wtn != (int)strlen(cp)) {
                    fclose(msg_file);
                    msg_file = (FILE *)-1;
                    printf("%s:ERROR: Failed to WRITE %d != %d to %s log file!\n", module, wtn, len, msg_log);
                }
            }
            if (msg_file && (msg_file != (FILE *)-1)) {
                printf("%s", cp);
            }
        } else {
            printf("%s:ERROR: Failed to OPEN/append %s log file!\n", module, msg_log);
            msg_file = (FILE *)-1;
        }
    }
    if (msg_file && (msg_file != (FILE *)-1)) {
        wtn = (int)fwrite(msg, 1, len, msg_file);
        if (wtn != len) {
            fclose(msg_file);
            msg_file = (FILE *)-1;
            printf("%s:ERROR: Failed to WRITE %d != %d to %s log file!\n", module, wtn, len, msg_log);
        }
    }
}

#define MX_H_BUF 256

// htons(client->sin_port), // converts a u_short from host to
// TCP/IP network byte order (which is big-endian).
static void show_client(char * src, int recvd, struct sockaddr_in * client)
{
    //static char _s_buf[MX_H_BUF];
    // ntohs(u_short) - converts a u_short from TCP/IP network byte order
    // to host byte order (which is little-endian on Intel processors).
    u_short port = ntohs(client->sin_port);
    char * IP = inet_ntoa(client->sin_addr);
    printf("%s: Recevied %d bytes, from IP %s:%d\n", module, recvd, IP, port);
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
    int sflags = 0;
    int status, len, iret = 0;
    /* The socket() function returns a socket */
    /* get a socket descriptor */
    SOCKET sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (SERROR(sd)) {
        PERROR("udp-recv: socket() error");
        iret = 1;
        goto Exit;
    }
    printf("%s: begin %s - socket() is OK! value 0x%X (%u)\n", module,
        get_datetime_str(), (unsigned int)sd, (unsigned int)sd);

    /* set non blocking, if desired */
    if (no_blocking) {
        status = setnonblocking(sd);
        printf("%s: set non-blocking - status = %d\n", module, status);
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
            PERROR("udp-recv: bind failed!\n");
            return 1;
        }

        printf("%s: bound IP %s, port %d, struct len %d\n", module, inet_ntoa(RecvAddr.sin_addr),
            ntohs(RecvAddr.sin_port), (int) sizeof(RecvAddr));

    }

    printf("%s: Any key to exit loop...\n", module);
    
    while (wait_recv) 
    {
        // non-blocking 'recvfrom' ...
        status = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)serveraddr, &serveraddrlen);
        if (SERROR(status))
        {
#ifdef _MSC_VER
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                if (check_key_sleep_ms(m_delay_time, (char *)module)) {
                    printf("%s: Got a keyboard input... going to exit...\n", module);
                    break;
                }
                // clock_wait(m_delay_time);
                waited_ms += (double)m_delay_time;
                if ((int)(waited_ms / 1000.0) >= show_time_secs) {
                    total_wait_secs += (waited_ms / 1000.0);
                    waited_ms = 0.0;
                    printf("%s: Waited %lf secs... any key to exit...\n", module, total_wait_secs);
                }
                continue;
            }
            else {
                if (err == WSAEINVAL) {
                    PERROR("udp-recv: recvfrom() error");
                    iret = 1;
                    break;
                }
            }
#else

            if (errno == EAGAIN) {
                if (check_key_sleep_ms(m_delay_time, (char *)module)) {
                    printf("%s: Got a keyboard input... going to exit...\n", module);
                    break;
                }
                // clock_wait(m_delay_time);
                continue;
            }
            else {
                PERROR("udp-recv: recvfrom() error");
                iret = 1;
                break;
            }
#endif
        }
        else if (status > 0) {
            len = status;
            write_msg_log(bufptr, len);  // write data, for length, to a log file
            show_client(bufptr, status, serveraddr);
            if (echo_received_data) {
                status = sendto(sd, bufptr, len, sflags, (const struct sockaddr *)serveraddr, serveraddrlen);
                if (SERROR(status)) {
                    PERROR("udp-recv: sendto failed!\n");
                    wait_recv = 0;  // exit this loop
                }
                else if (status != len) {
                    printf("%s: Send %d !- %d - warning...\n", module, len, status);
                }
                else {
                    printf("%s: Echoed %d back to client...\n", module, len);
                }
            }
        }
        else if (status == 0) {
            // doc says this is a graceful close, but this is udp, non-blocking,
            // so I guess this **NEVER** happens!!!
            printf("%s: status == 0! This **NEVER** happens!\n", module);
        }
    }

Exit:
    /* close() the socket descriptor. */
    printf("%s: close and exit %d... %s\n", module, iret, get_datetime_str());
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
