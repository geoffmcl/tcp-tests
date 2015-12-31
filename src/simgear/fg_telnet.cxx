// fg_telnet.cpp : Defines the entry point for the console application.
//

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#pragma warning(disable:4996)

#include <simgear/compiler.h>
#include <cstdlib>             // atoi()
#include <string>
#include <simgear/debug/logstream.hxx>
#include <simgear/io/iochannel.hxx>
#include <simgear/io/sg_file.hxx>
#include <simgear/io/sg_serial.hxx>
#include <simgear/io/sg_socket.hxx>
#include <simgear/io/sg_socket_udp.hxx>
#include <simgear\math\sg_geodesy.hxx>
//#include <simgear/math/sg_types.hxx>
//#include <simgear/timing/timestamp.hxx>
//#include <simgear/misc/strutils.hxx>

#include <time.h> // for clock_t, and more ...
#include "fg_telnet.hxx"
#include <conio.h>      // for _kbhit() and _getch()
#include <vector>

#define  ISNUM(a) (( a >= '0' )&&( a <= '9' ))

typedef struct tagFGIO {
    double lat,lon;
}FGIO, * PFGIO;

typedef vector<FGIO> vFGIO;

vFGIO vfgio;

int g_show_writes = 0;
int g_show_read = 0;

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
// ================================



// Pauses for a specified number of milliseconds.
void sleep_loop( clock_t wait )
{
   clock_t goal;
   goal = wait + clock();
   while( goal > clock() )
      ;
}
void sleep( clock_t ms )
{
   Sleep(ms);
}

int check_key_available( void )
{
   int chr = _kbhit();
   if(chr)
      chr = _getch();
   return chr;
}

int   kbd_sleep( int secs )
{
   int   chr = check_key_available();
   if(chr)
      return chr;
   int   ms = secs * 1000;
   int   sms;
   if(ms > 100)
      sms = 100;
   else
      sms = ms;
   sleep(sms);
   chr = check_key_available();
   if(chr)
      return chr;
   ms -= sms;
   while(ms)
   {
      if(ms > 100)
         sms = 100;
      else
         sms = ms;
      sleep(sms);
      ms -= sms;
      chr = check_key_available();
      if(chr)
         return chr;
   }
   return chr;
}

int   delay = 3;  // seconds

#define sprtf printf
#define  EndBuf(a)   ( a + strlen(a) )

#define  MY_HEX_WIDTH   16    /* out in lines of 16 characters */
// hexify some data ...
void print_hex( char * buf, int in_size, char * msg, __int64 offset, int skip )
{
   static char hex_buffer[256];
   static char asc_buffer[64];
   LARGE_INTEGER li;
   int   cnt, i, c, d, off, done, ext;
   int   sz = in_size;
   char * lpd = hex_buffer;   /* for HEX values */
   char * pab = asc_buffer;   /* for ASCII values */
   int   had_data = 0;
   int   skipped = 0;

   li.QuadPart = offset;
   //if( sz > 0 && msg )
   //   sprtf( "Hex of %d bytes, from buffer %#X ... %s\n", sz, buf, msg );

   i = 0;
   off = 0;
   *lpd = 0;
   done = 0;
   ext = 0;
   while( sz > 0 )
   {
      if( sz > MY_HEX_WIDTH )
         cnt = MY_HEX_WIDTH;
      else
         cnt = sz;

      had_data = 0;
      for( ; i < in_size; i++ )
      {
         c = buf[i];
         if(c)
            had_data++;

         if(( c < ' ')||( c & 0x80 ))
            d = '.';
         else
            d = c;

         pab[off++] = (char)d;
         if( d == '%' )
            pab[off++] = (char)d;

         sprintf( EndBuf(lpd), "%02X ", (c & 0xff) );

         ext++;   /* char done */
         if( ext >= cnt ) {
            i++;     /* move on to NEXT char */
            break;   /* end of this block */
         }
      }

      pab[off] = 0;  /* zero terminate ASCII stream */
      while( ext < MY_HEX_WIDTH )
      {
         strcat(lpd, "   ");  /* fill HEX out to 16 * 3 */
         ext++;
      }

      //if( had_data || !skip ) {
         //sprtf( "%08X%08X: %s %s %08X ([%s %d])\n",
         //   li.HighPart, li.LowPart,
         //   lpd, pab,
         //   (buf + done),
         //   ( msg ? msg : "" ),
         //   i );
      printf("%s %s\n", lpd, pab);
      //} else {
      //   skipped++;
      //}

      off = 0; /* restart ASCII offset */
      *lpd = 0;   /* restart HEX buffer */
      li.QuadPart += 16;   /* bump the OFFSET */
      sz -= cnt;  /* reduce by count */
      done += cnt;   /* and add to done count */
      ext = 0;    /* restart set counter ... */
   }  /* and loop, while sz > 0 */

   //if( skipped )
   //   sprtf( "Note, skipped %d lines of blanks (0) ...\n", skipped );

   ext = 0;
}



int fgfs_write( SGSocket *sock, string str )
{
    int iret;
    string s = str;
    s += "\015\012";
    iret = sock->write(s.c_str(),s.size());
    sleep(10); // give up cpu briefly
    if (g_show_writes) {
        printf("Written %d bytes...\n", s.size());
        print_hex((char *)s.c_str(), s.size(), NULL, 0, 0);
    }
    return iret;
}


int fgfs_get( SGSocket *sock, string path )
{
    string s = "get ";
    s += path;
    return fgfs_write( sock, s );
}

