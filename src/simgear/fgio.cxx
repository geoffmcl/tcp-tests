// fgio.cpp : Defines the entry point for the console application.
//
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#pragma warning(disable:4996)
#include <winsock2.h>
#include <simgear/compiler.h>
#include <cstdlib>             // atoi()
#include <string>
#include <simgear/debug/logstream.hxx>
#include <simgear/io/iochannel.hxx>
#include <simgear/io/sg_file.hxx>
#include <simgear/io/sg_serial.hxx>
#include <simgear/io/sg_socket.hxx>
#include <simgear/io/sg_socket_udp.hxx>
#include <simgear/math/sg_geodesy.hxx> // for geo_inverse_wgs_84, ...
//#include <simgear/math/sg_types.hxx>
//#include <simgear/timing/timestamp.hxx>
//#include <simgear/misc/strutils.hxx>

#include <time.h> // for clock_t, and more ...
#include "fgio.h"
#include <conio.h>      // for _kbhit() and _getch()
#include "net_fdm.hxx"
#include "mpmessages.hxx"
#include "fg_geometry.hxx"
#include "GenStg.hxx"

#undef TEST_FGMP_MESSAGE
// static char * sampinraw = (char *)"sample-raw.bin";

#ifndef RELAY_MAGIC
#define RELAY_MAGIC 0x53464746    // GSGF
#endif

#define  ISNUM(a) (( a >= '0' )&&( a <= '9' ))

#ifndef USE_SG_LOG
#define USE_SG_LOG 1
#endif

int g_is_server = 1;
int g_show_writes = 0;
int g_do_echo = 0; // server to ECHO data back, and client reads echo
SGProtocolDir g_direction = SG_IO_BI;
//SGProtocolDir g_direction = SG_IO_IN;
//SGProtocolDir g_direction = SG_IO_OUT;

#define is_server   (g_is_server > 0)
//#define is_server   ((g_direction == SG_IO_IN)||(g_direction == SG_IO_BI))
#define is_bi       (g_direction == SG_IO_BI)

#define DEF_HOST "localhost"
//#define DEF_HOST "127.0.0.1"
//#define DEF_HOST "192.168.1.174"
#define DEF_PORT "5500"
#define DEF_TYPE "tcp"

#define MyMagicNum 0xabcd

#if USE_SG_LOG
#define OUTGS SG_LOG( SG_IO, SG_INFO, globalStg.stg() )
#else
#define OUTGS printf("%s\n", globalStg.stg())
#endif

//bool net_byte_order = true;
bool net_byte_order = false;

int AlignBytes = sizeof(uint32_t);

typedef struct tagMYPACKET {
    uint32_t magic;
    uint32_t type;
    uint32_t count;
    char data[1];
} MYPACKET, * PMYPACKET;

// ================================

// Pauses for a specified number of milliseconds.
void sleep_loop( clock_t wait )
{
   clock_t goal;
   goal = wait + clock();
   while( goal > clock() )
      ;
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
   while(ms)
   {
      chr = check_key_available();
      if(chr)
         return chr;
      if(ms > 100)
         sms = 100;
      else
         sms = ms;
      sleep(sms);
      ms -= sms;
   }
   return chr;
}

int   delay = 3;  // seconds

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

      globalStg.sprintf("%s %s", lpd, pab);
      OUTGS;
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

   ext = 0;
}

// The function htond is defined this way due to the way some
// processors and OSes treat floating point values.  Some will raise
// an exception whenever a "bad" floating point value is loaded into a
// floating point register.  Solaris is notorious for this, but then
// so is LynxOS on the PowerPC.  By translating the data in place,
// there is no need to load a FP register with the "corruped" floating
// point value.  By doing the BIG_ENDIAN test, I can optimize the
// routine for big-endian processors so it can be as efficient as
// possible
static void htond (double &x)	
{
    if ( sgIsLittleEndian() ) {
        int    *Double_Overlay;
        int     Holding_Buffer;
    
        Double_Overlay = (int *) &x;
        Holding_Buffer = Double_Overlay [0];
    
        Double_Overlay [0] = htonl (Double_Overlay [1]);
        Double_Overlay [1] = htonl (Holding_Buffer);
    } else {
        return;
    }
}

// Float version
static void htonf (float &x)	
{
    if ( sgIsLittleEndian() ) {
        int    *Float_Overlay;
        int     Holding_Buffer;
    
        Float_Overlay = (int *) &x;
        Holding_Buffer = Float_Overlay [0];
    
        Float_Overlay [0] = htonl (Holding_Buffer);
    } else {
        return;
    }
}

