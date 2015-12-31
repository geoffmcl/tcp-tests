// fgmp_msg.cxx
#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#endif

#define TEST_GEO_CONV
#define TEST_MPPY_CONV

#include <stdio.h> // fopen, fead, ...
#include <simgear/misc/stdint.hxx> /* for uni32_t, etc ... */
#include "fgmpmsg.hxx" /* includes <vector> & <map>... */
#include "fg_geometry.hxx"
#include <ostream>
#ifdef TEST_GEO_CONV
#include <simgear/math/sg_geodesy.hxx>
#endif 
#if (defined(_MSC_VER) && !defined(GET_SECONDS_STAMP))
#include <Windows.h> /* to in the MM system */
#include <Mmsystem.h> /* for timeGetTime(), ... */
#pragma comment( lib, "Winmm" ) /* link with Winmm.[LIB|DLL] */
#endif

using namespace std;

static char * modName = (char *)"fgmp_msg";

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "0.0.1"
#endif

// forward refs
void _geoLlh2Efg (double lat, double lon, double height,
                    double *e,  double *f,  double *g);
void _geoEfg2Llh (double efg[], double *lat, double *lon, double *hgt);


// Automatic sorting of motion data according to its timestamp
typedef std::map<double,FGExternalMotionData> MotionInfo;
static MotionInfo mMotionInfo;
static double mLastTimestamp;

static size_t total_bytes, total_msgs, total_msgbytes;
static size_t total_bad;

static FILE * fSampRaw = 0;

static FGMP_MsgBuf m_msgBuf;
static int _s_verb = 0;
#define VERB1 (_s_verb >= 1)
#define VERB2 (_s_verb >= 3)
#define VERB5 (_s_verb >= 5)
#define VERB9 (_s_verb >= 9)

void set_fgmp_msg_verb(int val) { _s_verb = val; }

#ifdef TEST_MPPY_CONV
#define q_x 0
#define q_y 1
#define q_z 2
#define q_w 3
#define c_x 0
#define c_y 1
#define c_z 2

typedef struct tagGSTRUC {
    char callsign[MAX_CALLSIGN_LEN];
    double lon_degs;
    double lat_degs;
    double alt_feet;
    double pch; //# nose-up positive
	double hdg;
	double bnk; //# clockwise positive
} GSTRUC;
void geodtocart(double lat_degs, double lon_degs, double hgt_feet, double * xyz);
void quat_fromEuler(double x, double y, double z, double * xyzw);
void quat_fromLonLat(double lon_degs, double lat_degs, double * xyzw);
void quat_multiply(double v1[], double v2[], double * xyzw);
int quat_getAngleAxis(double q[], double * v);
void setOrientation( GSTRUC & g, double * ornt );

#endif // #ifdef TEST_MPPY_CONV


#ifndef GET_SECONDS_STAMP
#define GET_SECONDS_STAMP
double get_seconds_stamp(void)
{
    size_t _sec,_nsec;
#ifdef _WIN32
    unsigned int t;
    t = timeGetTime();
    _sec = t / 1000;
    _nsec = ( t - ( _sec * 1000 ) ) * 1000 * 1000;
#elif defined(_POSIX_TIMERS) && (0 < _POSIX_TIMERS)
    struct timespec ts;
#if defined(_POSIX_MONOTONIC_CLOCK)
    static clockid_t clockid = CLOCK_MONOTONIC;
    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        // For the first time test if the monotonic clock is available.
        // If so use this if not use the realtime clock.
        if (-1 == clock_gettime(clockid, &ts) && errno == EINVAL)
            clockid = CLOCK_REALTIME;
    }
    clock_gettime(clockid, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    _sec = ts.tv_sec;
    _nsec = ts.tv_nsec;
#elif defined( HAVE_GETTIMEOFDAY )
    struct timeval current;
    struct timezone tz;
    // sg_timestamp currtime;
    gettimeofday(&current, &tz);
    _sec = current.tv_sec;
    _nsec = current.tv_usec * 1000;
#elif defined( HAVE_GETLOCALTIME )
    SYSTEMTIME current;
    GetLocalTime(&current);
    _sec = current.wSecond;
    _nsec = current.wMilliseconds * 1000 * 1000;
#elif defined( HAVE_FTIME )
    struct timeb current;
    ftime(&current);
    _sec = current.time;
    _nsec = current.millitm * 1000 * 1000;
#else
# error Port me
#endif
    return 1e-9*_nsec + double(_sec);
}
#endif // GET_SECONDS_STAMP

int _addMotionInfo(FGExternalMotionData& motionInfo,double stamp)
{
    mLastTimestamp = stamp;

    if (!mMotionInfo.empty()) {
        double diff = motionInfo.time - mMotionInfo.rbegin()->first;
        // packet is very old -- MP has probably reset (incl. his timebase)
        if (diff < -10.0) {
            mMotionInfo.clear();
        } else if (diff < 0.0) {
            // drop packets arriving out of order
            return 1;
        }
    }

    mMotionInfo[motionInfo.time] = motionInfo;
    // We just copied the property (pointer) list - they are ours now. Clear the
    // properties list in given/returned object, so former owner won't deallocate them.
    motionInfo.properties.clear();
    return 0;
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
                            printf("%s - ERROR: Float ISNAN!\n", modName );
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
                    printf("%s - ERROR: Unknown Prop type %d\n", modName, id );
                    return false;
                    //xdr++;
                    break;
            }            
        } else {
            // give up; this is a malformed property list.
            printf("%s - ERROR: malformed property list.\n", modName );
            return false;
        }
    }
    return true;
}

#define METERS2FEET 3.28083989501312335958
#define FEET2METER  0.3048

#ifndef MY_PI
#define MY_PI  3.1415926535f
#endif

#ifndef DEGREES2RADIANS
#define DEGREES2RADIANS  (MY_PI/180.0)
#endif
#ifndef RADIANS2DEGREES
#define RADIANS2DEGREES  (180.0/MY_PI)
#endif

double get_degrees_1(double az)
{
    int degs10 = (int)((az + 0.05) * 10.0);
    return ((double)degs10 / 10.0);
}

