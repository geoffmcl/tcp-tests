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
/* -------------------------------------------------
   sockaddr

   The sockaddr structure varies depending on the protocol used.
   Except for the sin*_family parameter, sockaddr contents are expressed 
   in network byte order. A 16 byte block

   struct sockaddr {
      u_short sa_family; // address family
      char sa_data[14];  // up to 14 bytes of direct address
   };

   Winsock functions using sockaddr are not strictly interpreted to be 
   pointers to a sockaddr structure. The structure is interpreted 
   differently in the context of different address families. The only 
   requirements are that the first u_short is the address family and 
   the total size of the memory buffer in bytes is namelen. 

   The structures below are used with IPv4 and IPv6, respectively. 
   Other protocols use similar structures.

   struct sockaddr_in {
        short   sin_family;
        u_short sin_port;
        struct  in_addr sin_addr;
        char    sin_zero[8];
   };
   struct sockaddr_in6 {
        short   sin6_family;
        u_short sin6_port;
        u_long  sin6_flowinfo;
        struct  in6_addr sin6_addr;
        u_long  sin6_scope_id;
   };
   struct sockaddr_in6_old {
        short   sin6_family;        
        u_short sin6_port;          
        u_long  sin6_flowinfo;      
        struct  in6_addr sin6_addr;  
   };

   hostent
   * Structures returned by network data base library, taken from the
   * BSD file netdb.h.  All addresses are supplied in host order, and
   * returned in network order (suitable for use in system calls).

   struct  hostent {
        char    FAR * h_name;           // official name of host
        char    FAR * FAR * h_aliases;  // alias list
        short   h_addrtype;             // host address type
        short   h_length;               // length of address 
        char    FAR * FAR * h_addr_list; // list of addresses
     #define h_addr  h_addr_list[0]          // address, for backward compat
   };

   ------------------------------------------------- */

/* ===============================================================
   Some references reviewed
   from : http://www.yolinux.com/TUTORIALS/Sockets.html
   For BOTH -
   TCP / SOCK_STREAM / recv - AND 
   UDP / SOCK_DGRAM / recvfrom
   if (rc == 0) cerr << "ERROR! Socket closed" << endl;

   Also implies the 'difference' between 'recv' and 'read' is
   " read(): read a specific number of bytes from a file descriptor

   Comparison of sequence of BSD API calls: 

   Socket Server Socket Client 
   socket()      socket() 
   bind()        gethostbyname() 
   listen() 
   accept()      connect() 
   recv()/send() recv()/send() 
   close()       close() 
   ****************************************************************

   ================================================================ */
/* common includes */
#include <iostream>
#include <vector>
//#include <strstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <memory.h>

#ifdef _MSC_VER
///////////////////////////////////////////////////////////
#include <conio.h>
#include <WS2tcpip.h>
///////////////////////////////////////////////////////////
#else // !_MSC_VER
///////////////////////////////////////////////////////////
#include <unistd.h>
#include <sys/types.h> /* Types used in sys/socket.h and netinet/in.h */
#include <sys/socket.h> /* accept(), bind(), connect(), listen(), recv(), send(), setsockopt(), shutdown(), etc ... */
#include <netinet/in.h> /* Internet domain address structures and functions */
#include <arpa/inet.h> /* htonl(), htons(), ntohl(), ntohs(), inet_addr(), inet_ntoa(), etc ...*/
#include <string.h>
#include <errno.h>
#include <sys/errno.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <termios.h>
///////////////////////////////////////////////////////////
#endif  // _MSC_VER y/n

#include "simp_common.hxx" // common to SERVER and CLIENT
#include "winsockerr.cxx"

using namespace std;

#define KEEP_CYCLE_COUNT // get some idea of 'cycle per second'
#undef ADD_UDP_SUPPORT

#define DEF_BACKLOG 5   /* this should be enough PENDING connections */

typedef vector<PINCLIENT> vIN;
typedef vIN::iterator vINi;

static vIN vInClients; /* vector LIST of CLIENTS */

