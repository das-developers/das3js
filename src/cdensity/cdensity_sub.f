C-----------------------------------------------------------------------
      SUBROUTINE GET_KHRONOS ( Observer, Target, BodyName, ET, Radius,
     +                         Latitude, MagLatitude, Longitude,
     +                         LocalTime, LValue )
      
      IMPLICIT NONE

      INTEGER               Observer
      INTEGER               Target
      INTEGER               BodyName
      DOUBLE PRECISION      ET
      DOUBLE PRECISION      Radius
      DOUBLE PRECISION      Latitude
      DOUBLE PRECISION      MagLatitude
      DOUBLE PRECISION      Longitude
      DOUBLE PRECISION      LocalTime
      DOUBLE PRECISION      LValue

C
C   Local parameters
C
      INTEGER               Sun
      INTEGER               Enceladus

      PARAMETER           ( Sun       =    10 )
      PARAMETER           ( Enceladus =   602 )

C
C   Local function
C
      DOUBLE PRECISION      GET_EQUATORIAL_RADII

C
C   SPICE functions
C
      DOUBLE PRECISION      DPR
      DOUBLE PRECISION      TWOPI

C
C   Local variables
C
      INTEGER               Index
      DOUBLE PRECISION      Term1
      DOUBLE PRECISION      Term2
      DOUBLE PRECISION      Dummy1
      DOUBLE PRECISION      Dummy2
      DOUBLE PRECISION      Sun_Long
      DOUBLE PRECISION      LT
      DOUBLE PRECISION      RBody
      DOUBLE PRECISION      MagRadius
      DOUBLE PRECISION      Temp1     ( 3 )
      DOUBLE PRECISION      Temp2     ( 3 )
      DOUBLE PRECISION      SC_Pos    ( 3 )
      DOUBLE PRECISION      Sun_Pos   ( 3 )
      DOUBLE PRECISION      State     ( 6 )
      DOUBLE PRECISION      Tipm   ( 3, 3 )

C-----------------------------------------------------------------------
C
C   determine radius
C
      RBody = GET_EQUATORIAL_RADII ( BodyName )

      CALL SPKEZ ( Target, ET, 'J2000', 'LT+S', Observer, State, LT )

      DO 10 Index = 1, 3
         Temp1 ( Index ) = State ( Index )
   10 CONTINUE

      CALL VMINUS ( Temp1, Temp2 )

      CALL BODMAT ( BodyName, ( ET - LT ), TIPM )

      CALL MXV ( TIPM, Temp2, SC_Pos )

      CALL RECLAT ( SC_Pos, Radius, Longitude, Latitude )

C
C   determine geographic longitude and latitude
C
      IF ( Longitude .LT. 0.0D0 ) THEN
         Longitude = ( Longitude + TWOPI () )
      ENDIF

      Latitude = ( Latitude*DPR () )

      Longitude = ( TWOPI () - Longitude )
      Longitude = ( Longitude*DPR () )

C
C   determine longitude of Sun
C
      CALL SPKEZ ( Sun, ( ET - LT ), 'J2000', 'NONE', Target, State,
     +             Dummy1 )

      DO 20 Index = 1, 3
         Temp1 ( Index ) = State ( Index )
   20 CONTINUE

      CALL MXV ( Tipm, Temp1, Sun_Pos )

      CALL RECLAT ( Sun_Pos, Dummy1, Sun_Long, Dummy2 )

      IF ( Sun_Long .LT. 0.0D0 ) THEN
         Sun_Long = ( Sun_Long + TWOPI () )
      ENDIF

      Sun_Long = ( TWOPI () - Sun_Long )
      Sun_Long = ( Sun_Long*DPR () )

C
C   determine local time
C
      LocalTime = ( Sun_Long - Longitude )
      LocalTime = ( LocalTime + 180.0D0 )
      LocalTime = ( LocalTime/15.0D0 )

      IF ( LocalTime .LT. 0.0D0 ) THEN
         LocalTime = ( LocalTime + 24.0D0 )
      ENDIF

      IF ( LocalTime .GE. 24.0D0 ) THEN
         LocalTime = ( LocalTime - 24.0D0 )
      ENDIF

      Radius = ( Radius/RBody )

      IF ( BodyName .EQ. Enceladus ) THEN
         LValue = -1.0D0

         MagLatitude = -1.0D0

         RETURN
      ENDIF