char * get_meter_stg(double mets)
{
    static char _s_m_buf[256];
    char * cp = _s_m_buf;
    char * units;
    //char * form = "%3.6f";
    char * form = "%3.1f";
    double d;
    if (mets < 1e-24) {
        return "lt 1e-24m";
    } else if (mets < 1e-21) {
        d = mets * 1e+21;
        units = "ym";
    } else if (mets < 1e-18) {
        d = mets * 1e+18;
        units = "zm";
    } else if (mets < 1e-15) {
        d = mets * 1e+15;
        units = "am";
    } else if (mets < 1e-12) {
        d = mets * 1e+12;
        units = "fm";
    } else if (mets < 1e-9) {
        d = mets * 1e+9;
        units = "pm";
    } else if (mets < 1e-3) {
        d = mets * 1e+3;
        units = "um";
    } else if (mets < 1e-2) {
        d = mets * 1e+2;
        units = "mm";
    } else if (mets < 1e-1) {
        d = mets * 1e+1;
        units = "cm";
    } else {
        d = mets;
        units = "m";
    }
    sprintf(cp,form,d);
    strcat(cp,units);
    return cp;
}

char * get_speed_stg(double speed_kts)
{
    static char _s_spd_stg[32];
    char * cp = _s_spd_stg;
    if (speed_kts < 0.001) {
        return "[~stop]";
    }
    if (speed_kts < 0.1) {
        return "[~slow]";
    }
    sprintf(cp,"%3.1f", speed_kts);
    return cp;
}
void _ProcessChatMsg(void)
{

}
int _ProcessPosMsg(void)
{
    static double last_time = 0.0;
    static size_t tot_msg_cnt = 0;
    static double tot_elap = 0.0;
    static double last_lat, last_lon, last_alt;

    int iret = 0;
    unsigned int i;
    double d;
    float f;
    Point3D cart, geod;
    char * indent = (char *)"     ";
    double elap;

    const T_PositionMsg* PosMsg = m_msgBuf.posMsg();
    FGExternalMotionData motionInfo;
    motionInfo.time = XDR_decode_double(PosMsg->time);
    motionInfo.lag = XDR_decode_double(PosMsg->lag);
    double lat1, lon1, hgt1;
    for (i = 0; i < 3; i++) {
        d = XDR_decode_double(PosMsg->position[i]);
        motionInfo.position(i) = d;
        cart[i] = d;
    }
    sgCartToGeod( cart, geod );
    lat1 = geod.GetX();
    lon1 = geod.GetY();
    hgt1 = geod.GetZ();
    if (geod.GetZ() <= -9999.0) {
        printf("%s - ProcessPosMsg: Skipping due to -9999.0 altitude!\n", modName);
        return 1;
    }
    if (motionInfo.time <= 0.0) {
        printf("%s - ProcessPosMsg: Skipping due to no time!\n", modName);
        return 1;
    }

    /* ok, process the message... */
    if (last_time <= 0.0)
        last_time = motionInfo.time;

    tot_msg_cnt++;
    elap = motionInfo.time - last_time;
    tot_elap += elap;
    printf("\n%s - ProcessPosMsg: time %f, lag %f, elapsed %f, total %f secs.\n",
        modName,motionInfo.time,motionInfo.lag,
        elap, tot_elap);
    printf("%4d: Position: %f %f %f - sgCartToGeod()\n", tot_msg_cnt, lat1, lon1, hgt1 );

    double lat2, lon2, hgt2, efg[3], efg2[3];
    double az1, az2, s;
    int res = 0;
    az1 = az2 = s = 0.0;
    lat2 = lon2 = hgt2 = 0.0;
    for (i = 0; i < 3; i++) {
        efg[i] = motionInfo.position(i);
        efg2[i] = motionInfo.position(i);
    }

    if (VERB5) {
#ifdef TEST_GEO_CONV
        SGGeod g1,g2;
#ifdef SGGeodesy_H
        SGGeoc gc1, gc2;
        gc1.fromDegFt(lon1,lat1,hgt1);
#endif  
        g1.fromDegFt(lon1,lat1,hgt1);
        efg[0] = motionInfo.position(0);
        efg[1] = motionInfo.position(1);
        efg[2] = motionInfo.position(2);
        printf("Position: %f %f %f - raw cartesian <mi>.position\n", efg[0], efg[1], efg[2] );
        _geoLlh2Efg(lat1 * DEGREES2RADIANS,lon1 * DEGREES2RADIANS, hgt1 * FEET2METER,&efg[0],&efg[1],&efg[2]);
        printf("Position: %f %f %f - cart. _geoLlh2Efg()\n", efg[0], efg[1], efg[2] );

        //printf("Position: %f %f %f\n", motionInfo.position(0), motionInfo.position(1), motionInfo.position(2) );
        //printf("Position: %f %f %f\n", efg[0], efg[1], efg[2] );
        efg[0] = cart.GetX();
        efg[1] = cart.GetY();
        efg[2] = cart.GetZ();
        printf("Position: %f %f %f - raw cartesian - efg[]\n", efg[0], efg[1], efg[2] );
        _geoEfg2Llh( efg, &lat2, &lon2, &hgt2 );
        _geoLlh2Efg(lat2,lon2, hgt2,&efg2[0],&efg2[1],&efg2[2]);
        lat2 *= RADIANS2DEGREES;
        lon2 *= RADIANS2DEGREES;
        hgt2 *= METERS2FEET;
        printf("Position: %f %f %f - _geoEfg2Llh()\n", lat2, lon2, hgt2);
        printf("Position: %f %f %f - _geoLlh2Efg()\n", efg2[0], efg2[1], efg2[2] );
        g2.fromDegFt(lat2,lon2,hgt2);
#ifdef SGGeodesy_H
        gc2.fromDegFt(lon2,lat2,hgt2);
        // double distm = gc2.distanceM(gc1,gc2); // hmmm, this yield close to ZERO???
        printf("Diff    : %f %f %f", lat1-lat2, lon1-lon2, hgt1-hgt2);
        // printf(" dist %s.", get_meter_stg(distm) );
#ifdef _SG_GEODESY_HXX
        res = geo_inverse_wgs_84( lat1, lon1, lat2, lon2, &az1, &az2, &s );
        printf(" (%s [%1.1f feet], on az %d degs.) (%d)",
            get_meter_stg(s),
            (s * METERS2FEET),
            (int)(az1 +0.5), res );
#endif // _SG_GEODESY_HXX
        printf("\n");
#endif // SGGeodesy_H
#endif // TEST_GEO_CONV
    }

    if (VERB9) printf("%s Orientation: ", indent);
    SGVec3f angleAxis;
    for (i = 0; i < 3; i++) {
        f = XDR_decode_float(PosMsg->orientation[i]);
        angleAxis(i) = f;
        if (VERB9) printf("%f ", f);
    }
    if (VERB9) printf(" (raw)\n");
    motionInfo.orientation = SGQuatf::fromAngleAxis(angleAxis);
    for (i = 0; i < 3; i++)
        motionInfo.linearVel(i) = XDR_decode_float(PosMsg->linearVel[i]);
    for (i = 0; i < 3; i++)
        motionInfo.angularVel(i) = XDR_decode_float(PosMsg->angularVel[i]);
    for (i = 0; i < 3; i++)
        motionInfo.linearAccel(i) = XDR_decode_float(PosMsg->linearAccel[i]);
    for (i = 0; i < 3; i++)
        motionInfo.angularAccel(i) = XDR_decode_float(PosMsg->angularAccel[i]);

    SGGeod pos;	// WGS84 lat & lon in degrees, elev above sea-level in meters
    double hdg;		// True heading in degrees
    double roll;	// degrees, left is negative
    double pitch;	// degrees, nose-down is negative
    double speed;       // knots true airspeed
    double altitude_ft; // feet above sea level

    SGVec3d ecPos = motionInfo.position;
    SGQuatf ecOrient = motionInfo.orientation;
    SGVec3f linearVel = motionInfo.linearVel;
    SGVec3f angularVel = motionInfo.angularVel;
    pos = SGGeod::fromCart(ecPos);
    altitude_ft = pos.getElevationFt();
    // The quaternion rotating from the earth centered frame to the
    // horizontal local frame
    SGQuatf qEc2Hl = SGQuatf::fromLonLatRad((float)pos.getLongitudeRad(),
                                          (float)pos.getLatitudeRad());
    // The orientation wrt the horizontal local frame
    SGQuatf hlOr = conj(qEc2Hl)*ecOrient;
    float hDeg, pDeg, rDeg;
    hlOr.getEulerDeg(hDeg, pDeg, rDeg);
    hdg = hDeg;
    roll = rDeg;
    pitch = pDeg;
    speed = norm(motionInfo.linearVel) * SG_METER_TO_NM * 3600.0;
    if (VERB9) {
        // already shown once using sgCartToGeod()
        printf("%s Position: %f %f %f - SGGeod::fromCart()\n", indent,
            pos.getLatitudeDeg(), pos.getLongitudeDeg(), altitude_ft );
    }
    printf("%s Speed: %s kt, hdg: %d, roll: %f, pitch: %f", indent,
        get_speed_stg(speed), (int)(hdg + 0.5), roll, pitch );
#ifdef SG_KT_TO_MPS
    if (speed > 1.0)
        printf(" (%.1f mps)", speed * SG_KT_TO_MPS);
#endif // SG_KT_TO_MPS
    printf("\n");

    if (VERB9)
        printf("%s Orientation: %f %f %f (raw)\n", indent, angleAxis(0), angleAxis(1), angleAxis(2) );

#ifdef TEST_MPPY_CONV
    if (VERB2) {
        // void geodtocart(double lat_degs, double lon_degs, double hgt_feet, double * xyz)
        double xyz[3];
        Point3D cart2, geod2;

        geodtocart(lat1, lon1, hgt1, &xyz[0]);
        // cart[i] = d;
        printf("Org.cart: %f %f %f - raw cartesian (cart)\n", cart[0], cart[1], cart[2]);
        printf("g2c.cart: %f %f %f - from mp.py geodtocart()\n", xyz[0], xyz[1], xyz[2]);
        for (i = 0; i < 3; i++)
            cart2[i] = xyz[i];
        sgCartToGeod( cart2, geod2 );
        lat2 = geod2.GetX();
        lon2 = geod2.GetY();
        hgt2 = geod2.GetZ();
        printf("position: %f %f %f, mp.py+sgCartToGeod()", lat2, lon2, hgt2);
#ifdef _SG_GEODESY_HXX
        res = geo_inverse_wgs_84( lat1, lon1, lat2, lon2, &az1, &az2, &s );
        printf(" (diff: %s [%1.1f ft] az %3.1f degs.) (%d)",
            get_meter_stg(s), s * METERS2FEET, az1, res );
#endif // _SG_GEODESY_HXX
        printf("\n");
    }
#endif //#ifdef TEST_MPPY_CONV
    /* ================================================================== */

    const xdr_data_t* xdr = m_msgBuf.properties();   // start of properties
    const xdr_data_t* xdrend = m_msgBuf.propsRecvdEnd(); // end of properties
    if (PosMsg->pad != 0) {
        if (_verifyProperties(&PosMsg->pad, xdrend))
            xdr = &PosMsg->pad;
        else if (!_verifyProperties(xdr, xdrend))
            goto noprops;
    }
    while (xdr < m_msgBuf.propsRecvdEnd()) {
        // First element is always the ID
        unsigned int id = XDR_decode_uint32(*xdr);
        if (VERB9) printf("ID=%d ", id );
        xdr++;
    
        // Check the ID actually exists and get the type
        const IdPropertyList* plist = _findProperty(id);
        if (plist)
        {
          if (VERB9) printf("[%s] ", plist->name );
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
              if (VERB9) printf("INT %d\n", pData->int_value );
              break;
          case Typ_FLOAT:
          case Typ_DOUBLE:
              pData->float_value = XDR_decode_float(*xdr);
              xdr++;
              if (VERB9) printf("FLOAT %f\n", pData->float_value );
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
                  if (VERB9) printf("String: len %d [%s]\n", length, pData->string_value );
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
              // ATM the DEFAULT is assumed a 'float'!!!
              pData->float_value = XDR_decode_float(*xdr);
              // SG_LOG(SG_NETWORK, SG_DEBUG, "Unknown Prop type " << pData->id << " " << pData->type);
              if (VERB9) printf("Unknown Prop type!\n");
              xdr++;
              break;
          }
          motionInfo.properties.push_back(pData);
        } else {
            // We failed to find the property. We'll try the next packet immediately.
            T_MsgHdr* MsgHdr = m_msgBuf.msgHdr();
            if (VERB9) printf("\n");
            printf("%s - ERROR: message from [%s] has unknown property id %d\n",
                modName, MsgHdr->Callsign, id);
            iret = 1;
            break;
        }
    }
