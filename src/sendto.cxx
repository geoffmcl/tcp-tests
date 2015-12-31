
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

static const char *mod_name = "sendto";
static int server_port = 3333;
static char *server_addr = (char *)"localhost";

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
        SPRTF(" --IP addr     (-I) = Set IP address to connect to peer receiver. (def=%s)\n", server_addr);
    else
        SPRTF(" --IP addr     (-I) = Set IP address to connect to peer receiver. (def=IPADDR_ANY)\n");
    SPRTF(" --PORT val    (-P) = Set PORT address to connect to peer receiver. (dep=%d)\n", server_port);
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

static char SendBuf[1024];
void show_address( sockaddr_in *pAddr, int size )
{
    DWORD rblen = 1024;
    int res = WSAAddressToString( 
        (LPSOCKADDR)pAddr,  // _In_      LPSOCKADDR lpsaAddress,
        (DWORD)size,        // _In_      DWORD dwAddressLength,
        0,                  // _In_opt_  LPWSAPROTOCOL_INFO lpProtocolInfo,
        (LPTSTR)SendBuf,    // _Inout_   LPTSTR lpszAddressString,
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
    char *paddr = (char *)SendBuf;
    if (pAddr->sin_addr.s_addr == htonl(INADDR_ANY))
        paddr = "INADDR_ANY";
    else if (pAddr->sin_addr.s_addr == htonl(INADDR_NONE))
        paddr = "INADDR_NONE";
    int port = (int)ntohs( pAddr->sin_port );
    printf("NetAddr: IP %s, Port %d\n", paddr, port);
}

int main(int argc, char **argv)
{

    int BufLen = 1024;
    int iResult;
    WSADATA wsaData;
    SOCKET SendSocket = INVALID_SOCKET;
    sockaddr_in RecvAddr;
    unsigned short Port;
    int err;
    char *perr;

    parse_commands( argc, argv );

    Port = server_port;

#ifdef _MSC_VER
    //----------------------
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        //err = WSAGetLastError();
        perr = get_errmsg_txt(iResult);
        if (perr) {
            printf("WSAStartup failed with error: %d - %s\n", iResult, perr);
            LocalFree(perr);

        } else
            wprintf(L"WSAStartup failed with error: %d\n", iResult);
        return 1;
    }
#endif // _MSC_VER

    //---------------------------------------------
    // Create a socket for sending data
    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == INVALID_SOCKET) {
        err = WSAGetLastError();
        perr = get_errmsg_txt(err);
        if (perr) {
            printf("socket failed with error: %d - %s\n", err, perr);
            LocalFree(perr);
        } else
            wprintf(L"socket failed with error: %d\n", err);
        WSACleanup();
        return 1;
    }
    //---------------------------------------------
    // Set up the RecvAddr structure with the IP address of
    // the receiver (in this example case "192.168.1.1")
    // and the specified port number.
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(Port);
    // RecvAddr.sin_addr.s_addr = inet_addr("192.168.1.1");
	if (server_addr) {
        RecvAddr.sin_addr.s_addr = inet_addr(server_addr);
        if (RecvAddr.sin_addr.s_addr == INADDR_NONE) {
            struct hostent *hp = gethostbyname(server_addr);
            if (hp == NULL) {
                SPRTF("%s: Unable to resolve address %s! Aborting...\n",mod_name, server_addr);
                //RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
                //server_addr = 0;
                return 1;
            } else {
                SPRTF("%s: Resolved address %s to an IP\n",mod_name, server_addr);
                memcpy((char *)&RecvAddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
            }
        }
	} else {
        RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    show_address( &RecvAddr, sizeof(RecvAddr) );

    //---------------------------------------------
    // Send a datagram to the receiver
    wprintf(L"Sending a datagram to the receiver...\n");
    iResult = sendto(SendSocket,
                     SendBuf, BufLen, 0, (SOCKADDR *) & RecvAddr, sizeof (RecvAddr));
    if (iResult == SOCKET_ERROR) {
        err = WSAGetLastError();
        perr = get_errmsg_txt(err);
        if (perr) {
            printf("sendto failed with error: %d - %s\n", err, perr);
            LocalFree(perr);
        } else
            wprintf(L"sendto failed with error: %d\n", err);
        closesocket(SendSocket);
        WSACleanup();
        return 1;
    }
    //---------------------------------------------
    // When the application is finished sending, close the socket.
    wprintf(L"Finished sending. Closing socket.\n");
    iResult = closesocket(SendSocket);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"closesocket failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    //---------------------------------------------
    // Clean up and quit.
    wprintf(L"Exiting.\n");
    WSACleanup();
    return 0;
}

// sendto.cxx
