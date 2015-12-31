// fgmpmsg.hxx
// Multiplayer Messages
#ifndef _FGMPMSG_HXX_
#define _FGMPMSG_HXX_

#include <vector>
#include <map>
// *** THE SIMGEAR MATHS ***
#define NO_OPENSCENEGRAPH_INTERFACE
#include <simgear/math/SGMathFwd.hxx>
#include <simgear/math/SGLimits.hxx>
#include <simgear/math/SGGeodesy.hxx>
#include <simgear/math/SGVec2.hxx>
#include <simgear/math/SGVec3.hxx>
#include <simgear/math/SGGeod.hxx>
#include <simgear/math/SGQuat.hxx>

#ifndef ENDIAN_TEST
#define ENDIAN_TEST
/* simple ENDIAN test */
static const int _s_EndianTest = 1;
#define IsLittleEndian (*((char *) &_s_EndianTest ) != 0)
#define IsBigEndian    (*((char *) &_s_EndianTest ) == 0)
#endif

#ifndef _BINARY_SWAPS
#define _BINARY_SWAPS
static uint16_t bswap_16(uint16_t x) {
    x = (x >> 8) | (x << 8);
    return x;
}
static uint32_t bswap_32(uint32_t x) {
    x = ((x >>  8) & 0x00FF00FFL) | ((x <<  8) & 0xFF00FF00L);
    x = (x >> 16) | (x << 16);
    return x;
}
static uint64_t bswap_64(uint64_t x) {
    x = ((x >>  8) & 0x00FF00FF00FF00FFLL) | ((x <<  8) & 0xFF00FF00FF00FF00LL);
    x = ((x >> 16) & 0x0000FFFF0000FFFFLL) | ((x << 16) & 0xFFFF0000FFFF0000LL);
    x = (x >> 32) | (x << 32);
    return x;
}
#endif // _BINARY_SWAPS

// magic value for messages
const uint32_t MSG_MAGIC = 0x46474653;  // "FGFS"
// magic value for RELAY message
const uint32_t RELAY_MAGIC = 0x53464746;    // GSGF
// protocol version
const uint32_t PROTO_VER = 0x00010001;  // 1.1

// Message identifiers
#define CHAT_MSG_ID             1
#define UNUSABLE_POS_DATA_ID    2
#define OLD_OLD_POS_DATA_ID     3
#define OLD_POS_DATA_ID         4
#define OLD_PROP_MSG_ID         5
#define RESET_DATA_ID           6
#define POS_DATA_ID             7

// XDR demands 4 byte alignment, but some compilers use8 byte alignment
// so it's safe to let the overall size of a network message be a 
// multiple of 8!
#define MAX_CALLSIGN_LEN        8
#define MAX_CHAT_MSG_LEN        256
#define MAX_MODEL_NAME_LEN      96
#define MAX_PROPERTY_LEN        52

#define MAX_PACKET_SIZE 1200
#define MAX_TEXT_SIZE 128

typedef uint32_t    xdr_data_t;      /* 4 Bytes */
typedef uint64_t    xdr_data2_t;     /* 8 Bytes */

#define SWAP32(arg) IsLittleEndian ? bswap_32(arg) : arg
#define SWAP64(arg) IsLittleEndian ? bswap_64(arg) : arg

enum FGMP_Type {
    Typ_NONE = 0, /**< The node hasn't been assigned a value yet. */
    Typ_ALIAS, /**< The node "points" to another node. */
    Typ_BOOL,
    Typ_INT,
    Typ_LONG,
    Typ_FLOAT,
    Typ_DOUBLE,
    Typ_STRING,
    Typ_UNSPECIFIED,
    Typ_EXTENDED, /**< The node's value is not stored in the property;
               * the actual value and type is retrieved from an
               * SGRawValue node. This type is never returned by @see
               * SGPropertyNode::getType.
               */
    // Extended properties
    Typ_VEC3D,
    Typ_VEC4D
};