noprops:
    if (iret == 0) {
        double stamp = get_seconds_stamp();
        iret = _addMotionInfo(motionInfo,stamp);
    }
    if (tot_msg_cnt > 1) {
#ifdef _SG_GEODESY_HXX
        // moved from 'last' position to new position, over time
        res = geo_inverse_wgs_84( last_lat, last_lon, lat1, lon1, &az1, &az2, &s );
        if (s > 1.0) { // only interested if moved a meter or more
            printf("%s Moved %s [%1.1f ft] az %3.1f degs.) (%d)", indent,
                get_meter_stg(s),
                s * METERS2FEET,
                az1, 
                res );
            if (elap > 0.000001) {
                printf(" est. %3.1f mps", s / elap);
            }
            printf("\n");
        }
#endif // _SG_GEODESY_HXX
    }
    last_time = motionInfo.time;
    last_lat = lat1;
    last_lon = lon1;
    last_alt = hgt1;
    return iret;
}

#ifdef SGGeodesy_H
bool equalSGGeodc(SGGeoc & g1, SGGeoc & g2)
{
    double lat1 = g1.getLatitudeDeg();
    double lon1 = g1.getLongitudeDeg();
    double lat2 = g2.getLatitudeDeg();
    double lon2 = g2.getLongitudeDeg();
    if ((fabs(lat1 - lat2) < 0.00001)&&
        (fabs(lon1 - lon2) < 0.00001))
        return true;

    return false;
}
#endif


