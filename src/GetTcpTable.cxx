/*\
 * GetTcpTable.cxx
 *
 * Copyright (c) 2015 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
 * See: https://msdn.microsoft.com/en-us/library/windows/desktop/aa366026(v=vs.85).aspx
 *
\*/

// Need to link with Iphlpapi.lib and Ws2_32.lib
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <algorithm>    // std::sort
#include <vector>
// other includes

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

//     DWORD dwLocalPort;
typedef std::vector<u_short> vPORTS;

bool myfunction(u_short i, u_short j) 
{ 
    return (i < j); 
}

/* Note: could also use malloc() and free() */
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

static const char *module = "GetTcpTable";

void add_to_vector(vPORTS &v, u_short local)
{
    size_t ii, max = v.size();
    for (ii = 0; ii < max; ii++) {
        if (v[ii] == local)
            return;
    }
    v.push_back(local);
}

int do_test()
{
    vPORTS vPorts;
    // Declare and initialize variables
    PMIB_TCPTABLE pTcpTable;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    u_short us;
    size_t ii, max;

    char szLocalAddr[128];
    char szRemoteAddr[128];

    struct in_addr IpAddr;

    int i, i2;

    pTcpTable = (MIB_TCPTABLE *)MALLOC(sizeof(MIB_TCPTABLE));
    if (pTcpTable == NULL) {
        printf("Error allocating memory\n");
        return 1;
    }

    dwSize = sizeof(MIB_TCPTABLE);
    // Make an initial call to GetTcpTable to
    // get the necessary size into the dwSize variable
    if ((dwRetVal = GetTcpTable(pTcpTable, &dwSize, TRUE)) ==
        ERROR_INSUFFICIENT_BUFFER) {
        FREE(pTcpTable);
        pTcpTable = (MIB_TCPTABLE *)MALLOC(dwSize);
        if (pTcpTable == NULL) {
            printf("Error allocating memory\n");
            return 1;
        }
    }
    // Make a second call to GetTcpTable to get
    // the actual data we require
    if ((dwRetVal = GetTcpTable(pTcpTable, &dwSize, TRUE)) == NO_ERROR) {
        printf("%s: Number of entries: %d\n", module, (int)pTcpTable->dwNumEntries);
        for (i = 0; i < (int)pTcpTable->dwNumEntries; i++) {
            i2 = i + 1;
            IpAddr.S_un.S_addr = (u_long)pTcpTable->table[i].dwLocalAddr;
            strcpy_s(szLocalAddr, sizeof(szLocalAddr), inet_ntoa(IpAddr));
            IpAddr.S_un.S_addr = (u_long)pTcpTable->table[i].dwRemoteAddr;
            strcpy_s(szRemoteAddr, sizeof(szRemoteAddr), inet_ntoa(IpAddr));

            printf("\n\tTCP[%d] State: %ld - ", i2,
                pTcpTable->table[i].dwState);
            switch (pTcpTable->table[i].dwState) {
            case MIB_TCP_STATE_CLOSED:
                printf("CLOSED\n");
                break;
            case MIB_TCP_STATE_LISTEN:
                printf("LISTEN\n");
                break;
            case MIB_TCP_STATE_SYN_SENT:
                printf("SYN-SENT\n");
                break;
            case MIB_TCP_STATE_SYN_RCVD:
                printf("SYN-RECEIVED\n");
                break;
            case MIB_TCP_STATE_ESTAB:
                printf("ESTABLISHED\n");
                break;
            case MIB_TCP_STATE_FIN_WAIT1:
                printf("FIN-WAIT-1\n");
                break;
            case MIB_TCP_STATE_FIN_WAIT2:
                printf("FIN-WAIT-2 \n");
                break;
            case MIB_TCP_STATE_CLOSE_WAIT:
                printf("CLOSE-WAIT\n");
                break;
            case MIB_TCP_STATE_CLOSING:
                printf("CLOSING\n");
                break;
            case MIB_TCP_STATE_LAST_ACK:
                printf("LAST-ACK\n");
                break;
            case MIB_TCP_STATE_TIME_WAIT:
                printf("TIME-WAIT\n");
                break;
            case MIB_TCP_STATE_DELETE_TCB:
                printf("DELETE-TCB\n");
                break;
            default:
                printf("UNKNOWN dwState value\n");
                break;
            }
            printf("\tTCP[%d] Local Addr: %s\n", i2, szLocalAddr);
            us = ntohs((u_short)pTcpTable->table[i].dwLocalPort);
            printf("\tTCP[%d] Local Port: %d \n", i2, us);
            printf("\tTCP[%d] Remote Addr: %s\n", i2, szRemoteAddr);
            printf("\tTCP[%d] Remote Port: %d\n", i2,
                ntohs((u_short)pTcpTable->table[i].dwRemotePort));

            add_to_vector(vPorts, us);

        }
        printf("\n%s: Shown %d entries...\n", module, (int)pTcpTable->dwNumEntries);
        max = vPorts.size();
        // using function as comp
        std::sort(vPorts.begin(), vPorts.end(), myfunction);
        printf("%s: %d ports: ", module, (int)max);
        for (ii = 0; ii < max; ii++) {
            us = vPorts[ii];
            printf("%d ", us);
        }
        printf("\n");
    }
    else {
        printf("\tGetTcpTable failed with %d\n", dwRetVal);
        FREE(pTcpTable);
        return 1;
    }

    if (pTcpTable != NULL) {
        FREE(pTcpTable);
        pTcpTable = NULL;
    }

    return 0;
}


int show_tcp_table()
{
    int iret = 1;
    iret = do_test();
    return iret;
}

// implementation
int main(int argc, char *argv[])
{
    int iret = 1;

    iret = show_tcp_table();

    return iret;
}


// eof = GetTcpTable.cxx
