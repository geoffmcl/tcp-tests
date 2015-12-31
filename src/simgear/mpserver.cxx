/*******************mpserver.cxx*****************/
// 2011-03-31 - Build a UPD server to receive FGMP messages
//
// Written by Geoff R. McLane, started March 2011.
//
// Copyright (C) 2011 - ????  Geoff R. McLane  
//    - http://geoffair.org - reports@geoffair.info -
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id$

#include "config.h" /* always the FIRST include */
#if (defined(_MSC_VER) && !defined(_CONFIG_H_MSVC_))
#error "ERROR: Copy config.h-msvc to config.h for Windows compile!"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef _MSC_VER
#include <ctype.h> /* for toupper(), ... */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif // !_MSC_VER

#include "winsockerr.cxx"
#include "fgmpmsg.hxx"
#include <string>
static FGMP_MsgBuf msgbuf;
static uint32_t Magic, Version, MsgId, MsgLen, ReplyPort, client_id;
typedef struct tagMP {
    int id;
    struct sockaddr_in clientaddr;
}MP, * PMP;
typedef std::map<std::string,PMP> MPMap;
MPMap m_MPMap; // store of clients
static MP m_mp;
static size_t total_clients = 0;

static char * modName = (char *)"MP Server";

/* Server's default port number, listen at 3333 */
#define SERVPORT 3333
#ifndef DEF_PAUSE
#define DEF_PAUSE 160 /* default 160 ms pause */
#endif
#ifndef MY_MX_DATA
#define MY_MX_DATA 2048
#endif

static char hostName[264];
static int portNumber;
static int use_use_ip = 0;
static int do_an_echo = 0; /* MP server ONLY relays to OTHER clients */
static int no_blocking = 1;

static int verbosity = 0;
#define VERB1 (verbosity >= 1)
#define VERB2 (verbosity >= 2)
#define VERB5 (verbosity >= 5)
#define VERB9 (verbosity >= 9)

static char data_buffer[MY_MX_DATA];
static char hex_buffer[MY_MX_DATA];

/* just some statistics */
static time_t cycle_time;
static time_t last_info_time = 0;
static uint64_t cycles = 0;
static uint64_t cyc_per_sec = 0;
static uint64_t max_cyc_per_sec = 0;

/* pause the running */
static time_t pause_time;
static int is_paused = 0;

// when to show a message
static time_t last_time = 0;
static time_t nxt_msg_time = 20; /* show working dot each 20 seconds */

// stats kept
static uint64_t sends_done, sent_bytes, recvs_done, recv_bytes;
static uint64_t l_sends_done, l_sent_bytes, l_recvs_done, l_recv_bytes;

static char * tempinraw = (char *)"tempin-raw.bin";
//static char * tempinxdr = (char *)"tempin-xdr.bin";
static FILE * fTempRaw = 0;
//static FILE * fTempXdr = 0;

void WriteRawData(char * bufptr, int rlen)
{
    if (fTempRaw && (rlen > 0)) {
        size_t wlen = fwrite(bufptr,1,rlen,fTempRaw);
        if (wlen != rlen) {
            printf("%s - WRITE FAILED! Abandoning raw output file!\n", modName);
            fclose(fTempRaw);
            fTempRaw = NULL;
            return;
        }
        int i = rlen;
        int off = 0;
        while (i % 16) {
            hex_buffer[off++] = '=';
            i++;
        }
        i = 16;
        while(i--)
            hex_buffer[off++] = '=';
        fwrite(hex_buffer,1,off,fTempRaw);
    }
}

void show_stats(void)
{
    char prev[256];
    char * ioinfo = prev;
    time_t ct = time(0);
    strcpy(ioinfo,"(no io change!)");
    if (cyc_per_sec) {
        printf("Running at %" PRIu64 " cycles per second. timeout %d ms\n",
            cyc_per_sec, m_delay_time );
    }
    if (sends_done || recvs_done) {
        time_t diff = ct - last_info_time;
        if ((sends_done != l_sends_done)||(sent_bytes != l_sent_bytes)||
            (recvs_done != l_recvs_done)||(recv_bytes != l_recv_bytes)) {
            sprintf(ioinfo,"(Prev %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %d secs)",
                l_sends_done, l_sent_bytes, l_recvs_done, l_recv_bytes, diff );
        }
        printf("Done %" PRIu64 " sends, %" PRIu64 " bytes, %" PRIu64 " recvs, %" PRIu64 " bytes %s\n",
            sends_done, sent_bytes, recvs_done, recv_bytes, ioinfo );
        /* update the stat block */
        last_info_time = ct;
        l_sends_done = sends_done;
        l_sent_bytes = sent_bytes;
        l_recvs_done = recvs_done;
        l_recv_bytes = recv_bytes;
    }
    if (total_clients > 0)
        printf("Total clients %d, active = %d\n", total_clients, m_MPMap.size());
}