int test_fgmp_message(char * file)
{
    int iret = 0;
    T_MsgHdr* MsgHdr = m_msgBuf.msgHdr();
    char tmpbuf[64];
    size_t buflen = sizeof(T_MsgHdr);
    size_t rlen, i, off, rd_count;
    char * cp;

    total_bytes = total_msgs = total_msgbytes = total_bad = 0;

    fSampRaw = fopen(file,"rb");
    if (!fSampRaw) {
        printf("%s - ERROR: Failed to open file [%s]\n", modName, file);
        return 1;
    }
    printf("%s - Processing file [%s]...\n", modName, file);
    rd_count = 0;
    while (1) {
        buflen = sizeof(T_MsgHdr);
        cp = (char *)MsgHdr;
        rlen = fread((void *)MsgHdr, 1, buflen, fSampRaw);
        if (rlen != buflen) {
            printf("%s - Failed on read %d of file [%s], %d bytes req., read %d\n",
                modName, rd_count, file, buflen, rlen);
            if (rd_count == 0)
                iret = 1;
            break;
        }
        MsgHdr->Magic       = XDR_decode_uint32 (MsgHdr->Magic);
        MsgHdr->Version     = XDR_decode_uint32 (MsgHdr->Version);
        MsgHdr->MsgId       = XDR_decode_uint32 (MsgHdr->MsgId);
        MsgHdr->MsgLen      = XDR_decode_uint32 (MsgHdr->MsgLen);
        MsgHdr->ReplyPort   = XDR_decode_uint32 (MsgHdr->ReplyPort);
        MsgHdr->Callsign[MAX_CALLSIGN_LEN -1] = '\0';
        int msg_valid = 0;
        // invalid if NOT MSG_MAGIC or RELAY_MAGIC
        if ((MsgHdr->Magic != MSG_MAGIC)&&(MsgHdr->Magic != RELAY_MAGIC)) {
            printf("%s - message has invalid magic [%d] vs ", modName, MsgHdr->Magic);
            if (MsgHdr->Magic != MSG_MAGIC)
                printf("[%d]\n", MSG_MAGIC);
            else
                printf("[%d]\n", RELAY_MAGIC);
            total_bad++;
            break;
        }
        if (MsgHdr->Version != PROTO_VER) {
            printf("%s - message has invalid protocol [%d] vs [%d]\n", modName, MsgHdr->Version, PROTO_VER);
            total_bad++;
            break;
        }
        // ok, read balance of this message
        cp += rlen; // bump read point
        i = MsgHdr->MsgLen - rlen; // length to READ
        rlen = fread((void *)cp, 1, i, fSampRaw);
        if (rlen != i) {
            printf("%s - Failed on read %d of file [%s], %d bytes req., read %d\n",
                modName, rd_count, file, i, rlen);
            break;
        }
        buflen += rlen;
        total_msgs++;
        total_msgbytes += buflen;
        total_bytes += buflen;
        // read the rounding up bytes written by server
        i = buflen;
        off = 0;
        rd_count++;
        while (i % 16) {
            off++;
            i++;
        }
        off += 16;
        fread(tmpbuf,1,off,fSampRaw);
        total_bytes += off;
        switch (MsgHdr->MsgId)
        {
        case CHAT_MSG_ID:
            _ProcessChatMsg();
            break;
        case POS_DATA_ID:
            if (_ProcessPosMsg())
                total_bad++;
            break;
        case UNUSABLE_POS_DATA_ID:
        case OLD_OLD_POS_DATA_ID:
        case OLD_PROP_MSG_ID:
        case OLD_POS_DATA_ID:
            printf("%s - message from [%s] has obsolete ID %d!\n",
                modName, MsgHdr->Callsign, MsgHdr->MsgId );
            iret = 1;
            total_bad++;
            break;
        default:
            printf("%s - message from [%s] has UNKNOWN ID %d!\n",
                modName, MsgHdr->Callsign, MsgHdr->MsgId );
            iret = 1;
            total_bad++;
            break;
        }
    }

    printf("From [%s], total bytes = %d, total msgs = %d, of bytes = %d. Bad %d (map=%d)\n",
        file, total_bytes, total_msgs, total_msgbytes, total_bad,
        mMotionInfo.size() );

    fclose(fSampRaw);
    fSampRaw = NULL;

    if (VERB9) {
        if ( mMotionInfo.size() ) {
            double lastTm, prevTm;
            SGVec3d ecPos;
            SGQuatf ecOrient;
            double speed, diff, dist;
            bool chg;
#ifdef SGGeodesy_H
            SGGeoc geodc, pgeodc;
#else
            Point3D cart, geod, pgeod;
#endif
            MotionInfo::iterator it = mMotionInfo.begin();
            rd_count = 0;
            off = 0;
            for ( ; it != mMotionInfo.end(); it++ )
            {
                FGExternalMotionData& motionInfo = it->second;
                double newTm = it->second.time;
                SGVec3d ecP = it->second.position;
                SGQuatf ecO = it->second.orientation;
                double spd = norm(it->second.linearVel) * SG_METER_TO_NM * 3600.0;
                chg = true;
#ifdef SGGeodesy_H
                SGGeodesy::SGCartToGeoc(ecP,geodc);
                if (rd_count) {
                    dist = geodc.distanceM(pgeodc,geodc);
                    diff = newTm - prevTm;
                    if ((dist > 0.1) || !equalSGGeodc(pgeodc,geodc)) {
                        printf("%d Position: %f %f (d=%f m, in %f secs, mc=%d)\n", rd_count,
                            geodc.getLatitudeDeg(), geodc.getLongitudeDeg(), dist, diff, off );
                        off = 0;
                    } else {
                        chg = false;
                        // printf("%d Position: unchanged\n", rd_count );
                        off++;
                    }
                } else {
                    printf("First Position: %f %f\n",
                        geodc.getLatitudeDeg(), geodc.getLongitudeDeg() );
                }
#else
                cart.SetX(ecP.x());
                cart.SetY(ecP.y());
                cart.SetZ(ecP.z());
                sgCartToGeod( cart, geod );
                if (rd_count) {
                    //if (ecP != ecPos) {
                    if (pgeod != geod) {
                        printf("%d Position: %f %f %f\n", rd_count, geod.GetX(), geod.GetY(), geod.GetZ() );
                    } else {
                        printf("%d Position: unchanged\n", rd_count );
                        chg = false;
                    }
                } else {
                    printf("First Position: %f %f %f\n", geod.GetX(), geod.GetY(), geod.GetZ() );
                }
#endif
                ecPos = ecP;
                ecOrient = ecO;
                speed = spd;
                lastTm = newTm;
                if (chg) {
                    prevTm = newTm;
#ifdef SGGeodesy_H
                    pgeodc = geodc;
#else
                    pgeod = geod;
#endif
                }
                rd_count++;
            }
        }
    }
    mMotionInfo.clear();
    return iret;
}