static char rd_buf[MAXSTR+1];
static char rd_ack[MAXSTR+1];
static char cleaned[MAXSTR+1];
static char hostName[264];

static int total_clients = 0;
static int max_rw_errors = 3; /* erase a client if errors exceed this... */

static int socket_type = DEF_SOCK_TYPE;

static int verbosity = 0;
#define VERB1 (verbosity >= 1)
#define VERB2 (verbosity >= 2)
#define VERB5 (verbosity >= 5)
#define VERB9 (verbosity >= 9)

static int add_nonblocking = 1;
static int use_use_ip = 0;
static double curr_timeout = 0.01; // seconds to timeout
static struct timeval timeout;

static fd_set ListenSocket;
static SOCKET highest = 0;
static INCLIENT hostServ;

// forward reference
void add_client(SOCKET newsockfd, struct sockaddr* addr, int addrlen);
void process_clients(void);
void clear_client_list(void);

int is_paused = 0;
time_t pause_time;
int do_echo = 1;
time_t msg_wait_time = 30; // show paused, every 30 seconds

time_t cycle_time;
uint64_t cycles = 0;
uint64_t cyc_per_sec = 0;

// stats kept
static uint64_t sends_done = 0;
static uint64_t sent_bytes = 0;
static uint64_t recvs_done = 0;
static uint64_t recv_bytes = 0;
// totals
static uint64_t t_sends_done = 0;
static uint64_t t_sent_bytes = 0;
static uint64_t t_recvs_done = 0;
static uint64_t t_recv_bytes = 0;
static time_t last_time = 0;
static time_t begin_time = 0;

void set_time_out(void)
{
    int usecs = (int)(curr_timeout * 1000000.0);
    timeout.tv_sec = usecs / 1000000;
    timeout.tv_usec = usecs % 1000000;
}
char * get_to_stg(void)
{
    static char to[32];
    set_time_out();
    sprintf(to, "%u.%06u secs", (unsigned int)timeout.tv_sec, (unsigned int)timeout.tv_usec);
    return to;
}

#ifndef DEF_MESSAGE_LOG
#define DEF_MESSAGE_LOG "fgio_server.log"
#endif

static char *msg_log = (char *)DEF_MESSAGE_LOG;
static FILE *msg_file = NULL;

void write_msg_log(const char *msg, int len)
{
    if (msg_file == NULL) {
        msg_file = fopen(msg_log,"ab");
        if (!msg_file) {
            printf("ERROR: Failed to OPEN/append %s log file!\n", msg_log);
            msg_file = (FILE *)-1;
        }
    }
    if (msg_file && (msg_file != (FILE *)-1)) {
        int wtn = (int)fwrite(msg,1,len,msg_file);
        if (wtn != len) {
            fclose(msg_file);
            msg_file = (FILE *)-1;
            printf("ERROR: Failed to WRITE %d != %d to %s log file!\n", wtn, len, msg_log);
        }
    }
}

void show_stats(void)
{
    time_t curr_time = time(NULL);
    // get elapsed
    time_t elapsed1 = curr_time - last_time;
    time_t elapsed2 = curr_time - begin_time;
    // update totals
    t_sends_done += sends_done;
    t_sent_bytes += sent_bytes;
    t_recvs_done += recvs_done;
    t_recv_bytes += recv_bytes;
    // show it
    printf("Stats: %4" PRIu64 " sends, %6" PRIu64 " bytes, %4" PRIu64 " recvs, %6" PRIu64 " bytes",
        sends_done, sent_bytes, recvs_done, recv_bytes );
    printf(" in %d secs,\n", (int)elapsed1);
    printf("Total: %4" PRIu64 " sends, %6" PRIu64 " bytes, %4" PRIu64 " recvs, %6" PRIu64 " bytes",
        t_sends_done, t_sent_bytes, t_recvs_done, t_recv_bytes );
    printf(" in %d secs.\n", (int)elapsed2 );
    // clear running totals
    sends_done = 0;
    sent_bytes = 0;
    recvs_done = 0;
    recv_bytes = 0;
    // update time
    last_time = curr_time;
}