C
C   determine L
C
      CALL GetMagneticCoords ( Observer, Target, BodyName, ET,
     +                         MagRadius, MagLatitude )

      Term1 = DCOS ( MagLatitude )
      Term2 = ( Term1*Term1 )

      LValue = ( MagRadius/Term2 )

      MagLatitude = ( MagLatitude*DPR () )

C-----------------------------------------------------------------------
      RETURN
      END

C-----------------------------------------------------------------------
C-----------------------------------------------------------------------
      SUBROUTINE GetMagneticCoords ( Observer, Target, BodyName, ET,
     +                               MagRadius, MagLatitude )

      IMPLICIT NONE

      INTEGER               Observer
      INTEGER               Target
      INTEGER               BodyName
      DOUBLE PRECISION      ET
      DOUBLE PRECISION      MagRadius
      DOUBLE PRECISION      MagLatitude

C
C   Local parameter
C
      DOUBLE PRECISION      DipoleOffset
      DOUBLE PRECISION      RSaturnModel

      PARAMETER           ( DipoleOffset  = 0.038D0   )
      PARAMETER           ( RSaturnModel  = 60330.0D0 )

C
C   Local variables
C
      INTEGER               Index
      DOUBLE PRECISION      LT
      DOUBLE PRECISION      DTemp1
      DOUBLE PRECISION      DTemp2
      DOUBLE PRECISION      Temp1     ( 3 )
      DOUBLE PRECISION      Temp2     ( 3 )
      DOUBLE PRECISION      SC_Pos    ( 3 )
      DOUBLE PRECISION      State     ( 6 )
      DOUBLE PRECISION      Tipm   ( 3, 3 )

      CALL SPKEZ ( Target, ET, 'J2000', 'LT+S', Observer, State, LT )

      DO 10 Index = 1, 3
         Temp1 ( Index ) = State ( Index )
   10 CONTINUE

      CALL VMINUS ( Temp1, Temp2 )

      CALL BODMAT ( BodyName, ( ET - LT ), TIPM )

      CALL MXV ( TIPM, Temp2, SC_Pos )

      DTemp1 = DSQRT ( ( SC_Pos ( 1 )*SC_Pos ( 1 ) ) +
     +                 ( SC_Pos ( 2 )*SC_Pos ( 2 ) ) )

      DTemp2 = SC_Pos ( 3 )

      DTemp2 = ( DTemp2 - ( DipoleOffset*RSaturnModel ) )

      MagRadius = DSQRT ( ( DTemp1*DTemp1 ) + ( DTemp2*DTemp2 ) )

      MagRadius = ( MagRadius/RSaturnModel )

      MagLatitude = DATAN2 ( DTemp2, DTemp1 )

C-----------------------------------------------------------------------
      RETURN
      END

C-----------------------------------------------------------------------
C-----------------------------------------------------------------------
      SUBROUTINE GET_ALTITUDE ( Satellite, Observer, ET, ALT, Radius )

      IMPLICIT NONE

      INTEGER               Satellite
      INTEGER               Observer
      DOUBLE PRECISION      ET
      DOUBLE PRECISION      ALT
      DOUBLE PRECISION      Radius

C
C   SPICELIB functions
C
      DOUBLE PRECISION      ZZ_VNORM

C
C   Local function
C
      DOUBLE PRECISION      GET_EQUATORIAL_RADII

C
C   Local parameters
C
      INTEGER               Cassini
      INTEGER               Saturn
      INTEGER               Enceladus
      INTEGER               Titan
      INTEGER               CORLEN
      INTEGER               MTHLEN

      PARAMETER           ( Cassini   = -82 )
      PARAMETER           ( Saturn    = 699 )
      PARAMETER           ( Enceladus = 602 )
      PARAMETER           ( Titan     = 606 )
      PARAMETER           ( CORLEN    =   5 )
      PARAMETER           ( MTHLEN    =  50 )