#define MMXBUF 256
#define MMXBUFS 16
static char _s_nxtbuf[MMXBUF * MMXBUFS];
static int nxt_buf = 0;
char * getnxtbuf(void)
{
    nxt_buf++;
    if (nxt_buf >= MMXBUFS) nxt_buf = 0;
    return &_s_nxtbuf[MMXBUF * nxt_buf];
}


char * get_dist_stg( double m )
{
    char * cp = getnxtbuf();
    if (m > 1000.0) {
        sprintf(cp,"%f km", m / 1000.0);
    } else if (m > 100) {
        sprintf(cp,"%f m", m);
    } else {
        sprintf(cp,"%f mm", m * 1000.0);
    }
    return cp;
}

void convert_net_buf(char * buf, int len)
{
    PMYPACKET pp = (PMYPACKET)buf;
    pp->magic = ntohl(pp->magic);
    pp->type  = ntohl(pp->type);
    pp->count = ntohl(pp->count);
}

void convert_net_fdm(FGNetFDM *net)
{
    uint32_t i;
    // Convert to the net buffer from network format
    net->version = ntohl(net->version);

    htond(net->longitude);
    htond(net->latitude);
    htond(net->altitude);
    htonf(net->agl);
    htonf(net->phi);
    htonf(net->theta);
    htonf(net->psi);
    htonf(net->alpha);
    htonf(net->beta);

    htonf(net->phidot);
    htonf(net->thetadot);
    htonf(net->psidot);
    htonf(net->vcas);
    htonf(net->climb_rate);
    htonf(net->v_north);
    htonf(net->v_east);
    htonf(net->v_down);
    htonf(net->v_wind_body_north);
    htonf(net->v_wind_body_east);
    htonf(net->v_wind_body_down);

    htonf(net->A_X_pilot);
    htonf(net->A_Y_pilot);
    htonf(net->A_Z_pilot);

    htonf(net->stall_warning);
    htonf(net->slip_deg);

    net->num_engines = htonl(net->num_engines);
    for ( i = 0; i < net->num_engines; ++i ) {
        net->eng_state[i] = htonl(net->eng_state[i]);
        htonf(net->rpm[i]);
        htonf(net->fuel_flow[i]);
        htonf(net->fuel_px[i]);
        htonf(net->egt[i]);
        htonf(net->cht[i]);
        htonf(net->mp_osi[i]);
        htonf(net->tit[i]);
        htonf(net->oil_temp[i]);
        htonf(net->oil_px[i]);
    }

    net->num_tanks = htonl(net->num_tanks);
    for ( i = 0; i < net->num_tanks; ++i ) {
        htonf(net->fuel_quantity[i]);
    }

    net->num_wheels = htonl(net->num_wheels);
    for ( i = 0; i < net->num_wheels; ++i ) {
        net->wow[i] = htonl(net->wow[i]);
        htonf(net->gear_pos[i]);
        htonf(net->gear_steer[i]);
        htonf(net->gear_compression[i]);
    }

    net->cur_time = htonl(net->cur_time);
    net->warp = ntohl(net->warp);
    htonf(net->visibility);

    htonf(net->elevator);
    htonf(net->elevator_trim_tab);
    htonf(net->left_flap);
    htonf(net->right_flap);
    htonf(net->left_aileron);
    htonf(net->right_aileron);
    htonf(net->rudder);
    htonf(net->nose_wheel);
    htonf(net->speedbrake);
    htonf(net->spoilers);
}

int is_mp_message( char * Msg, int len )
{
    T_MsgHdr*       MsgHdr;
    T_PositionMsg*  PosMsg;
    uint32_t        MsgId;
    uint32_t        MsgMagic;
    time_t          Timestamp;
    Point3D         SenderPosition;
    Point3D         SenderOrientation;

    MsgHdr    = (T_MsgHdr *) Msg;
    Timestamp = time(0);
    MsgMagic  = XDR_decode<uint32_t> (MsgHdr->Magic);
    MsgId     = XDR_decode<uint32_t> (MsgHdr->MsgId);

    if (MsgId == POS_DATA_ID)
    {
        PosMsg = (T_PositionMsg *) (Msg + sizeof(T_MsgHdr));
        SenderPosition.Set (
            XDR_decode64<double> (PosMsg->position[X]),
            XDR_decode64<double> (PosMsg->position[Y]),
            XDR_decode64<double> (PosMsg->position[Z]));
        SenderOrientation.Set (
            XDR_decode<float> (PosMsg->orientation[X]),
            XDR_decode<float> (PosMsg->orientation[Y]),
            XDR_decode<float> (PosMsg->orientation[Z]));
    }
    if (MsgMagic == RELAY_MAGIC) // not a local client
    {
        return 0;
    }

    return 0;
}