static void show_help(void)
{
    printf("KEY HELP\n");
    printf(" p     = Toggle PAUSE. Currently %s\n", (is_paused ? "On" : "OFF") );
    printf(" 01259 = Set verbosity level. Now=%d\n", verbosity );
    printf(" t/T   = Decrease/Increase cycle delay time by 10 ms. Now=%d\n", m_delay_time);
    printf(" ESC   = Begin Exit...\n");
    printf("STATS - info\n");
    show_stats();
}

static int check_key(void)
{
    int chr = 0;
#ifdef GOT_KEYBOARD_TEST
    chr = test_for_input();
    if (chr) {
        switch (chr)
        {
        case 0x1b:
            printf("MP Server - ESC key - commence exit\n");
            return 1;
            break;
        case '?':
            show_help();
            break;
        case 'p':
            pause_time = time(0);
            is_paused = (is_paused ? 0 : 1);
            printf("p - Toggled PAUSE. Currently %s\n",
                (is_paused ? "On" : "OFF") );
            break;
        case 't':
            if (m_delay_time > 10)
                m_delay_time -= 10;
            else
                m_delay_time = 0;
            printf("t - Decreased cycle delay to %d ms\n", m_delay_time);
            break;
        case 'T':
            m_delay_time += 10;
            printf("t - Increased cycle delay to %d ms\n", m_delay_time);
            break;
        case '0':
        case '1':
        case '2':
        case '5':
        case '9':
            verbosity = (chr - '0');
            printf("%c - Set verbosity to %d\n", chr, verbosity);
            break;
        default:
            printf("Unused key input...\n");
            break;
        }
        chr = 0;
    }
#endif // #ifdef GOT_KEYBOARD_TEST
    return chr;
}

void ProcessChatMsg(FGMP_MsgBuf *msgBuf, PMP pmp)
{
    printf("MP Server - ProcessCharMsg\n");
}

bool _verifyProperties(const xdr_data_t* data, const xdr_data_t* end)
{
    // using namespace simgear;
    const xdr_data_t* xdr = data;
    while (xdr < end) {
        unsigned int id = XDR_decode_uint32(*xdr);
        const struct IdPropertyList* plist = _findProperty(id);
    
        if (plist) {
            xdr++;
            // How we decode the remainder of the property depends on the type
            switch (plist->type) {
                case Typ_INT:
                case Typ_BOOL:
                case Typ_LONG:
                    xdr++;
                    break;
                case Typ_FLOAT:
                case Typ_DOUBLE:
                    {
                        float val = XDR_decode_float(*xdr);
                        if (_isnan(val)) {
                            printf("MP Server - ERROR: Float ISNAN!\n" );
                            return false;
                        }
                        xdr++;
                    }
                    break;
                case Typ_STRING:
                case Typ_UNSPECIFIED:
                    {
                        // String is complicated. It consists of
                        // The length of the string
                        // The string itself
                        // Padding to the nearest 4-bytes.
                        // XXX Yes, each byte is padded out to a word! Too late
                        // to change...
                        uint32_t length = XDR_decode_uint32(*xdr);
                        xdr++;
                        // Old versions truncated the string but left the length
                        // unadjusted.
                        if (length > MAX_TEXT_SIZE)
                            length = MAX_TEXT_SIZE;
                        xdr += length;
                        // Now handle the padding
                        while ((length % 4) != 0)
                        {
                            xdr++;
                            length++;
                        }
                    }
                    break;
                default:
                    printf("MP Server - ERROR: Unknown Prop type %d\n", id );
                    return false;
                    //xdr++;
                    break;
            }            
        } else {
            // give up; this is a malformed property list.
            printf("MP Server - ERROR: malformed property list.\n" );
            return false;
        }
    }
    return true;
}

