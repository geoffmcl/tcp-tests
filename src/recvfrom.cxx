
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h> // for exit()

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define SPRTF   printf

#ifndef VERSION
#define VERSION "1.0.0"
#endif

static const char *mod_name = "recvfrom";
static int server_port = 3333;
static char *server_addr = 0;

// get a message from the system for this error value
char *get_errmsg_txt( int err )
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

void give_help( char *pname )
{
    char *bn = (char *)mod_name; // pname; // get_base_name(pname);
    SPRTF("%s - version %s, compiled %s, at %s\n", bn, VERSION, __DATE__, __TIME__);
    SPRTF("UDP peer connection\n");
    if (server_addr)
        SPRTF(" --IP addr     (-I) = Set IP address to connect to fgms. (def=%s)\n", server_addr);
    else
        SPRTF(" --IP addr     (-I) = Set IP address to connect to fgms. (def=IPADDR_ANY)\n");
    SPRTF(" --PORT val    (-P) = Set PORT address to connect to fgms. (dep=%d)\n", server_port);
    exit(0);
}

int parse_commands( int argc, char **argv )
{
    int i, i2;
    char *arg;
    char *sarg;
    
    for ( i = 1; i < argc; i++ ) {
        i2 = i + 1;
        arg = argv[i];
        sarg = arg;
        if ((strcmp(arg,"--help") == 0) || (strcmp(arg,"-h") == 0) ||
            (strcmp(arg,"-?") == 0) || (strcmp(arg,"--version") == 0)||
            (strcmp(arg,"-v") == 0)) {
            give_help(argv[0]);
            exit(0);
        } else if (*sarg == '-') {
            sarg++;
            while (*sarg == '-') sarg++;
            switch (*sarg) 
            {
            case 'i':
            case 'I':
                if (i2 < argc) {
                    sarg = argv[i2];
                    //ip_address = strdup(sarg);
                    server_addr = strdup(sarg);
                    i++;
                } else {
                    SPRTF("fgms IP address must follow!\n");
                    goto Bad_ARG;
                }
                break;
            case 'p':
            case 'P':
                if (i2 < argc) {
                    sarg = argv[i2];
                    server_port = atoi(sarg);
                    i++;
                } else {
                    SPRTF("fgms server PORT value must follow!\n");
                    goto Bad_ARG;
                }
                break;
            default:
                goto Bad_ARG;
            }
        } else {
Bad_ARG:
            SPRTF("ERROR: Unknown argument [%s]! Try -?\n",arg);
            exit(1);
        }
    }
    return 0;
}

static char RecvBuf[1024];

void show_address( sockaddr_in *pAddr, int size )
{
    DWORD rblen = 1024;
    int res = WSAAddressToString( 
        (LPSOCKADDR)pAddr,  // _In_      LPSOCKADDR lpsaAddress,
        (DWORD)size,        // _In_      DWORD dwAddressLength,
        0,                  // _In_opt_  LPWSAPROTOCOL_INFO lpProtocolInfo,
        (LPTSTR)RecvBuf,    // _Inout_   LPTSTR lpszAddressString,
        &rblen );           // _Inout_   LPDWORD lpdwAddressStringLength
    if (res == SOCKET_ERROR) {
        int err = WSAGetLastError();
        char *perr = get_errmsg_txt(err);
        if (perr) {
            printf("WSAAddressToString returned error %d - %s", err, perr);
            LocalFree(perr);
        } else
            printf("WSAAddressToString returned error %d", err);
    }
    char *paddr = (char *)RecvBuf;
    if (pAddr->sin_addr.s_addr == htonl(INADDR_ANY))
        paddr = "INADDR_ANY";
    else if (pAddr->sin_addr.s_addr == htonl(INADDR_NONE))
        paddr = "INADDR_NONE";
    int port = (int)ntohs( pAddr->sin_port );
    printf("NetAddr: IP %s, Port %d\n", paddr, port);
}

#define USE_SELECT
#define MY_TIMEOUT 5.0; // 0.5    /* in seconds */