FGNetFDM last_buf;
static int net_count = 0;
static double first_lat, first_lon, first_alt;
static char _s_buf1[1024+16];
static char _s_buf2[1024+16];
static char _s_buf3[1024+16];

// return count read(server), written(client)
// return 0 no read - just timeout for server
// return -1 ERROR condition
int test_server( SGSocket *sock, int cnt )
{
    char * pbuf1 = _s_buf1;
    char * pbuf2 = _s_buf2;
    char * pbuf3 = _s_buf3;
    int blen = 1024;
    int length = sizeof(FGNetFDM);
    int rlen, update, wlen;
    double clat, clon, calt, llat, llon, lalt;
    double az1,az2,s;
    double faz1,faz2,fs;
    int check_position = 1;
    CLEARGS;
    if (is_server) {
        // server READ
        ZeroMemory(pbuf1, blen);
        ZeroMemory(pbuf2, blen);
        ZeroMemory(pbuf2, blen);
        rlen = sock->read( pbuf1, blen );
        update = 1;
        if (rlen > 0) {
            FGNetFDM * pn = (FGNetFDM *)pbuf1;
            PMYPACKET pp = (PMYPACKET)pbuf3;
            memcpy(pbuf2,pbuf1,rlen); // get a COPY
            memcpy(pbuf3,pbuf1,rlen); // or two

            if (net_byte_order) {
                convert_net_fdm(pn);
                //convert_net_buf(pbuf3, rlen);
            }
            if (is_mp_message(pbuf3,rlen)) {
                globalStg.appendf("%d: %d byte mpmessage - ", cnt, rlen);
            } else if ((rlen == length) && (pn->version == FG_NET_FDM_VERSION)) {
                globalStg.appendf("%d: %d byte FGNetFDM - ", cnt, rlen);
                clat = pn->latitude * SG_RADIANS_TO_DEGREES;
                clon = pn->longitude * SG_RADIANS_TO_DEGREES;
                calt = pn->altitude * SG_METER_TO_FEET;
                if (net_count) {
                    if (check_position &&
                        ((fabs(pn->latitude - last_buf.latitude) > SG_EPSILON) ||
                        (fabs(pn->longitude - last_buf.longitude) > SG_EPSILON) ||
                        (fabs(pn->altitude - last_buf.altitude) > SG_EPSILON) ))
                    {
                        globalStg.appendf("same position");
                        //update = 0;
                    } else {
                        llat = last_buf.latitude * SG_RADIANS_TO_DEGREES;
                        llon = last_buf.longitude * SG_RADIANS_TO_DEGREES;
                        lalt = pn->altitude * SG_METER_TO_FEET;
                        geo_inverse_wgs_84( llat, llon, clat, clon, &az1, &az2, &s);
                        geo_inverse_wgs_84( first_lat, first_lon, clat, clon, &faz1, &faz2, &fs);
                        globalStg.appendf("lat=%f, lon=%f, alt=%f - %s on %d (%s on %d)\n",
                            pn->latitude * SG_RADIANS_TO_DEGREES,
                            pn->longitude * SG_RADIANS_TO_DEGREES,
                            pn->altitude * SG_METER_TO_FEET,
                            get_dist_stg(s), (int)(az1 + 0.5),
                            get_dist_stg(fs), (int)(faz1 + 0.5));
                    }
                } else {
                    first_lat = clat;
                    first_lon = clon;
                    first_alt = calt;
                    globalStg.appendf("first lat=%f, lon=%f, alt=%f\n",
                        first_lat, first_lon, first_alt );
                }
                if (update)
                    last_buf = *pn;
                net_count++;
            } else if ((rlen > sizeof(MYPACKET)) && (pp->magic == MyMagicNum)) {
                globalStg.appendf("Read %d packet bytes...\n", rlen);
                globalStg.appendf("Mag %x, type %d, count %d, stg %s\n",
                    pp->magic, pp->type, pp->count,
                    &pp->data[0] );
            } else {
                globalStg.appendf("Read %d bytes...", rlen);
                OUTGS;
                print_hex( pbuf2, rlen, NULL, 0, 0 );
                CLEARGS;
            }
            if (g_do_echo && is_bi) {
                wlen = sock->write( pbuf1, rlen );
                if (wlen == rlen) {
                    globalStg.appendf("Echoed %d bytes back to sender.", wlen);
                } else {
                    globalStg.appendf("Appears echo FAILED (%d)", wlen);
                }
            }
            if (LENGTHGS) OUTGS;
            return rlen;
        } else if (rlen < 0) {
            if (rlen == -2) {
                globalStg.appendf("Read TIMEOUT (%d)...", rlen );
            } else {
                globalStg.appendf("Read ERROR (%d)...", rlen );
                OUTGS;
                return -1; // READ ERROR
            }
        }
    }
    if (LENGTHGS) OUTGS;
    return 0;
}

