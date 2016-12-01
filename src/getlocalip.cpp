// 20160730 - from : http://tangentsoft.net/wskfaq/examples/ipaddr.html
// Borland C++ 5.0: bcc32.cpp getlocalip.cpp
// Visual C++ 5.0: cl getlocalip.cpp wsock32.lib
//
// This sample program is hereby placed in the public domain.

#include <iostream>
#include <winsock2.h>

using namespace std;

static const char *module = "getlocalip";

int net_init()
{
#ifdef WIN32
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        cerr << "Error " << WSAGetLastError() <<
                " when doing 1.1 startup." << endl;
        return 1;
    }
#endif
    return 0;

}

void net_close()
{
#ifdef WIN32
    WSACleanup();
#endif
}

// int doit(int, char **)
int getinfo()
{
    char ac[80];
    int i, cnt = 0;
    if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
        cerr << "Error " << WSAGetLastError() <<
                " when getting local host name." << endl;
        return 1;
    }
    cout << module << ": Host (localhost) name is " << ac << "." << endl;

    struct hostent *phe = gethostbyname(ac);
    if (phe == 0) {
        cerr << "Yow! Bad host lookup." << endl;
        return 1;
    }
    for (i = 0; phe->h_addr_list[i] != 0; ++i) {
        cnt++;
    }
    cout << " Address(es) " << cnt << endl;

    for (i = 0; phe->h_addr_list[i] != 0; ++i) {
        struct in_addr addr;
        memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
        cout << "  Address " << (i+1) << ": " << inet_ntoa(addr) << endl;
    }
    
    return 0;
}

// rough check of any command entered
// expect none, but 0 exit if '?' or 'h' is that start...
int chk_cmd(int argc, char *argv[])
{
    int iret = 0;
    int i, c;
    char * arg, *sarg;
    // cout << "Check of " << (argc - 1) << " commands..." << endl;
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        sarg = &arg[0];
        // cout << " Check of " << (i + 1) << " " << arg << endl;
        while (*sarg == '-')
            sarg++;
        c = *sarg;
        if ((c == '?') || (c == 'h')) {
            cerr << "This " << module << " app takes no arguments... but '?', 'h' exit 0" << endl;
            return 2;
        }
        cerr << "This " << module << " app takes no arguments..." << endl;
        iret = 1;
        break;
    }
    return iret;
}

int main(int argc, char *argv[])
{
    int iret = 0;

    iret = chk_cmd(argc,argv);
    if (iret) {
        if (iret==2)
            iret = 0;
        return iret;
    }

    if (net_init())
        return 1;

    int retval = getinfo();

    net_close();

    return retval;
}

// eof
