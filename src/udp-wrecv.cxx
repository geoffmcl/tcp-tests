

#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <time.h>

#ifdef WIN32
// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")
#endif

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

int main()
{

    int iResult = 0;

    SOCKET RecvSocket;
    sockaddr_in RecvAddr;

    unsigned short Port = 5556; // 27015;

    char RecvBuf[1024];
    int BufLen = 1024;

    sockaddr_in SenderAddr;
    int SenderAddrSize = sizeof (SenderAddr);

    //-----------------------------------------------
    if (net_init()) // Initialize Winsock
        return 1;

    //-----------------------------------------------
    // Create a receiver socket to receive datagrams
    RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (RecvSocket == INVALID_SOCKET) {
        wprintf(L"socket failed with error %d\n", WSAGetLastError());
        return 1;
    }
    //-----------------------------------------------
    // Bind the socket to any address and the specified port.
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(Port);
    RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    iResult = bind(RecvSocket, (SOCKADDR *) & RecvAddr, sizeof (RecvAddr));
    if (iResult != 0) {
        wprintf(L"bind failed with error %d\n", WSAGetLastError());
        return 1;
    }

    printf("UDP wrecvr - IP %s, port %d, struct len %d\n", inet_ntoa(RecvAddr.sin_addr),
        ntohs(RecvAddr.sin_port), sizeof (RecvAddr) );

    //-----------------------------------------------
    // Call the recvfrom function to receive datagrams
    // on the bound socket.
    wprintf(L"Receiving datagrams...\n");
    iResult = recvfrom(RecvSocket,
                       RecvBuf, BufLen, 0, (SOCKADDR *) & SenderAddr, &SenderAddrSize);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"recvfrom failed with error %d\n", WSAGetLastError());
    }
 
    //-----------------------------------------------
    // Close the socket when finished receiving datagrams
    wprintf(L"Finished receiving. Closing socket.\n");
    iResult = closesocket(RecvSocket);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"closesocket failed with error %d\n", WSAGetLastError());
        return 1;
    }

    //-----------------------------------------------
    // Clean up and exit.
    wprintf(L"Exiting.\n");
    WSACleanup();
    return 0;
}

// eof
