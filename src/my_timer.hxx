// my_timer.hxx
#ifndef _my_timer_hxx_
#define _my_timer_hxx_

#include <windows.h>

// ================================
typedef  struct { /* tm */
   BOOL  tm_bInUse;
   BOOL  tm_bt;
   LARGE_INTEGER  tm_lif, tm_lib, tm_lie;
   DWORD tm_dwtc;
}GTM, * PGTM;

class MyTimer
{
public:
    MyTimer() { reset(); };
    ~MyTimer() { };
    double elapsed(void);
    void reset(void);
    GTM gtm;
};

#endif // #ifndef _my_timer_hxx_
// eof - my_timer.hxx

