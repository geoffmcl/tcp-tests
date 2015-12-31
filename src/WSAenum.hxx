// WSAenum.hxx
#ifndef _WSAenum_hxx_
#define _WSAenum_hxx_

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#include <Winsock2.h>
#include <stdio.h>
#include <tchar.h>

// link with ws2_32.lib/dll
#pragma comment( lib, "ws2_32" )

#endif // #ifndef _WSAenum_hxx_
// eof - WSAenum.hxx