// is client
int test_client( SGSocket *sock, int cnt )
{
    char * pbuf1 = _s_buf1;
    char * pbuf2 = _s_buf2;
    char * pbuf3 = _s_buf3;
    int blen = 1024;
    int length = sizeof(FGNetFDM);
    int wlen, rndup, sendsize;
    int check_position = 1;
    PMYPACKET pp = (PMYPACKET)pbuf1;
    pp->magic = MyMagicNum;
    pp->type = 1;
    pbuf1 = &pp->data[0];
    strcpy(pbuf1, "Hello World 1");
    blen = (int)strlen(pbuf1);
    pp->count = blen;
    blen++; // plus the NULL terminator
    strcpy(&pbuf1[blen],"blahblahblah");
    rndup = blen % AlignBytes; // get any remainder
    if (rndup)
        rndup = AlignBytes - rndup;
    sendsize = sizeof(MYPACKET) + blen + rndup;
    wlen = sock->write( (const char *)pp, sendsize );
    CLEARGS;
    if (wlen > 0) {
        globalStg.appendf("Written [%s](%d) returned %d (%d)", pbuf1, blen, wlen, sendsize);
        if (g_do_echo) {
            sleep(50); // give the echoer a chance to do his stuff
            int rlen = sock->read(pbuf3, wlen);
            if (rlen > 0) {
                globalStg.appendf("\nRead %d bytes...", rlen);
                OUTGS;
                print_hex(pbuf3, rlen, NULL, 0, 0 );
                CLEARGS;
            } else {
                globalStg.appendf( "\nRead FAILED (%d)?", rlen);
            }
        }
    } else { // if (rlen == 0) {
        // FAILED IN WRITE
        globalStg.appendf("Write FAILED\n");
        OUTGS;
        return -1; // WRITE ERROR
    }
    if (LENGTHGS) OUTGS;
    return 0;
}

int fg_io_test( SGSocket *sock, int cnt )
{
    if (is_server) {
        return test_server(sock,cnt);
    }
    return test_client(sock,cnt);
}

char * get_dir_stg(void)
{
    if (is_server) {
        if (is_bi)
            return "BI(server)";
        else
            return "IN(server)";
    }
    if (is_bi)
        return "BI(client)";

    return "OUT(client)";
}

void show_socket_error(void)
{
    DWORD dwd = WSAGetLastError();
    globalStg.sprintf("SOCKET ERROR: FAILED with %d!\n", dwd );
    OUTGS;
}