/* ==============================================
    WSG84 conversions
   ============================================== */
#define   GEO_E       0         //!< E coordinate of Earth Fixed Geodetic coordinate ( METERS )
#define   GEO_F       1         //!< F coordinate of Earth Fixed Geodetic coordinate ( METERS )
#define   GEO_G       2         //!< G coordinate of Earth Fixed Geodetic coordinate ( METERS )
#define sqr2(n)          pow(n,2.0)       //!< Squared value: \f$n^2\f$
// Default Ellipsoid (WGS 1984) Default datum (WGS84)
#define MAJ_AXIS 6378137.0
#define INV_FLAT 298.257223563
#define GEO_B(a,f)   (a*(1.0-(1.0/f)))
#define GEO_FL(f)    (1.0/f)
#define GEO_E2(a,f)  (((a*a) - ((GEO_B(a,f))*(GEO_B(a,f))))/(a*a))
#define GEO_E2P(a,f) (((a*a) - ((GEO_B(a,f))*(GEO_B(a,f))))/((GEO_B(a,f))*(GEO_B(a,f))))

void _geoEfg2Llh (double efg[], double *lat, double *lon, double *hgt)
{
    double p, u, u_prime, a, flat, b, e2, ee2,sign = 1.0;
    /* Get the ellipsoid parameters */
    // geoGetEllipsoid (&a, &b, &e2, &ee2, &flat, datum);
    /* - Major Axis. */
    a   = MAJ_AXIS;

    /* - Earth Flattening. */
    flat   = GEO_FL(INV_FLAT);

    /* - Minor axis value. */
    b   = GEO_B(MAJ_AXIS, (INV_FLAT));

    /* - Eccentricity squared. */
    e2  = GEO_E2(MAJ_AXIS, (INV_FLAT));

    /* - Eccentricity squared prime. */
    ee2 = GEO_E2P(MAJ_AXIS, (INV_FLAT));

#ifndef EFG2LLH
    // Computes EFG to lat, lon & height
    p = sqrt(sqr2(efg[GEO_E]) + sqr2(efg[GEO_F]));
    // u = atan((efg[GEO_G]/p)  * (a/b));
    u = atan2(efg[GEO_G] * a , p * b);

    *lat = atan((efg[GEO_G] + ee2 * b * pow(sin(u),3.0) ) /
          ( p - e2 * a * pow(cos(u),3.0) ) );

    u_prime =  atan((1.0 - flat) * tan(*lat));
    if((p - a * cos(u_prime) ) < 0.0) sign= -1.0; // determine sign

    // *hgt = p / cos(*lat) - ( a / (sqrt(1.0 - e2 * pow(sin(*lat),2.0))));   //same results
    *hgt =  sign * sqrt( sqr2( p - a * cos(u_prime)) +
                         sqr2(efg[GEO_G] - b * sin(u_prime)));
    *lon = atan2(efg[GEO_F], efg[GEO_E]);  // atan(f/e)
#else
    // Computes EFG to lat, lon & height
//    p = sqrt(sqr2(efg[GEO_E]) + sqr2(efg[GEO_F]));
//    u = atan((efg[GEO_G]/p) * ((1.0 - flat) + (e2 * a / (sqrt(sqr2(p)+sqr2(efg[GEO_G])))))); //  atan((efg[GEO_G]/p)  * (a/b));
//    *lat = atan2((efg[GEO_G]*(1.0 - flat) + e2 * a * pow(sin(u),3.0) ),
//          ((1.0-flat) * ( p - e2 * a * pow(cos(u),3.0) ) ) );

//    u_prime =  atan((1.0 - flat) * tan(*lat));
//    if((p - a * cos(u_prime) ) < 0.0) sign= -1.0; // determine sign

//    *hgt =  sign * sqrt( sqr2( p - a * cos(u_prime)) +
//                         sqr2(efg[GEO_G] - b * sin(u_prime)));
    *lon = atan2(efg[GEO_F], efg[GEO_E]);  // atan(f/e)
     r = sqrt(sqr2(efg[GEO_E]) + sqr2(efg[GEO_F]));
     phi = atan2(efg[GEO_G], r);
 //     printf("phi(1)=%f\n",phi * RAD_TO_DEG);
     for(i=0;i<15;i++)
     {
        phi1 = phi;
        c = sqrt(1.0 - e2*pow(sin(phi1),2.0));
        phi = atan2((efg[GEO_G] + a * c * e2 * sin(phi1)),r);
//        printf("phi=%f\n",phi * RAD_TO_DEG);
        if( phi == phi1) break;
     }
     *lat = phi;
//     printf("c=%f\n",c);
//     *hgt = r / cos(phi) - ( a / (sqrt(1.0 - e2 * pow(sin(phi),2.0))));

     *hgt = r / cos(phi) - a*c;

#endif
}