C
C   Local variables
C
      INTEGER               Index
      DOUBLE PRECISION      LT
      DOUBLE PRECISION      Dummy1
      DOUBLE PRECISION      Dummy2
      DOUBLE PRECISION      RSaturn
      DOUBLE PRECISION      TRGEPC
      DOUBLE PRECISION      SPOINT    ( 3 )
      DOUBLE PRECISION      SRFVEC    ( 3 )
      DOUBLE PRECISION      Temp1     ( 3 )
      DOUBLE PRECISION      Position  ( 3 )
      DOUBLE PRECISION      State     ( 6 )
      CHARACTER*(CORLEN)    ABCORR
      CHARACTER*(MTHLEN)    FIXREF
      CHARACTER*(MTHLEN)    METHOD
      CHARACTER*(MTHLEN)    OBSRVR
      CHARACTER*(MTHLEN)    TARGET

C
C   Initial values
C
      DATA                  ABCORR / 'LT+S' /

      IF ( Observer .EQ. Cassini ) THEN
         OBSRVR = 'CASSINI'
      ENDIF

      IF ( Satellite .EQ. Saturn ) THEN
         TARGET = 'SATURN'
         FIXREF = 'IAU_SATURN'
      ELSE IF ( Satellite .EQ. Enceladus ) THEN
         TARGET = 'ENCELADUS'
         FIXREF = 'IAU_ENCELADUS'
      ELSE IF ( Satellite .EQ. Titan ) THEN
         TARGET = 'TITAN'
         FIXREF = 'IAU_TITAN'
      ENDIF

C
C   Compute the sub-spacecraft point using the
C   "NEAR POINT: ELLIPSOID" definition.
C   Compute the results using the LT+S
C   aberration correction.
C
      METHOD = 'Near point: ellipsoid'

      CALL SUBPNT ( METHOD,
     +              TARGET, ET, FIXREF, ABCORR,
     +              OBSRVR, SPOINT, TRGEPC, SRFVEC )

C
C   Compute the observer's altitude above SPOINT.
C
      ALT = ZZ_VNORM ( SRFVEC )

C
C   determine radial distance
C
      RSaturn = GET_EQUATORIAL_RADII ( Satellite )

      CALL SPKEZ ( Satellite, ET, 'J2000', 'LT+S', Observer, State, LT )

      DO 10 Index = 1, 3
         Temp1 ( Index ) = State ( Index )
   10 CONTINUE

      CALL VMINUS ( Temp1, Position )

      CALL RECLAT ( Position, Radius, Dummy1, Dummy2 )

      Radius = ( Radius/RSaturn )

C-----------------------------------------------------------------------
      RETURN
      END

C-----------------------------------------------------------------------
C-----------------------------------------------------------------------
      SUBROUTINE GET_MAG_EPHEM ( Observer, Target, BodyName, ET, Radius,
     +                           CoLatitude )
      
      IMPLICIT NONE

      INTEGER               Observer
      INTEGER               Target
      INTEGER               BodyName
      DOUBLE PRECISION      ET
      DOUBLE PRECISION      Radius
      DOUBLE PRECISION      CoLatitude

C
C   Local function
C
      DOUBLE PRECISION      GET_EQUATORIAL_RADII

C
C   Local variables
C
      INTEGER               Index
      DOUBLE PRECISION      LT
      DOUBLE PRECISION      Dummy1
      DOUBLE PRECISION      RBody
      DOUBLE PRECISION      Temp1     ( 3 )
      DOUBLE PRECISION      Temp2     ( 3 )
      DOUBLE PRECISION      SC_Pos    ( 3 )
      DOUBLE PRECISION      State     ( 6 )
      DOUBLE PRECISION      Tipm   ( 3, 3 )

C-----------------------------------------------------------------------
C
C   determine radius
C
      RBody = GET_EQUATORIAL_RADII ( BodyName )

      CALL SPKEZ ( Target, ET, 'J2000', 'LT+S', Observer, State, LT )

      DO 10 Index = 1, 3
         Temp1 ( Index ) = State ( Index )
   10 CONTINUE

      CALL VMINUS ( Temp1, Temp2 )

      CALL BODMAT ( BodyName, ( ET - LT ), TIPM )

      CALL MXV ( TIPM, Temp2, SC_Pos )

      CALL RECSPH ( SC_Pos, Radius, CoLatitude, Dummy1 )

      Radius = ( Radius/RBody )

C-----------------------------------------------------------------------
      RETURN
      END

C-----------------------------------------------------------------------

