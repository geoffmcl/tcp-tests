//////////////////////////////////////////////////////////////////////
//
//	Tiny XDR implementation for flightgear
//	written by Oliver Schroeder
//	released to the public domain
//
//	This implementation is not complete, but implements
//	everything we need.
//
//	For further reading on XDR read RFC 1832.
//
//  NEW
//
//////////////////////////////////////////////////////////////////////

#ifndef TINY_XDR_HEADER
#define TINY_XDR_HEADER

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include <simgear/misc/stdint.hxx>
#include <simgear/debug/logstream.hxx>

#ifdef USE_FG_TINY_XDR

#define SWAP32(arg) sgIsLittleEndian() ? sg_bswap_32(arg) : arg
#define SWAP64(arg) sgIsLittleEndian() ? sg_bswap_64(arg) : arg

#define XDR_BYTES_PER_UNIT  4

typedef uint32_t    xdr_data_t;      /* 4 Bytes */
typedef uint64_t    xdr_data2_t;     /* 8 Bytes */

/* XDR 8bit integers */
xdr_data_t      XDR_encode_int8     ( const int8_t &  n_Val );
xdr_data_t      XDR_encode_uint8    ( const uint8_t & n_Val );
int8_t          XDR_decode_int8     ( const xdr_data_t & n_Val );
uint8_t         XDR_decode_uint8    ( const xdr_data_t & n_Val );

/* XDR 16bit integers */
xdr_data_t      XDR_encode_int16    ( const int16_t & n_Val );
xdr_data_t      XDR_encode_uint16   ( const uint16_t & n_Val );
int16_t         XDR_decode_int16    ( const xdr_data_t & n_Val );
uint16_t        XDR_decode_uint16   ( const xdr_data_t & n_Val );

/* XDR 32bit integers */
xdr_data_t      XDR_encode_int32    ( const int32_t & n_Val );
xdr_data_t      XDR_encode_uint32   ( const uint32_t & n_Val );
int32_t         XDR_decode_int32    ( const xdr_data_t & n_Val );
uint32_t        XDR_decode_uint32   ( const xdr_data_t & n_Val );

/* XDR 64bit integers */
xdr_data2_t     XDR_encode_int64    ( const int64_t & n_Val );
xdr_data2_t     XDR_encode_uint64   ( const uint64_t & n_Val );
int64_t         XDR_decode_int64    ( const xdr_data2_t & n_Val );
uint64_t        XDR_decode_uint64   ( const xdr_data2_t & n_Val );

//////////////////////////////////////////////////
//
//  FIXME: #1 these funtions must be fixed for
//         none IEEE-encoding architecturs
//         (eg. vax, big suns etc)
//  FIXME: #2 some compilers return 'double'
//         regardless of return-type 'float'
//         this must be fixed, too
//  FIXME: #3 some machines may need to use a
//         different endianess for floats!
//
//////////////////////////////////////////////////
/* float */
xdr_data_t      XDR_encode_float    ( const float & f_Val );
float           XDR_decode_float    ( const xdr_data_t & f_Val );

/* double */
xdr_data2_t     XDR_encode_double   ( const double & d_Val );
double          XDR_decode_double   ( const xdr_data2_t & d_Val );

#else // #ifdef USE_FG_TINY_XDR

#define SWAP16(arg) sgIsLittleEndian() ? sg_bswap_16(arg) : arg
#define SWAP32(arg) sgIsLittleEndian() ? sg_bswap_32(arg) : arg
#define SWAP64(arg) sgIsLittleEndian() ? sg_bswap_64(arg) : arg
#define XDR_BYTES_PER_UNIT  4

typedef uint32_t    xdr_data_t;      /* 4 Bytes */
typedef uint64_t    xdr_data2_t;     /* 8 Bytes */

#ifdef FG_NDEBUG
#       undef FG_TMPDEBUG
#       define FG_NDEBUG
#endif
#define FG_NDEBUG

/**
 * xdr encode 8, 16 and 32 Bit values
 */
template<typename TYPE>
xdr_data_t XDR_encode ( TYPE Val )
{
	union
	{
		xdr_data_t	encoded;
		TYPE		raw;
	} tmp;

	tmp.raw = Val;
        tmp.encoded = SWAP32(tmp.encoded);
        if (sizeof (TYPE) < 4)
        {
                SG_LOG (SG_IO, SG_DEBUG, "XDR_encode ("
                  << (int32_t) Val << ") -> " << (int32_t) tmp.encoded);
        }
        else
        {
                SG_LOG (SG_IO, SG_DEBUG, "XDR_encode ("
                  << (int32_t) Val << ") -> " << tmp.encoded);
        }
	return (tmp.encoded);
}

/**
 * xdr decode 8, 16 and 32 Bit values
 */
template<typename TYPE>
TYPE XDR_decode ( xdr_data_t Val )
{
	union
	{
		xdr_data_t	encoded;
		TYPE		raw;
	} tmp;

	tmp.encoded = SWAP32(Val);
        if (sizeof (TYPE) < 4)
        {
                SG_LOG (SG_IO, SG_DEBUG, "XDR_decode (" << (int32_t) Val
                  << ") -> " << (int32_t) tmp.raw);
        }
        else
        {
                SG_LOG (SG_IO, SG_DEBUG, "XDR_decode (" << (int32_t) Val
                  << ") -> " << tmp.raw);
        }
	return (tmp.raw);
}