void _geoEfg2Llh2 (double efg[], double *lat, double *lon, double *hgt)
{
    int i;
    double a, flat, b, e2, ee2,sign = 1.0;
    double r,phi,phi1,c;
    /* Get the ellipsoid parameters */
    // geoGetEllipsoid (&a, &b, &e2, &ee2, &flat, datum);
    /* - Major Axis. */
    a   = MAJ_AXIS;

    /* - Earth Flattening. */
    flat   = GEO_FL(INV_FLAT);

    /* - Minor axis value. */
    b   = GEO_B(MAJ_AXIS, (INV_FLAT));

    /* - Eccentricity squared. */
    e2  = GEO_E2(MAJ_AXIS, (INV_FLAT));

    /* - Eccentricity squared prime. */
    ee2 = GEO_E2P(MAJ_AXIS, (INV_FLAT));

    // Computes EFG to lat, lon & height
//    p = sqrt(sqr2(efg[GEO_E]) + sqr2(efg[GEO_F]));
//    u = atan((efg[GEO_G]/p) * ((1.0 - flat) + (e2 * a / (sqrt(sqr2(p)+sqr2(efg[GEO_G])))))); //  atan((efg[GEO_G]/p)  * (a/b));
//    *lat = atan2((efg[GEO_G]*(1.0 - flat) + e2 * a * pow(sin(u),3.0) ),
//          ((1.0-flat) * ( p - e2 * a * pow(cos(u),3.0) ) ) );
//    u_prime =  atan((1.0 - flat) * tan(*lat));
//    if((p - a * cos(u_prime) ) < 0.0) sign= -1.0; // determine sign

//    *hgt =  sign * sqrt( sqr2( p - a * cos(u_prime)) +
//                         sqr2(efg[GEO_G] - b * sin(u_prime)));
    *lon = atan2(efg[GEO_F], efg[GEO_E]);  // atan(f/e)
     r = sqrt(sqr2(efg[GEO_E]) + sqr2(efg[GEO_F]));
     phi = atan2(efg[GEO_G], r);
 //     printf("phi(1)=%f\n",phi * RAD_TO_DEG);
     for(i=0;i<15;i++)
     {
        phi1 = phi;
        c = sqrt(1.0 - e2*pow(sin(phi1),2.0));
        phi = atan2((efg[GEO_G] + a * c * e2 * sin(phi1)),r);
//        printf("phi=%f\n",phi * RAD_TO_DEG);
        if( phi == phi1) break;
     }
     *lat = phi;
//     printf("c=%f\n",c);
//     *hgt = r / cos(phi) - ( a / (sqrt(1.0 - e2 * pow(sin(phi),2.0))));

     *hgt = r / cos(phi) - a*c;

}

void _geoLlh2Efg (double lat, double lon, double height,
                    double *e,  double *f,  double *g)

{
    double N,a,e2,ee2, b, flat;

    /* Get the ellipsoid parameters */
    //geoGetEllipsoid(&a,&b,&e2,&ee2,&flat,datum);
    /* - Major Axis. */
    a   = MAJ_AXIS;
    /* - Earth Flattening. */
    flat   = GEO_FL(INV_FLAT);
    /* - Minor axis value. */
    b   = GEO_B(MAJ_AXIS, (INV_FLAT));
    /* - Eccentricity squared. */
    e2  = GEO_E2(MAJ_AXIS, (INV_FLAT));
    /* - Eccentricity squared prime. */
    ee2 = GEO_E2P(MAJ_AXIS, (INV_FLAT));


    /* Compute the radius of curvature */
    N = a / (sqrt(1.0 - e2 * pow(sin(lat),2.0)));

    /* Compute the EFG(XYZ) coordinates (earth centered) */
    *e = (N + height) * cos(lat) * cos(lon);
    *f = (N + height) * cos(lat) * sin(lon);
    *g = (N * (1.0 - e2) + height) * sin(lat);

}

#ifdef TEST_MPPY_CONV

