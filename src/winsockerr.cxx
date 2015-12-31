/* winsockerr.cxx */
#ifndef HAVE_SOCK_ERROR
#define HAVE_SOCK_ERROR
#include "simp_common.hxx"

#ifdef _MSC_VER
#include <Winsock2.h>
typedef int socklen_t;
/* =============================================================
   Provided nice error strings, NOT available in system errors.
   ============================================================= */
typedef struct tagSOCKERRS {
   int err;       /* WSAGetLastError() value */
   char * desc;   /* description of error */
} SOCKERR;

/* could/should perhaps be by the actual call,
   but for now, just one big list, with some repeats
*/

SOCKERR sSockErr[] = {
   { WSANOTINITIALISED,
     "WSANOTINITIALISED - "
     "WSAStartup must be called before using this function." },
   { WSAENETDOWN,
     "WSAENETDOWN - "
     "The network subsystem has failed." },
   { WSAEACCES,
     "WSAEACCES - "
     "Attempt to connect datagram socket to broadcast address failed "
     "because setsockopt option SO_BROADCAST is not enabled." },
   { WSAEADDRINUSE,
     "WSAEADDRINUSE - "
     "A process on the computer is already bound to the same fully-qualified "
     "address and the socket has not been marked to allow address reuse with "
     "SO_REUSEADDR. For example, the IP address and port are bound in the "
     "af_inet case). (See the SO_REUSEADDR socket option under setsockopt.)" },
   { WSAEADDRNOTAVAIL,
     "WSAEADDRNOTAVAIL - "
     "The specified address is not a valid address for this computer." },
   { WSAEFAULT,
     "WSAEFAULT - "
     "The name or namelen parameter is not a valid part of the user "
     "address space, the namelen parameter is too small, the name parameter "
     "contains an incorrect address format for the associated "
     "address family, or the first two bytes of the memory block "
     "specified by name does not match the address family associated with "
     "the socket descriptor s." },
   { WSAEINPROGRESS,
     "WSAEINPROGRESS - "
     "A blocking Windows Sockets 1.1 call is in progress, or the "
     "service provider is still processing a callback function." },
   { WSAEINVAL,
     "WSAEINVAL - "
     "The socket is already bound to an address." },
   { WSAENOBUFS,
     "WSAENOBUFS - "
     "Not enough buffers available, too many connections." },
   { WSAENOTSOCK,
     "WSAENOTSOCK - "
     "The descriptor is not a socket." },

   // setsocketopt
   { WSAENETRESET,
     "WSAENETRESET - "
     "Connection has timed out when SO_KEEPALIVE is set." },
   { WSAENOPROTOOPT,
     "WSAENOPROTOOPT - "
     "The option is unknown or the specified provider "
     "or socket is not capable of implementing it "
     "(see SO_GROUP_PRIORITY limitations)." },
   { WSAENOTCONN,
     "WSAENOTCONN - "
     "Connection has been reset when SO_KEEPALIVE is set." },

   // WSAStartup
   { WSASYSNOTREADY,
     "WSASYSNOTREADY - "
     "The underlying network subsystem is not ready for "
     "network communication." },
   { WSAVERNOTSUPPORTED,
     "WSAVERNOTSUPPORTED - "
     "The version of Windows Sockets function requested is not provided "
     "by this particular Windows Sockets implementation." },
   { WSAEINPROGRESS,
     "WSAEINPROGRESS - "
     "A blocking Windows Sockets 1.1 operation is in progress." },
   { WSAEPROCLIM,
     "WSAEPROCLIM - "
     "Limit on the number of tasks allowed by the Windows Sockets "
     "implementation has been reached." },
   { WSAEFAULT,
     "WSAEFAULT - "
     "The lpWSAData is not a valid pointer." },
   // listen
   { WSANOTINITIALISED,
     "WSANOTINITIALISED - "
     "A successful WSAStartup call must occur before using this function." },
   { WSAENETDOWN,
     "WSAENETDOWN - "
     "The network subsystem has failed." },
   { WSAEADDRINUSE,
     "WSAEADDRINUSE - "
     "The socket's local address is already in use and the socket "
     "was not marked to allow address reuse with SO_REUSEADDR.  "
     "This error usually occurs during execution of the bind function, "
     "but could be delayed until this function if the bind was to "
     "a partially wildcard address (involving ADDR_ANY) "
     "and if a specific address needs to be committed at the time "
     "of this function call." },
   { WSAEINPROGRESS,
     "WSAEINPROGRESS - "
     "A blocking Windows Sockets 1.1 call is in progress, "
     "or the service provider is still processing a callback function." },
   { WSAEINVAL,
     "WSAEINVAL - "
     "The socket has not been bound with bind." },
   { WSAEISCONN,
     "WSAEISCONN - "
     "The socket is already connected." },
   { WSAEMFILE,
     "WSAEMFILE - "
     "No more socket descriptors are available." },
   { WSAENOBUFS,
     "WSAENOBUFS - "
     "No buffer space is available." },
   { WSAENOTSOCK,
     "WSAENOTSOCK - "
     "The descriptor is not a socket." },
   { WSAEOPNOTSUPP,
     "WSAEOPNOTSUPP - "
     "The referenced socket is not of a type that has a listen operation." },

   // getpeername
   { WSANOTINITIALISED,
     "WSANOTINITIALISED - "
     "A successful WSAStartup call must occur before using this function." },
   { WSAENETDOWN,
     "WSAENETDOWN - "
     "The network subsystem has failed." },
   { WSAEFAULT,
     "WSAEFAULT - "
     "The name or the namelen parameter is not a valid part of the "
     "user address space, or the namelen parameter is too small." },
   { WSAEINPROGRESS,
     "WSAEINPROGRESS - "
     "A blocking Windows Sockets 1.1 call is in progress, "
     "or the service provider is still processing a callback function." },
   { WSAENOTCONN,
     "WSAENOTCONN - "
     "The socket is not connected." },
   { WSAENOTSOCK,
     "WSAENOTSOCK - "
     "The descriptor is not a socket." },

   // accept
   { WSANOTINITIALISED,
     "WSANOTINITIALISED - "
     "A successful WSAStartup call must occur before using this function." },
   { WSAENETDOWN,
     "WSAENETDOWN - "
     "The network subsystem has failed." },
   { WSAEFAULT,
     "WSAEFAULT - "
     "The addrlen parameter is too small or addr is not a valid part "
     "of the user address space." },
   { WSAEINTR,
     "WSAEINTR - "
     "A blocking Windows Sockets 1.1 call was canceled through "
     "WSACancelBlockingCall." },
   { WSAEINPROGRESS,
     "WSAEINPROGRESS - "
     "A blocking Windows Sockets 1.1 call is in progress, "
     "or the service provider is still processing a callback function." },
   { WSAEINVAL,
     "WSAEINVAL - "
     "The listen function was not invoked prior to accept." },
   { WSAEMFILE,
     "WSAEMFILE - "
     "The queue is nonempty upon entry to accept and "
     "there are no descriptors available." },
   { WSAENOBUFS,
     "WSAENOBUFS - "
     "No buffer space is available." },
   { WSAENOTSOCK,
     "WSAENOTSOCK - "
     "The descriptor is not a socket." },
   { WSAEOPNOTSUPP,
     "WSAEOPNOTSUPP - "
     "The referenced socket is not a type that offers connection-oriented "
     "service." },
   { WSAEWOULDBLOCK,
     "WSAEWOULDBLOCK - "
     "The socket is marked as nonblocking and no connections are present "
     "to be accepted." },

   /* must be last entry */
   { 0,            0 }
};