#define OUT_SS(a) printf("%s", a.str().c_str()); a.str("")
void show_help(void)
{
    ostringstream ss;
    char * cp = get_to_stg();
    ss << "HELP on keys" << endl;
    ss << " ?     = This help..." << endl;
    ss << " e     = Toggle ECHO. Currently " <<
        (do_echo ? "On" : "OFF") << endl;
    ss << " p     = Toggle PAUSE. Currently " <<
        (is_paused ? "On" : "OFF") << endl;
    ss << " s     = Show STATS." << endl;
    ss << " 01259 = Set verbosity level. Now=" << verbosity << endl;
    ss << "ESC    = EXIT application..." << endl;
    ss << "STATS - info:" << endl;
    ss << "Current timeout = " << cp << endl;
#ifdef KEEP_CYCLE_COUNT // get some idea of 'cycle per second'
    if (cyc_per_sec > 0)
        ss << "Running at " << cyc_per_sec << " cycles per second." << endl;
#endif
    OUT_SS(ss);
    if (sends_done || recvs_done) {
        show_stats();
    }
    if (total_clients) {
        printf("Connected to %d clients, presently %d active\n",
            (int)total_clients, (int)vInClients.size() );
    }
}

int check_key(void)
{
    int chr = test_for_input();
    if (chr) {
        switch (chr)
        {
        case '?':
            show_help();
            break;
        case 'e':
            do_echo = (do_echo ? 0 : 1);
            printf("e - Toggled ECHO. Currently %s.\n",
                (do_echo ? "On" : "OFF"));
            break;
        case 'p':
            pause_time = time(0);
            is_paused = (is_paused ? 0 : 1);
            printf("p - Toggled PAUSE. Currently %s.\n",
                (is_paused ? "On" : "OFF") );
            break;
        case 's':
            show_stats();
            break;
        case 0x1b:
            printf("ESC - Commence exit...\n");
            return 1;
            break;
        case '0':
        case '1':
        case '2':
        case '5':
        case '9':
            verbosity = (chr - '0');
            printf("%c - Set verbosity to %d\n", chr, verbosity);
            break;
        default:
            printf("Unused key input... %d (%x)\n", chr, chr);
            break;
        }
    }
    return 0;
}

int show_local_host(void)
{
    int iret = 1; // assume success
    hostent* pHost;
    char * pIP;
    char * perr = (char *)"ERROR: gethostbyname() FAILED!";
    // Get the local host information
    pHost = gethostbyname("localhost");
    if (pHost) {
        pIP = inet_ntoa (*(struct in_addr *)*pHost->h_addr_list);
        if (pIP)
            printf("LOCAL: Host %s, IP %s\n", pHost->h_name, pIP );
        else
            fprintf(stderr, "ERROR: inte_ntoa() FAILED!\n");
    } else {
#ifdef _MSC_VER
        win_set_wsa_msg();
        if (last_wsa_error != 0)
            fprintf(stderr, "%s [%s] %d\n", perr, last_wsa_emsg, last_wsa_error );
        else
            fprintf(stderr, "%s\n", perr);
#else // !_MSC_VER
        fprintf(stderr, "%s\n", perr);
#endif // MSC_VER y/n
        iret = 0;
    }
    return iret; // 1 = succes
}


void show_comp_date(char * name)
{
    printf("Running: %s, version " PACKAGE_VERSION ", compiled %s, at %s\n",
        name, __DATE__, __TIME__ );
}

void cmd_help(char * name)
{
    char * cp = get_to_stg();
    printf("HELP\n");
    printf(" -?    = This help, and exit 0\n");;
    printf(" -h <host> = Set the HOST name, or IP. Def=<none>\n");
    printf(" -p <port> = Set the PORT number. Def=%d\n", PORTNUM );
#ifdef ADD_UDP_SUPPORT
    printf(" -u        = Use udp socket. Def=tcp\n");
#endif // ADD_UDP_SUPPORT
    printf(" -v[nn]    = Increment/Set verbosity. Def=%d\n", verbosity );
    printf("Current timeout = %s\n", cp);
}