C-----------------------------------------------------------------------
C-----------------------------------------------------------------------
      DOUBLE PRECISION FUNCTION GET_EQUATORIAL_RADII ( Target )

      IMPLICIT NONE

      INTEGER           Target

C
C   local variables
C
      INTEGER           ArrayDimension
      INTEGER           R1, R2, R3
      DOUBLE PRECISION  Equatorial_Radii
      DOUBLE PRECISION  Value ( 3 )

      ArrayDimension = 3

      CALL BODVAR ( Target, 'RADII', ArrayDimension, Value )

      R1 = IDINT ( ( Value ( 1 )*10000.0D0 ) )
      R2 = IDINT ( ( Value ( 2 )*10000.0D0 ) )
      R3 = IDINT ( ( Value ( 3 )*10000.0D0 ) )

      IF ( ( R1 .EQ. R2 ) .AND. ( R1 .EQ. R3 ) ) THEN
         Equatorial_Radii = Value ( 1 )
      ELSE IF ( ( R1 .EQ. R2 ) .AND. ( R1 .NE. R3 ) ) THEN
         Equatorial_Radii = ( Value ( 1 ) + Value ( 2 ) )
         Equatorial_Radii = ( Equatorial_Radii / 2.0D0 )
      ELSE IF ( ( R1 .NE. R2 ) .AND. ( R1 .NE. R3 ) ) THEN
         Equatorial_Radii = ( Value ( 1 ) + Value ( 2 ) + Value ( 3 ) )
         Equatorial_Radii = ( Equatorial_Radii / 3.0D0 )
      ENDIF

      GET_EQUATORIAL_RADII = Equatorial_Radii

C-----------------------------------------------------------------------
      RETURN
      END
C-----------------------------------------------------------------------
C-----------------------------------------------------------------------

      SUBROUTINE GET_SCALE_HEIGHT_1 (lval, yval)
      IMPLICIT NONE

      INTEGER nradius, index
      DOUBLE PRECISION lval, yval
      DOUBLE PRECISION radius (36), sheight (36), y2 (36)
      DOUBLE PRECISION xvalue (36), yvalue (36)
      DOUBLE PRECISION dtemp

C
C   /home/jbg/cassini/diffuse/satden20080910.input
C
      DATA radius /   3.0,    3.2,    3.4,    3.6,    3.8,    4.0,
     1                4.2,    4.4,    4.6,    4.8,    5.0,    5.2,
     2                5.4,    5.6,    5.8,    6.0,    6.2,    6.4,
     3                6.6,    6.8,    7.0,    7.2,    7.4,    7.6,
     4                7.8,    8.0,    8.2,    8.4,    8.6,    8.8,
     5                9.0,    9.2,    9.4,    9.6,    9.8,   10.0  /

C
C   Water group scale height
C
      DATA sheight /  0.03000, 0.11000, 0.22000, 0.26000,
     1                0.26000, 0.32000, 0.34000, 0.34000,
     2                0.38000, 0.44000, 0.60000, 0.73000,
     3                0.81000, 0.80000, 0.80602, 0.94000,
     4                0.71000, 0.97000, 1.17000, 1.15000,
     5                0.93000, 0.83000, 0.68000, 0.67000,
     6                0.76000, 0.90000, 0.90000, 1.05000,
     7                1.17000, 1.11000, 1.32000, 1.15000,
     8                1.22000, 1.28000, 1.35000, 1.62000  /

      nradius = 36

      DO index = 1, nradius
         xvalue (index) = DLOG10 (radius (index))

         yvalue (index) = DLOG10 (sheight (index))
      ENDDO

      CALL SPLINE (xvalue, yvalue, nradius, 1.0D31, 1.0D31, y2)

      dtemp = LOG10 (lval)

      CALL SPLINT (xvalue, yvalue, y2, nradius, dtemp, yval)

      RETURN
      END
C-----------------------------------------------------------------------
C-----------------------------------------------------------------------

      SUBROUTINE GET_SCALE_HEIGHT_2 (lval, yval)
      IMPLICIT NONE

      INTEGER nradius, index
      DOUBLE PRECISION lval, yval
      DOUBLE PRECISION radius (36), sheight (36), y2 (36)
      DOUBLE PRECISION xvalue (36), yvalue (36)
      DOUBLE PRECISION dtemp