/* 2011-03-25 - Update from Winsock2 documention */
SOCKERR sSocketErr2[] = {
 { WSAEINTR             , "WSAEINTR - Interrupted function call" }, // 10004
 { WSAEACCES            , "WSAEACCES - Permission denied" }, // 10013
 { WSAEFAULT            , "WSAEFAULT - Bad address" }, // 10014
 { WSAEINVAL            , "WSAEINVAL - Invalid argument" }, // 10022
 { WSAEMFILE            , "WSAEMFILE - Too many open files" }, // 10024
 { WSAEWOULDBLOCK       , "WSAEWOULDBLOCK - Resource temporarily unavailable" }, // 10035
 { WSAEINPROGRESS       , "WSAEINPROGRESS - Operation now in progress" }, // 10036
 { WSAEALREADY          , "WSAEALREADY - Operation already in progress" }, // 10037
 { WSAENOTSOCK          , "WSAENOTSOCK - Socket operation on nonsocket" }, // 10038
 { WSAEDESTADDRREQ      , "WSAEDESTADDRREQ - Destination address required" }, // 10039
 { WSAEMSGSIZE          , "WSAEMSGSIZE - Message too long" }, // 10040
 { WSAEPROTOTYPE        , "WSAEPROTOTYPE - Protocol wrong type for socket" }, // 10041
 { WSAENOPROTOOPT       , "WSAENOPROTOOPT - Bad protocol option" }, // 10042
 { WSAEPROTONOSUPPORT   , "WSAEPROTONOSUPPORT - Protocol not supported" }, // 10043
 { WSAESOCKTNOSUPPORT   , "WSAESOCKTNOSUPPORT - Socket type not supported" }, // 10044
 { WSAEOPNOTSUPP        , "WSAEOPNOTSUPP - Operation not supported" }, // 10045
 { WSAEPFNOSUPPORT      , "WSAEPFNOSUPPORT - Protocol family not supported" }, // 10046
 { WSAEAFNOSUPPORT      , "WSAEAFNOSUPPORT - Address family not supported by protocol family" }, // 10047
 { WSAEADDRINUSE        , "WSAEADDRINUSE - Address already in use" }, // 10048
 { WSAEADDRNOTAVAIL     , "WSAEADDRNOTAVAIL - Cannot assign requested address" }, // 10049
 { WSAENETDOWN          , "WSAENETDOWN - Network is down" }, // 10050
 { WSAENETUNREACH       , "WSAENETUNREACH - Network is unreachable" }, // 10051
 { WSAENETRESET         , "WSAENETRESET - Network dropped connection on reset" }, // 10052
 { WSAECONNABORTED      , "WSAECONNABORTED - Software caused connection abort" }, // 10053
 { WSAECONNRESET        , "WSAECONNRESET - Connection reset by peer" }, // 10054
 { WSAENOBUFS           , "WSAENOBUFS - No buffer space available" }, // 10055
 { WSAEISCONN           , "WSAEISCONN - Socket is already connected" }, // 10056
 { WSAENOTCONN          , "WSAENOTCONN - Socket is not connected" }, // 10057
 { WSAESHUTDOWN         , "WSAESHUTDOWN - Cannot send after socket shutdown" }, // 10058
 { WSAETIMEDOUT         , "WSAETIMEDOUT - Connection timed out" }, // 10060
 { WSAECONNREFUSED      , "WSAECONNREFUSED - Connection refused" }, // 10061
 { WSAEHOSTDOWN         , "WSAEHOSTDOWN - Host is down" }, // 10064
 { WSAEHOSTUNREACH      , "WSAEHOSTUNREACH - No route to host" }, // 10065
 { WSAEPROCLIM          , "WSAEPROCLIM - Too many processes" }, // 10067
 { WSASYSNOTREADY       , "WSASYSNOTREADY - Network subsystem is unavailable" }, // 10091
 { WSAVERNOTSUPPORTED   , "WSAVERNOTSUPPORTED - Winsock.dll version out of range" }, // 10092
 { WSANOTINITIALISED    , "WSANOTINITIALISED - Successful WSAStartup not yet performed" }, // 10093
 { WSAEDISCON           , "WSAEDISCON - Graceful shutdown in progress" }, // 10101
 { WSATYPE_NOT_FOUND    , "WSATYPE_NOT_FOUND - Class type not found" }, // 10109
 { WSAHOST_NOT_FOUND    , "WSAHOST_NOT_FOUND - Host not found" }, // 11001
 { WSATRY_AGAIN         , "WSATRY_AGAIN - Nonauthoritative host not found" }, // 11002
 { WSANO_RECOVERY       , "WSANO_RECOVERY - This is a nonrecoverable error" }, // 11003
 { WSANO_DATA           , "WSANO_DATA - Valid name, no data record of requested type" }, // 11004
 { WSA_INVALID_HANDLE   , "WSA_INVALID_HANDLE - Specified event object handle is invalid" }, // OS dependent
 { WSA_INVALID_PARAMETER, "WSA_INVALID_PARAMETER - One or more parameters are invalid" }, // OS dependent
 { WSA_IO_INCOMPLETE    , "WSA_IO_INCOMPLETE - Overlapped I/O event object not in signaled state" }, // OS dependent
 { WSA_IO_PENDING       , "WSA_IO_PENDING - Overlapped operations will complete later" }, // OS dependent
 { WSA_NOT_ENOUGH_MEMORY, "WSA_NOT_ENOUGH_MEMORY - Insufficient memory available" }, // OS dependent
 { WSA_OPERATION_ABORTED, "WSA_OPERATION_ABORTED - Overlapped operation aborted" }, // OS dependent
#ifdef WSAINVALIDPROCTABLE
 { WSAINVALIDPROCTABLE  , "WSAINVALIDPROCTABLE - Invalid procedure table from service provider" }, // OS dependent
#endif
#ifdef WSAINVALIDPROVIDER
 { WSAINVALIDPROVIDER   , "WSAINVALIDPROVIDER - Invalid service provider version number" }, // OS dependent
#endif
#ifdef WSAPROVIDERFAILEDINIT
 { WSAPROVIDERFAILEDINIT, "WSAPROVIDERFAILEDINIT - Unable to initialize a service provider" }, // OS dependent
#endif
 { WSASYSCALLFAILURE    , "WSASYSCALLFAILURE - System call failure" }, // OS dependent
 { 0                    , NULL }
};