int main(int argc, char **argv)
{

    int iResult = 0;
    int BufLen = 1024;
    WSADATA wsaData;
    SOCKET RecvSocket;
    sockaddr_in RecvAddr;
    unsigned short Port;
    sockaddr_in SenderAddr;
    int SenderAddrSize = sizeof (SenderAddr);
    int err;
    char *perr;

    parse_commands( argc, argv );
    
    Port = server_port;

    //-----------------------------------------------
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        wprintf(L"WSAStartup failed with error %d\n", iResult);
        return 1;
    }
    //-----------------------------------------------
    // Create a receiver socket to receive datagrams
    RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (RecvSocket == INVALID_SOCKET) {
        err = WSAGetLastError();
        perr = get_errmsg_txt(err);
        if (perr) {
            printf("socket failed with error: %d - %s\n", err, perr);
            LocalFree(perr);
        } else
            wprintf(L"socket failed with error: %d\n", err);
        return 1;
    }
    //-----------------------------------------------
    // Bind the socket to any address and the specified port.
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(Port);
	if (server_addr) {
        RecvAddr.sin_addr.s_addr = inet_addr(server_addr);
        if (RecvAddr.sin_addr.s_addr == INADDR_NONE) {
            struct hostent *hp = gethostbyname(server_addr);
            if (hp == NULL) {
                SPRTF("%s: Unable to resolve address %s, reverting to INADDR_ANY!\n",mod_name, server_addr);
                RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
                server_addr = 0;
            } else {
                memcpy((char *)&RecvAddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
            }
        }
	} else {
        RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    show_address( &RecvAddr, sizeof(RecvAddr) );

    iResult = bind(RecvSocket, (SOCKADDR *) &RecvAddr, sizeof (RecvAddr));
    if (iResult != 0) {
        err = WSAGetLastError();
        perr = get_errmsg_txt(err);
        if (perr) {
            printf("bind failed with error %d - %s\n", err, perr);
            LocalFree(perr);
        } else
            wprintf(L"bind failed with error %d\n", WSAGetLastError());
        return 1;
    }
    //-----------------------------------------------
    // Call the recvfrom function to receive datagrams
    // on the bound socket.
#ifdef USE_SELECT
    double time_out = (double)MY_TIMEOUT;
    struct timeval timeout;  /* Timeout for select */
    printf("Using 'select()' to wait for NEW connections on the listening. TImeout %f secs\n",time_out);
    while (1) {
        fd_set socks;  /* Socket file descriptors we want to wake up for, using select() */
        SOCKET highsock = 0; /* Highest #'d file descriptor, needed for select() */
        int usecs = (int)(time_out * 1000000.0);
        timeout.tv_sec = usecs / 1000000;
        timeout.tv_usec = usecs % 1000000;
        FD_ZERO(&socks);
	    FD_SET(RecvSocket,&socks); /* set our listening port */
        // The select function returns the total number of socket handles that are ready
        // and contained in the fd_set structures, zero if the time limit expired,
        // or SOCKET_ERROR if an error occurred
	    int	readsocks = select((int)highsock+1, &socks, (fd_set *) 0, (fd_set *) 0,&timeout);
        if (readsocks == SOCKET_ERROR) {
            err = WSAGetLastError();
            perr = get_errmsg_txt(err);
            if (perr) {
                printf("recvfrom failed with error %d - %s\n", err, perr);
                LocalFree(perr);
            } else
                wprintf(L"recvfrom failed with error %d\n", WSAGetLastError());
            break;
        } else if (readsocks == 0) {
            printf(".");
            fflush(stdout);
        } else {
            iResult = recvfrom(RecvSocket,
                           RecvBuf, BufLen, 0, (SOCKADDR *) &SenderAddr, &SenderAddrSize);
            wprintf(L"Received %d bytes...\n", iResult);
        }
    }
#else // !USE_SELECT
    //while (1) {
        wprintf(L"Receiving datagram... Blocked until received...\n");
        iResult = recvfrom(RecvSocket,
                           RecvBuf, BufLen, 0, (SOCKADDR *) &SenderAddr, &SenderAddrSize);
        if (iResult == SOCKET_ERROR) {
            err = WSAGetLastError();
            perr = get_errmsg_txt(err);
            if (perr) {
                printf("recvfrom failed with error %d - %s\n", err, perr);
                LocalFree(perr);
            } else
                wprintf(L"recvfrom failed with error %d\n", WSAGetLastError());
        }
        if (iResult > 0) {
            wprintf(L"Received %d bytes...\n", iResult);
        }
    //}
#endif // USE_SELECT y/n
    //-----------------------------------------------
    // Close the socket when finished receiving datagrams
    wprintf(L"Finished receiving. Closing socket.\n");
    iResult = closesocket(RecvSocket);
    if (iResult == SOCKET_ERROR) {
        err = WSAGetLastError();
        perr = get_errmsg_txt(err);
        if (perr) {
            printf("closesocket failed with error %d - %s\n", err, perr);
            LocalFree(perr);
        } else
            wprintf(L"closesocket failed with error %d\n", WSAGetLastError());
        return 1;
    }

    //-----------------------------------------------
    // Clean up and exit.
    wprintf(L"Exiting.\n");
    WSACleanup();
    return 0;
}

// eof - recvfrom.cxx