C
C   /home/jbg/cassini/diffuse/satden20080910.input
C
      DATA radius /   3.0,    3.2,    3.4,    3.6,    3.8,    4.0,
     1                4.2,    4.4,    4.6,    4.8,    5.0,    5.2,
     2                5.4,    5.6,    5.8,    6.0,    6.2,    6.4,
     3                6.6,    6.8,    7.0,    7.2,    7.4,    7.6,
     4                7.8,    8.0,    8.2,    8.4,    8.6,    8.8,
     5                9.0,    9.2,    9.4,    9.6,    9.8,   10.0  /

C
C   Hydrogen ion scale height
C
      DATA sheight /  1.3818,  1.4464,  1.1600,  1.5900,
     1                1.6200,  3.0944,  2.9431,  2.5029,
     2                3.1632,  3.5153,  5.1220,  3.6694,
     3                2.9211,  2.5992,  2.6120,  2.2687,
     4                1.7656,  1.9730,  2.8421,  3.5896,
     5                3.8867,  3.6391,  3.7877,  3.7382,
     6                5.1248,  5.7895,  6.5072,  6.5550,
     7                7.8469,  6.6507,  8.4689,  8.3254,
     8                8.1818,  9.4500,  5.5502,  7.7100   /

      nradius = 36

      DO index = 1, nradius
         xvalue (index) = DLOG10 (radius (index))

         yvalue (index) = DLOG10 (sheight (index))
      ENDDO

      CALL SPLINE (xvalue, yvalue, nradius, 1.0D31, 1.0D31, y2)

      dtemp = LOG10 (lval)

      CALL SPLINT (xvalue, yvalue, y2, nradius, dtemp, yval)

      RETURN
      END
C-----------------------------------------------------------------------
C-----------------------------------------------------------------------

      SUBROUTINE GET_N_ZERO_1 (lval, yval)
      IMPLICIT NONE

      INTEGER nradius, index
      DOUBLE PRECISION lval, yval
      DOUBLE PRECISION radius (36), density (36), y2 (36)
      DOUBLE PRECISION xvalue (36), yvalue (36)
      DOUBLE PRECISION dtemp

C
C   /home/jbg/cassini/diffuse/satden20080910.input
C
      DATA radius /   3.0,    3.2,    3.4,    3.6,    3.8,    4.0,
     1                4.2,    4.4,    4.6,    4.8,    5.0,    5.2,
     2                5.4,    5.6,    5.8,    6.0,    6.2,    6.4,
     3                6.6,    6.8,    7.0,    7.2,    7.4,    7.6,
     4                7.8,    8.0,    8.2,    8.4,    8.6,    8.8,
     5                9.0,    9.2,    9.4,    9.6,    9.8,   10.0  /

C
C   Water group density
C
      DATA density / 52.690, 68.910, 53.790, 49.710, 53.090, 52.140,
     1               47.700, 52.510, 61.000, 62.490, 51.150, 40.280,
     2               38.170, 36.290, 30.174, 24.140, 25.700, 21.640,
     3               20.800, 18.130, 16.790, 11.770, 10.010,  7.300,
     4                7.520,  7.120,  7.260,  5.490,  5.330,  5.160,
     5                3.860,  3.690,  3.570,  3.640,  3.190,  2.860  /

      nradius = 36

      DO index = 1, nradius
         xvalue (index) = DLOG10 (radius (index))

         yvalue (index) = DLOG10 (density (index))
      ENDDO

      CALL SPLINE (xvalue, yvalue, nradius, 1.0D31, 1.0D31, y2)

      dtemp = LOG10 (lval)

      CALL SPLINT (xvalue, yvalue, y2, nradius, dtemp, yval)

      RETURN
      END
C-----------------------------------------------------------------------
C-----------------------------------------------------------------------

      SUBROUTINE GET_N_ZERO_2 (lval, yval)
      IMPLICIT NONE

      INTEGER nradius, index
      DOUBLE PRECISION lval, yval
      DOUBLE PRECISION radius (36), density (36), y2 (36)
      DOUBLE PRECISION xvalue (36), yvalue (36)
      DOUBLE PRECISION dtemp