int ProcessPosMsg(FGMP_MsgBuf *msgBuf, PMP pmp)
{
    int iret = 0;
    unsigned int i;
    const T_PositionMsg* PosMsg = msgBuf->posMsg();
    FGExternalMotionData motionInfo;
    motionInfo.time = XDR_decode_double(PosMsg->time);
    motionInfo.lag = XDR_decode_double(PosMsg->lag);
    for (i = 0; i < 3; i++)
        motionInfo.position(i) = XDR_decode_double(PosMsg->position[i]);

    if (VERB9)
        printf("MP Server - ProcessPosMsg\n");

    SGVec3f angleAxis;
    for (i = 0; i < 3; i++)
        angleAxis(i) = XDR_decode_float(PosMsg->orientation[i]);
    motionInfo.orientation = SGQuatf::fromAngleAxis(angleAxis);
    for (i = 0; i < 3; i++)
        motionInfo.linearVel(i) = XDR_decode_float(PosMsg->linearVel[i]);
    for (i = 0; i < 3; i++)
        motionInfo.angularVel(i) = XDR_decode_float(PosMsg->angularVel[i]);
    for (i = 0; i < 3; i++)
        motionInfo.linearAccel(i) = XDR_decode_float(PosMsg->linearAccel[i]);
    for (i = 0; i < 3; i++)
        motionInfo.angularAccel(i) = XDR_decode_float(PosMsg->angularAccel[i]);
    const xdr_data_t* xdr = msgBuf->properties();   // start of properties
    const xdr_data_t* xdrend = msgBuf->propsRecvdEnd(); // end of properties
    if (PosMsg->pad != 0) {
        if (_verifyProperties(&PosMsg->pad, xdrend))
            xdr = &PosMsg->pad;
        else if (!_verifyProperties(xdr, xdrend))
            goto noprops;
  }
  while (xdr < msgBuf->propsRecvdEnd()) {
      // First element is always the ID
      unsigned int id = XDR_decode_uint32(*xdr);
      //cout << pData->id << " ";
      xdr++;
    
      // Check the ID actually exists and get the type
      const IdPropertyList* plist = _findProperty(id);
      if (plist)
      {
          FGPropertyData* pData = new FGPropertyData;
          pData->id = id;
          pData->type = plist->type;
          // How we decode the remainder of the property depends on the type
          switch (pData->type)
          {
          case Typ_INT:
          case Typ_BOOL:
          case Typ_LONG:
              pData->int_value = XDR_decode_uint32(*xdr);
              xdr++;
              //cout << pData->int_value << "\n";
              break;
          case Typ_FLOAT:
          case Typ_DOUBLE:
              pData->float_value = XDR_decode_float(*xdr);
              xdr++;
              //cout << pData->float_value << "\n";
              break;
          case Typ_STRING:
          case Typ_UNSPECIFIED:
              {
                  // String is complicated. It consists of
                  // The length of the string
                  // The string itself
                  // Padding to the nearest 4-bytes.
                  uint32_t length = XDR_decode_uint32(*xdr);
                  xdr++;
                  //cout << length << " ";
                  // Old versions truncated the string but left the length unadjusted.
                  if (length > MAX_TEXT_SIZE)
                      length = MAX_TEXT_SIZE;
                  pData->string_value = new char[length + 1];
                  //cout << " String: ";
                  for (unsigned i = 0; i < length; i++)
                  {
                      pData->string_value[i] = (char) XDR_decode_int8(*xdr);
                      xdr++;
                      //cout << pData->string_value[i];
                  }
                  pData->string_value[length] = '\0';
                  // Now handle the padding
                  while ((length % 4) != 0)
                  {
                      xdr++;
                      length++;
                      //cout << "0";
                  }
                  //cout << "\n";
              }
              break;

          default:
              pData->float_value = XDR_decode_float(*xdr);
              // SG_LOG(SG_NETWORK, SG_DEBUG, "Unknown Prop type " << pData->id << " " << pData->type);
              xdr++;
              break;
          }
          motionInfo.properties.push_back(pData);
      } else {
          // We failed to find the property. We'll try the next packet immediately.
          T_MsgHdr* MsgHdr = msgBuf->msgHdr();
          printf("MP Server - ERROR: message from [%s] has unknown property id %d\n",
              MsgHdr->Callsign, id);
          iret = 1;
          break;
      }
  }
noprops:
  return iret;
}

