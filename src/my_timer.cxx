// my_timer.cxx

#include "my_timer.hxx"


void MyTimer::reset()
{
    PGTM ptm = &gtm;
    ptm->tm_bt = QueryPerformanceFrequency( &ptm->tm_lif );
    if(ptm->tm_bt)
        QueryPerformanceCounter( &ptm->tm_lib ); // counter value
    else
        ptm->tm_dwtc = GetTickCount(); // ms since system started
}

double MyTimer::elapsed(void)
{
    PGTM ptm = &gtm;
    double db = 0.0;
    if( ptm->tm_bt ) {
        LARGE_INTEGER lid;
        QueryPerformanceCounter( &ptm->tm_lie ); // counter value
        lid.QuadPart = ( ptm->tm_lie.QuadPart - ptm->tm_lib.QuadPart ); // get difference
        db  = (double)lid.QuadPart / (double)ptm->tm_lif.QuadPart;
    } else {
        DWORD dwd = (GetTickCount() - ptm->tm_dwtc);   // ms elapsed
        db = ((double)dwd / 1000.0);
    }
    return db;
}

// eof - my_timer.cxx