// Header for use with all messages sent 
struct T_MsgHdr {
    xdr_data_t  Magic;                  // Magic Value
    xdr_data_t  Version;                // Protocoll version
    xdr_data_t  MsgId;                  // Message identifier 
    xdr_data_t  MsgLen;                 // absolute length of message
    xdr_data_t  ReplyAddress;           // (player's receiver address
    xdr_data_t  ReplyPort;              // player's receiver port
    char Callsign[MAX_CALLSIGN_LEN];    // Callsign used by the player
};

// Chat message 
struct T_ChatMsg {
    char Text[MAX_CHAT_MSG_LEN];       // Text of chat message
};

// Position message
struct T_PositionMsg {
    char Model[MAX_MODEL_NAME_LEN];    // Name of the aircraft model

    // Time when this packet was generated
    xdr_data2_t time;
    xdr_data2_t lag;

    // position wrt the earth centered frame
    xdr_data2_t position[3];
    // orientation wrt the earth centered frame, stored in the angle axis
    // representation where the angle is coded into the axis length
    xdr_data_t orientation[3];

    // linear velocity wrt the earth centered frame measured in
    // the earth centered frame
    xdr_data_t linearVel[3];
    // angular velocity wrt the earth centered frame measured in
    // the earth centered frame
    xdr_data_t angularVel[3];

    // linear acceleration wrt the earth centered frame measured in
    // the earth centered frame
    xdr_data_t linearAccel[3];
    // angular acceleration wrt the earth centered frame measured in
    // the earth centered frame
    xdr_data_t angularAccel[3];
    // Padding. The alignment is 8 bytes on x86_64 because there are
    // 8-byte types in the message, so the size should be explicitly
    // rounded out to a multiple of 8. Of course, it's a bad idea to
    // put a C struct directly on the wire, but that's a fight for
    // another day...
    xdr_data_t pad;
};

struct FGPropertyData {
  unsigned int id;
  
  // While the type isn't transmitted, it is needed for the destructor
  FGMP_Type type;
  union { 
    int int_value;
    double float_value;
    char* string_value;
  }; 
  
  ~FGPropertyData() {
    if ((type == Typ_STRING) || (type == Typ_UNSPECIFIED))
    {
      delete [] string_value;
    }
  }
};

// Position message
struct FGExternalMotionData {
  // simulation time when this packet was generated
  double time;
  // the artificial lag the client should stay behind the average
  // simulation time to arrival time difference
  // FIXME: should be some 'per model' instead of 'per packet' property
  double lag;
  
  // position wrt the earth centered frame
  SGVec3d position;
  // orientation wrt the earth centered frame
  SGQuatf orientation;
  
  // linear velocity wrt the earth centered frame measured in
  // the earth centered frame
  SGVec3f linearVel;
  // angular velocity wrt the earth centered frame measured in
  // the earth centered frame
  SGVec3f angularVel;
  
  // linear acceleration wrt the earth centered frame measured in
  // the earth centered frame
  SGVec3f linearAccel;
  // angular acceleration wrt the earth centered frame measured in
  // the earth centered frame
  SGVec3f angularAccel;
  
  // The set of properties recieved for this timeslot
  std::vector<FGPropertyData*> properties;

  ~FGExternalMotionData()
  {
      std::vector<FGPropertyData*>::const_iterator propIt;
      std::vector<FGPropertyData*>::const_iterator propItEnd;
      propIt = properties.begin();
      propItEnd = properties.end();

      while (propIt != propItEnd)
      {
        delete *propIt;
        propIt++;
      }
  }
};

/**
 * The buffer that holds a multi-player message, suitably aligned.
 */
union FGMP_MsgBuf
{
    void MsgBuf(void)
    {
        memset(&Msg, 0, sizeof(Msg));
    }

    T_MsgHdr* msgHdr()
    {
        return &Header;
    }

    const T_MsgHdr* msgHdr() const
    {
        return reinterpret_cast<const T_MsgHdr*>(&Header);
    }

    T_PositionMsg* posMsg()
    {
        return reinterpret_cast<T_PositionMsg*>(Msg + sizeof(T_MsgHdr));
    }

    const T_PositionMsg* posMsg() const
    {
        return reinterpret_cast<const T_PositionMsg*>(Msg + sizeof(T_MsgHdr));
    }

    xdr_data_t* properties()
    {
        return reinterpret_cast<xdr_data_t*>(Msg + sizeof(T_MsgHdr)
                                             + sizeof(T_PositionMsg));
    }