void setup_fd_set(SOCKET sock)
{
    PINCLIENT pic;
    FD_ZERO(&ListenSocket);
    FD_SET(sock,&ListenSocket); /* set our main listening socket */
    vINi vi;
    for (vi = vInClients.begin(); vi != vInClients.end(); vi++) {
        pic = *vi;
        if (pic->sock && !SERROR(pic->sock))
            FD_SET(pic->sock,&ListenSocket); /* set any 'active' clients */
    }
}

int run_server(SOCKET sockfd)
{
    int iret = 0;
    int status;
    SOCKET newsockfd;
    socklen_t clilen;
    struct sockaddr_in cli_addr;

	// repeat forever... or until ESC key exit
    printf("SERVER: Entering forever loop... ESC to exit...\n" );
    cycle_time = time(0);
    while (1)
    {
        cycles++; /* bump the cycles */
        if (time(0) != cycle_time) {
            cycle_time = time(0);
            cyc_per_sec = cycles;
            cycles = 0;
        }
        if (check_key())
            break;

        if (is_paused) {
            if ((time(0) - pause_time) > msg_wait_time) {
                printf("SERVER: presently PAUSED!\n");
                pause_time = time(0);
            }
            sleep(DEF_SLEEP);
            continue;
        }
        // if a NEW client added, then also must
        // be ADDED to a FD_SET, and a NEW 'select' done
        // ===========================================
        setup_fd_set(sockfd);
        set_time_out();
        status = select( (int)highest+1, &ListenSocket, NULL, NULL, &timeout );
        if (SERROR(status)) {
            PERROR("ERROR: 'select' FAILED!");
            iret = 1;
            break;
        }
        if (status > 0) {
            if (FD_ISSET(sockfd,&ListenSocket)) {
		        clilen = sizeof(cli_addr);
                bzero((char *)&cli_addr,clilen);
		        // Accept a client's request, and get the client's address info into the local variable cli_addr.
		        newsockfd = accept(sockfd,(struct sockaddr *)&cli_addr,&clilen);
                if (SERROR(newsockfd)) {
                    PERROR("WARNING: accept() FAILED!");
                } else {
    		        // After accepting the connection, all transaction with this client would happen with the new socket descriptor - newsockfd.
                    printf("SERVER: accept connection to : %d (0x%X)\n",
                        newsockfd, newsockfd );
			        add_client(newsockfd,(struct sockaddr*)&cli_addr, clilen);
                }
            } else {
                process_clients(); // process existing clients
            }
        }
	}   /* FOREVER */

    return iret;
}

void clean_data(char * dest, char * src, int len)
{
    int i, off, c;
    off = 0;
    for (i = 0; i < len; i++) {
        c = src[i];
        if (c & 0x80) { // got high bit on
            dest[off++] = '@';
            c &= ~0x80;
        }
        if (c < ' ') {
            dest[off++] = '^';
            c += '@';
        }
        dest[off++] = c;
    }
    dest[off] = 0;
}


