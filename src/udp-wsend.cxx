
#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <time.h>

// Link with ws2_32.lib
#ifdef WIN32
#pragma comment(lib, "Ws2_32.lib")
#endif

/////////////////////////////////////////////////////////////////////////
/* ***************************************** */
/* keyboard check */
#ifndef GOT_KEYBOARD_TEST
#define GOT_KEYBOARD_TEST

#ifdef _MSC_VER
#include <conio.h>
int test_for_input(void)
{
    int chr = _kbhit();
    if (chr)
        chr = _getch();
    return chr;
}
#else
int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}
int test_for_input(void)
{
    int chr = kbhit();
    if (chr)
        chr = getchar();
    return chr;
}
#endif
#endif /* #ifndef GOT_KEYBOARD_TEST */
/* ============================================= */

int chk_keyboard()
{
    int chr = test_for_input();
    switch (chr) {
    case 0:
        break;
    case 0x1b:
        return 1;
        break;
    default:
        break;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////

int net_init()
{
#ifdef WIN32
    //----------------------
    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        wprintf(L"WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

#endif // WIN32
    return 0;
}

// static
int check_key(void)
{
    int chr = 0;
#ifdef GOT_KEYBOARD_TEST
    chr = test_for_input();
    if (chr) {
        switch (chr)
        {
        case 0x1b:
            wprintf(L"ESC key - commence exit\n");
            return 1;
            break;
        case '?':
            //show_help();
            break;
        case 'p':
            //pause_time = time(0);
            //is_paused = (is_paused ? 0 : 1);
            //printf("p - Toggled PAUSE. Currently %s\n",
            //    (is_paused ? "On" : "OFF") );
            break;
        case '0':
        case '1':
        case '2':
        case '5':
        case '9':
            //verbosity = (chr - '0');
            //printf("%c - Set verbosity to %d\n", chr, verbosity);
            break;
        default:
            wprintf(L"Unused key input...%d (%x)\n", chr, chr);
            break;
        }
        chr = 0;
    }
#endif // #ifdef GOT_KEYBOARD_TEST
    return chr;
}


int main()
{
    int iLoop = 1;
    int iRet = 0;
    int iResult;
    int sendcnt = 0;
    time_t now, last = 0;
    SOCKET SendSocket = INVALID_SOCKET;
    sockaddr_in RecvAddr;
    const char *Host = "192.168.1.1";
    unsigned short Port = 5556;

    char SendBuf[1024];
    int BufLen = 1024;

    if (net_init())
        return 1;

    //---------------------------------------------
    // Create a socket for sending data
    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == INVALID_SOCKET) {
        wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    //---------------------------------------------
    // Set up the RecvAddr structure with the IP address of
    // the receiver (in this example case "192.168.1.1")
    // and the specified port number.
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(Port);
    RecvAddr.sin_addr.s_addr = inet_addr(Host);

    printf("UDP server - IP %s, port %d, struct len %d\n", inet_ntoa(RecvAddr.sin_addr),
        ntohs(RecvAddr.sin_port), (int)sizeof (RecvAddr) );
    //printf("UDP server - IP %s, port %d, struct len %d\n", Host, Port, sizeof (RecvAddr) );

    while (iLoop) {

        now = time(NULL);
        if (now != last) {
            last = now;
            //---------------------------------------------
            // Send a datagram to the receiver
            sendcnt++;
            wprintf(L"Sending %d datagram to the receiver...\n", sendcnt);
            iResult = sendto(SendSocket,
                             SendBuf, BufLen, 0, (SOCKADDR *) & RecvAddr, sizeof (RecvAddr));
            if (iResult == SOCKET_ERROR) {
                wprintf(L"sendto failed with error: %d\n", WSAGetLastError());
                iRet = 1;
                iLoop = 0;
                break;
            }
        }
        if (check_key()) {
            iLoop = 0;
        }
    }
    //---------------------------------------------
    // When the application is finished sending, close the socket.
    wprintf(L"Finished sending %d. Closing socket.\n", sendcnt);
    iResult = closesocket(SendSocket);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"closesocket failed with error: %d\n", WSAGetLastError());
        iRet = 1;
    }
    //---------------------------------------------
    // Clean up and quit.
    wprintf(L"Exiting %d.\n",iRet);
    WSACleanup();
    return iRet;
}


// eof