C
C   /home/jbg/cassini/diffuse/satden20080910.input
C
      DATA radius /   3.0,    3.2,    3.4,    3.6,    3.8,    4.0,
     1                4.2,    4.4,    4.6,    4.8,    5.0,    5.2,
     2                5.4,    5.6,    5.8,    6.0,    6.2,    6.4,
     3                6.6,    6.8,    7.0,    7.2,    7.4,    7.6,
     4                7.8,    8.0,    8.2,    8.4,    8.6,    8.8,
     5                9.0,    9.2,    9.4,    9.6,    9.8,   10.0  /

C
C   Hydrogen ion density
C
      DATA density  / 4.21300, 3.79860, 4.64000, 6.99000,
     1                6.74000, 8.73910, 8.43480, 7.95650,
     2                8.91300, 8.56520, 6.91300, 6.60870,
     3                6.08700, 6.67320, 6.54570, 5.14250,
     4                4.24940, 3.76500, 3.13510, 3.12750,
     5                3.89860, 4.97390, 5.30360, 5.21230,
     6                4.16230, 3.08260, 2.53910, 2.15940,
     7                1.67390, 1.39860, 1.16670, 1.03770,
     8                0.89638, 0.88000, 0.86377, 0.77000  /

      nradius = 36

      DO index = 1, nradius
         xvalue (index) = DLOG10 (radius (index))

         yvalue (index) = DLOG10 (density (index))
      ENDDO

      CALL SPLINE (xvalue, yvalue, nradius, 1.0D31, 1.0D31, y2)

      dtemp = LOG10 (lval)

      CALL SPLINT (xvalue, yvalue, y2, nradius, dtemp, yval)

      RETURN
      END
C-----------------------------------------------------------------------
C-----------------------------------------------------------------------

      SUBROUTINE spline (x, y, n, yp1, ypn, y2)

      INTEGER n, NMAX
      INTEGER i, k

      PARAMETER (NMAX = 100)

      REAL*8 yp1, ypn, x (n), y (n), y2 (n)
      REAL*8 p, qn, sig, un, u (NMAX)

      IF (yp1 .GT. 0.99e30) THEN
         y2 (1) = 0.0
         u (1) = 0.0
      ELSE
         y2 (1) = -0.5
         u (1) = (3.0/(x(2) - x(1)))*((y(2) - y(1))/(x(2) - x(1)) - yp1)
      ENDIF

      DO 11 i = 2, n - 1
         sig = (x(i) - x(i - 1))/(x(i + 1) - x(i - 1))
         p = sig*y2(i - 1) + 2.0
         y2 (i) = (sig - 1.0)/p
         u(i) = (6.0*((y(i + 1) - y(i))/(x(i+
     A1)-x(i))-(y(i)-y(i-1))/(x(i)-x(i-1)))/(x(i+1)-x(i-1))-sig*
     Bu(i-1))/p
11    CONTINUE

      IF (ypn .GT. 0.99e30) THEN
         qn = 0.0
         un = 0.0
      ELSE
         qn = 0.5
         un = (3.0/(x(n)-x(n-1)))*(ypn-(y(n)-y(n-1))/(x(n)-x(n-1)))
      ENDIF

      y2 (n) = (un - qn*u(n - 1))/(qn*y2(n - 1) + 1.0)

      DO 12 k = n - 1, 1, -1
         y2 (k) = y2 (k)*y2 (k + 1) + u (k)
12    CONTINUE

      RETURN
      END
C-----------------------------------------------------------------------
C-----------------------------------------------------------------------

      SUBROUTINE splint (xa, ya, y2a, n, x, y)

      INTEGER n
      REAL*8 x, y, xa (n), y2a (n), ya (n)
      INTEGER k, khi, klo
      REAL*8 a, b, h

      klo = 1
      khi = n

    1 IF ((khi - klo) .GT. 1) THEN
         k = (khi + klo)/2

         IF (xa (k) .gt. x) THEN
            khi = k
         ELSE
            klo = k
         ENDIF

         GOTO 1
      ENDIF

      h = xa(khi) - xa(klo)

      IF (h .EQ. 0.0) THEN
         print *, 'bad xa input in splint'
         STOP
      ENDIF

      a = (xa (khi) - x)/h
      b = (x - xa (klo))/h

      y=a*ya(klo)+b*ya(khi)+((a**3-a)*y2a(klo)+(b**3-b)*y2a(khi))*(h**
     A2)/6.

      RETURN
      END
C-----------------------------------------------------------------------