int handle_receive(SOCKET newsockfd, PINCLIENT pic)
{
	// Read from the socket...
    rd_buf[0] = 0;
    if (VERB9)
        printf("SERVER: Read from socket : %d\n", newsockfd);
#ifdef _MSC_VER
	int res = recv(newsockfd,rd_buf,MAXSTR,0);
    if (SERROR(res)) {
        win_set_wsa_msg();
        if (add_nonblocking && (last_wsa_error == WSAEWOULDBLOCK)) {
            if (VERB9)
                printf("SERVER: Nothing to receive - would block...\n");
        } else {
            PERROR("ERROR: recv() FAILED!");
            return 1;
        }
        return 0;
    }
#else
	//int res = read(newsockfd,(void *)&rd_buf,MAXSTR);
	int res = recv(newsockfd,(void *)&rd_buf,MAXSTR,0);
    if ( res < 0 ) {
        if (add_nonblocking && (errno == EAGAIN)) {
            if (VERB9)
                printf("SERVER: Nothing to receive - would block...\n");
        } else {
            PERROR("ERROR: recv() FAILED!");
            return 1;
        }
        return 0;
    }
#endif
    if (res == 0) {
        printf("SERVER: Received NULL = assumed closed\n");
        return 1;
    }

    if (res > 0) {
        recvs_done++;
        recv_bytes += res;
        write_msg_log(rd_buf, res);
        pic->last_read = time(0);
        if (VERB9) {
            clean_data(cleaned,rd_buf,res);
            printf("SERVER: from client %d: [%s] %d\n", newsockfd, cleaned, res);
        }
        if (do_echo) {
            rd_buf[res] = 0;
            strcpy(rd_ack, get_datetime_str());
            strcat(rd_ack," echo : Reply from server...\n");
            //strcat(rd_ack,rd_buf);
    	    // Write back the response to the socket
            int elen = (int)strlen(rd_ack);
            if (VERB9) {
                clean_data(cleaned,rd_ack,elen);
                printf("SERVER: to   client %d: [%s] %d\n", newsockfd, cleaned, elen);
            }
            res = send(newsockfd,rd_ack,elen,0);
            if (SERROR(res)) {
                PERROR("ERROR: Send FAILED!");
                return 1;
            }
            sends_done++;
            sent_bytes += res;
            if (VERB9)
                printf("SERVER: Write/Send returned %d\n", res);
        }
    }
    return 0;
}

void process_clients(void)
{
    SOCKET newsockfd;
    PINCLIENT pic;
    int had_errors = 0;
    vINi it = vInClients.begin();
    for ( ; it != vInClients.end(); it++ ) {
        pic = *it;
        if ((pic->sock == 0) || SERROR(pic->sock))
            continue;

        newsockfd = pic->sock;
        if (FD_ISSET(newsockfd,&ListenSocket)) {
            if ( handle_receive(newsockfd,pic) ) {
                pic->had_rw_error++;
                if ( pic->had_rw_error > max_rw_errors )
                    had_errors++;
            }
        }
    }
    int offset = 0;
    while (had_errors--) {
        offset = 0;
        int cnt = 0;
        for ( it = vInClients.begin() ; it != vInClients.end(); it++ ) {
            cnt++;
            pic = *it;
            if ((pic->sock == 0) || SERROR(pic->sock))
                continue;
            if ( pic->had_rw_error > max_rw_errors ) {
                char * IP = pic->IP;
                printf("client: %d ", cnt );
                printf("close %d, IP %s, due to %d rw errors\n",
                    pic->sock, IP, pic->had_rw_error );
                SCLOSE(pic->sock);
                pic->sock = 0;
                delete pic;
                break;
            }
            offset++;
        }
        if (it != vInClients.end()) {
            vInClients.erase( vInClients.begin() + offset );
        }
    }
}

void clear_client_list(void)
{
    PINCLIENT pic;
    char * IP;
    vINi it;
    int cnt = 0;
    for ( it = vInClients.begin(); it != vInClients.end(); it++ ) {
        cnt++;
        pic = *it;
        if (pic->sock && !SERROR(pic->sock)) {
            IP = pic->IP;
            printf( "C:%d: close %d, IP %s\n",
                cnt, pic->sock, IP );
            SCLOSE(pic->sock);
        }
        delete pic; /* delete allocation */
    }
    vInClients.clear(); // finally, CLEAR THE VECTOR
}