void test_socket ( string host, string port, string style )
{
   int   cnt = 0;
   int res, chr;
   bool  pause = false;

   globalStg.sprintf("Socket to host %s, port %s, style %s",
       host.c_str(), port.c_str(), style.c_str());
   OUTGS;
   SGSocket *sock = new SGSocket(host, port, style);
   if(sock) {
       if( sock->open(g_direction) ) { // if( sock->open(SG_IO_OUT) )
           BOOL val = TRUE;
           if ( setsockopt( sock->getsockHandle(), SOL_SOCKET, SO_REUSEADDR,
               (char *)&val, sizeof(BOOL) ) == 0 )
           {
               sock->nonblock();
               globalStg.sprintf( "SGSocket->open(%s) to %s, on %s, by %s SUCCESS.\n",
                  get_dir_stg(),
                  host.c_str(), port.c_str(), style.c_str() );
               globalStg.appendf("ESC to exit, +/- change delay, p=pause" );
               OUTGS;
               sock->set_valid(true);
             while(sock->isvalid()) {
                if (! pause ) {
                   cnt++;
                   if ((cnt % 20) == 0) {
                       globalStg.sprintf( "R: SGSocket->open(%s) to %s, on %s, by %s.",
                           get_dir_stg(),
                           host.c_str(), port.c_str(), style.c_str() );
                       OUTGS;
                   }

                   res = fg_io_test(sock, cnt);
                   if ((res > 0) && is_server) {
                        sleep(10);
                        continue;
                   } else if (res < 0) {
                        break;
                   }
                }
                chr = kbd_sleep( delay );
                if(chr == 0x1b) {
                   globalStg.sprintf("Exit key ...\n");
                   OUTGS;
                   break;
                } else if(chr == '+') {
                   delay++;
                   globalStg.sprintf("Delay increased to %d ...\n", delay);
                   OUTGS;
                } else if(chr == '-') {
                   if(delay) delay--;
                   globalStg.sprintf("Delay decreased to %d ...\n", delay);
                   OUTGS;
                } else if(chr == 'p') {
                   pause = !pause;
                   globalStg.sprintf("Pause is %s ...\n", (pause ? "On" : "Off"));
                   OUTGS;
                } else if (chr == 'R') {
                    sock->close();
                    delete sock;
                    sock = new SGSocket(host, port, style);
                    if(sock) {
                        if( sock->open(g_direction) ) {
                            val = TRUE;
                            if ( setsockopt( sock->getsockHandle(), SOL_SOCKET, SO_REUSEADDR,
                                (char *)&val, sizeof(BOOL) ) == 0 )
                            {
                                sock->nonblock();
                                globalStg.sprintf( "RESET: SGSocket->open(%s) to %s, on %s, by %s SUCCESS.",
                                    get_dir_stg(),
                                    host.c_str(), port.c_str(), style.c_str() );
                                OUTGS;
                                sock->set_valid(true);
                            } else {
                                goto Reset_Failed;
                            }
                        } else {
                            goto Reset_Failed;
                        }
                    } else {
Reset_Failed:
                        show_socket_error();
                        globalStg.sprintf("RESET FAILED!");
                        OUTGS;
                        if (sock) {
                           sock->close();
                           delete sock;
                           sock = NULL;
                        }
                        break;
                    }
                } else if (chr) {
                   globalStg.sprintf("Unused key [%c]...\n", chr);
                   OUTGS;
                }
             }
          } else {
              show_socket_error();
          }
          if( cnt ) {
              globalStg.sprintf( "Did %d cycles...", cnt );
              OUTGS;
          }
          globalStg.sprintf( "Closing socket...", cnt );
          OUTGS;
          if (sock) {
              sock->close();
              delete sock;
          }
      }
      else
      {
         globalStg.sprintf( "SGSocket->open(%s) to %s, on %s, by %s FAILED!\n",
              get_dir_stg(),
            host.c_str(), port.c_str(), style.c_str() );
         globalStg.appendf( "Note, the socket reader (server) needs to be running first!" );
         OUTGS;
      }
   }
}

void give_help ( char * pname, const char * pip, const char * pport,
                const char * pstyle, int ret)
{
   globalStg.sprintf( "%s [ip=<address>] [-port=<number>] [-style=<type> [-client] [-pause=<secs>]\n", pname );
   globalStg.appendf( "Default ip=%s, port=%s, type=%s, pause=%d, as server.\n", pip, pport, pstyle, delay );
   globalStg.appendf( "Style must be either 'tcp', or 'udp'\n");
   globalStg.appendf( "Aborting ...(%d)\n", ret );
   OUTGS;
   exit(ret);
}

int is_all_number( char * cp )
{
    size_t len = strlen(cp);
    size_t i;
    for (i = 0; i < len; i++) {
        if ( !ISNUM(cp[i]) )
            return 0;
    }
    return 1;
}

void show_endian(void)
{
    union {
        short s;
        char c[sizeof(short)];
    }un;
    un.s = 0x0102;
    if (sizeof(short) == 2) {
        if ((un.c[0] == 1) && (un.c[1] == 2)) {
            globalStg.sprintf("Machine is big-endian");
        } else if ((un.c[0] == 2) && (un.c[1] == 1)) {
            globalStg.sprintf("Machine is little-endian");
        } else {
            globalStg.sprintf("Machine is UNKNOWN!");
        }
   } else {
        globalStg.sprintf("sizeof(short) = %d, NOT 2!!!\n", sizeof(short));
   }
    OUTGS;
}

#ifdef TEST_FGMP_MESSAGE
extern int test_fgmp_message(char * file);
int main(int argc, char *argv[])
{
    int i, iret;
    char * file = sampinraw;
    for (i = 1; i < argc; i++)
        file = argv[i];
    iret = test_fgmp_message(file);
    return iret;
}
#else // !#define TEST_FGMP_MESSAGE