/* ------------
g = {	'callsign' : 'sq_gear',
	'lon' : 0,
	'lat' : 0,
	'alt' : 80,
	'pch' : 10,	# nose-up positive
	'hdg' : 60,
	'bnk' : 30 } # clockwise positive
def pack_header(a, callsign):
  a.pack_uint(0x46474653)	# "FGFS"
  a.pack_uint(0x00010001)	# version
  a.pack_uint(7)		# POS_DATA_ID
  a.pack_uint(232)
  a.pack_uint(0)		# ReplyAddress
  a.pack_uint(5000)		# ReplyPort
  a.pack_fstring(8, callsign)

def pack_pos(a, c, o, model, v):
  global time0
  a.pack_fstring(96, model)
  a.pack_double(time.time() - time0)
  #a.pack_double(50)		# time
  a.pack_double(5)		# lag
  a.pack_double(c[0])		# position
  a.pack_double(c[1])
  a.pack_double(c[2])
  a.pack_float(o[0])		# orientation
  a.pack_float(o[1])
  a.pack_float(o[2])
  a.pack_float(v[0])		# linearVel
  a.pack_float(v[1])
  a.pack_float(v[2])
  a.pack_float(0)		# angularVel
  a.pack_float(0)
  a.pack_float(0)
  a.pack_float(0)		# linearAccel
  a.pack_float(0)
  a.pack_float(0)
  a.pack_float(0)		# angularAccel
  a.pack_float(0)
  a.pack_float(0)
  a.pack_uint(0)		# pad to 200 bytes

def open_mp(addr):
  """ opens an unconnected UDP socket """
  # called from main loop
  global s_mp, time0, mp_opened, mp_addr
  mp_opened = 0
  mp_addr = addr
  try:
    s_mp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    #s_mp.connect(addr)
    mp_opened = 1
  except socket.error:
    s_mp = None
    print 'ERROR opening multiplayer connection to FlightGear'
  time0 = time.time()
def inject_fg(g):
  global s_mp, mp_addr
  if s_mp == None: return
  tcas = xdrlib.Packer()
  c = geodtocart(g)
#  print "position ", c
  ornt = quat_getAngleAxis( quat_multiply(
		  quat_fromLonLat(g['lon'], g['lat']),
		  quat_fromEuler(g['hdg'], g['pch'], g['bnk']) ) )
  s = g['spd'] * (1852. / 3600)		# knot -> m/s
  v = [s, 0, 0]
    #v = quat_fromEuler(g['hdg'], g['pch'], 0)
    #v['x'] *= s
    #v['y'] *= s
    #v['z'] *= s
  #print v
  pack_header(tcas, g['callsign'])
  pack_pos(tcas, c, ornt, g['model'], v)
#  print g['callsign'], c, s, v, ornt
  try:
#    sys.stdout.write(tcas.get_buffer())
    i = s_mp.sendto(tcas.get_buffer(), mp_addr)
  except socket.error:
    # reset the socket on error
    s_mp.close()
    s_mp = None
    mp_opened = 0

  ------------- */

// def geodtocart(geod):
//  """ degrees and feet """
void geodtocart(double lat_degs, double lon_degs, double hgt_feet, double * xyz)
{
    // """ degrees and feet """
  double a = 6378137.0;
  double e2 = fabs(1 - 0.9966471893352525192801545 * 0.9966471893352525192801545);
  double lmbda  = DEGREES2RADIANS * lon_degs;
  double phi    = DEGREES2RADIANS * lat_degs;
  double h      = hgt_feet / 3.2808399;
  double sphi   = sin(phi);
  double cphi   = cos(phi);
  double n      = a / sqrt(1 - e2 * sphi * sphi);
  double slambda = sin(lmbda);
  double clambda = cos(lmbda);
  xyz[c_x] = (h+n)*cphi*clambda;
  xyz[c_y] = (h+n)*cphi*slambda;
  xyz[c_z] = (h+n-e2*n)*sphi;
}

//def quat_fromEuler(z, y, x):
//  """ degrees """
void quat_fromEuler(double x, double y, double z, double * xyzw)
{
    double zd2 = 0.5 * z * DEGREES2RADIANS;
    double yd2 = 0.5 * y * DEGREES2RADIANS;
    double xd2 = 0.5 * x * DEGREES2RADIANS;
    double Szd2 = sin(zd2);
    double Syd2 = sin(yd2);
    double Sxd2 = sin(xd2);
    double Czd2 = cos(zd2);
    double Cyd2 = cos(yd2);
    double Cxd2 = cos(xd2);
    double Cxd2Czd2 = Cxd2 * Czd2;
    double Cxd2Szd2 = Cxd2 * Szd2;
    double Sxd2Szd2 = Sxd2 * Szd2;
    double Sxd2Czd2 = Sxd2 * Czd2;
    xyzw[q_w] = Cxd2*Czd2*Cyd2 + Sxd2*Szd2*Syd2; // 'w' : Cxd2Czd2*Cyd2 + Sxd2Szd2*Syd2,
	xyzw[q_x] = Sxd2*Czd2*Cyd2 - Cxd2*Szd2*Syd2; // 'x' : Sxd2Czd2*Cyd2 - Cxd2Szd2*Syd2,
	xyzw[q_y] = Cxd2*Czd2*Syd2 + Sxd2*Szd2*Cyd2; // 'y' : Cxd2Czd2*Syd2 + Sxd2Szd2*Cyd2,
	xyzw[q_z] = Cxd2*Szd2*Cyd2 - Sxd2*Czd2*Syd2; // 'z' : Cxd2Szd2*Cyd2 - Sxd2Czd2*Syd2 }
}

//def quat_fromLonLat(lon, lat):
//  """ degrees """
void quat_fromLonLat(double lon_degs, double lat_degs, double * xyzw)
{
    double zd2 = 0.5 * lon_degs * DEGREES2RADIANS;
    double yd2 = -0.25 * M_PI - 0.5 * lat_degs * DEGREES2RADIANS;
    double Szd2 = sin(zd2);
    double Syd2 = sin(yd2);
    double Czd2 = cos(zd2);
    double Cyd2 = cos(yd2);
    xyzw[q_w] = Czd2*Cyd2;  // 'w' : Czd2*Cyd2,
	xyzw[q_x] = -Szd2*Syd2; // 'x' : -Szd2*Syd2,
	xyzw[q_y] = Czd2*Syd2;  // 'y' : Czd2*Syd2,
	xyzw[q_z] = Szd2*Cyd2;  // 'z' : Szd2*Cyd2
}