int run_udpserver(SOCKET sd)
{
    int iret = 0;
    int repeat = 1;
    char * bufptr = data_buffer;
    int buflen = sizeof(data_buffer);
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    int i, rlen, slen;
    bufptr = (char *)&msgbuf;
    // buflen = sizeof(msgbuf);
    buflen = MAX_PACKET_SIZE;   /* go for the MAX packet size */
    // do_an_echo = 0;
    client_id = 0; // start client ID count

    printf("MP Server - Doing recvfrom()... %s... %s...\n",
        (no_blocking ? "nonblocking" : "blocking until received"),
#ifdef GOT_KEYBOARD_TEST
        "ESC key to exit"
#else
        "Ctrl+c to exit"
#endif
        );

    fTempRaw = fopen(tempinraw,"wb");
    if (!fTempRaw) {
        printf("MP Server - WARNING: Failed to create file [%s]\n", tempinraw);
        if (check_key_sleep(10,modName))
            return 1;
    }
    show_help();
    printf("MP Server - Entering repeat loop. Verbosity = %d.\n", verbosity);
    last_info_time = time(0);
    max_cyc_per_sec = 0;
    cycles = 0;
    cycle_time = time(0);
    while (repeat) { /* repeat */
        /* ========================================================== */
        cycles++; /* bump the cycles */

        if (time(0) != cycle_time) {
            cycle_time = time(0);
            cyc_per_sec = cycles;
            if (cycles > max_cyc_per_sec)
                max_cyc_per_sec = cycles;
            cycles = 0;
        }
        if ((time(0) - last_time) > nxt_msg_time) {
            show_stats();
            last_time = time(0); /* set for NEXT message time */
        }

        if (check_key())
            break;

        /* Use the recvfrom() function to receive the data */
        rlen = recvfrom(sd, bufptr, buflen, 0, (struct sockaddr *)&clientaddr, &clientaddrlen);
        if ( SERROR(rlen) ) {
            if (no_blocking)
            {
#ifdef _MSC_VER
                if (WSAGetLastError() == WSAEWOULDBLOCK) {
                    clock_wait(m_delay_time);
                    continue;
                }
#else /* !_MSC_VER */
                if (errno == EAGAIN) {
                    clock_wait(m_delay_time);
                    continue;
                }
#endif /* _MSC_VER y/n */
            }
            PERROR("MP Server - recvfrom() error");
            iret = 1;
            break;
        }

        // stats kept
        recvs_done++;
        recv_bytes += rlen;
        WriteRawData(bufptr,rlen);
        if (VERB1)
            printf("MP Server - recvfrom() OK IP %s, port %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

        if (VERB9)
            printf("%s - received message, len %d\n", modName, rlen);
        PMP pmp = new MP;
        T_MsgHdr* MsgHdr = msgbuf.msgHdr();
        Magic       = XDR_decode_uint32 (MsgHdr->Magic);
        Version     = XDR_decode_uint32 (MsgHdr->Version);
        MsgId       = XDR_decode_uint32 (MsgHdr->MsgId);
        MsgLen      = XDR_decode_uint32 (MsgHdr->MsgLen);
        ReplyPort   = XDR_decode_uint32 (MsgHdr->ReplyPort);
        MsgHdr->Callsign[MAX_CALLSIGN_LEN -1] = '\0';
        int msg_valid = 0;
        MsgHdr->MsgLen = MsgLen; // ensure 'correct' length
        // invalid if NOT MSG_MAGIC or RELAY_MAGIC
        if ((Magic != MSG_MAGIC)&&(Magic != RELAY_MAGIC)) {
            printf("MP Server - message has invalid magic [%d] vs ", Magic);
            if (Magic != MSG_MAGIC)
                printf("[%d]\n", MSG_MAGIC);
            else
                printf("[%d]\n", RELAY_MAGIC);
            if (check_key())
                break;
            clock_wait(m_delay_time);
            continue;
        }
        if (Version != PROTO_VER) {
            printf("MP Server - message has invalid protocol [%d] vs [%d]\n", Version, PROTO_VER);
            if (check_key())
                break;
            clock_wait(m_delay_time);
            continue;
        }
        if (MsgLen != rlen) {
            printf("MP Server - message from [%s] has invalid length!\n", MsgHdr->Callsign );
            if (check_key())
                break;
            clock_wait(m_delay_time);
            continue;
        }
        memcpy(&pmp->clientaddr, &clientaddr, sizeof(clientaddr));
        switch (MsgId)
        {
        case CHAT_MSG_ID:
            ProcessChatMsg(&msgbuf, pmp);
            break;
        case POS_DATA_ID:
            if ( ProcessPosMsg(&msgbuf, pmp) ) {
                printf("MP Server - PosMsg message from [%s] has invalid entry!\n", MsgHdr->Callsign );
                if (check_key())
                    break;
                clock_wait(m_delay_time);
                continue;
            } else {
                msg_valid = 1;
            }
            break;
        case UNUSABLE_POS_DATA_ID:
        case OLD_OLD_POS_DATA_ID:
        case OLD_PROP_MSG_ID:
        case OLD_POS_DATA_ID:
            printf("MP Server - message from [%s] has obsolete ID %d!\n",
                MsgHdr->Callsign, MsgId );
            break;
        default:
            printf("MP Server - message from [%s] has UNKNOWN ID %d!\n",
                MsgHdr->Callsign, MsgId );
            break;
        }
        if (msg_valid) {
            std::string s = MsgHdr->Callsign;
            if (0 < m_MPMap.count(s)) {
                // this client already exists
                if (VERB9)
                    printf("MP Server - Client [%s] EXISTS.\n", s.c_str() );
            } else {
                // a NEW client
                client_id++;
                pmp->id = client_id;
                //memcpy(&pmp->clientaddr, &clientaddr, sizeof(clientaddr));
                m_MPMap[s] = pmp;
                total_clients++;
                printf("MP Server - Added client [%s]!\n", s.c_str() );
            }
        } else
            delete pmp;
        // break; // exit after ONE
        if (do_an_echo) {
            /* Send a reply by using the sendto() function. */
            /* In this example, the system echoes the received */
            /* data back to the client. */
            MsgHdr->Magic = XDR_encode_uint32(RELAY_MAGIC);

            slen = sendto(sd, bufptr, rlen, 0, (struct sockaddr *)&clientaddr, clientaddrlen);
            if( SERROR(slen) ) {
                PERROR("MP Server - sendto() error");
                iret = 1;
                break;
            }

            // stats kept
            sends_done++;
            sent_bytes += slen;

            if (VERB5)
                printf("MP Server - sendto() sent %d bytes [%s]\n", slen, bufptr);
        }
        // repeat--;
        /* ========================================================== */
    } /* while 'repeat', or ESC key if keyboard */

    return iret;
}

int get_host_information(char * in_host_name)
{
    // Declare variables
    char* IP = NULL;
    hostent* Host = NULL;
    unsigned int addr;
    char * host_name = in_host_name;
    if (( host_name == NULL ) || ( host_name[0] == 0 )) {
        host_name = (char *)"localhost";
    }
    // If an alpha name for the host, use gethostbyname()
    if (isalpha(host_name[0])) {   /* host address is a name */
        // if hostname terminated with newline '\n', remove and zero-terminate 
        if (host_name[strlen(host_name)-1] == '\n')
            host_name[strlen(host_name)-1] = '\0'; 
        Host = gethostbyname(host_name);
    } else  {
        // If not, get host by addr (assume IPv4)
        addr = inet_addr(host_name);
        Host = gethostbyaddr((char *)&addr, 4, AF_INET);
    }
#ifdef _MSC_VER
    if ((WSAGetLastError() != 0)||(Host == NULL)) {
        PERROR("ERROR: gethost... FAILED!");
        return -1;
    }
#else // !_MSC_VER
    if ( Host == NULL ) {
        PERROR("ERROR: gethost... FAILED!");
        return -1;
    }
#endif // _MSC_VER y/n
    // The Host structure can now be used to
    // access information about the host
    // Get the local host information
    IP = inet_ntoa (*(struct in_addr *)*Host->h_addr_list);
    if (IP == NULL)
        return -1; /* FAILED */
    // ELSE, have resolved Host name
    localHost = Host;
    localIP = strdup(IP);
    use_use_ip = 1; /* can use the user supplied IP */
    return 0; // success
}

void cmd_help(char * name)
{
    //char * cp = get_to_stg();
    printf("HELP\n");
    printf(" -?    = This help, and exit 0\n");;
    printf(" -h <host> = Set the HOST name, or IP. Def=<none>\n");
    printf(" -p <port> = Set the PORT number. Def=%d\n", PORTNUM );
    printf(" -v[nn]    = Increment/Set verbosity. Def=%d\n", verbosity );
    //printf("Current timeout = %s\n", cp);
}

void test_sizes(void)
{
    printf("Sizeof uint32_t    = %d\n", sizeof(uint32_t)   );
    printf("Sizeof uint64_t    = %d\n", sizeof(uint64_t)   );
    printf("Sizeof long long   = %d\n", sizeof(long long)  );
    printf("Sizeof xdr_data_t  = %d\n", sizeof(xdr_data_t) );
    printf("Sizeof xdr_data2_t = %d\n", sizeof(xdr_data2_t));
    printf("Sizeof a float     = %d\n", sizeof(float)      );
    printf("Sizeof a double    = %d\n", sizeof(double)     );
    exit(0);
}

void app_init(void)
{
    portNumber = SERVPORT;
    hostName[0] = 0;
    l_sends_done = l_sent_bytes = l_recvs_done = l_recv_bytes = 0;
    sends_done = sent_bytes = recvs_done = recv_bytes = 0;
    last_info_time = time(0);
    max_cyc_per_sec = 0;
    cycles = 0;
    cycle_time = time(0);
}

int main(int argc, char *argv[])
{
    /* Variable and structure definitions. */
    int iret = 0;
    SOCKET sd;
    struct sockaddr_in serveraddr;
    socklen_t serveraddrlen = sizeof(serveraddr);
    int i, status;
    // test_sizes();
    printf("%s - compiled on %s, at %s\n", modName, __DATE__, __TIME__);
    sock_init(); // need some init for windows
    app_init();  // setup some variables
	// Validate the command line
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (*arg == '-') {
            arg++;
            while(*arg == '-') arg++;
            int c = *arg;
            switch (c) {
            case '?':
                cmd_help(argv[0]);
                goto Server_Exit;
                break;
            case 'h':
                if ((i + 1) < argc) {
                    i++;
                    strcpy(hostName,argv[i]);
                } else {
                    goto Bad_Arg;
                }
                break;
            case 'p':
                if ((i + 1) < argc) {
                    i++;
                    portNumber = atoi(argv[i]);
                } else {
                    goto Bad_Arg;
                }
                break;
            case 'v':
                arg++;
                if (*arg) {
                    // expect digits
                    if (is_digits(arg)) {
                        verbosity = atoi(arg);
                    } else if (*arg == 'v') {
                        verbosity++; /* one inc for first */
                        while(*arg == 'v') {
                            verbosity++;
                            arg++;
                        }
                    } else
                        goto Bad_Arg;
                } else
                    verbosity++;
                printf("SERVER: Set verbosity to %d\n", verbosity);
                break;
            default:
Bad_Arg:
                fprintf(stderr,"ERROR: Invalid command line! arg [%s] unknown\n", argv[i] );
                iret = 1;
                goto Server_Exit;
                break;
            }
        } else {
            fprintf(stderr,"ERROR: Invalid command line! bare arg [%s] unknown\n", argv[i] );
            iret = 1;
            goto Server_Exit;
        }
	}

	// At this point, the command line has been validated.
    if (hostName[0]) { /* if USER supplied name/IP address */
        status = get_host_information(hostName); /* may use supplied HOST */
        if (status == -1) {
            printf("ERROR: Failed on host name %s\n",hostName);
            iret = 1;
            goto Server_Exit;
        }
    }

    /* The socket() function returns a socket */
    /* get a socket descriptor */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if( SERROR(sd) ) {
        PERROR("MP Server - socket() error");
        iret = 1;
        goto Server_Exit;
    }

    /* set non blocking, if desired */
    if (no_blocking) {
        status = setnonblocking(sd);
        printf("MP Server - set non-blocking - status = %d\n", status );
    }

    printf("MP Server - begin %s - socket() is OK sd=%u (0x%X)",
        get_datetime_str(), sd, sd );
    /* bind to address */
    memset(&serveraddr, 0x00, serveraddrlen);
    serveraddr.sin_family = AF_INET;
    if (use_use_ip && localHost && localIP) {
        memcpy((char *)&serveraddr.sin_addr,(char *)localHost->h_addr,localHost->h_length);
        printf(" setting IP %s\n", localIP);
    } else {
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
        printf(" setting IP INADDR_ANY\n");
    }
    serveraddr.sin_port = htons(portNumber);
    
    printf("MP Server - try to bind() IP %s, port %d, struct len %d\n", inet_ntoa(serveraddr.sin_addr),
        ntohs(serveraddr.sin_port), serveraddrlen );
        
    status = bind(sd, (struct sockaddr *)&serveraddr, serveraddrlen);
    if( SERROR(status) ) {
        PERROR("MP Server - bind() error");
        iret = 1;
        goto Server_Exit;
    }

    printf("MP Server - bind() OK IP %s, port %d\n",
        inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port));

    iret = run_udpserver(sd);

Server_Exit:

    /* close() the socket descriptor. */
    printf("MP Server - close and exit %d... %s\n", iret, get_datetime_str());
    if (sd && !SERROR(sd))
        SCLOSE(sd);

    sock_end();
    return iret;
}

// eof - udpserver.c