int main(int argc, char **argv)
{
    string host = DEF_HOST;
    string port = DEF_PORT;
    string style = DEF_TYPE;
    globalStg.sprintf( "Running %s, compile %s, at %s", argv[0], __DATE__, __TIME__ );
#if USE_SG_LOG
    sglog().setLogLevels( SG_ALL, SG_INFO );
#endif
    OUTGS;
    show_endian();

   int   i;
   for( i = 1; i < argc; i++) {
      char * oarg = argv[i];
      char * arg = oarg;
      globalStg.sclear(); /* clear any previous */
      if(arg && *arg) {
         if(*arg == '-') {
            do { arg++; } while (*arg == '-');
            if( strncmp(arg,"ip=", 3) == 0 ) {
               arg += 3;
               if(*arg == 0)
                  goto Cmd_ERROR;
               host = arg;
               globalStg.appendf("Set HOST to [%s]", host.c_str());
            } else if( strncmp(arg, "port=", 5) == 0 ) {
               arg += 5;
               if(*arg == 0)
                  goto Cmd_ERROR;
               if(!ISNUM(*arg))
                  goto Cmd_ERROR;
               port = arg;
               globalStg.appendf("Set PORT to [%s]", port.c_str());
            } else if( strncmp(arg, "style=", 6) == 0 ) {
               arg += 6;
               if (strcmp(arg,"tcp") == 0) {
                    style = arg;
               } else if (strcmp(arg,"udp") == 0) {
                    style = arg;
               } else {
                   globalStg.appendf( "Style must be either 'tcp', or 'udp'\n");
                   goto Cmd_ERROR;
               }
               globalStg.appendf("Set TYPE to [%s]", style.c_str());
            } else if( strcmp(arg,"client") == 0 ) {
                g_direction = SG_IO_OUT;
                g_is_server = 0;
                globalStg.appendf("set to run as client...");
            } else if( strncmp(arg, "pause=", 6) == 0 ) {
                arg += 6;
                if (is_all_number(arg)) {
                    delay = atoi(arg);
                    globalStg.appendf("Set delay to %d seconds...", delay);
                } else {
                    globalStg.appendf("ERROR: Argument [%s] can only be a number of seconds!\n", oarg);
                    goto Cmd_ERROR;
                }
            } else if(( strncmp(arg, "?", 1) == 0 )||
                ( strncmp(arg, "h", 1) == 0 )) {
                 give_help( argv[0], host.c_str(), port.c_str(), style.c_str(), 0 );
            } else {
                 goto Cmd_ERROR;
            }
         } else {
Cmd_ERROR:
            globalStg.appendf("ERROR: Unknown argument [%s] ... aborting ...", argv[i]);
            OUTGS;
            give_help( argv[0], host.c_str(), port.c_str(), style.c_str(), 1 );
         }
         if (LENGTHGS) OUTGS;
      }
   }

   test_socket (host, port, style);
   
	return 0;
}

#endif // #define TEST_FGMP_MESSAGE y/n


#if 0
int main_test(int argc, char **argv)
{
    int iret = 0;

    double gs = 3 * SGD_DEGREES_TO_RADIANS;
    double alt = 1500.0 * SG_FEET_TO_METER;
    double at = atan(gs);
    double at1 = at * 1000.0;
    double dist;
    printf( "atan of 3 degrees is %f (%f K)\n", at, at1 );

    alt = 2500.0 * SG_FEET_TO_METER;
    dist = alt / at1;
    printf("From 2500 feet, at 3 degrees is dist %f Km\n",
        dist );
    alt = 2000.0 * SG_FEET_TO_METER;
    dist = alt / at1;
    printf("From 2000 feet, at 3 degrees is dist %f Km\n",
        dist );
    alt = 1500.0 * SG_FEET_TO_METER;
    dist = alt / at1;
    printf("From 1500 feet, at 3 degrees is dist %f Km\n",
        dist );
    alt = 1000.0 * SG_FEET_TO_METER;
    dist = alt / at1;
    printf("From 1000 feet, at 3 degrees is dist %f Km\n",
        dist );
    alt = 500.0 * SG_FEET_TO_METER;
    dist = alt / at1;
    printf("From  500 feet, at 3 degrees is dist %f Km\n",
        dist );

    //iret = fgio_main(argc,argv);

    return iret;
}
#endif // 0

// eof - fgio.cxx
