// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <tchar.h>

#include <simgear/math/SGMath.hxx>
#include <simgear/constants.h>

// TODO: reference additional headers your program requires here