    const xdr_data_t* properties() const
    {
        return reinterpret_cast<const xdr_data_t*>(Msg + sizeof(T_MsgHdr)
                                                   + sizeof(T_PositionMsg));
    }
    /**
     * The end of the properties buffer.
     */
    xdr_data_t* propsEnd()
    {
        return reinterpret_cast<xdr_data_t*>(Msg + MAX_PACKET_SIZE);
    };

    const xdr_data_t* propsEnd() const
    {
        return reinterpret_cast<const xdr_data_t*>(Msg + MAX_PACKET_SIZE);
    };
    /**
     * The end of properties actually in the buffer. This assumes that
     * the message header is valid.
     */
    xdr_data_t* propsRecvdEnd()
    {
        return reinterpret_cast<xdr_data_t*>(Msg + Header.MsgLen);
    }

    const xdr_data_t* propsRecvdEnd() const
    {
        return reinterpret_cast<const xdr_data_t*>(Msg + Header.MsgLen);
    }
    
    xdr_data2_t double_val;
    char Msg[MAX_PACKET_SIZE];
    T_MsgHdr Header;
};

int8_t XDR_decode_int8 ( const xdr_data_t & n_Val )
{
    return (static_cast<int8_t> (SWAP32(n_Val)));
}

int32_t XDR_decode_int32 ( const xdr_data_t & n_Val )
{
    return (static_cast<int32_t> (SWAP32(n_Val)));
}
xdr_data_t XDR_encode_uint32 ( uint32_t n_Val )
{
    return (SWAP32(static_cast<xdr_data_t> (n_Val)));
}
uint32_t XDR_decode_uint32 ( xdr_data_t n_Val )
{
    return (static_cast<uint32_t> (SWAP32(n_Val)));
}

int64_t XDR_decode_int64 ( const xdr_data2_t & n_Val )
{
    return (static_cast<int64_t> (SWAP64(n_Val)));
}

float XDR_decode_float ( const xdr_data_t & f_Val )
{
    union {
        xdr_data_t x;
        float f;
    } tmp;

    tmp.x = XDR_decode_int32 (f_Val);
    return tmp.f;
}

double XDR_decode_double ( xdr_data2_t d_Val )
{
    union {
        xdr_data2_t x;
        double d;
    } tmp;

    tmp.x = XDR_decode_int64 (d_Val);
    return tmp.d;
}


static int myFillMsgHdr(T_MsgHdr *MsgHdr, int MsgId, unsigned int _len,
                         char * pCallSign)
{
  uint32_t len;
  switch (MsgId) {
  case CHAT_MSG_ID:
    len = sizeof(T_MsgHdr) + sizeof(T_ChatMsg);
    break;
  case POS_DATA_ID:
    len = _len;
    break;
  default:
    len = sizeof(T_MsgHdr);
    break;
  }
  MsgHdr->Magic           = XDR_encode_uint32(MSG_MAGIC);
  MsgHdr->Version         = XDR_encode_uint32(PROTO_VER);
  MsgHdr->MsgId           = XDR_encode_uint32(MsgId);
  MsgHdr->MsgLen          = XDR_encode_uint32(len);
  MsgHdr->ReplyAddress    = 0; // Are obsolete, keep them for the server for
  MsgHdr->ReplyPort       = 0; // now
  // strncpy(MsgHdr->Callsign, mCallsign.c_str(), MAX_CALLSIGN_LEN);
  // MsgHdr->Callsign[MAX_CALLSIGN_LEN - 1] = '\0';
  if (strlen(pCallSign) < MAX_CALLSIGN_LEN)
      strcpy(MsgHdr->Callsign, pCallSign);
  else
      return 1;
  return 0;
}

struct IdPropertyList {
    unsigned id;
    const char* name;
    FGMP_Type type;
  };