/**
 * xdr encode 64 Bit values
 */
template<typename TYPE>
xdr_data2_t XDR_encode64 ( TYPE Val )
{
	union
	{
		xdr_data2_t	encoded;
		TYPE		raw;
	} tmp;

	tmp.raw = Val;
        tmp.encoded = SWAP64(tmp.encoded);
        SG_LOG (SG_IO, SG_DEBUG, "XDR_encode64 (" << (int32_t) Val << ") -> "
          << tmp.encoded);
	return (tmp.encoded);
}

/**
 * xdr decode 64 Bit values
 */
template<typename TYPE>
TYPE XDR_decode64 ( xdr_data2_t Val )
{
	union
	{
		xdr_data2_t	encoded;
		TYPE		raw;
	} tmp;

	tmp.encoded = SWAP64 (Val);
        SG_LOG (SG_IO, SG_DEBUG, "XDR_decode64 (" << (int32_t) Val << ") -> "
          << tmp.raw);
	return (tmp.raw);
}


//////////////////////////////////////////////////////////////////////
//
//      encode to network byte order
//
/////////////////////////////////////////////////////////////////////

/**
 * encode 8-Bit values to network byte order
 * (actually encodes nothing, just to satisfy the templates)
 */
template<typename TYPE>
uint8_t
NET_encode8 ( TYPE Val )
{
        union
        {
                uint8_t netbyte;
                TYPE    raw;
        } tmp;

        tmp.raw = Val;
        SG_LOG (SG_IO, SG_DEBUG, "NET_encode8 (" << (int32_t) Val << ") -> "
          << (int32_t) tmp.netbyte);
        return (tmp.netbyte);
}

/**
 * decode 8-Bit values from network byte order
 * (actually decodes nothing, just to satisfy the templates)
 */
template<typename TYPE>
TYPE
NET_decode8 ( uint8_t Val )
{
        union
        {
                uint8_t netbyte;
                TYPE    raw;
        } tmp;

        tmp.netbyte = Val;
        SG_LOG (SG_IO, SG_DEBUG, "NET_decode8 (" << (int32_t) Val << ") -> "
          << (int32_t) tmp.raw);
        return (tmp.raw);
}

/**
 * encode 16-Bit values to network byte order
 */
template<typename TYPE>
uint16_t
NET_encode16 ( TYPE Val )
{
        union
        {
                uint16_t        netbyte;
                TYPE            raw;
        } tmp;

        tmp.raw = Val;
        tmp.netbyte = SWAP16(tmp.netbyte);
        SG_LOG (SG_IO, SG_DEBUG, "NET_encode16 (" << Val << ") -> "
          << tmp.netbyte);
        return (tmp.netbyte);
}

/**
 * decode 16-Bit values from network byte order
 */
template<typename TYPE>
TYPE
NET_decode16 ( uint16_t Val )
{
        union
        {
                uint16_t        netbyte;
                TYPE            raw;
        } tmp;

        tmp.netbyte = SWAP16(Val);
        SG_LOG (SG_IO, SG_DEBUG, "NET_decode16 (" << Val << ") -> "
          << tmp.raw);
        return (tmp.raw);
}

/**
 * encode 32-Bit values to network byte order
 */
template<typename TYPE>
uint32_t
NET_encode32 ( TYPE Val )
{
        union
        {
                uint32_t        netbyte;
                TYPE            raw;
        } tmp;

        tmp.raw = Val;
        tmp.netbyte = SWAP32(tmp.netbyte);
        SG_LOG (SG_IO, SG_DEBUG, "NET_encode32 (" << Val << ") -> "
          << tmp.netbyte);
        return (tmp.netbyte);
}

/**
 * decode 32-Bit values from network byte order
 */
template<typename TYPE>
TYPE
NET_decode32 ( uint32_t Val )
{
        union
        {
                uint32_t        netbyte;
                TYPE            raw;
        } tmp;

        tmp.netbyte = SWAP32(Val);
        SG_LOG (SG_IO, SG_DEBUG, "NET_decode32 (" << Val << ") -> "
          << tmp.raw);
        return (tmp.raw);
}

/**
 * encode 64-Bit values to network byte order
 */
template<typename TYPE>
uint64_t
NET_encode64 ( TYPE Val )
{
        union
        {
                uint64_t        netbyte;
                TYPE            raw;
        } tmp;

        tmp.raw = Val;
        tmp.netbyte = SWAP64(tmp.netbyte);
        SG_LOG (SG_IO, SG_DEBUG, "NET_encode64 (" << Val << ") -> "
          << tmp.netbyte);
        return (tmp.netbyte);
}

/**
 * decode 64-Bit values from network byte order
 */
template<typename TYPE>
TYPE
NET_decode64 ( uint64_t Val )
{
        union
        {
                uint64_t        netbyte;
                TYPE            raw;
        } tmp;

        tmp.netbyte = SWAP64(Val);
        SG_LOG (SG_IO, SG_DEBUG, "NET_decode64 (" << Val << ") -> "
          << tmp.raw);
        return (tmp.raw);
}

#endif // #ifdef USE_FG_TINY_XDR y/n


#endif
// eof - copy of tiny_xdr.hxx