//def quat_multiply(v1, v2):
//  return {
//    'x': v1['w']*v2['x'] + v1['x']*v2['w'] + v1['y']*v2['z'] - v1['z']*v2['y'],
//    'y': v1['w']*v2['y'] - v1['x']*v2['z'] + v1['y']*v2['w'] + v1['z']*v2['x'],
//    'z': v1['w']*v2['z'] + v1['x']*v2['y'] - v1['y']*v2['x'] + v1['z']*v2['w'],
//    'w': v1['w']*v2['w'] - v1['x']*v2['x'] - v1['y']*v2['y'] - v1['z']*v2['z']
//  }
void quat_multiply(double v1[], double v2[], double * xyzw)
{
    xyzw[q_x] = v1[q_w]*v2[q_x] + v1[q_x]*v2[q_w] + v1[q_y]*v2[q_z] - v1[q_z]*v2[q_y];
    xyzw[q_y] = v1[q_w]*v2[q_y] - v1[q_x]*v2[q_z] + v1[q_y]*v2[q_w] + v1[q_z]*v2[q_x];
    xyzw[q_z] = v1[q_w]*v2[q_z] + v1[q_x]*v2[q_y] - v1[q_y]*v2[q_x] + v1[q_z]*v2[q_w];
    xyzw[q_w] = v1[q_w]*v2[q_w] - v1[q_x]*v2[q_x] - v1[q_y]*v2[q_y] - v1[q_z]*v2[q_z];
}

//def quat_getAngleAxis(q):
//  """ returns a Vec3 """
int quat_getAngleAxis(double q[], double * v)
{
    double nrm = sqrt(q[q_x]*q[q_x] + q[q_y]*q[q_y] + q[q_z]*q[q_z] + q[q_w]*q[q_w]);
    //  #if nrm <= ?limit? return 0
    if (nrm <= 0.0) return 0;
    double rNrm = 1 / nrm;
    double a = rNrm * q[q_w];
    if (a > 1.0)
        a = 1.0;
    else if (a < -1.0)
        a = -1.0;
    double angle = acos(a);
    double sAng = sin(angle);
    //  #if math.fabs(sAng) < ?limit?
    if (fabs(sAng) < 1e-16) {
        // return [ 1, 0, 0 ]
        v[c_x] = 1.0;
        v[c_y] = 0.0;
        v[c_z] = 0.0;
    } else {
        angle *= 2.0;
        double d = (rNrm/sAng)*angle;
        // return [ d*q['x'], d*q['y'], d*q['z'] ]
        v[c_x] = d*q[q_x];
        v[c_y] = d*q[q_y];
        v[c_z] = d*q[q_z];
    }
    return 1;
}

//  ornt = quat_getAngleAxis( quat_multiply(
//		  quat_fromLonLat(g['lon'], g['lat']),
//		  quat_fromEuler(g['hdg'], g['pch'], g['bnk']) ) )
void setOrientation( GSTRUC & g, double * ornt )
{
    double q1[4];
    double q2[4];
    double qm[4];
    quat_fromLonLat( g.lon_degs, g.lat_degs, &q1[0] );
    quat_fromEuler(g.hdg, g.pch, g.bnk, &q2[0]);
    quat_multiply( q1, q2, &qm[0] );
    quat_getAngleAxis( qm, ornt );
}

#endif // #ifdef TEST_MPPY_CONV

static char * sampinraw = (char *)"sample-raw.bin";
#ifndef ISNUM
#define ISNUM(a) ((a >= '0')&&(a <= '9'))
#endif

#ifndef ADDED_IS_DIGITS
#define ADDED_IS_DIGITS
static int is_digits(char * arg)
{
    size_t len,i;
    len = strlen(arg);
    for (i = 0; i < len; i++) {
        if ( !ISNUM(arg[i]) )
            return 0;
    }
    return 1; /* is all digits */
}
#endif // #ifndef ADDED_IS_DIGITS

static void show_help(char * name)
{
    printf("%s [Options] [Input_file]\n", name);
    printf("Version: %s, compiled %s, at %s\n", PACKAGE_VERSION, __DATE__, __TIME__);
    printf("\nOptions\n");
    printf(" -h (or -?) = This help and exit 0\n");
    printf(" -v[nn]     = Bump, or set verbosity to 'nn'. def =%d\n", _s_verb);
    printf("If no input file given, will default to [%s]\n", sampinraw );
    printf("\nInput file will be parsed as a 'raw' dump of FlightGear Multiplayer\n");
    printf(" messages, padded to 16 bytes, plus a 16 byte seperator.\n");
}

void show_sizes(void)
{
    short s = -1;
    int i = -1;
    size_t u = i;
    size_t u2 = u / 2;
    printf("Short %u (+/-%u), Int %u (+/-%u)\n", (s & 0xffff), (s & 0xffff) / 2,
        u, u2 );
    exit(1);
}


int main(int argc, char *argv[])
{
    int i, iret, val;
    char * file = sampinraw;
    char * arg;
    // show_sizes();
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        if (*arg == '-') {
            while(*arg == '-') arg++;
            switch (*arg)
            {
            case '?':
            case 'h':
                show_help(argv[0]);
                return 0;
                break;
            case 'v':
                if (is_digits(&arg[1])) {
                    val = atoi(&arg[1]);
                    _s_verb = val;
                } else {
                    while (*arg == 'v') {
                        _s_verb++;
                        arg++;
                    }
                }
                printf("Set verbosity to %d\n",_s_verb);
                break;
            default:
                printf("ERROR: Unknown option [%s]. Aborting...\n", argv[i]);
                return 1;
            }
        } else {
            file = arg;
            printf("Set input file to %s\n",file);
        }
    }
    iret = test_fgmp_message(file);
    return iret;
}

// eof - test_fgmp_message.cxx