struct IdPropertyList sIdPropertyList[] = {
  {100, "surface-positions/left-aileron-pos-norm",  Typ_FLOAT},
  {101, "surface-positions/right-aileron-pos-norm", Typ_FLOAT},
  {102, "surface-positions/elevator-pos-norm",      Typ_FLOAT},
  {103, "surface-positions/rudder-pos-norm",        Typ_FLOAT},
  {104, "surface-positions/flap-pos-norm",          Typ_FLOAT},
  {105, "surface-positions/speedbrake-pos-norm",    Typ_FLOAT},
  {106, "gear/tailhook/position-norm",              Typ_FLOAT},
  {107, "gear/launchbar/position-norm",             Typ_FLOAT},
  {108, "gear/launchbar/state",                     Typ_STRING},
  {109, "gear/launchbar/holdback-position-norm",    Typ_FLOAT},
  {110, "canopy/position-norm",                     Typ_FLOAT},
  {111, "surface-positions/wing-pos-norm",          Typ_FLOAT},
  {112, "surface-positions/wing-fold-pos-norm",     Typ_FLOAT},

  {200, "gear/gear[0]/compression-norm",           Typ_FLOAT},
  {201, "gear/gear[0]/position-norm",              Typ_FLOAT},
  {210, "gear/gear[1]/compression-norm",           Typ_FLOAT},
  {211, "gear/gear[1]/position-norm",              Typ_FLOAT},
  {220, "gear/gear[2]/compression-norm",           Typ_FLOAT},
  {221, "gear/gear[2]/position-norm",              Typ_FLOAT},
  {230, "gear/gear[3]/compression-norm",           Typ_FLOAT},
  {231, "gear/gear[3]/position-norm",              Typ_FLOAT},
  {240, "gear/gear[4]/compression-norm",           Typ_FLOAT},
  {241, "gear/gear[4]/position-norm",              Typ_FLOAT},

  {300, "engines/engine[0]/n1",  Typ_FLOAT},
  {301, "engines/engine[0]/n2",  Typ_FLOAT},
  {302, "engines/engine[0]/rpm", Typ_FLOAT},
  {310, "engines/engine[1]/n1",  Typ_FLOAT},
  {311, "engines/engine[1]/n2",  Typ_FLOAT},
  {312, "engines/engine[1]/rpm", Typ_FLOAT},
  {320, "engines/engine[2]/n1",  Typ_FLOAT},
  {321, "engines/engine[2]/n2",  Typ_FLOAT},
  {322, "engines/engine[2]/rpm", Typ_FLOAT},
  {330, "engines/engine[3]/n1",  Typ_FLOAT},
  {331, "engines/engine[3]/n2",  Typ_FLOAT},
  {332, "engines/engine[3]/rpm", Typ_FLOAT},
  {340, "engines/engine[4]/n1",  Typ_FLOAT},
  {341, "engines/engine[4]/n2",  Typ_FLOAT},
  {342, "engines/engine[4]/rpm", Typ_FLOAT},
  {350, "engines/engine[5]/n1",  Typ_FLOAT},
  {351, "engines/engine[5]/n2",  Typ_FLOAT},
  {352, "engines/engine[5]/rpm", Typ_FLOAT},
  {360, "engines/engine[6]/n1",  Typ_FLOAT},
  {361, "engines/engine[6]/n2",  Typ_FLOAT},
  {362, "engines/engine[6]/rpm", Typ_FLOAT},
  {370, "engines/engine[7]/n1",  Typ_FLOAT},
  {371, "engines/engine[7]/n2",  Typ_FLOAT},
  {372, "engines/engine[7]/rpm", Typ_FLOAT},
  {380, "engines/engine[8]/n1",  Typ_FLOAT},
  {381, "engines/engine[8]/n2",  Typ_FLOAT},
  {382, "engines/engine[8]/rpm", Typ_FLOAT},
  {390, "engines/engine[9]/n1",  Typ_FLOAT},
  {391, "engines/engine[9]/n2",  Typ_FLOAT},
  {392, "engines/engine[9]/rpm", Typ_FLOAT},