void add_client(SOCKET newsockfd, struct sockaddr* addr, int addrlen)
{
    PINCLIENT pic;
    struct sockaddr_in * pain = (struct sockaddr_in *)addr;
    char * IP;
    vINi it;
    for ( it = vInClients.begin(); it != vInClients.end(); it++ ) {
        if ( (*it)->sock == newsockfd ) {
            printf("SERVER: New socket : %d already in vector\n", newsockfd);
            return;
        }
    }
    if (newsockfd > highest) highest = newsockfd;

    pic = new INCLIENT;
    bzero((char *)pic, sizeof(INCLIENT));
    pic->sock = newsockfd;
    pic->addr = *addr;
    pic->addrlen = addrlen;
    pic->created = time(0);
    struct in_addr * pin = (struct in_addr *)&pain->sin_addr;
    IP = inet_ntoa ( *pin );
    if (IP) {
        strcpy( pic->IP, IP );
        unsigned int ipval = inet_addr(pic->IP);
        struct hostent * rH = gethostbyaddr((char *)&ipval, sizeof(ipval), AF_INET);
        if (rH)
            strcpy( pic->h_name, rH->h_name );
        else
            strcpy( pic->h_name, "not found");

    } else {
        strcpy( pic->IP, "inet_ntoa failed");
    }
    
    printf("SERVER: New socket: %d, from IP %s, name %s\n",
        newsockfd, pic->IP, pic->h_name );

    if (add_nonblocking) {
        if (VERB9)
            printf("SERVER: Adding non-blocking mode for %d\n", newsockfd );
        int status = setnonblocking(newsockfd);
        if (VERB9)
            printf("SERVER: io ctrl socket(%d,...) returned %d\n", newsockfd, status);
    }
    total_clients++;
    vInClients.push_back(pic); // store new CLIENT
}

SOCKET create_a_listen_socket(int port)
{
    // Declare variables
    int status = -1;
    SOCKET ListenSocket;
    sockaddr_in saServer;
    hostent* pHost;
    char* pIP = NULL;

    // Create a listening socket
    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (SERROR(ListenSocket))
        return status;
    // Get the local host information
    pHost = gethostbyname("localhost");
    if (!pHost)
        return status;

    pIP = inet_ntoa (*(struct in_addr *)*pHost->h_addr_list);
    if (!pIP)
        return status;

    // Set up the sockaddr structure
    saServer.sin_family = AF_INET;
    saServer.sin_addr.s_addr = inet_addr(pIP);
    saServer.sin_port = htons(port);

    // Bind the listening socket using the
    // information in the sockaddr structure
    status = bind( ListenSocket, (sockaddr *)&saServer, sizeof(saServer) );
    if (status == -1)
        return status;

    return ListenSocket;
}

char * get_local_host_name(void)
{
    static char _s_local_host_name[264];
    char * cp = _s_local_host_name;
    struct hostent* host = gethostbyname("localhost");
    if (host && host->h_name && host->h_name[0])
        strcpy(cp,host->h_name);
    else
        strcpy(cp,"<NOT FOUND>");
    return cp;
}

int get_host_information(char * in_host_name)
{
    // Declare variables
    char* IP = NULL;
    hostent* Host = NULL;
    unsigned int addr;
    char * host_name = in_host_name;
    if (( host_name == NULL ) || ( host_name[0] == 0 )) {
        host_name = (char *)"localhost";
    }
    // If an alpha name for the host, use gethostbyname()
    if (isalpha(host_name[0])) {   /* host address is a name */
        // if hostname terminated with newline '\n', remove and zero-terminate 
        if (host_name[strlen(host_name)-1] == '\n')
            host_name[strlen(host_name)-1] = '\0'; 
        Host = gethostbyname(host_name);
    } else  {
        // If not, get host by addr (assume IPv4)
        addr = inet_addr(host_name);
        Host = gethostbyaddr((char *)&addr, 4, AF_INET);
    }
#ifdef _MSC_VER
    if ((WSAGetLastError() != 0)||(Host == NULL)) {
        PERROR("ERROR: gethost... FAILED!");
        return -1;
    }
#else // !_MSC_VER
    if ( Host == NULL ) {
        PERROR("ERROR: gethost... FAILED!");
        return -1;
    }
#endif // _MSC_VER y/n
    // The Host structure can now be used to
    // access information about the host
    // Get the local host information
    IP = inet_ntoa (*(struct in_addr *)*Host->h_addr_list);
    if (IP == NULL)
        return -1; /* FAILED */
    // ELSE, have resolved Host name
    localHost = Host;
    localIP = strdup(IP);
    use_use_ip = 1; /* can use the user supplied IP */
    return 0; // success
}