char * getWSAError(int wsaErrno)
{
    SOCKERR * pseP = &sSocketErr2[0];  /* initial value */
    while (pseP->desc) {
        if (pseP->err == wsaErrno)
            return pseP->desc;
        ++pseP;
    }
    pseP = &sSockErr[0];  /* initial value */
    while (pseP->desc) {
        if (pseP->err == wsaErrno)
            return pseP->desc;
        ++pseP;
    }
    return "(no description available)";
}

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
    return NULL;
}

static void win_set_wsa_msg(void)
{
    last_wsa_error = WSAGetLastError();
    last_wsa_emsg = getWSAError(last_wsa_error);
}
static void win_wsa_perror(char * msg)
{
    win_set_wsa_msg();
    fprintf(stderr,"%s [%s]\n", msg, last_wsa_emsg);
}

#endif /* #ifdef _MSC_VER */

void sock_init(void)
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

void sock_end(void)
{
#ifdef _MSC_VER
    WSACleanup();
#endif /* _MSC_VER */
}

int setnonblocking(SOCKET sock)
{
    int status = 0;
#ifdef _MSC_VER
    u_long opts;
    opts = 1;
    status = ioctlsocket(sock, FIONBIO, (u_long *) &opts);
    if (SERROR(status)) {
        PERROR("ERROR: ioctlsocket() FAILED!");
    }
#else // !_MSC_VER
    status = fcntl(sock,F_GETFL);
    if (status < 0) {
        perror("fcntl(F_GETFL)");
        exit(EXIT_FAILURE);
    }
    status = (status | O_NONBLOCK);
    status = fcntl(sock,F_SETFL,status);
    if (status < 0) {
        perror("fcntl(F_SETFL)");
    }
#endif // _MSC_VER
    return status;
}