int fgfs_get_value( SGSocket * sock, string path, double * pd )
{
    int iret = 0;
    static char _s_buf[1024];
    char * pbuf = _s_buf;
    int max_wait_ms = 1000;
    MyTimer * pt = new MyTimer;

    *pbuf = 0;
    if (fgfs_get( sock, path )) {
        pt->reset();
        while (max_wait_ms) {
            int len = sock->read( pbuf, 256 );
            if (len > 0) {
                if (g_show_read) {
                    printf("Read %d bytes in %f secs\n", len, pt->elapsed());
                    print_hex( pbuf, len, NULL, 0, 0 );
                }
                double d = atof(pbuf);
                *pd = d;
                iret = 1;
                break;
            } else if (len == -2) {
                sleep(20);
                if (max_wait_ms > 20)
                    max_wait_ms -= 20;
                else {
                    printf("Timeout doing socket read...\n");
                    max_wait_ms = 0;
                }
            } else {
                printf("Read appears to have failed... %d\n", len);
                break;
            }
        }
    } else {
        printf("Send FAILED!\n");
    }
    delete pt;
    return iret;
}

int fgfs_get_coord( SGSocket *sock, double * plon, double * plat )
{
    int iret = 0;
    if (( fgfs_get_value(sock, "/position/longitude-deg", plon) ) &&
        ( fgfs_get_value(sock, "/position/latitude-deg", plat ) ) )
    {
        iret = 1;
    }
    return iret;
}


void fg_telnet_test(int cnt, SGSocket *sock )
{
    double lat, lon;
    FGIO fgio;
    fgfs_write(sock, "data");
    sleep(10);
    if ( fgfs_get_coord(sock, &lon, &lat) ) {
        fgio.lat = lat;
        fgio.lon = lon;
        if (cnt == 1) {
            printf("First lat/lon %f,%f \n", lat, lon );
        } else {
            FGIO fgio1 = vfgio[vfgio.size()-1];
            double az1,az2,s;
            geo_inverse_wgs_84( fgio1.lat, fgio1.lon, fgio.lat, fgio.lon, &az1, &az2, &s);
            if (s < SG_EPSILON) {
                printf("%d lat/lon %f,%f - no movement!\n", cnt, lat, lon );
            } else {
                if (s < 1000.0) {
                    printf("%3d lat/lon %f,%f - dist %f meters, on %d degs\n",
                    cnt, lat, lon,
                    s, (int)(az1 + 0.5) );
                } else {
                    printf("%3d lat/lon %f,%f - dist %f km, on %d degs\n",
                    cnt, lat, lon,
                    s / 1000.0, (int)(az1 + 0.5) );
                }
            }
        }
        vfgio.push_back(fgio);
    } else {
        printf("Failed to get coordinates...\n");
        exit(1);
    }
}


void test_socket ( string host, string port, string style )
{
   int   cnt = 0;
   bool  pause = false;

   printf("Socket to host %s, port %s, style %s\n",
       host.c_str(), port.c_str(), style.c_str());
   SGSocket *sock = new SGSocket(host, port, style);
   if(sock)
   {
      if( sock->open(SG_IO_OUT) ) // if( sock->open(SG_IO_OUT) )
      {
         printf( "SGSocket->open(SG_IO_OUT) to %s, on %s, by %s SUCCESS.\n",
            host.c_str(), port.c_str(), style.c_str() );

         printf("ESC to exit, +/- change delay, p=pause\n" );
         while(1)
         {
            if (! pause ) {
               cnt++;
               fg_telnet_test(cnt, sock);
            }
            int chr = kbd_sleep( delay );
            if(chr == 0x1b) {
               printf("Exit key ...\n");
               break;
            } else if(chr == '+') {
               delay++;
               printf("Delay increased to %d ...\n", delay);
            } else if(chr == '-') {
               if(delay) delay--;
               printf("Delay decreased to %d ...\n", delay);
            } else if(chr == 'p') {
               pause = !pause;
               printf("Pause is %s ...\n", (pause ? "On" : "Off"));
            }
         }

         printf( "Read %d messages...\n", cnt );
         sock->close();
      }
      else
      {
         printf( "SGSocket->open(SG_IO_OUT) to %s, on %s, by %s FAILED!\n",
            host.c_str(), port.c_str(), style.c_str() );
         printf( "Note, the socket reader (server) needs to be running first!\n" );
      }

   }

}

void give_help ( char * pname, const char * pip, const char * pport )
{
   printf( "%s [ip=<address>] [-port=<number>]\n" );
   printf( "Default ip=%s, port=%s\n", pip, pport );
   printf( "Aborting ...\n" );
   exit(0);
}

int main(int argc, char **argv)
{
   string host = "127.0.0.1";
   string port = "5555";
   string style = "tcp";
   printf( "Running %s, compile %s, at %s\n", argv[0], __DATE__, __TIME__ );

   int   i;
   for( i = 1; i < argc; i++)
   {
      char * arg = argv[i];
      if(arg && *arg)
      {
         if(*arg == '-')
         {
            do { arg++; } while (*arg == '-');
            if( strncmp(arg,"ip=", 3) == 0 )
            {
               arg += 3;
               if(*arg == 0)
                  goto Cmd_ERROR;
               host = arg;
            }
            else if( strncmp(arg, "port=", 5) == 0 )
            {
               arg += 5;
               if(*arg == 0)
                  goto Cmd_ERROR;
               if(!ISNUM(*arg))
                  goto Cmd_ERROR;
               port = arg;
            }
            else if( strncmp(arg, "?", 1) == 0 )
            {
               give_help( argv[0], host.c_str(), port.c_str() );
            }
            else
            {
               goto Cmd_ERROR;
            }
         }
         else
         {
Cmd_ERROR:
            printf("ERROR: Unknown argument [%s] ... aborting ...\n", argv[i]);
            exit(1);
         }
      }
   }

   test_socket (host, port, style);
   
   return 0;
}



// eof - fg_telnet.cxx