/* OS ENTRY */
int main( int argc, char *argv[])
{
    int iret = 0;
    int portNumber, i, c, status;
    char * arg;
    PINCLIENT pic = NULL;
    struct in_addr * pin = NULL;
    char * IP = NULL;
    SOCKET sockfd,newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr,cli_addr;
    struct sockaddr Hostname;
    socklen_t Hostname_len = sizeof(Hostname);
    
    //struct sigaction action;
    show_comp_date(argv[0]);
    sock_init();
    hostName[0] = 0;
    portNumber = PORTNUM; // set default
	// Validate the command line
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        if (*arg == '-') {
            arg++;
            while(*arg == '-') arg++;
            c = *arg;
            switch (c) {
            case '?':
                cmd_help(argv[0]);
                return 0;
                break;
            case 'h':
                if ((i + 1) < argc) {
                    i++;
                    strcpy(hostName,argv[i]);
                } else {
                    goto Bad_Arg;
                }
                break;
            case 'p':
                if ((i + 1) < argc) {
                    i++;
                    portNumber = atoi(argv[i]);
                } else {
                    goto Bad_Arg;
                }
                break;
            case 'v':
                arg++;
                if (*arg) {
                    // expect digits
                    if (is_digits(arg)) {
                        verbosity = atoi(arg);
                    } else if (*arg == 'v') {
                        verbosity++; /* one inc for first */
                        while(*arg == 'v') {
                            verbosity++;
                            arg++;
                        }
                    } else
                        goto Bad_Arg;
                } else
                    verbosity++;
                printf("SERVER: Set verbosity to %d\n", verbosity);
                break;
#ifdef ADD_UDP_SUPPORT
            case 'u':
                socket_type = SOCK_DGRAM;
                break;
#endif // ADD_UDP_SUPPORT
            default:
Bad_Arg:
                fprintf(stderr,"ERROR: Invalid command line! arg [%s] unknown\n", argv[i] );
                return 1;
                break;
            }
        } else {
            fprintf(stderr,"ERROR: Invalid command line! bare arg [%s] unknown\n", argv[i] );
            return 1;
        }
	}

    printf("SERVER: ");
    show_local_host();

    printf("SERVER: host = %s, port %d, soc.type %s\n",
        (hostName[0] ? hostName : "localhost"),
        portNumber,
        ((socket_type == SOCK_STREAM) ? "tcp" : "udp"));

	// At this point, the command line has been validated.
    if (hostName[0]) { /* if USER supplied name/IP address */
        status = get_host_information(hostName); /* may use supplied HOST */
        if (status == -1) {
            printf("ERROR: Failed on host name %s\n",hostName);
            iret = 1;
            goto End_App;
        }
    }

	// Create a new TCP socket...
	sockfd = socket(AF_INET,socket_type,0);
	if (SERROR(sockfd)) {
        PERROR("ERROR: socket() FAILED!");
        iret = 1;
        goto End_App;
	}
    highest = sockfd;
    Hostname_len = sizeof(Hostname);
    printf("SERVER: Got socket %d (0x%X) %s",
        sockfd, sockfd,
        (socket_type == SOCK_STREAM ? "tcp" : "udp"));

    bzero((char *)&serv_addr,sizeof(serv_addr)) ;
	serv_addr.sin_family = AF_INET; // = 2 - internetwork: UDP, TCP, etc.
    if (use_use_ip && localHost && localIP) {
        memcpy((char *)&serv_addr.sin_addr,(char *)localHost->h_addr,localHost->h_length);
        printf(" setting IP %s\n", localIP);
    } else {
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        printf(" setting IP INADDR_ANY\n");
    }
	serv_addr.sin_port = htons(portNumber);

	// Bind the socket to the server's ( this process ) address.
    status = bind(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	if (SERROR(status)) {
        PERROR("ERROR: Can't bind address!");
		iret = 1;
        goto End_App;
	}

    printf("SERVER: bound to port %d\n", portNumber );
    status = getsockname( sockfd, &Hostname, &Hostname_len );
    if (SERROR(status)) {
        PERROR("ERROR: getsockname() FAILED!");
        iret = 1;
        goto End_App;
    }
    printf("SERVER: getsockname ok\n");

    if (add_nonblocking) {
        if (VERB9)
            printf("SERVER: Adding non-blocking mode...\n");
        status = setnonblocking(sockfd);
        if (VERB9)
            printf("SERVER: io ctrl socket(%d,...) returned %d\n", sockfd, status );
    }

    // get some more details of the server
    pic = &hostServ;
    pin = (struct in_addr *)&serv_addr.sin_addr;
    if (VERB9)
        printf("SERVER: get IP from address...\n");
    IP = inet_ntoa ( *pin );
    if (IP) {
        strcpy( pic->IP, IP );
        if (VERB9)
            printf("SERVER: IP value - inet_addr... IP=%s\n", pic->IP);
        unsigned int ipval = inet_addr(pic->IP);
        if (ipval == 0) {
            strcpy(pic->h_name,get_local_host_name());
        } else {
            if (VERB9)
                printf("SERVER: From ipval, gethostbyaddr()...\n");
            struct hostent * rH = gethostbyaddr((char *)&ipval, sizeof(ipval), AF_INET);
            if (rH)
                strcpy( pic->h_name, rH->h_name );
            else
                strcpy( pic->h_name, "not found");
        }
    } else {
        strcpy( pic->IP, "inet_ntoa failed");
    }

    printf("SERVER: Got connection... IP %s, name %s\n", pic->IP, pic->h_name );

  	// Listen for connections in this socket
    printf("SERVER: listen for TCP connection... Backlog=%d\n", DEF_BACKLOG);
    status = listen(sockfd,DEF_BACKLOG);
    if (SERROR(status)) {
        PERROR("ERROR: listen() FAILED!");
        iret = 1;
        goto End_App;
    }
    printf("SERVER: listen status : %d\n", status );

    printf("SERVER: Connected to port : %d\n", portNumber );

    show_help();
    begin_time = time(NULL);
    last_time = begin_time;

	// repeat forever... or until ESC key exit
    iret = run_server(sockfd);

    // out of the LOOP
End_App:

    show_stats();

    clear_client_list();

    printf("SERVER: ");
    if (sockfd && !SERROR(sockfd)) {
        printf("Close socket... ");
        SCLOSE(sockfd);
    }
#ifdef _MSC_VER
    printf("WSACleanup()... ");
#endif
    sock_end();
    printf("End app %s, exit %d\n", get_datetime_str(), iret );
    return iret;
}

#if (defined(_WIN32) && defined(_MSC_VER) && !defined(_WS2TCPIP_H_))

const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
    if (af == AF_INET) {
        struct sockaddr_in in;
        memset(&in, 0, sizeof(in));
        in.sin_family = AF_INET;
        memcpy(&in.sin_addr, src, sizeof(struct in_addr));
        getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
        return dst;
    } else if (af == AF_INET6) {
        struct sockaddr_in6 in;
        memset(&in, 0, sizeof(in));
        in.sin6_family = AF_INET6;
        memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
        getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
        return dst;
    }
    return NULL;
}

int inet_pton(int af, const char *src, void *dst)
{
    struct addrinfo hints, *res, *ressave;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = af;
    if (getaddrinfo(src, NULL, &hints, &res) != 0) {
        // dolog(LOG_ERR, "Couldn't resolve host %s\n", src);
        return -1;
    }
    ressave = res;
    while (res) {
        memcpy(dst, res->ai_addr, res->ai_addrlen);
        res = res->ai_next;
    }

    freeaddrinfo(ressave);
    return 0;
}

#endif // #if (defined(_WIN32) && defined(_MSC_VER) && !defined(_WS2TCPIP_H_))


// eof simp_server.cxx

