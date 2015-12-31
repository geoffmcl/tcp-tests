// fg_dist.cpp : Defines the entry point for the console application.
//

#include "fg_dist.hxx"
#include <simgear\math\sg_geodesy.hxx>

#define  ISNUM(a) (( a >= '0' ) && ( a <= '9' ))

static int verbosity = 1;
#define  VERB1 (verbosity > 0)
#define  VERB2 (verbosity > 1)
#define  VERB3 (verbosity > 2)
#define  VERB4 (verbosity > 3)

double D2R = M_PI / 180.0;
double R2D = 180.0 / M_PI;
double ERAD = 6378138.12;

double FG_EQURAD = 6378137.0;    // also the IUGG value

// High-precision versions of the above produced with an arbitrary
// precision calculator (the compiler might lose a few bits in the FPU
// operations).  These are specified to 81 bits of mantissa, which is
// higher than any FPU known to me:
double HP_SQUASH = 0.9966471893352525192801545;
double HP_STRETCH = 1.0033640898209764189003079;
double HP_POLRAD = 6356752.3142451794975639668;

double WIKI_ERAD1 = 6356750.0;   // polar radius
double WIKI_ERAD2 = 6378135.0;   // equatorial radius

char * get_short_name( char * pgm )
{
   char * p = pgm;
   char * cp = _strdup(p);
   if(cp) {
      p = strrchr(cp,'\\');
      if(p)
         p++;
      else
         p = cp;
   }
   return p;
}

void give_help( char * pgm )
{
   char * p = get_short_name(pgm);
   printf( "%s [Options] position1 position2\n", p );
   printf( "Options:\n" );
   printf( " -? or -h This brief help.\n" );
   printf( " -v[vvv]  Set verbosity.\n" );
   printf( "The positions must be given as a latitude,logitude string,\n" );
   printf( "like 0.0,0.0 22.3083428148148,113.917764876543\n" );
   exit(2);
}

int is_number( char * arg )
{
   int   j, ok;
   char c;
   size_t len = strlen(arg);
   size_t i;

   j = ok = 0;
   for( i = 0; i < len; i++ )
   {
      c = arg[i];
      if( c == ',' ) {
         j++;
      } else if( (c == '.') || ISNUM(c) || ( c == '+' ) || ( c == '-' ) ) {
         ok++; // ok
      } else {
         return 0;
      }
   }
   if( ( j == 0 ) || ( j > 1 ) )
      return 0;

   return 1;
}

void ll2xyz( double in_lon, double in_lat, double * pi, double * pj, double * pk)
{
   double lon = in_lon * D2R;
   double lat = in_lat * D2R;
	double cosphi = cos(lat);
	*pi = cosphi * cos(lon);
	*pj = cosphi * sin(lon);
	*pk = sin( lat );
}

void xyz2ll( double di, double dj, double dk, double * plat, double *plon )
{
   double aux = (di * di) + (dj * dj);
	*plat = atan2(dk, sqrt(aux) * R2D);
	*plon = atan2(dj, di) * R2D;
}

double coord_dist_sq( double xa, double ya, double za, double xb, double yb, double zb )
{
   double x = xb - xa;
   double y = yb - ya;
   double z = zb - za;
	return ((x * x) + (y * y) + (z * z));
}