int setreuseaddr(SOCKET sock)
{
    int status;
    int reuse_addr = 1;
	status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr,
		sizeof(reuse_addr));
    return status;
}

#ifdef _MSC_VER
typedef signed char      int8_t;
typedef signed short     int16_t;
typedef signed int       int32_t;
typedef signed __int64   int64_t;
typedef unsigned char    uint8_t;
typedef unsigned short   uint16_t;
typedef unsigned int     uint32_t;
typedef unsigned __int64 uint64_t;
typedef int ssize_t;
#elif defined(sgi) || defined(__sun)
# include <sys/types.h>
#else
# include <stdint.h>
#endif

#ifndef _BINARY_SWAPS
#define _BINARY_SWAPS

static uint16_t bswap_16(uint16_t x) {
    x = (x >> 8) | (x << 8);
    return x;
}

static uint32_t bswap_32(uint32_t x) {
    x = ((x >>  8) & 0x00FF00FFL) | ((x <<  8) & 0xFF00FF00L);
    x = (x >> 16) | (x << 16);
    return x;
}

static uint64_t bswap_64(uint64_t x) {
    x = ((x >>  8) & 0x00FF00FF00FF00FFLL) | ((x <<  8) & 0xFF00FF00FF00FF00LL);
    x = ((x >> 16) & 0x0000FFFF0000FFFFLL) | ((x << 16) & 0xFFFF0000FFFF0000LL);
    x = (x >> 32) | (x << 32);
    return x;
}
#endif // _BINARY_SWAPS

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
    localHost = gethostbyname("localhost");
    if (localHost) {
        localIP = inet_ntoa (*(struct in_addr *)*localHost->h_addr_list);
        if (localIP)
            return 1;
    }
    
    return 0;
}


#endif /* #ifndef HAVE_SOCK_ERROR */

/* eof - winsockerr.cxx */