  {800, "rotors/main/rpm", Typ_FLOAT},
  {801, "rotors/tail/rpm", Typ_FLOAT},
  {810, "rotors/main/blade[0]/position-deg",  Typ_FLOAT},
  {811, "rotors/main/blade[1]/position-deg",  Typ_FLOAT},
  {812, "rotors/main/blade[2]/position-deg",  Typ_FLOAT},
  {813, "rotors/main/blade[3]/position-deg",  Typ_FLOAT},
  {820, "rotors/main/blade[0]/flap-deg",  Typ_FLOAT},
  {821, "rotors/main/blade[1]/flap-deg",  Typ_FLOAT},
  {822, "rotors/main/blade[2]/flap-deg",  Typ_FLOAT},
  {823, "rotors/main/blade[3]/flap-deg",  Typ_FLOAT},
  {830, "rotors/tail/blade[0]/position-deg",  Typ_FLOAT},
  {831, "rotors/tail/blade[1]/position-deg",  Typ_FLOAT},

  {900, "sim/hitches/aerotow/tow/length",                       Typ_FLOAT},
  {901, "sim/hitches/aerotow/tow/elastic-constant",             Typ_FLOAT},
  {902, "sim/hitches/aerotow/tow/weight-per-m-kg-m",            Typ_FLOAT},
  {903, "sim/hitches/aerotow/tow/dist",                         Typ_FLOAT},
  {904, "sim/hitches/aerotow/tow/connected-to-property-node",   Typ_BOOL},
  {905, "sim/hitches/aerotow/tow/connected-to-ai-or-mp-callsign",   Typ_STRING},
  {906, "sim/hitches/aerotow/tow/brake-force",                  Typ_FLOAT},
  {907, "sim/hitches/aerotow/tow/end-force-x",                  Typ_FLOAT},
  {908, "sim/hitches/aerotow/tow/end-force-y",                  Typ_FLOAT},
  {909, "sim/hitches/aerotow/tow/end-force-z",                  Typ_FLOAT},
  {930, "sim/hitches/aerotow/is-slave",                         Typ_BOOL},
  {931, "sim/hitches/aerotow/speed-in-tow-direction",           Typ_FLOAT},
  {932, "sim/hitches/aerotow/open",                             Typ_BOOL},
  {933, "sim/hitches/aerotow/local-pos-x",                      Typ_FLOAT},
  {934, "sim/hitches/aerotow/local-pos-y",                      Typ_FLOAT},
  {935, "sim/hitches/aerotow/local-pos-z",                      Typ_FLOAT},

  {1001, "controls/flight/slats",  Typ_FLOAT},
  {1002, "controls/flight/speedbrake",  Typ_FLOAT},
  {1003, "controls/flight/spoilers",  Typ_FLOAT},
  {1004, "controls/gear/gear-down",  Typ_FLOAT},
  {1005, "controls/lighting/nav-lights",  Typ_FLOAT},
  {1006, "controls/armament/station[0]/jettison-all",  Typ_BOOL},

  {1100, "sim/model/variant", Typ_INT},
  {1101, "sim/model/livery/file", Typ_STRING},

  {1200, "environment/wildfire/data", Typ_STRING},
  {1201, "environment/contrail", Typ_INT},

  {1300, "tanker", Typ_INT},

  {10001, "sim/multiplay/transmission-freq-hz",  Typ_STRING},
  {10002, "sim/multiplay/chat",  Typ_STRING},

  {10100, "sim/multiplay/generic/string[0]", Typ_STRING},
  {10101, "sim/multiplay/generic/string[1]", Typ_STRING},
  {10102, "sim/multiplay/generic/string[2]", Typ_STRING},
  {10103, "sim/multiplay/generic/string[3]", Typ_STRING},
  {10104, "sim/multiplay/generic/string[4]", Typ_STRING},
  {10105, "sim/multiplay/generic/string[5]", Typ_STRING},
  {10106, "sim/multiplay/generic/string[6]", Typ_STRING},
  {10107, "sim/multiplay/generic/string[7]", Typ_STRING},
  {10108, "sim/multiplay/generic/string[8]", Typ_STRING},
  {10109, "sim/multiplay/generic/string[9]", Typ_STRING},
  {10110, "sim/multiplay/generic/string[10]", Typ_STRING},
  {10111, "sim/multiplay/generic/string[11]", Typ_STRING},
  {10112, "sim/multiplay/generic/string[12]", Typ_STRING},
  {10113, "sim/multiplay/generic/string[13]", Typ_STRING},
  {10114, "sim/multiplay/generic/string[14]", Typ_STRING},
  {10115, "sim/multiplay/generic/string[15]", Typ_STRING},
  {10116, "sim/multiplay/generic/string[16]", Typ_STRING},
  {10117, "sim/multiplay/generic/string[17]", Typ_STRING},
  {10118, "sim/multiplay/generic/string[18]", Typ_STRING},
  {10119, "sim/multiplay/generic/string[19]", Typ_STRING},