int _tmain(int argc, _TCHAR* argv[])
{
   int   i, j, ok;
   double d[4];
   SGGeod pos1, pos2;	// WGS84 lat & lon in degrees, elev above sea-level in meters
   double course1, course2, distance, nm;
   char  c;
   double x1, y1, z1, lat1, lon1;
   double x2, y2, z2, lat2, lon2;
   double d2;

   printf( "Running %s ...\n", get_short_name(argv[0]) );
   if( argc < 3 )
      give_help(argv[0]);

   j = 0;
   ok = 0;
   for( i = 1; i < argc; i++ )
   {
      char * arg = argv[i];
      if(arg && *arg)
      {
         c = *arg;
         if( (( c == '-' )||( c == '/' )) && !is_number(arg) )
         {
            arg++;
            c = toupper(*arg);
            if(( c == '?' )||( c == 'H' ))
            {
               give_help(argv[0]);
            }
            else if( c == 'V' )
            {
               verbosity++;
               arg++;
               while( toupper(*arg) == 'V' )
               {
                  verbosity++;
                  arg++;
               }
            }
            else
            {
               printf("ERROR: UKNONWN Argument [%s]!\n", argv[i] );
               exit(1);
            }
         }
         else if( is_number(arg) )
         {
            char * cp = _strdup(arg);
            if(cp)
            {
               char * p = strchr(cp,',');
               if(p)
               {
                  *p = 0;
                  p++;
                  if(j)
                  {
                     d[2] = atof(cp);
                     d[3] = atof(p);
                     pos2.setElevationFt(0.0);
                     lat2 = d[2];
                     lon2 = d[3];
                     pos2.setLatitudeDeg(lat2);
                     pos2.setLongitudeDeg(lon2);
                     ok = 1;
                     ll2xyz( lon2, lat2, &x2, &y2, &z2 );
                  }
                  else
                  {
                     d[0] = atof(cp);
                     d[1] = atof(p);
                     j++;
                     lat1 = d[0];
                     lon1 = d[1];
                     pos1.setElevationFt(0.0);
                     pos1.setLatitudeDeg(lat1);
                     pos1.setLongitudeDeg(lon1);
                     ll2xyz( lon1, lat1, &x1, &y1, &z1 );
                  }
               }
               else
                  goto Error1;

               free(cp);
            }
            else
            {
               printf("ERROR: MEMORY ALLOCATION FAILED!\n" );
               exit(1);
            }
         }
         else
         {
Error1:
            printf("ERROR: Argument [%s] is NOT two numeric values, comma separated!\n", arg);
            exit(1);
         }
      }
   }
   if(ok)
   {
      SGGeodesy::inverse( pos1, pos2, course1, course2, distance);
      nm = distance * SG_METER_TO_NM;
      printf( "SGGeodesy dist %f,%f to %f,%f is %f Km (%f nm).\n",
         d[0], d[1], d[2], d[3], distance / 1000.0, nm );

      printf( "geocentric (x1=%f y1=%f z1=%f) to (x2=%f y2=%f z2=%f)\n",
            x1, y1, z1, x1, y2, z2 );
      d2 = coord_dist_sq( x1, y1, z1, x2, y2, z2 );
      printf( "Using coord_dist_sq %f = %f Km.\n",
          d2,
         (sqrt(d2) *  ERAD) / 1000.0 );

      double az1,az2,s;
      geo_inverse_wgs_84( d[0], d[1], d[2], d[3], &az1, &az2, &s);
      printf("Results of geo_inverse_wgs_84(%f,%f to %f,%f)\n",  d[0], d[1], d[2], d[3]);
      printf("Distance %f km, %f nm, on %d (%d)\n",
          s / 1000.0, s * SG_METER_TO_NM,
          (int)(az1 + 0.5), (int)(az2 + 0.5));
   }
   if(VERB3)
   {
      distance = M_PI * SG_EARTH_RAD * 2.0 ;
      nm = distance * 1000.0 * SG_METER_TO_NM;
      printf( "Earths Radius is %f Km, circumference %f Km (%f nm).\n",
         SG_EARTH_RAD, distance, nm );

      lat1 = 0.0;
      lon1 = 0.0;
      pos1.setElevationFt(0.0);
      pos1.setLatitudeDeg(lat1);
      pos1.setLongitudeDeg(lon1);
      lat2 = 0.0;
      lon2 = 89.9999;
      pos2.setElevationFt(0.0);
      pos2.setLatitudeDeg(lat2);
      pos2.setLongitudeDeg(lon2);
      SGGeodesy::inverse( pos1, pos2, course1, course2, distance);
      nm = distance * SG_METER_TO_NM;
      ll2xyz( lon1, lat1, &x1, &y1, &z1 );
      ll2xyz( lon2, lat2, &x2, &y2, &z2 );
      d2 = coord_dist_sq( x1, y1, z1, x2, y2, z2 );
      printf( "Distance from %f,%f to %f,%f is %f Km (%f nm) circum=%f nm. (d=%f Km)\n",
            pos1.getLatitudeDeg(), pos1.getLongitudeDeg(),
            pos2.getLatitudeDeg(), pos2.getLongitudeDeg(),
            distance / 1000.0, nm, nm * 4.0,
            (sqrt(d2) *  ERAD) / 1000.0 );
      if(VERB4)
      {
         printf( "x1=%f y1=%f z1=%f x2=%f y2=%f z2=%f \n",
            x1, y1, z1, x1, y2, z2 );
      }
      lat2 = 89.9999;
      lon2 = 0.0;
      pos2.setLatitudeDeg(lat2);
      pos2.setLongitudeDeg(lon2);
      SGGeodesy::inverse( pos1, pos2, course1, course2, distance);
      nm = distance * SG_METER_TO_NM;
      ll2xyz( lon2, lat2, &x2, &y2, &z2 );
      d2 = coord_dist_sq( x1, y1, z1, x2, y2, z2 );
      printf( "Distance from %f,%f to %f,%f is %f Km (%f nm) circum=%f nm. (d=%f Km)\n",
            pos1.getLatitudeDeg(), pos1.getLongitudeDeg(),
            pos2.getLatitudeDeg(), pos2.getLongitudeDeg(),
            distance / 1000.0, nm, nm * 4.0,
            (sqrt(d2) *  ERAD) / 1000.0 );
      if(VERB4)
      {
         printf( "x1=%f y1=%f z1=%f x2=%f y2=%f z2=%f \n",
            x1, y1, z1, x1, y2, z2 );
      }

   }
	return 0;
}


// eof - fg_dist.cxx