  {10200, "sim/multiplay/generic/float[0]", Typ_FLOAT},
  {10201, "sim/multiplay/generic/float[1]", Typ_FLOAT},
  {10202, "sim/multiplay/generic/float[2]", Typ_FLOAT},
  {10203, "sim/multiplay/generic/float[3]", Typ_FLOAT},
  {10204, "sim/multiplay/generic/float[4]", Typ_FLOAT},
  {10205, "sim/multiplay/generic/float[5]", Typ_FLOAT},
  {10206, "sim/multiplay/generic/float[6]", Typ_FLOAT},
  {10207, "sim/multiplay/generic/float[7]", Typ_FLOAT},
  {10208, "sim/multiplay/generic/float[8]", Typ_FLOAT},
  {10209, "sim/multiplay/generic/float[9]", Typ_FLOAT},
  {10210, "sim/multiplay/generic/float[10]", Typ_FLOAT},
  {10211, "sim/multiplay/generic/float[11]", Typ_FLOAT},
  {10212, "sim/multiplay/generic/float[12]", Typ_FLOAT},
  {10213, "sim/multiplay/generic/float[13]", Typ_FLOAT},
  {10214, "sim/multiplay/generic/float[14]", Typ_FLOAT},
  {10215, "sim/multiplay/generic/float[15]", Typ_FLOAT},
  {10216, "sim/multiplay/generic/float[16]", Typ_FLOAT},
  {10217, "sim/multiplay/generic/float[17]", Typ_FLOAT},
  {10218, "sim/multiplay/generic/float[18]", Typ_FLOAT},
  {10219, "sim/multiplay/generic/float[19]", Typ_FLOAT},

  {10300, "sim/multiplay/generic/int[0]", Typ_INT},
  {10301, "sim/multiplay/generic/int[1]", Typ_INT},
  {10302, "sim/multiplay/generic/int[2]", Typ_INT},
  {10303, "sim/multiplay/generic/int[3]", Typ_INT},
  {10304, "sim/multiplay/generic/int[4]", Typ_INT},
  {10305, "sim/multiplay/generic/int[5]", Typ_INT},
  {10306, "sim/multiplay/generic/int[6]", Typ_INT},
  {10307, "sim/multiplay/generic/int[7]", Typ_INT},
  {10308, "sim/multiplay/generic/int[8]", Typ_INT},
  {10309, "sim/multiplay/generic/int[9]", Typ_INT},
  {10310, "sim/multiplay/generic/int[10]", Typ_INT},
  {10311, "sim/multiplay/generic/int[11]", Typ_INT},
  {10312, "sim/multiplay/generic/int[12]", Typ_INT},
  {10313, "sim/multiplay/generic/int[13]", Typ_INT},
  {10314, "sim/multiplay/generic/int[14]", Typ_INT},
  {10315, "sim/multiplay/generic/int[15]", Typ_INT},
  {10316, "sim/multiplay/generic/int[16]", Typ_INT},
  {10317, "sim/multiplay/generic/int[17]", Typ_INT},
  {10318, "sim/multiplay/generic/int[18]", Typ_INT},
  {10319, "sim/multiplay/generic/int[19]", Typ_INT}
};

unsigned int numProperties = (sizeof(sIdPropertyList)
                                 / sizeof(sIdPropertyList[0]));


const IdPropertyList* _findProperty(unsigned id)
{
    struct IdPropertyList * ppl = &sIdPropertyList[0];
    for (unsigned int i = 0; i < numProperties; i++)
    {
        if (ppl->id == id)
            return ppl;
        ppl++;  /* bump to next property */
    }
    return NULL;
}

#endif // #ifndef _FGMPMSG_HXX_
// eof - fgmpmsg.hxx
