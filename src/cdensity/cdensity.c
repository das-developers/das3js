#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <libpq-fe.h>
#include <das2/das1.h>

#include "y1958.h"
#include "cdensity.h"

#define SepChars " "

static char *progname;

static double volts;
static double a_one;
static double a_two;
static double alpha;
static double beta_one;
static double beta_two;

void get_mag_ephem_ (int *observer, int *target, int *BodyName, double *et, double *radius, double *colatitude);
void get_khronos_ (int *observer, int *target, int *bodyname, double *et, double *radius, double *latitude, double *mlatitude, double *longitude, double *localtime, double *lvalue);

void get_n_zero_1_ (double *lvalue, double *yvalue);
void get_n_zero_2_ (double *lvalue, double *yvalue);

void get_scale_height_1_ (double *lvalue, double *yvalue);
void get_scale_height_2_ (double *lvalue, double *yvalue);

void get_altitude_ (int *satellite, int *observer, double *et, double *altitude, double *radius);

int pgbeg_ (int *unit, char *file, int *nxsub, int *nysub, int filel);
void pgvstd_ (void);
void pgsvp_ (float *xleft, float *xright, float *ybot, float *ytop);
void pgsls_ (int *lstyle);
void pgslw_ (int *lwidth);
void pgscr_ (int *ci, float *cr, float *cg, float *cb);
void pgswin_ (float *x1, float *x2, float *y1, float *y2);
void pgline_ (int *n, float xpts [], float ypts []);
void pgmtxt_ (char *side, float *disp, float *coord, float *fjust, char *text, int sl, int tl);
void pgptxt_ (float *x, float *y, float *angle, float *fjust, char *text, int tlen);
void pgbox_ (char *xopt, float *xtick, int *nxsub, char *yopt, float *ytick, int *nysub, int xl, int yl);
void pgsci_ (int *ci);
void pgmove_ (float *x, float *y);
void pgdraw_ (float *x, float *y);
void pgaxis_ (char *opt, float *x1, float *y1, float *x2, float *y2, float *v1, float *v2, float *step, int *nsub, float *dmajl, float *dmajr, float *fmin, float *disp, float *orient, int optl);
void pgpt1_ (float *x, float *y, int *symbol);
void pgsch_ (float *size);
void pgend_ (void);

char *makeword (char *line, char stop);
char *fmakeword (FILE *f, char stop, int *len);
char x2c (char *what);
void unescape_url (char *url);
void plustospace (char *str);

void furnsh_ (char *filename, int namelen);
void utc2et_ (char *fstring, double *et, int len);

void utc2et (char *utcstr, double *et)
{
   long slutcstr = strlen (utcstr);

   utc2et_ (utcstr, et, slutcstr);
}

int past_1958 (int year, int day)
{
   int yr, past;

   yr = year - 1958;
   past = days_past [yr];
   past += (day - 1);

   return (past);
}

void ms2dhms (a, b, c, d, e)
int *a, *b, *c;
double *d;
double e;
{
   *d = (double) fmod ((double) (e / 1000.0), 60.0);
   *c = (int) fmod ((double) (e / 60000.0), 60.0);
   *b = (int) fmod ((double) (e / 3600000.0), 24.0);
   *a = (int) (e / 86400000.0);
}

void yrdy1958 (year, day, dy58)
int *year, *day;
int dy58;
{
   int index;

   index = 0;
   while (dy58 >= days_past [index])
      index++;

   index--;
   *year = 1958 + index;
   *day = dy58 - days_past [index] + 1;
}

int how_many (char *s, int c)
{
   int n = 0;

   while (s)
   {
      s = strchr (s, c);
      if (s) n++, s++;
   }

   return n;
}

void get_files (struct file_structure *dbase, int rows, int *max, PGresult *R)
{
   int ch = ' ';
   int i, nChars;
   int TokenIndex;
   int NumberOfBlanks;
   char *Token;
   char TempName [500];
   char value1 [80];
   char value2 [80];
   char value3 [80];

   for (i = 0; (i < rows) && (i < MAXFILES); i++)
   {
      sprintf (TempName, "%s%s%s", PQgetvalue(R, i, 0),
               PQgetvalue(R, i, 1), PQgetvalue(R, i, 2));

      NumberOfBlanks = how_many (TempName, ch);

      if (NumberOfBlanks == 0)
      {
         sprintf ((dbase + i)->line, "%s%s%s", PQgetvalue(R, i, 0),
                  PQgetvalue(R, i, 1), PQgetvalue(R, i, 2));
      }
      else
      {
         sprintf (TempName, "%s%s%s", PQgetvalue(R, i, 0),
                  PQgetvalue(R, i, 1), PQgetvalue(R, i, 2));

         TokenIndex = 0;

         Token = strtok (TempName, SepChars);

         while (Token != NULL)
         {
            if (TokenIndex == 0)
               sprintf (value1, "%s", Token);
            else if (TokenIndex == 1)
               sprintf (value2, "%s", Token);
            else if (TokenIndex == 2)
               sprintf (value3, "%s", Token);

            Token = strtok (NULL, SepChars);

            TokenIndex++;
         }

         sprintf ((dbase + i)->line, "%s%s%s", value1, value2, value3);
      }
   }

   (*max) += i;
}

float GetTimePosition (st, CurrentTime)
struct time_struc *st;
double CurrentTime;
{
   int past, yr, dy, hr, mn;
   int TempPast, TempYear, TempDoy;
   float xpt;
   double sec;
   double secval, minutetime, daytime, yeartime;
   double TempMS0, TempMS1, temp1, temp2, temp3;

   if (st->MnDyOrYr == Zeroeth)
   {
      minutetime = (double) ((CurrentTime/60000.0) -
                      (double) (st->st_past*1440));

      xpt = (float) minutetime;
   }
   else if (st->MnDyOrYr == First)
   {
      ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
      yrdy1958 (&yr, &dy, past);

      secval = (double) (hr*3600.0) +
               (double) (mn*60.0) +
               (double) (sec);

      secval = (secval/86400.0);

      daytime = ((double) past + secval);

      xpt = (float) daytime;
   }
   else if (st->MnDyOrYr == Second)
   {
      ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
      yrdy1958 (&yr, &dy, past);

      secval = (double) (hr*3600.0) +
               (double) (mn*60.0) +
               (double) (sec);

      secval = (secval/86400.0);

      daytime = ((double) past + secval);

      xpt = (float) daytime;
   }
   else if (st->MnDyOrYr == Third)
   {
      ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
      yrdy1958 (&yr, &dy, past);

      TempYear = yr;
      TempDoy = 1;
      TempPast = past_1958 (TempYear, TempDoy);
      TempMS0 = (double) (TempPast*86400000.0);

      TempYear++;
      TempPast = past_1958 (TempYear, TempDoy);
      TempMS1 = (double) (TempPast*86400000.0);

      temp1 = (CurrentTime - TempMS0);
      temp2 = (TempMS1 - TempMS0);

      temp3 = (temp1/temp2);

      xpt = (float) yr;
      xpt += (float) temp3;
   }

   return (xpt);
}

double GetTime (yr, day, hr, mn, ss)
int yr, day, hr, mn;
float ss;
{
   double temp_dy, total_ms;

   temp_dy = (double) past_1958 (yr, day);
   total_ms = (double) (temp_dy*86400.0) +
              (double) (hr*3600) +
              (double) (mn*60) +
              (double) ss;

   total_ms = (total_ms*1000.0);

   return (total_ms);
}

double n1value (latitude, lvalue, n_zero_one, scale_height_one, potential)
double latitude, lvalue, n_zero_one, scale_height_one, potential;
{
   double ret_val;
   double temp1, temp2;
   double temp3, term1, term2, term3, term4;
   double b_over_b_zero, rlatitude, cosine_six;

   rlatitude = (RPD*latitude);

   temp1 = cos (rlatitude);
   temp2 = (temp1*temp1);

   cosine_six = (temp2*temp2*temp2);

   temp1 = sin (rlatitude);
   temp2 = (temp1*temp1);
   temp2 *= 3.0;
   temp2 += 1.0;
   temp2 = sqrt (temp2);
   temp3 = (1.0/cosine_six);

   b_over_b_zero = (temp2*temp3);

   temp1 = (-1.0*a_one);
   temp2 = log (b_over_b_zero);

   term1 = (temp1*temp2);

   temp1 = (lvalue*lvalue);
   temp2 = (scale_height_one*scale_height_one);
   temp3 = (1.0 - cosine_six);

   term2 = (temp1/temp2);
   term2 *= temp3;
   term2 /= -3.0;

   temp1 = tan (rlatitude);
   temp2 = (temp1*temp1);
   temp3 = (scale_height_one*scale_height_one);

   term3 = (temp2/temp3);
   term3 /= lvalue;
   term3 *= alpha;

   temp1 = (scale_height_one*scale_height_one);
   temp2 = (beta_one/temp1);

   term4 = (temp2*potential);
   term4 *= -1.0;

   temp1 = (term1 + term2 + term3 + term4);
   temp2 = exp (temp1);

   ret_val = (temp2*n_zero_one);

   return (ret_val);
}

double n2value (latitude, lvalue, n_zero_two, scale_height_two, potential)
double latitude, lvalue, n_zero_two, scale_height_two, potential;
{
   double ret_val;
   double temp1, temp2, temp3;
   double term1, term2, term3, term4;
   double b_over_b_zero, rlatitude, cosine_six;

   rlatitude = (RPD*latitude);

   temp1 = cos (rlatitude);
   temp2 = (temp1*temp1);

   cosine_six = (temp2*temp2*temp2);

   temp1 = sin (rlatitude);
   temp2 = (temp1*temp1);
   temp2 *= 3.0;
   temp2 += 1.0;
   temp2 = sqrt (temp2);
   temp3 = (1.0/cosine_six);

   b_over_b_zero = (temp2*temp3);

   temp1 = (-1.0*a_two);
   temp2 = log (b_over_b_zero);

   term1 = (temp1*temp2);

   temp1 = (lvalue*lvalue);
   temp2 = (scale_height_two*scale_height_two);
   temp3 = (1.0 - cosine_six);

   term2 = (temp1/temp2);
   term2 *= temp3;
   term2 /= -3.0;

   temp1 = tan (rlatitude);
   temp2 = (temp1*temp1);
   temp3 = (scale_height_two*scale_height_two);

   term3 = (temp2/temp3);
   term3 /= lvalue;
   term3 *= alpha;

   temp1 = (scale_height_two*scale_height_two);
   temp2 = (beta_two/temp1);

   term4 = (temp2*potential);
   term4 *= -1.0;

   temp1 = (term1 + term2 + term3 + term4);
   temp2 = exp (temp1);

   ret_val = (temp2*n_zero_two);

   return (ret_val);
}

double nevalue (n_zero_one, n_zero_two, potential)
double n_zero_one, n_zero_two, potential;
{
   double ret_val;
   double temp1, temp2, temp3;

   temp1 = (potential/volts);
   temp2 = exp (temp1);
   temp3 = (n_zero_one + n_zero_two);

   ret_val = (temp3*temp2);

   return (ret_val);
}

double Get_We (LValue)
double LValue;
{
   double AValue, BValue;
   double WeValue;

   AValue = 0.106984;
   BValue = 1.984475;

   WeValue = pow (LValue, BValue);
   WeValue *= AValue;

   return (WeValue);
}

double Get_ScaleHeight_OHplus (LValue)
double LValue;
{
   double temp;
   double YValue;

   get_scale_height_1_ (&LValue, &temp);
   YValue = pow (10.0, temp);

   return (YValue);
}

double Get_ScaleHeight_Hplus (LValue)
double LValue;
{
   double temp;
   double YValue;

   get_scale_height_2_ (&LValue, &temp);
   YValue = pow (10.0, temp);

   return (YValue);
}

double Get_EqDensity_OHplus (LValue)
double LValue;
{
   double temp;
   double YValue;

   get_n_zero_1_ (&LValue, &temp);
   YValue = pow (10.0, temp);

   return (YValue);
}

double Get_EqDensity_Hplus (LValue)
double LValue;
{
   double temp;
   double YValue;

   get_n_zero_2_ (&LValue, &temp);
   YValue = pow (10.0, temp);

   return (YValue);
}

double Get_Anisotropy (int OH_or_H, double CurrentL)
{
   int dflag;
   int index, count;
   float lvalue, nvalue1, nvalue2;
   double temp1, temp2;
   double xvals [20], yvals [20];
   double slope [20], intercept [20];
   double AnisotropyValue;
   char *FileName = "/home/jbg/Linux/jbg/cassini/density2/wilson_binned_anisotropies.data";
   FILE *infp;

   /* Open the input file */
   if (!(infp = fopen (FileName, "r")))
      (void) fprintf (stderr, "Can't open input file.\n");

   count = 0;

   while (!feof (infp))
   {
      dflag = fscanf (infp, "%f %f %f", &lvalue, &nvalue1, &nvalue2);

      if (dflag < 0)
         break;

      if (OH_or_H)
      {
         yvals [count] = (double) nvalue1;
      }
      else
      {
         yvals [count] = (double) nvalue2;
      }

      xvals [count] = (double) lvalue;

      count++;
   }

   fclose (infp);

   for (index = 0; index < (count - 1); index++)
   {
      temp1 = (yvals [(index + 1)] - yvals [index]);
      temp2 = (xvals [(index + 1)] - xvals [index]);

      slope [index] = (temp1/temp2);

      temp1 = (slope [index]*xvals [index]);
      intercept [index] = (yvals [index] - temp1);
   }

   temp1 = (CurrentL - 3.5);
   index = (int) (temp1/0.5);

   temp1 = (slope [index]*CurrentL);
   temp1 += intercept [index];

   AnisotropyValue = temp1;

   return (AnisotropyValue);
}

void GeographicCoords (st, target, rad, lon, lat, mlat, loct, lval)
double st;
int target;
double *rad, *lon, *lat, *mlat, *loct, *lval;
{
   int past, year, doy, hr, mn;
   int observer;
   int BodyName;
   double et, radius, longitude, latitude, mlatitude;
   double sec, loctime;
   double thelvalue;
   char scstr [80];

   BodyName = target;

   observer = Cassini;

   ms2dhms (&past, &hr, &mn, &sec, st);
   yrdy1958 (&year, &doy, past);

   sprintf (scstr, "%04d %03d // %02d:%02d:%06.3f",
            year, doy, hr, mn, (float) sec);

   utc2et (scstr, &et);

   get_khronos_ (&observer, &target, &BodyName, &et, &radius, &latitude, &mlatitude, &longitude, &loctime, &thelvalue);

   *rad  = radius;
   *lon  = longitude;
   *lat  = latitude;
   *mlat = mlatitude;
   *loct = loctime;
   *lval = thelvalue;
}

void InitPlotOther (st)
struct time_struc *st;
{
   int unit, nx, ny, fg_color, bg_color, lwidth;
   int xtick2, ytick2;
   int nsub;
   int index1, index2, leap1, leap2, dy_mn1, dy_mn2;
   int m_days [2][13] = {{1,32,60,91,121,152,182,213,244,274,305,335,366},
                         {1,32,61,92,122,153,183,214,245,275,306,336,367}};
   float x1, x2, y1, y2, xtick1, ytick1, rgb;
   float xl = 0.22, xr = 0.78, yb = 0.123, yt = 0.877;
   float coord, fjust, displ;
   float step, v1, v2, dmajl, dmajr, fmin, disp, orient;
   char *xname1 = "Saturn radii (R\\dS\\u)";
   char *xname2 = "Enceladus radii (R\\dE\\u)";
   char *xname3 = "L";
   char *xname4 = "Saturn magnetic latitude (degrees)";
   char *xname5 = "Saturn local time (hours)";
   char *xname6 = "Titan radii (R\\dT\\u)";
   char *xname7 = "Titan latitude (degrees)";
   char *xname8 = "Titan local time (hours)";
   char *xname9 = "Saturn latitude (degrees)";
   char *xyname1 = "N\\de\\u, Electron Density (cm\\u-3\\d)";
   char *xyname2 = "N\\de\\u/|B| (nT\\u-1\\d\\(0218) cm\\u-3\\d)";
   char *xyname3 = "Saturn altitude (km)";
   char *xyname4 = "Enceladus altitude (km)";
   char *xyname5 = "F\\dp\\u/F\\c\\u";
   char *xyname6 = "Titan altitude (km)";
   char name [80];
   char months [12][12] = {  "January", "February",    "March",    "April",
                                 "May",     "June",     "July",   "August",
                           "September",  "October", "November", "December"};


   unit = 0;

   nx = 1;
   ny = 1;

   x1 = (float) st->xmin;
   x2 = (float) st->xmax;

   y1 = (float) st->ymin;
   y2 = (float) st->ymax;

   fg_color = 1;
   bg_color = 0;

   rgb = 0.0;

   lwidth = 4;

   xtick1 = 0.0;
   ytick1 = 0.0;

   xtick2 = 0;
   ytick2 = 0;

   if (((st->XAxis == SaturnMLatitude) || (st->XAxis == SaturnKLatitude)) && ((st->xmax - st->xmin) >= 60.0))
   {
      xtick1 = 30.0;
      xtick2 = 3;
   }

   if (st->psorgif)
      fprintf (stdout, "Content-type: application/postscript\n\n");
   else
      fprintf (stdout, "Content-type: image/png\n\n");

   fflush (stdout);

   if (st->psorgif)
      pgbeg_ (&unit, "-/cps", &nx, &ny, 5L);
   else
      pgbeg_ (&unit, "-/png", &nx, &ny, 5L);

   pgsvp_ (&xl, &xr, &yb, &yt);

   pgslw_ (&lwidth);

   pgscr_ (&fg_color, &rgb, &rgb, &rgb);

   rgb = 1.0;

   pgscr_ (&bg_color, &rgb, &rgb, &rgb);

   pgswin_ (&x1, &x2, &y1, &y2);

   if ((st->XAxis == Ne) || (st->XAxis == NeOverB))
      pgbox_ ("BCILNST2", &xtick1, &xtick2, "BCIMNSTV", &ytick1, &ytick2, 8L, 8L);
   else if ((st->XAxis == SaturnLocalTime) || (st->XAxis == TitanLocalTime))
   {
      xtick1 = 4.0;
      ytick1 = 0.0;

      xtick2 = 4;
      ytick2 = 0;

      pgbox_ ("BCIST", &xtick1, &xtick2, "BCILNSTV2", &ytick1, &ytick2, 5L, 9L);

      x1 = 12.0;
      x2 = 20.0;

      y2 = y1;

      v1 = 12.0;
      v2 = 20.0;

      step = xtick1;
      nsub = xtick2;

      dmajl = 0.0;
      dmajr = 0.5;

      fmin = 0.25;
      disp = 0.8;
      orient = 0.0;

      pgaxis_ ("N", &x1, &y1, &x2, &y2, &v1, &v2, &step, &nsub, &dmajl, &dmajr, &fmin, &disp, &orient, 1L);

      x1 = 24.0;
      x2 = 36.0;

      v1 =  0.0;
      v2 = 12.0;

      pgaxis_ ("N", &x1, &y1, &x2, &y2, &v1, &v2, &step, &nsub, &dmajl, &dmajr, &fmin, &disp, &orient, 1L);
   }
   else if (((st->YAxis == Ne) || (st->YAxis == NeOverB) || (st->YAxis == FpOverFc)) && (st->LinOrLog))
      pgbox_ ("BCINST1", &xtick1, &xtick2, "BCILMNSTV2", &ytick1, &ytick2, 7L, 10L);
   else if (((st->YAxis == Ne) || (st->YAxis == NeOverB) || (st->YAxis == FpOverFc)) && (!(st->LinOrLog)))
      pgbox_ ("BCILNST", &xtick1, &xtick2, "BCILMNSTV2", &ytick1, &ytick2, 7L, 10L);

   coord = 0.5;
   fjust = 0.5;
   displ = 4.0;

   if (st->XAxis == Ne)
      pgmtxt_ ("B", &displ, &coord, &fjust, xyname1, 1L, (long) strlen (xyname1));
   else if (st->XAxis == NeOverB)
      pgmtxt_ ("B", &displ, &coord, &fjust, xyname2, 1L, (long) strlen (xyname2));
   else if (st->XAxis == SaturnAlt)
      pgmtxt_ ("B", &displ, &coord, &fjust, xyname3, 1L, (long) strlen (xyname3));
   else if (st->XAxis == EnceladusAlt)
      pgmtxt_ ("B", &displ, &coord, &fjust, xyname4, 1L, (long) strlen (xyname4));
   else if (st->XAxis == TitanAlt)
      pgmtxt_ ("B", &displ, &coord, &fjust, xyname6, 1L, (long) strlen (xyname6));
   else if (st->XAxis == SaturnRad)
      pgmtxt_ ("B", &displ, &coord, &fjust, xname1, 1L, (long) strlen (xname1));
   else if (st->XAxis == EnceladusRad)
      pgmtxt_ ("B", &displ, &coord, &fjust, xname2, 1L, (long) strlen (xname2));
   else if (st->XAxis == TheLValue)
      pgmtxt_ ("B", &displ, &coord, &fjust, xname3, 1L, (long) strlen (xname3));
   else if (st->XAxis == SaturnMLatitude)
      pgmtxt_ ("B", &displ, &coord, &fjust, xname4, 1L, (long) strlen (xname4));
   else if (st->XAxis == SaturnLocalTime)
      pgmtxt_ ("B", &displ, &coord, &fjust, xname5, 1L, (long) strlen (xname5));
   else if (st->XAxis == TitanRad)
      pgmtxt_ ("B", &displ, &coord, &fjust, xname6, 1L, (long) strlen (xname6));
   else if (st->XAxis == TitanLatitude)
      pgmtxt_ ("B", &displ, &coord, &fjust, xname7, 1L, (long) strlen (xname7));
   else if (st->XAxis == TitanLocalTime)
      pgmtxt_ ("B", &displ, &coord, &fjust, xname8, 1L, (long) strlen (xname8));
   else if (st->XAxis == SaturnKLatitude)
      pgmtxt_ ("B", &displ, &coord, &fjust, xname9, 1L, (long) strlen (xname9));

   displ = 3.0;

   if (st->YAxis == Ne)
      pgmtxt_ ("L", &displ, &coord, &fjust, xyname1, 1L, (long) strlen (xyname1));
   else if (st->YAxis == NeOverB)
      pgmtxt_ ("L", &displ, &coord, &fjust, xyname2, 1L, (long) strlen (xyname2));
   else if (st->YAxis == SaturnAlt)
   {
      displ = 5.0;
      pgmtxt_ ("L", &displ, &coord, &fjust, xyname3, 1L, (long) strlen (xyname3));
   }
   else if (st->YAxis == EnceladusAlt)
   {
      displ = 5.0;
      pgmtxt_ ("L", &displ, &coord, &fjust, xyname4, 1L, (long) strlen (xyname4));
   }
   else if (st->YAxis == TitanAlt)
   {
      displ = 5.0;
      pgmtxt_ ("L", &displ, &coord, &fjust, xyname6, 1L, (long) strlen (xyname6));
   }
   else if (st->YAxis == FpOverFc)
      pgmtxt_ ("L", &displ, &coord, &fjust, xyname5, 1L, (long) strlen (xyname5));

   /* draw plot title */
   if ((st->st_year % 4) == 0)
      leap1 = 1;
   else
      leap1 = 0;

   for (index1 = 0; index1 < 12; index1++)
      if (st->st_day < m_days [leap1][index1 + 1])
         break;

   dy_mn1 = st->st_day - m_days [leap1][index1] + 1;

   if ((st->sp_year % 4) == 0)
      leap2 = 1;
   else
      leap2 = 0;

   for (index2 = 0; index2 < 12; index2++)
      if (st->sp_day < m_days [leap2][index2 + 1])
         break;

   dy_mn2 = st->sp_day - m_days [leap2][index2] + 1;

   if (st->st_past == st->sp_past)
      (void) sprintf (name, "%.4d %.3d (%s %d) %02d:%02d:%02.0f - %02d:%02d:%02.0f",
                      st->st_year, st->st_day,
                      months [index1], dy_mn1,
                      st->st_hr, st->st_mn, st->st_sec,
                      st->sp_hr, st->sp_mn, st->sp_sec);
   else
      (void) sprintf (name, "%.4d %.3d (%s %d) %02d:%02d:%02.0f - %.4d %.3d (%s %d) %02d:%02d:%02.0f",
                      st->st_year, st->st_day,
                      months [index1], dy_mn1,
                      st->st_hr, st->st_mn, st->st_sec,
                      st->sp_year, st->sp_day,
                      months [index2], dy_mn2,
                      st->sp_hr, st->sp_mn, st->sp_sec);

   displ = 1.6;
   pgmtxt_ ("T", &displ, &coord, &fjust, name, 1L, (long) strlen (name));

   displ = 3.6;
   pgmtxt_ ("T", &displ, &coord, &fjust, st->title, 1L, (long) strlen (st->title));
}

void InitPlotTime (st)
struct time_struc *st;
{
   int ncount, lwidth = 3, index, target;
   int EphemSetTrue, EphemSetIndex;
   int fg_color, bg_color;
   int unit, nx, ny, one, xtick2, ytick2;
   int index1, index2, leap1, leap2, dy_mn1, dy_mn2;
   int past, year, dy, hr, mn, thisday, dayofyear, daytick;
   int MajorTickIndex, MinorTickIndex;
   int nsub;
   int ModInt, ModHour;
   int yr;
   int MnOrSs = True;
   int EphemSetValues [18] = { False, False, False, False, False, False,
                               False, False, False, False, False, False,
                               False, False, False, False, False, False };
   int EphemSetMasks [18] = { SaturnR_MASK,      EnceladusR_MASK,      TitanR_MASK,
                              SaturnLat_MASK,    EnceladusLat_MASK,    TitanLat_MASK,
                              SaturnMLat_MASK,   EnceladusMLat_MASK,   TitanMLat_MASK,
                              SaturnLon_MASK,    EnceladusLon_MASK,    TitanLon_MASK,
                              SaturnLT_MASK,     EnceladusLT_MASK,     TitanLT_MASK,
                              SaturnLValue_MASK, EnceladusLValue_MASK, TitanLValue_MASK };
   int m_days [2][13] = {{1,32,60,91,121,152,182,213,244,274,305,335,366},
                         {1,32,61,92,122,153,183,214,245,275,306,336,367}};
   float v1, v2, step, dmajl, dmajr, fmin, disp, orient;
   float x1, x2, y1, y2, rgb, displ, coord, fjust, xtick1, ytick1;
   float xl, xr, yb, yt;
   float YBottomValues [19] = { 0.04, 0.08, 0.11, 0.14, 0.16, 0.19,
                                0.22, 0.24, 0.27, 0.30, 0.33, 0.36,
                                0.39, 0.42, 0.45, 0.48, 0.51, 0.54, 0.57 };
   double CurrentTime;
   double MajorTicks [100], MinorTicks [100];
   double fday, numer, denom;
   double tempr, tempg, tempb;
   double thisms, thistick, frac, DeltaTime, dummy, sec;
   double OneSecond = 1000.0, OneMinute = 60000.0, OneDay = 8.64e+07;
   double SaturnR, SaturnLon, SaturnLat, SaturnMLat, SaturnLT, SaturnL;
   double EnceladusR, EnceladusLon, EnceladusLat, EnceladusLT;
   double TitanR, TitanLon, TitanLat, TitanLT;
   double EphemDataValues [18];
   char *xnameT0 = "SCET";
   char *yname1 = "N\\de\\u, Electron Density (cm\\u-3\\d)";
   char *yname2 = "N\\de\\u/|B| (nT\\u-1\\d\\(0218) cm\\u-3\\d)";
   char *yname3 = "F\\dp\\u/F\\dc\\u";
   char name [80];
   char xnames [18][20] = { "R\\dS\\u",   "R\\dE\\u",   "R\\dT\\u",
                             "Lat (S)",    "Lat (E)",    "Lat (T)",
                            "MLat (S)", "never used", "never used",
                             "Lon (S)",    "Lon (E)",    "Lon (T)",
                              "LT (S)",     "LT (E)",     "LT (T)",
                                   "L", "never used", "never used" };
   char months [12][12] = {  "January", "February",    "March",    "April",
                                 "May",     "June",     "July",   "August",
                           "September",  "October", "November", "December"};

   unit = 0;

   nx = 1;
   ny = 1;

   fg_color = 1;
   bg_color = 0;

   rgb = 0.0;
   one = 1;

   if ((st->ephemeris != 0) &&
       (st->MnDyOrYr != Second) &&
       (st->MnDyOrYr != Third))
   {
      for (EphemSetIndex = 0; EphemSetIndex < 6; EphemSetIndex++)
      {
         EphemSetTrue = st->ephemeris;

         EphemSetTrue &= EphemSetMasks [3*(EphemSetIndex)];
         EphemSetTrue = (EphemSetTrue >> EphemSetIndex);

         if (EphemSetTrue)
            EphemSetValues [3*(EphemSetIndex)] = True;

         EphemSetTrue = st->ephemeris;

         EphemSetTrue &= EphemSetMasks [(3*(EphemSetIndex) + 1)];

         EphemSetTrue = (EphemSetTrue >> 5);
         EphemSetTrue = (EphemSetTrue >> EphemSetIndex);

         if (EphemSetTrue)
            EphemSetValues [(3*(EphemSetIndex) + 1)] = True;

         EphemSetTrue = st->ephemeris;

         EphemSetTrue &= EphemSetMasks [(3*(EphemSetIndex) + 2)];

         EphemSetTrue = (EphemSetTrue >> 11);
         EphemSetTrue = (EphemSetTrue >> EphemSetIndex);

         if (EphemSetTrue)
            EphemSetValues [(3*(EphemSetIndex) + 2)] = True;
      }
   }

   if (st->MnDyOrYr == Zeroeth)
   {
      x1 = (float) (st->xmin - (double) (st->st_past*1440));
      x2 = (float) (st->xmax - (double) (st->st_past*1440));
   }
   else if (st->MnDyOrYr == First)
   {
      x1 = (float) st->xmin;
      x2 = (float) st->xmax;
   }
   else if (st->MnDyOrYr == Second)
   {
      x1 = (float) st->xmin;
      x2 = (float) st->xmax;
   }
   else if (st->MnDyOrYr == Third)
   {
      x1 = (float) st->xmin;
      x2 = (float) st->xmax;
   }

   y1 = (float) st->ymin;
   y2 = (float) st->ymax;

   if (st->MnDyOrYr == Zeroeth)
   {
      if ((st->xmax - st->xmin) < 3.0)
      {
         MnOrSs = False;

         xtick1 = 0.5;
         xtick2 = 3;
      }
      else if (((st->xmax - st->xmin) >= 3.0) &&
               ((st->xmax - st->xmin) <  8.0))
      {
         xtick1 = 1.0;
         xtick2 = 3;
      }
      else if (((st->xmax - st->xmin) >=  8.0) &&
               ((st->xmax - st->xmin) <  15.0))
      {
         xtick1 = 2.0;
         xtick2 = 4;
      }
      else if (((st->xmax - st->xmin) >= 15.0) &&
               ((st->xmax - st->xmin) <  30.0))
      {
         xtick1 = 5.0;
         xtick2 = 5;
      }
      else if (((st->xmax - st->xmin) >= 30.0) &&
               ((st->xmax - st->xmin) <  50.0))
      {
         xtick1 = 10.0;
         xtick2 = 5;
      }
      else if (((st->xmax - st->xmin) >= 50.0) &&
               ((st->xmax - st->xmin) <  90.0))
      {
         xtick1 = 15.0;
         xtick2 = 3;
      }
      else if (((st->xmax - st->xmin) >= 90.0) &&
               ((st->xmax - st->xmin) < 180.0))
      {
         xtick1 = 30.0;
         xtick2 = 3;
      }
      else if (((st->xmax - st->xmin) >= 180.0) &&
               ((st->xmax - st->xmin) <  360.0))
      {
         xtick1 = 60.0;
         xtick2 = 4;
      }
      else if (((st->xmax - st->xmin) >= 360.0) &&
               ((st->xmax - st->xmin) <  720.0))
      {
         xtick1 = 120.0;
         xtick2 = 4;
      }
      else if (((st->xmax - st->xmin) >= 720.0) &&
               ((st->xmax - st->xmin) < 1440.0))
      {
         xtick1 = 240.0;
         xtick2 = 4;
      }
      else if (((st->xmax - st->xmin) >= 1440.0) &&
               ((st->xmax - st->xmin) <  2880.0))
      {
         xtick1 = 480.0;
         xtick2 = 4;
      }
   }
   else if (st->MnDyOrYr == First)
   {
      if ((st->xmax - st->xmin) < 6.0)
      {
         xtick1 = 1.0;
         xtick2 = 4;

         ModInt = 1;
         ModHour = 6;
      }
      else if (((st->xmax - st->xmin) >=  6.0) &&
               ((st->xmax - st->xmin) <  10.0))
      {
         xtick1 = 2.0;
         xtick2 = 4;

         ModInt = 1;
         ModHour = 12;
      }
   }
   else if (st->MnDyOrYr == Second)
   {
      if (((st->xmax - st->xmin) >= 10.0) &&
          ((st->xmax - st->xmin) <  20.0))
      {
         xtick1 = 5.0;
         xtick2 = 5;

         ModInt = (int) (xtick1/(float) xtick2);
      }
      else if (((st->xmax - st->xmin) >= 20.0) &&
               ((st->xmax - st->xmin) <  40.0))
      {
         xtick1 = 10.0;
         xtick2 = 5;

         ModInt = (int) (xtick1/(float) xtick2);
      }
      else if (((st->xmax - st->xmin) >= 40.0) &&
               ((st->xmax - st->xmin) <  80.0))
      {
         xtick1 = 20.0;
         xtick2 = 4;

         ModInt = (int) (xtick1/(float) xtick2);
      }
      else if (((st->xmax - st->xmin) >=  80.0) &&
               ((st->xmax - st->xmin) <  160.0))
      {
         xtick1 = 40.0;
         xtick2 = 4;

         ModInt = (int) (xtick1/(float) xtick2);
      }
      else if (((st->xmax - st->xmin) >= 160.0) &&
               ((st->xmax - st->xmin) <  320.0))
      {
         xtick1 = 80.0;
         xtick2 = 4;

         ModInt = (int) (xtick1/(float) xtick2);
      }
      else if (((st->xmax - st->xmin) >= 320.0) &&
               ((st->xmax - st->xmin) <  600.0))
      {
         xtick1 = 120.0;
         xtick2 = 4;

         ModInt = (int) (xtick1/(float) xtick2);
      }
      else
      {
         xtick1 = 150.0;
         xtick2 = 3;

         ModInt = (int) (xtick1/(float) xtick2);
      }
   }
   else if (st->MnDyOrYr == Third)
   {
      if ((st->xmax - st->xmin) < 7.0)
      {
         xtick1 = 1.0;
         xtick2 = 3;
      }
      else if (((st->xmax - st->xmin) >= 7.0) && (st->xmax - st->xmin < 12.0))
      {
         xtick1 = 2.0;
         xtick2 = 4;
      }
      else if (((st->xmax - st->xmin) >= 12.0) && (st->xmax - st->xmin < 20.0))
      {
         xtick1 = 5.0;
         xtick2 = 5;
      }
   }

   ytick1 = 0.0;
   ytick2 = 0;

   xl = 0.1;

   if ((st->ephemeris == 0) ||
       (st->MnDyOrYr == Second) ||
       (st->MnDyOrYr == Third))
   {
      xr = 0.91;
      yb = 0.107;
   }
   else
   {
      xr = 0.85;
      yb = 0.04;

      index = 0;

      for (EphemSetIndex = 0; EphemSetIndex < 18; EphemSetIndex++)
      {
         if (EphemSetValues [EphemSetIndex])
            index++;
      }

      yb = YBottomValues [index];
   }

   yt = 0.89;

   if (st->psorgif)
      fprintf (stdout, "Content-type: application/postscript\n\n");
   else
      fprintf (stdout, "Content-type: image/png\n\n");

   fflush (stdout);

   if (st->psorgif)
      pgbeg_ (&unit, "-/cps", &nx, &ny, 5L);
   else
      pgbeg_ (&unit, "-/png", &nx, &ny, 5L);

   pgsvp_ (&xl, &xr, &yb, &yt);

   pgslw_ (&lwidth);
   pgscr_ (&fg_color, &rgb, &rgb, &rgb);
   rgb = 1.0;
   pgscr_ (&bg_color, &rgb, &rgb, &rgb);
   pgswin_ (&x1, &x2, &y1, &y2);
   pgsci_ (&one);

   coord = 0.5;
   fjust = 0.5;
   displ = 3.3;

   if (st->YAxis == Ne)
      pgmtxt_ ("L", &displ, &coord, &fjust, yname1, 1L, (long) strlen (yname1));
   else if (st->YAxis == NeOverB)
      pgmtxt_ ("L", &displ, &coord, &fjust, yname2, 1L, (long) strlen (yname2));
   else if (st->YAxis == FpOverFc)
      pgmtxt_ ("L", &displ, &coord, &fjust, yname3, 1L, (long) strlen (yname3));

   /* draw plot title */
   if ((st->st_year % 4) == 0)
      leap1 = 1;
   else
      leap1 = 0;

   for (index1 = 0; index1 < 12; index1++)
      if (st->st_day < m_days [leap1][index1 + 1])
         break;

   dy_mn1 = st->st_day - m_days [leap1][index1] + 1;

   if ((st->sp_year % 4) == 0)
      leap2 = 1;
   else
      leap2 = 0;

   for (index2 = 0; index2 < 12; index2++)
      if (st->sp_day < m_days [leap2][index2 + 1])
         break;

   dy_mn2 = st->sp_day - m_days [leap2][index2] + 1;

   if (st->st_past == st->sp_past)
      (void) sprintf (name, "%.4d %.3d (%s %d) %02d:%02d:%02.0f - %02d:%02d:%02.0f",
                      st->st_year, st->st_day,
                      months [index1], dy_mn1,
                      st->st_hr, st->st_mn, st->st_sec,
                      st->sp_hr, st->sp_mn, st->sp_sec);
   else
      (void) sprintf (name, "%.4d %.3d (%s %d) %02d:%02d:%02.0f - %.4d %.3d (%s %d) %02d:%02d:%02.0f",
                      st->st_year, st->st_day,
                      months [index1], dy_mn1,
                      st->st_hr, st->st_mn, st->st_sec,
                      st->sp_year, st->sp_day,
                      months [index2], dy_mn2,
                      st->sp_hr, st->sp_mn, st->sp_sec);

   displ = 1.6;

   pgmtxt_ ("T", &displ, &coord, &fjust, name, 1L, (long) strlen (name));

   displ = 3.6;

   pgmtxt_ ("T", &displ, &coord, &fjust, st->title, 1L, (long) strlen (st->title));

   if (st->MnDyOrYr == Zeroeth)
   {
      if (st->LinOrLogYAxis)
         pgbox_ ("BCIST", &xtick1, &xtick2, "BCINSTV1", &ytick1, &ytick2, 5L, 8L);
      else
         pgbox_ ("BCIST", &xtick1, &xtick2, "BCILNSTV2", &ytick1, &ytick2, 5L, 9L);

      thisms = st->st_total_ms;
      thistick = (double) (xtick1*60000.0);

      while (thisms <= st->sp_total_ms)
      {
         frac = modf ((thisms/thistick), &dummy);

         if (frac == 0.0)
         {
            numer = (thisms/60000.0);
            numer = (numer - st->xmin);
            denom = (st->xmax - st->xmin);
            fday = (numer/denom);

            ms2dhms (&past, &hr, &mn, &sec, thisms);
            yrdy1958 (&year, &dy, past);

            if (MnOrSs)
               sprintf (name, "%02d %02d", hr, mn);
            else
               sprintf (name, "%02d %02d %02d", hr, mn, (int) sec);

            displ = 1.5;
            coord = (float) fday;

            pgmtxt_ ("B", &displ, &coord, &fjust, name, 1L, (long) strlen (name));

            if ((EphemSetValues  [0]) || (EphemSetValues  [3]) ||
                (EphemSetValues  [6]) || (EphemSetValues  [9]) ||
                (EphemSetValues [12]) || (EphemSetValues [15]))
            {
               target = Saturn;

               GeographicCoords (thisms, target, &SaturnR,
                                 &SaturnLon, &SaturnLat,
                                 &SaturnMLat, &SaturnLT, &SaturnL);

               EphemDataValues  [0] = SaturnR;
               EphemDataValues  [3] = SaturnLat;
               EphemDataValues  [6] = SaturnMLat;
               EphemDataValues  [9] = SaturnLon;
               EphemDataValues [12] = SaturnLT;
               EphemDataValues [15] = SaturnL;
            }

            if ((EphemSetValues  [1]) || (EphemSetValues  [4]) ||
                (EphemSetValues [10]) || (EphemSetValues [13]))
            {
               target = Enceladus;

               GeographicCoords (thisms, target, &EnceladusR,
                                 &EnceladusLon, &EnceladusLat,
                                 &dummy, &EnceladusLT, &dummy);

               EphemDataValues  [1] = EnceladusR;
               EphemDataValues  [4] = EnceladusLat;
               EphemDataValues [10] = EnceladusLon;
               EphemDataValues [13] = EnceladusLT;
            }

            if ((EphemSetValues  [2]) || (EphemSetValues  [5]) ||
                (EphemSetValues [11]) || (EphemSetValues [14]))
            {
               target = Titan;

               GeographicCoords (thisms, target, &TitanR,
                                 &TitanLon, &TitanLat,
                                 &dummy, &TitanLT, &dummy);

               EphemDataValues  [2] = TitanR;
               EphemDataValues  [5] = TitanLat;
               EphemDataValues [11] = TitanLon;
               EphemDataValues [14] = TitanLT;
            }

            displ = 2.6;

            for (EphemSetIndex = 0; EphemSetIndex < 18; EphemSetIndex++)
            {
               if (EphemSetValues [EphemSetIndex])
               {
                  sprintf (name, "%6.2f",
                           (float) EphemDataValues [EphemSetIndex]);

                  pgmtxt_ ("B", &displ, &coord, &fjust,
                           name, 1L, (long) strlen (name));

                  displ += 1.1;
               }
            }
         }

         thisms += OneSecond;
      }
   }
   else if (st->MnDyOrYr == First)
   {
      if (st->LinOrLogYAxis)
         pgbox_ ("BC", &xtick1, &xtick2, "BCINSTV1", &ytick1, &ytick2, 2L, 8L);
      else
         pgbox_ ("BC", &xtick1, &xtick2, "BCILNSTV2", &ytick1, &ytick2, 2L, 9L);

      if ((st->st_sec == 0.0) && (st->sp_sec == 0.0))
         DeltaTime = OneMinute;
      else
         DeltaTime = OneSecond;

      thisms = st->st_total_ms;
      thistick = (double) (xtick1*OneDay);

      MajorTickIndex = 0;
      MinorTickIndex = 0;

      while (thisms <= st->sp_total_ms)
      {
         ms2dhms (&past, &hr, &mn, &sec, thisms);
         yrdy1958 (&yr, &dy, past);

         if (((dy % (int) xtick1) == 0) && (hr == 0) && (mn == 0) && (sec == 0.0))
         {
            CurrentTime = GetTime (yr, dy, hr, mn, (float) sec);

            MajorTicks [MajorTickIndex] = CurrentTime;
            MajorTickIndex++;
         }

         if (((dy % ModInt) == 0) && ((hr % ModHour) == 0) && (mn == 0) && (sec == 0.0))
         {
            CurrentTime = GetTime (yr, dy, hr, mn, (float) sec);

            MinorTicks [MinorTickIndex] = CurrentTime;
            MinorTickIndex++;
         }

         frac = modf ((thisms/thistick), &dummy);

         if (frac == 0.0)
         {
            numer = (thisms/OneDay);
            numer = (numer - st->xmin);
            denom = (st->xmax - st->xmin);
            fday = (numer/denom);

            ms2dhms (&past, &hr, &mn, &sec, thisms);
            yrdy1958 (&year, &dy, past);

            sprintf (name, "%04d %03d", year, dy);

            displ = 1.5;

            coord = (float) fday;

            pgmtxt_ ("B", &displ, &coord, &fjust, name, 1L, (long) strlen (name));

            if ((EphemSetValues  [0]) || (EphemSetValues  [3]) ||
                (EphemSetValues  [6]) || (EphemSetValues  [9]) ||
                (EphemSetValues [12]) || (EphemSetValues [15]))
            {
               target = Saturn;

               GeographicCoords (thisms, target, &SaturnR,
                                 &SaturnLon, &SaturnLat,
                                 &SaturnMLat, &SaturnLT, &SaturnL);

               EphemDataValues  [0] = SaturnR;
               EphemDataValues  [3] = SaturnLat;
               EphemDataValues  [6] = SaturnMLat;
               EphemDataValues  [9] = SaturnLon;
               EphemDataValues [12] = SaturnLT;
               EphemDataValues [15] = SaturnL;
            }

            if ((EphemSetValues  [1]) || (EphemSetValues  [4]) ||
                (EphemSetValues [10]) || (EphemSetValues [13]))
            {
               target = Enceladus;

               GeographicCoords (thisms, target, &EnceladusR,
                                 &EnceladusLon, &EnceladusLat,
                                 &dummy, &EnceladusLT, &dummy);

               EphemDataValues  [1] = EnceladusR;
               EphemDataValues  [4] = EnceladusLat;
               EphemDataValues [10] = EnceladusLon;
               EphemDataValues [13] = EnceladusLT;
            }

            if ((EphemSetValues  [2]) || (EphemSetValues  [5]) ||
                (EphemSetValues [11]) || (EphemSetValues [14]))
            {
               target = Titan;

               GeographicCoords (thisms, target, &TitanR,
                                 &TitanLon, &TitanLat,
                                 &dummy, &TitanLT, &dummy);

               EphemDataValues  [2] = TitanR;
               EphemDataValues  [5] = TitanLat;
               EphemDataValues [11] = TitanLon;
               EphemDataValues [14] = TitanLT;
            }

            displ = 2.6;

            for (EphemSetIndex = 0; EphemSetIndex < 18; EphemSetIndex++)
            {
               if (EphemSetValues [EphemSetIndex])
               {
                  sprintf (name, "%6.2f",
                           (float) EphemDataValues [EphemSetIndex]);
                  pgmtxt_ ("B", &displ, &coord, &fjust,
                           name, 1L, (long) strlen (name));

                  displ += 1.1;
               }
            }
         }

         thisms += DeltaTime;
      }

      nsub = 0;
      fmin = 0.0;
      disp = 1.0;
      orient = 0.0;

      step = xtick1;

      for (index = 0; index < MajorTickIndex; index++)
      {
         CurrentTime = MajorTicks [index];

         numer = (CurrentTime/OneDay);
         numer = (numer - st->xmin);
         denom = (st->xmax - st->xmin);
         fday = (numer/denom);

         ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
         yrdy1958 (&yr, &dy, past);

         if (index < (MajorTickIndex - 1))
         {
            dmajr = 0.5;
            dmajl = 0.0;

            y1 = (float) st->ymin;
            y2 = (float) st->ymin;

            x1 = GetTimePosition (st, CurrentTime);
            v1 = dy;

            CurrentTime = MajorTicks [(index + 1)];

            x2 = GetTimePosition (st, CurrentTime);
            v2 = (v1 + step);
            v2 += 0.001;

            pgaxis_ ("", &x1, &y1, &x2, &y2, &v1, &v2, &step, &nsub, &dmajl, &dmajr, &fmin, &disp, &orient, 0L);

            dmajr = 0.0;
            dmajl = 0.5;

            y1 = (float) st->ymax;
            y2 = (float) st->ymax;

            pgaxis_ ("", &x1, &y1, &x2, &y2, &v1, &v2, &step, &nsub, &dmajl, &dmajr, &fmin, &disp, &orient, 0L);
         }

         sprintf (name, "%04d %03d", yr, dy);

         displ = 1.5;
         coord = fday;
         fjust = 0.5;

         pgmtxt_ ("B", &displ, &coord, &fjust, name, 1L, (long) strlen (name));
      }

      step = (float) (xtick1/ (float) xtick2);

      for (index = 0; index < (MinorTickIndex - 1); index++)
      {
         dmajr = 0.25;
         dmajl = 0.0;

         y1 = (float) st->ymin;
         y2 = (float) st->ymin;

         CurrentTime = MinorTicks [index];

         ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
         yrdy1958 (&yr, &dy, past);

         x1 = GetTimePosition (st, CurrentTime);
         v1 = dy;

         CurrentTime = MinorTicks [(index + 1)];

         ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
         yrdy1958 (&yr, &dy, past);

         x2 = GetTimePosition (st, CurrentTime);
         v2 = (v1 + step);
         v2 += 0.001;

         pgaxis_ ("", &x1, &y1, &x2, &y2, &v1, &v2, &step, &nsub, &dmajl, &dmajr, &fmin, &disp, &orient, 0L);

         dmajr = 0.0;
         dmajl = 0.25;

         y1 = (float) st->ymax;
         y2 = (float) st->ymax;

         pgaxis_ ("", &x1, &y1, &x2, &y2, &v1, &v2, &step, &nsub, &dmajl, &dmajr, &fmin, &disp, &orient, 0L);
      }
   }
   else if (st->MnDyOrYr == Second)
   {
      if (st->LinOrLogYAxis)
         pgbox_ ("BC", &xtick1, &xtick2, "BCINSTV1", &ytick1, &ytick2, 2L, 8L);
      else
         pgbox_ ("BC", &xtick1, &xtick2, "BCILNSTV2", &ytick1, &ytick2, 2L, 9L);

      if ((st->st_sec == 0.0) && (st->sp_sec == 0.0))
         DeltaTime = OneMinute;
      else
         DeltaTime = OneSecond;

      thisms = st->st_total_ms;
      thistick = (double) (xtick1*OneDay);

      MajorTickIndex = 0;
      MinorTickIndex = 0;

      while (thisms <= st->sp_total_ms)
      {
         frac = modf ((thisms/thistick), &dummy);

         ms2dhms (&past, &hr, &mn, &sec, thisms);
         yrdy1958 (&yr, &dy, past);

         if (((dy % (int) xtick1) == 0) && (hr == 0) && (mn == 0) && (sec == 0.0))
         {
            CurrentTime = GetTime (yr, dy, hr, mn, (float) sec);

            MajorTicks [MajorTickIndex] = CurrentTime;
            MajorTickIndex++;
         }

         if (((dy % ModInt) == 0) && (hr == 0) && (mn == 0) && (sec == 0.0))
         {
            CurrentTime = GetTime (yr, dy, hr, mn, (float) sec);

            MinorTicks [MinorTickIndex] = CurrentTime;
            MinorTickIndex++;
         }

         thisms += DeltaTime;
      }

      nsub = 0;
      fmin = 0.0;
      disp = 1.0;
      orient = 0.0;

      step = xtick1;

      for (index = 0; index < MajorTickIndex; index++)
      {
         CurrentTime = MajorTicks [index];

         numer = (CurrentTime/OneDay);
         numer = (numer - st->xmin);
         denom = (st->xmax - st->xmin);
         fday = (numer/denom);

         ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
         yrdy1958 (&yr, &dy, past);

         if (index < (MajorTickIndex - 1))
         {
            dmajr = 0.5;
            dmajl = 0.0;

            y1 = (float) st->ymin;
            y2 = (float) st->ymin;

            x1 = GetTimePosition (st, CurrentTime);
            v1 = dy;

            CurrentTime = MajorTicks [(index + 1)];

            x2 = GetTimePosition (st, CurrentTime);
            v2 = (v1 + step);
            v2 += 0.001;

            pgaxis_ ("", &x1, &y1, &x2, &y2, &v1, &v2, &step, &nsub, &dmajl, &dmajr, &fmin, &disp, &orient, 0L);

            dmajr = 0.0;
            dmajl = 0.5;

            y1 = (float) st->ymax;
            y2 = (float) st->ymax;

            pgaxis_ ("", &x1, &y1, &x2, &y2, &v1, &v2, &step, &nsub, &dmajl, &dmajr, &fmin, &disp, &orient, 0L);
         }

         sprintf (name, "%04d %03d", yr, dy);

         displ = 1.5;
         coord = fday;
         fjust = 0.5;

         pgmtxt_ ("B", &displ, &coord, &fjust, name, 1L, (long) strlen (name));
      }

      step = (float) (xtick1/ (float) xtick2);

      for (index = 0; index < (MinorTickIndex - 1); index++)
      {
         dmajr = 0.25;
         dmajl = 0.0;

         y1 = (float) st->ymin;
         y2 = (float) st->ymin;

         CurrentTime = MinorTicks [index];

         ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
         yrdy1958 (&yr, &dy, past);

         x1 = GetTimePosition (st, CurrentTime);
         v1 = dy;

         CurrentTime = MinorTicks [(index + 1)];

         ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
         yrdy1958 (&yr, &dy, past);

         x2 = GetTimePosition (st, CurrentTime);
         v2 = (v1 + step);
         v2 += 0.001;

         pgaxis_ ("", &x1, &y1, &x2, &y2, &v1, &v2, &step, &nsub, &dmajl, &dmajr, &fmin, &disp, &orient, 0L);

         dmajr = 0.0;
         dmajl = 0.25;

         y1 = (float) st->ymax;
         y2 = (float) st->ymax;

         pgaxis_ ("", &x1, &y1, &x2, &y2, &v1, &v2, &step, &nsub, &dmajl, &dmajr, &fmin, &disp, &orient, 0L);
      }
   }
   else if (st->MnDyOrYr == Third)
   {
      pgbox_ ("ABCINST", &xtick1, &xtick2, "BCILNSTV2", &ytick1, &ytick2, 7L, 9L);
   }

   if ((st->ephemeris == 0) ||
       (st->MnDyOrYr == Second) ||
       (st->MnDyOrYr == Third))
   {
      coord = 0.5;
      fjust = 0.5;

      displ = 3.0;

      pgmtxt_ ("B", &displ, &coord, &fjust, xnameT0, 1L, (long) strlen (xnameT0));
   }
   else
   {
      coord = 1.07;
      fjust = 0.0;

      displ = 1.5;

      pgmtxt_ ("B", &displ, &coord, &fjust, xnameT0, 1L, (long) strlen (xnameT0));

      displ = 2.6;

      for (EphemSetIndex = 0; EphemSetIndex < 18; EphemSetIndex++)
      {
         if (EphemSetValues [EphemSetIndex])
         {
            pgmtxt_ ("B", &displ, &coord, &fjust, xnames [EphemSetIndex],
                     1L, (long) strlen (xnames [EphemSetIndex]));

            displ += 1.1;
         }
      }
   }
}

void GetTimeStats (st, MnDyOrYr, xmin, xmax)
struct time_struc *st;
int *MnDyOrYr;
double *xmin, *xmax;
{
   int itemp;
   int TempPast, TempYear, TempDoy;
   double temp1, temp2, temp3, xtemp1, xtemp2, TempMS0, TempMS1;
   double st_ms, sp_ms, sec;
   double FortyEightHours = 1.728e+08, OneKDays = 8.64e+10;

   st_ms = (double) st->st_past*86400000.0 +
           (double) st->st_hr*3600000.0 +
           (double) st->st_mn*60000.0 +
           (double) st->st_sec*1000.0;

   sp_ms = (double) st->sp_past*86400000.0 +
           (double) st->sp_hr*3600000.0 +
           (double) st->sp_mn*60000.0 +
           (double) st->sp_sec*1000.0;

   if ((sp_ms - st_ms) < FortyEightHours)
      itemp = Zeroeth;
   else if (((sp_ms - st_ms) >= FortyEightHours) &&
            ((sp_ms - st_ms) <  OneKDays))
      itemp = Second;
   else
      itemp = Third;

   if (itemp == Zeroeth)
   {
      /* convert to minutes */
      xtemp1 = (st->st_total_ms/60000.0);
      xtemp2 = (st->sp_total_ms/60000.0);
   }
   else if (itemp == Second)
   {
      /* convert to days */
      st_ms = (double) (st->st_hr*3600.0) +
              (double) (st->st_mn*60.0) +
              (double) (st->st_sec);

      st_ms = (st_ms/86400.0);
      xtemp1 = (double) ((double) st->st_past + st_ms);

      sp_ms = (double) (st->sp_hr*3600.0) +
              (double) (st->sp_mn*60.0) +
              (double) (st->sp_sec);

      sp_ms = (sp_ms/86400.0);
      xtemp2 = (double) ((double) st->sp_past + sp_ms);
   }
   else if (itemp == Third)
   {
      TempYear = st->st_year;
      TempDoy = 1;
      TempPast = past_1958 (TempYear, TempDoy);
      TempMS0 = (double) (TempPast*86400000.0);

      TempYear++;
      TempPast = past_1958 (TempYear, TempDoy);
      TempMS1 = (double) (TempPast*86400000.0);

      temp1 = (st->st_total_ms - TempMS0);
      temp2 = (TempMS1 - TempMS0);

      temp3 = (temp1/temp2);
      xtemp1 = (double) st->st_year;
      xtemp1 += temp3;

      TempYear = st->sp_year;
      TempDoy = 1;
      TempPast = past_1958 (TempYear, TempDoy);
      TempMS0 = (double) (TempPast*86400000.0);

      TempYear++;
      TempPast = past_1958 (TempYear, TempDoy);
      TempMS1 = (double) (TempPast*86400000.0);

      temp1 = (st->sp_total_ms - TempMS0);
      temp2 = (TempMS1 - TempMS0);

      temp3 = (temp1/temp2);
      xtemp2 = (double) st->sp_year;
      xtemp2 += temp3;
   }

   *MnDyOrYr = itemp;

   *xmin = xtemp1;
   *xmax = xtemp2;
}

double GetRadValue (st, CurrentTime)
struct time_struc *st;
double CurrentTime;
{
   int past, year, doy, hr, mn;
   int target;
   int observer;
   double sec;
   double et;
   double radius;
   double altitude;
   double ReturnValue;
   char scstr [80];

   observer = Cassini;

   if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
       (st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt))
   {
      target = Saturn;
   }
   else if ((st->XAxis == EnceladusRad) || (st->XAxis == EnceladusAlt) ||
            (st->YAxis == EnceladusRad) || (st->YAxis == EnceladusAlt))
   {
      target = Enceladus;
   }
   else if ((st->XAxis == TitanRad) || (st->XAxis == TitanAlt) ||
            (st->YAxis == TitanRad) || (st->YAxis == TitanAlt))
   {
      target = Titan;
   }

   ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
   yrdy1958 (&year, &doy, past);

   sprintf (scstr, "%04d %03d // %02d:%02d:%06.3f",
            year, doy, hr, mn, (float) sec);

   utc2et (scstr, &et);

   get_altitude_ (&target, &observer, &et, &altitude, &radius);

   if ((st->XAxis == SaturnRad) || (st->YAxis == SaturnRad))
      ReturnValue = radius;
   else if ((st->XAxis == SaturnAlt) || (st->YAxis == SaturnAlt))
      ReturnValue = altitude;
   else if ((st->XAxis == EnceladusRad) || (st->YAxis == EnceladusRad))
      ReturnValue = radius;
   else if ((st->XAxis == EnceladusAlt) || (st->YAxis == EnceladusAlt))
      ReturnValue = altitude;
   else if ((st->XAxis == TitanRad) || (st->YAxis == TitanRad))
      ReturnValue = radius;
   else if ((st->XAxis == TitanAlt) || (st->YAxis == TitanAlt))
      ReturnValue = altitude;

   return (ReturnValue);
}

double GetLValue (st, CurrentTime)
struct time_struc *st;
double CurrentTime;
{
   int past, year, doy, hr, mn;
   int observer;
   int target;
   int BodyName;
   double radius;
   double latitude;
   double mlatitude;
   double longitude;
   double loctime;
   double thelvalue;
   double sec;
   double et;
   char scstr [80];

   ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
   yrdy1958 (&year, &doy, past);

   sprintf (scstr, "%04d %03d // %02d:%02d:%06.3f",
            year, doy, hr, mn, (float) sec);

   utc2et (scstr, &et);

   if ((st->XAxis == SaturnMLatitude) || (st->XAxis == TheLValue) || (st->XAxis == SaturnKLatitude))
   {
      BodyName = Saturn;
      target = Saturn;
   }
   else if (st->XAxis == TitanLatitude)
   {
      BodyName = Titan;
      target = Titan;
   }

   observer = Cassini;

   get_khronos_ (&observer, &target, &BodyName, &et, &radius, &latitude, &mlatitude, &longitude, &loctime, &thelvalue);

   if (st->XAxis == SaturnMLatitude)
      return (mlatitude);
   else if (st->XAxis == SaturnKLatitude)
      return (latitude);
   else if (st->XAxis == TitanLatitude)
      return (latitude);
   else
      return (thelvalue);
}

void DZonal (g1, g2, g3, radius, theta, Br, Bt, Bm)
double g1, g2, g3, radius, theta, *Br, *Bt, *Bm;
{
   double cosTheta, sinTheta, cosSqTheta;
   double InvRad, InvRadSq, InvRadCu;
   double x, g2s1, g3s1;
   double Bradius, Btheta, Bmag;

/*
   Ref: Connerney, J. E. P., M. H. Acuna, and N. F. Ness,
   "Zonal Harmonic Model of Saturn's magnetic Field from
   Voyager 1 and 2 Observations", NATURE 298, 44-46, 1982.
*/

   cosTheta = cos (theta);
   sinTheta = sin (theta);

   cosSqTheta = (cosTheta*cosTheta);

   InvRad   = (1.0/radius);
   InvRadSq = (InvRad*InvRad);
   InvRadCu = (InvRadSq*InvRad);

   g2s1 = (g2/g1);
   g3s1 = (g3/g1);

   x  = cosTheta;
   x += 9.0*g2s1*InvRad*(cosSqTheta - 1.0/3.0)/4.0;
   x += 5.0*g3s1*InvRadSq*cosTheta*(cosSqTheta - 3.0/5.0);

   Bradius = (2.0*g1*x*InvRadCu);

   x  = 1.0;
   x +=  3.0*g2s1*InvRad*cosTheta;
   x += 15.0*g3s1*InvRadSq*(cosSqTheta - 1.0/5.0)/2.0;

   Btheta = (g1*sinTheta*x*InvRadCu);
   Bmag = sqrt ((Bradius*Bradius) + (Btheta*Btheta));

   *Br = Bradius;
   *Bt = Btheta;
   *Bm = Bmag;
}

double Z3Model (CurrentTime)
double CurrentTime;
{
   int past, year, doy, hr, mn;
   int observer, target, BodyName;
   double et;
   double g1, g2, g3;
   double colatitude, radius;
   double Br, Btheta, Bmag;
   double sec;
   char scstr [80];

   g1 = 0.21184;
   g2 = 0.01606;
   g3 = 0.02669;

   ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
   yrdy1958 (&year, &doy, past);

   sprintf (scstr, "%04d %03d // %02d:%02d:%06.3f",
            year, doy, hr, mn, (float) sec);

   utc2et (scstr, &et);

   BodyName = Saturn;
   target = Saturn;
   observer = Cassini;

   get_mag_ephem_ (&observer, &target, &BodyName, &et, &radius, &colatitude);

   DZonal (g1, g2, g3, radius, colatitude, &Br, &Btheta, &Bmag);

   Bmag *= Gauss2nT;

   return (Bmag);
}

void ProcessModel (st)
struct time_struc *st;
{
   int index;
   int past, yr, dy, hr, mn;
   int WaterOrHydrogen;
   int target;
   float xpt, xval;
   float xvals [MaxPoints], yvals [MaxPoints];
   double sec;
   double n1result, n2result, latitude;
   double neresult, temp1, temp2, potential, deriv_final_value;
   double old_potential, h1_squared, h2_squared, deriv_n1result;
   double deriv_n2result, potential_diff;
   double deriv_neresult;
   double final_value;
   double lvalue;
   double n_zero_one, n_zero_two;
   double scale_height_one, scale_height_two;
   double radius, latitutde;
   double RadLatitude, CurrentLShell;
   double temp3, temp4;
   double OldCurrentTime;
   double CurrentTime;
   double delta;
   double dummy1, dummy2, dummy3, dummy4;
   double ModelTimeGap;
   double temp5;
   double edensity;
   double BmagValue;
   double TempLog;

   index = 0;

   alpha = 4.308;
   beta_one = 0.03639;
   beta_two = 0.655;

   delta = st->DataGapTime;

   ModelTimeGap = delta;
   ModelTimeGap += 1000.0;

   CurrentTime = st->st_total_ms;

   while (CurrentTime <= st->sp_total_ms)
   {
      if (((CurrentTime - OldCurrentTime) > ModelTimeGap) && (index > 0))
      {
         pgline_ (&index, xvals, yvals);
         index = 0;
      }

      target = Saturn;

      GeographicCoords (CurrentTime, target, &radius,
                        &dummy1, &latitude, &dummy2, &dummy3, &dummy4);

      RadLatitude = (latitude*RPD);

      temp3 = cos (RadLatitude);
      temp4 = (temp3*temp3);

      CurrentLShell = (radius/temp4);

      if ((CurrentLShell < 3.5) || (CurrentLShell > 10.0))
      {
         ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
         yrdy1958 (&yr, &dy, past);

         CurrentTime += delta;

         continue;
      }

      lvalue = CurrentLShell;

      volts = Get_We (CurrentLShell);

      scale_height_one = Get_ScaleHeight_OHplus (CurrentLShell);

      scale_height_two = Get_ScaleHeight_Hplus (CurrentLShell);

      n_zero_one = Get_EqDensity_OHplus (CurrentLShell);

      n_zero_two = Get_EqDensity_Hplus (CurrentLShell);

      WaterOrHydrogen = True;

      a_one = Get_Anisotropy (WaterOrHydrogen, CurrentLShell);

      WaterOrHydrogen = False;

      a_two = Get_Anisotropy (WaterOrHydrogen, CurrentLShell);

      h1_squared = (scale_height_one*scale_height_one);
      h2_squared = (scale_height_two*scale_height_two);

      potential = 2.0;
      potential_diff = 99999.9;

      while (potential_diff > 1.0e-08)
      {
         n1result = n1value (latitude, lvalue, n_zero_one,
                                scale_height_one, potential);

         n2result = n2value (latitude, lvalue, n_zero_two,
                                scale_height_two, potential);

         neresult = nevalue (n_zero_one, n_zero_two, potential);

         deriv_n1result = (beta_one/h1_squared);
         deriv_n1result *= -1.0;
         deriv_n1result *= n1result;

         deriv_n2result = (beta_two/h2_squared);
         deriv_n2result *= -1.0;
         deriv_n2result *= n2result;

         deriv_neresult = (1.0/volts);
         deriv_neresult *= neresult;

         temp1 = (n1result + n2result);
         final_value = (neresult - temp1);

         temp1 = (deriv_n1result + deriv_n2result);
         deriv_final_value = (deriv_neresult - temp1);

         temp1 = (final_value/deriv_final_value);
         old_potential = potential;

         potential -= temp1;
         potential_diff = fabs ((potential - old_potential));
      }

      temp1 = latitude;
      temp1 *= RPD;
      temp2 = cos (temp1);
      temp1 = pow (temp2, 6.0);
      temp1 *= -1.0;
      temp1 += 1.0;

      temp1 = n1value (latitude, lvalue, n_zero_one, scale_height_one,
                       potential);

      temp2 = temp1;

      temp1 = n2value (latitude, lvalue, n_zero_two, scale_height_two,
                       potential);

      temp2 += temp1;

      edensity = temp2;

      if (st->YAxis == Ne)
      {
         if (st->LinOrLogYAxis)
            yvals [index] = (float) edensity;
         else
            yvals [index] = (float) log10 ((double) edensity);
      }
      else if (st->YAxis == NeOverB)
      {
         BmagValue = Z3Model (CurrentTime);
         temp5 = edensity;
         temp5 /= BmagValue;

         yvals [index] = (float) log10 (temp5);
      }
      else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
               (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
         yvals [index] = (float) GetRadValue (st, CurrentTime);

      if (st->XAxis == Time)
      {
         xval = GetTimePosition (st, CurrentTime);

         xvals [index] = xval;
      }
      else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
               (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
      {
         if (st->LinOrLog)
            xvals [index] = (float) GetRadValue (st, CurrentTime);
         else
         {
            TempLog = GetRadValue (st, CurrentTime);
            xvals [index] = (float) log10 (TempLog);
         }
      }
      else if (st->XAxis == Ne)
      {
         xvals [index] = (float) log10 ((double) edensity);
      }
      else if (st->XAxis == NeOverB)
      {
         BmagValue = Z3Model (CurrentTime);
         temp5 = (double) edensity;
         temp5 /= BmagValue;

         xvals [index] = (float) log10 (temp5);
      }
      else if ((st->XAxis == TheLValue)       ||
               (st->XAxis == SaturnMLatitude) ||
               (st->XAxis == SaturnKLatitude) ||
               (st->XAxis == TitanLatitude))
      {
         if (st->LinOrLog)
            xvals [index] = (float) GetLValue (st, CurrentTime);
         else
         {
            TempLog = GetLValue (st, CurrentTime);
            xvals [index] = (float) log10 (TempLog);
         }
      }

      OldCurrentTime = CurrentTime;
      CurrentTime += delta;
      index++;
   }

   if (index > 0)
      pgline_ (&index, xvals, yvals);
}

void ProcessModel_Points (st)
struct time_struc *st;
{
   int past, yr, dy, hr, mn;
   int WaterOrHydrogen;
   int target;
   int nsymbol = 850;
   float xpt;
   float xval, yval;
   double sec;
   double n1result, n2result, latitude;
   double neresult, temp1, temp2, potential, deriv_final_value;
   double old_potential, h1_squared, h2_squared, deriv_n1result;
   double deriv_n2result, potential_diff;
   double deriv_neresult;
   double final_value;
   double lvalue;
   double n_zero_one, n_zero_two;
   double scale_height_one, scale_height_two;
   double radius, latitutde;
   double RadLatitude, CurrentLShell;
   double temp3, temp4;
   double CurrentTime;
   double delta;
   double dummy1, dummy2, dummy3, dummy4;
   double temp5;
   double edensity;
   double BmagValue;
   double TempLog;

   alpha = 4.308;
   beta_one = 0.03639;
   beta_two = 0.655;

   delta = st->DataGapTime;

   CurrentTime = st->st_total_ms;

   while (CurrentTime <= st->sp_total_ms)
   {
      target = Saturn;

      GeographicCoords (CurrentTime, target, &radius,
                        &dummy1, &latitude, &dummy2, &dummy3, &dummy4);

      RadLatitude = (latitude*RPD);

      temp3 = cos (RadLatitude);
      temp4 = (temp3*temp3);

      CurrentLShell = (radius/temp4);

      if ((CurrentLShell < 3.5) || (CurrentLShell > 10.0))
      {
         ms2dhms (&past, &hr, &mn, &sec, CurrentTime);
         yrdy1958 (&yr, &dy, past);

         CurrentTime += delta;

         continue;
      }

      lvalue = CurrentLShell;

      volts = Get_We (CurrentLShell);

      scale_height_one = Get_ScaleHeight_OHplus (CurrentLShell);

      scale_height_two = Get_ScaleHeight_Hplus (CurrentLShell);

      n_zero_one = Get_EqDensity_OHplus (CurrentLShell);

      n_zero_two = Get_EqDensity_Hplus (CurrentLShell);

      WaterOrHydrogen = True;

      a_one = Get_Anisotropy (WaterOrHydrogen, CurrentLShell);

      WaterOrHydrogen = False;

      a_two = Get_Anisotropy (WaterOrHydrogen, CurrentLShell);

      h1_squared = (scale_height_one*scale_height_one);
      h2_squared = (scale_height_two*scale_height_two);

      potential = 2.0;
      potential_diff = 99999.9;

      while (potential_diff > 1.0e-08)
      {
         n1result = n1value (latitude, lvalue, n_zero_one,
                                scale_height_one, potential);

         n2result = n2value (latitude, lvalue, n_zero_two,
                                scale_height_two, potential);

         neresult = nevalue (n_zero_one, n_zero_two, potential);

         deriv_n1result = (beta_one/h1_squared);
         deriv_n1result *= -1.0;
         deriv_n1result *= n1result;

         deriv_n2result = (beta_two/h2_squared);
         deriv_n2result *= -1.0;
         deriv_n2result *= n2result;

         deriv_neresult = (1.0/volts);
         deriv_neresult *= neresult;

         temp1 = (n1result + n2result);
         final_value = (neresult - temp1);

         temp1 = (deriv_n1result + deriv_n2result);
         deriv_final_value = (deriv_neresult - temp1);

         temp1 = (final_value/deriv_final_value);
         old_potential = potential;

         potential -= temp1;
         potential_diff = fabs ((potential - old_potential));
      }

      temp1 = latitude;
      temp1 *= RPD;
      temp2 = cos (temp1);
      temp1 = pow (temp2, 6.0);
      temp1 *= -1.0;
      temp1 += 1.0;

      temp1 = n1value (latitude, lvalue, n_zero_one, scale_height_one,
                       potential);

      temp2 = temp1;

      temp1 = n2value (latitude, lvalue, n_zero_two, scale_height_two,
                       potential);

      temp2 += temp1;

      edensity = temp2;

      if (st->YAxis == Ne)
      {
         if (st->LinOrLogYAxis)
            yval = (float) edensity;
         else
            yval = (float) log10 ((double) edensity);
      }
      else if (st->YAxis == NeOverB)
      {
         BmagValue = Z3Model (CurrentTime);
         temp5 = edensity;
         temp5 /= BmagValue;

         yval = (float) log10 (temp5);
      }
      else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
               (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
         yval = (float) GetRadValue (st, CurrentTime);

      if (st->XAxis == Time)
         xval = GetTimePosition (st, CurrentTime);
      else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
               (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
      {
         if (st->LinOrLog)
            xval = (float) GetRadValue (st, CurrentTime);
         else
         {
            TempLog = GetRadValue (st, CurrentTime);
            xval = (float) log10 (TempLog);
         }
      }
      else if (st->XAxis == Ne)
      {
         xval = (float) log10 ((double) edensity);
      }
      else if (st->XAxis == NeOverB)
      {
         BmagValue = Z3Model (CurrentTime);
         temp5 = (double) edensity;
         temp5 /= BmagValue;

         xval = (float) log10 (temp5);
      }
      else if ((st->XAxis == TheLValue)       ||
               (st->XAxis == SaturnMLatitude) ||
               (st->XAxis == SaturnKLatitude) ||
               (st->XAxis == TitanLatitude))
      {
         if (st->LinOrLog)
            xval = (float) GetLValue (st, CurrentTime);
         else
         {
            TempLog = GetLValue (st, CurrentTime);
            xval = (float) log10 (TempLog);
         }
      }

      pgpt1_ (&xval, &yval, &nsymbol);

      CurrentTime += delta;
   }
}

int ProcessData (st, dbase, n_files)
struct time_struc *st;
struct file_structure *dbase;
int n_files;
{
   char freq_ch;
   int year, day, hour, minute;
   int past, yr, dy, hr, mn;
   int no_data = True, dflag, index, std_inx, cl;
   int OldQuality;
   int target;
   float ss, edensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double NewQuality;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double LocalTime;
   double OldLocalTime;
   double dummy1;
   double dummy2;
   double dummy3;
   double dummy4;
   double dummy5;
   FILE *finput;

   std_inx = 0;
   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen ((dbase + std_inx)->line, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n",
               progname, (dbase + std_inx)->line);
      exit (1);
   }

   while (!feof (finput))
   {
      if (st->SaturnOrTitan)
         dflag = fscanf (finput, "%d %d %d %d %f %*f %*f %e %*f %*f %*f %d %c",
                         &yr, &dy, &hr, &mn, &ss, &edensity, &cl, &freq_ch);
      else
         dflag = fscanf (finput, "%d %d %d %d %f %*f %*f %e %*f %*f %*f %*f %d %c",
                         &yr, &dy, &hr, &mn, &ss, &edensity, &cl, &freq_ch);

      if (dflag < 0)
      {
         fclose (finput);

         std_inx++;

         if (std_inx < n_files)
         {
            if (!(finput = fopen ((dbase + std_inx)->line, "r")))
            {
               fprintf (stderr, "%s:  error opening %s\n",
                        progname, (dbase + std_inx)->line);
               exit (1);
            }

            if (st->SaturnOrTitan)
               dflag = fscanf (finput, "%d %d %d %d %f %*f %*f %e %*f %*f %*f %d %c",
                               &yr, &dy, &hr, &mn, &ss, &edensity, &cl, &freq_ch);
            else
               dflag = fscanf (finput, "%d %d %d %d %f %*f %*f %e %*f %*f %*f %*f %d %c",
                               &yr, &dy, &hr, &mn, &ss, &edensity, &cl, &freq_ch);
         }
         else
            break;
      }

      if ((st->SkipDropout) && (freq_ch == 'd'))
         continue;

      if (freq_ch == 'q')
         continue;

      if (cl < 0)
      {
         NewQuality = (double) cl;
         NewQuality /= -100.0;

         if (NewQuality >= st->NewQuality)
            continue;
      }
      else
      {
         OldQuality = cl;

         if (OldQuality > st->cl)
            continue;
      }

      OldTime = CurrentTime;
      CurrentTime = GetTime (yr, dy, hr, mn, ss);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) edensity;
            else
               ypts [index] = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if (st->YAxis == FpOverFc)
         {
         }
         else if ((st->YAxis == SaturnRad)    || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == TitanRad)     || (st->YAxis == TitanAlt)  ||
                  (st->YAxis == EnceladusRad) || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad)    || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == TitanRad)     || (st->XAxis == TitanAlt)  ||
                  (st->XAxis == EnceladusRad) || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) edensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude) ||
                  (st->XAxis == TitanLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == SaturnLocalTime)
         {
            target = Saturn;

            OldLocalTime = LocalTime;

            GeographicCoords (CurrentTime, target, &dummy1,
                              &dummy2, &dummy3,
                              &dummy4, &LocalTime, &dummy5);

            if (LocalTime < 12.0)
               LocalTime += 24.0;

            if ((fabs (LocalTime - OldLocalTime) > 12.0) && (index > 0))
            {
               pgline_ (&index, xpts, ypts);
               index = 0;
            }

            xpts [index] = (float) LocalTime;

            index++;
         }
         else if (st->XAxis == TitanLocalTime)
         {
            target = Titan;

            OldLocalTime = LocalTime;

            GeographicCoords (CurrentTime, target, &dummy1,
                              &dummy2, &dummy3,
                              &dummy4, &LocalTime, &dummy5);

            if (LocalTime < 12.0)
               LocalTime += 24.0;

            if ((fabs (LocalTime - OldLocalTime) > 12.0) && (index > 0))
            {
               pgline_ (&index, xpts, ypts);
               index = 0;
            }

            xpts [index] = (float) LocalTime;

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

/*
   fclose (finput);
*/

   return (no_data);
}

int ProcessData_Points (st, dbase, n_files)
struct time_struc *st;
struct file_structure *dbase;
int n_files;
{
   char freq_ch;
   int nsymbol = 850;
   int year, day, hour, minute;
   int past, yr, dy, hr, mn;
   int no_data = True, dflag, std_inx, cl;
   int OldQuality;
   int target;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double NewQuality;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double LocalTime;
   double dummy1;
   double dummy2;
   double dummy3;
   double dummy4;
   double dummy5;
   FILE *finput;

   std_inx = 0;

   if (!(finput = fopen ((dbase + std_inx)->line, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n",
               progname, (dbase + std_inx)->line);
      exit (1);
   }

   while (!feof (finput))
   {

      if (st->SaturnOrTitan)
         dflag = fscanf (finput, "%d %d %d %d %f %*f %*f %e %*f %*f %*f %d %c",
                         &yr, &dy, &hr, &mn, &ss, &edensity, &cl, &freq_ch);
      else
         dflag = fscanf (finput, "%d %d %d %d %f %*f %*f %e %*f %*f %*f %*f %d %c",
                         &yr, &dy, &hr, &mn, &ss, &edensity, &cl, &freq_ch);

      if (dflag < 0)
      {
         fclose (finput);

         std_inx++;

         if (std_inx < n_files)
         {
            if (!(finput = fopen ((dbase + std_inx)->line, "r")))
            {
               fprintf (stderr, "%s:  error opening %s\n",
                        progname, (dbase + std_inx)->line);
               exit (1);
            }

            if (st->SaturnOrTitan)
               dflag = fscanf (finput, "%d %d %d %d %f %*f %*f %e %*f %*f %*f %d %c",
                               &yr, &dy, &hr, &mn, &ss, &edensity, &cl, &freq_ch);
            else
               dflag = fscanf (finput, "%d %d %d %d %f %*f %*f %e %*f %*f %*f %*f %d %c",
                               &yr, &dy, &hr, &mn, &ss, &edensity, &cl, &freq_ch);
         }
         else
            break;
      }

      if ((st->SkipDropout) && (freq_ch == 'd'))
         continue;

      if (freq_ch == 'q')
         continue;

      if (cl < 0)
      {
         NewQuality = (double) cl;
         NewQuality /= -100.0;

         if (NewQuality >= st->NewQuality)
            continue;
      }
      else
      {
         OldQuality = cl;

         if (OldQuality > st->cl)
            continue;
      }

      CurrentTime = GetTime (yr, dy, hr, mn, ss);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if (st->YAxis == FpOverFc)
         {
         }
         else if ((st->YAxis == SaturnRad)    || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == TitanRad)     || (st->YAxis == TitanAlt)  ||
                  (st->YAxis == EnceladusRad) || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad)    || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == TitanRad)     || (st->XAxis == TitanAlt)  ||
                  (st->XAxis == EnceladusRad) || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude) ||
                  (st->XAxis == TitanLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == SaturnLocalTime)
         {
            target = Saturn;

            GeographicCoords (CurrentTime, target, &dummy1,
                              &dummy2, &dummy3,
                              &dummy4, &LocalTime, &dummy5);

            if (LocalTime < 12.0)
               LocalTime += 24.0;

            xval = (float) LocalTime;
         }
         else if (st->XAxis == TitanLocalTime)
         {
            target = Titan;

            GeographicCoords (CurrentTime, target, &dummy1,
                              &dummy2, &dummy3,
                              &dummy4, &LocalTime, &dummy5);

            if (LocalTime < 12.0)
               LocalTime += 24.0;

            xval = (float) LocalTime;
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

/*
   fclose (finput);
*/

   return (no_data);
}

int ProcessDataProxy (st, dbase, n_files)
struct time_struc *st;
struct file_structure *dbase;
int n_files;
{
   int year, month, doy, day, hour, min;
   int past, yr, mh, dy, hr, mn;
   int no_data = True, dflag, index, std_inx, cl;
   int target;
   float ss, edensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   double LocalTime;
   double OldLocalTime;
   double dummy1;
   double dummy2;
   double dummy3;
   double dummy4;
   double dummy5;
   char TimeString [MAXCHARS];
   FILE *finput;

   std_inx = 0;
   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen ((dbase + std_inx)->line, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n",
               progname, (dbase + std_inx)->line);
      exit (1);
   }

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*f %*f %e",
                      &yr, &mh, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
      {
         fclose (finput);

         std_inx++;

         if (std_inx < n_files)
         {
            if (!(finput = fopen ((dbase + std_inx)->line, "r")))
            {
               fprintf (stderr, "%s:  error opening %s\n",
                        progname, (dbase + std_inx)->line);
               exit (1);
            }

            dflag = fscanf (finput, "%d %d %d %d %d %f %*f %*f %e",
                            &yr, &mh, &dy, &hr, &mn, &ss, &edensity);
         }
         else
            break;
      }

      if (mh == 1)
         sprintf (TimeString, "%4d-Jan-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  2)
         sprintf (TimeString, "%4d-Feb-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  3)
         sprintf (TimeString, "%4d-Mar-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  4)
         sprintf (TimeString, "%4d-Apr-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  5)
         sprintf (TimeString, "%4d-May-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  6)
         sprintf (TimeString, "%4d-Jun-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  7)
         sprintf (TimeString, "%4d-Jul-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  8)
         sprintf (TimeString, "%4d-Aug-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  9)
         sprintf (TimeString, "%4d-Sep-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh == 10)
         sprintf (TimeString, "%4d-Oct-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh == 11)
         sprintf (TimeString, "%4d-Nov-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh == 12)
         sprintf (TimeString, "%4d-Dec-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);

      if (parsetime (TimeString, &year, &month, &day, &doy, &hour, &min, &sec))
      {
         fprintf (stderr, "%s: error parsing %s\n", progname, TimeString);
         exit (1);
      }

      OldTime = CurrentTime;
      CurrentTime = GetTime (year, doy, hour, min, (float) sec);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) edensity;
            else
               ypts [index] = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if (st->YAxis == FpOverFc)
         {
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) edensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == SaturnLocalTime)
         {
            target = Saturn;

            OldLocalTime = LocalTime;

            GeographicCoords (CurrentTime, target, &dummy1,
                              &dummy2, &dummy3,
                              &dummy4, &LocalTime, &dummy5);

            if (LocalTime < 12.0)
               LocalTime += 24.0;

            if ((fabs (LocalTime - OldLocalTime) > 12.0) && (index > 0))
            {
               pgline_ (&index, xpts, ypts);
               index = 0;
            }

            xpts [index] = (float) LocalTime;

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

/*
   fclose (finput);
*/

   return (no_data);
}

int ProcessDataProxy_Points (st, dbase, n_files)
struct time_struc *st;
struct file_structure *dbase;
int n_files;
{
   int nsymbol = 850;
   int year, month, doy, day, hour, min;
   int past, yr, mh, dy, hr, mn;
   int no_data = True, dflag, std_inx, cl;
   int target;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   double LocalTime;
   double dummy1;
   double dummy2;
   double dummy3;
   double dummy4;
   double dummy5;
   char TimeString [MAXCHARS];
   FILE *finput;

   std_inx = 0;

   if (!(finput = fopen ((dbase + std_inx)->line, "r")))
   {
      printf ("%s:  error opening %s\n",
               progname, (dbase + std_inx)->line);
      exit (1);
   }

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*f %*f %e",
                      &yr, &mh, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
      {
         fclose (finput);

         std_inx++;

         if (std_inx < n_files)
         {
            if (!(finput = fopen ((dbase + std_inx)->line, "r")))
            {
               fprintf (stderr, "%s:  error opening %s\n",
                        progname, (dbase + std_inx)->line);
               exit (1);
            }

            dflag = fscanf (finput, "%d %d %d %d %d %f %*f %*f %e",
                            &yr, &mh, &dy, &hr, &mn, &ss, &edensity);
         }
         else
            break;
      }

      if (mh == 1)
         sprintf (TimeString, "%4d-Jan-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  2)
         sprintf (TimeString, "%4d-Feb-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  3)
         sprintf (TimeString, "%4d-Mar-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  4)
         sprintf (TimeString, "%4d-Apr-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  5)
         sprintf (TimeString, "%4d-May-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  6)
         sprintf (TimeString, "%4d-Jun-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  7)
         sprintf (TimeString, "%4d-Jul-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  8)
         sprintf (TimeString, "%4d-Aug-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh ==  9)
         sprintf (TimeString, "%4d-Sep-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh == 10)
         sprintf (TimeString, "%4d-Oct-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh == 11)
         sprintf (TimeString, "%4d-Nov-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);
      else if (mh == 12)
         sprintf (TimeString, "%4d-Dec-%02dT%02d:%02d:%06.3f", yr, dy, hr, mn, ss);

      if (parsetime (TimeString, &year, &month, &day, &doy, &hour, &min, &sec))
      {
         fprintf (stderr, "%s: error parsing %s\n", progname, TimeString);
         exit (1);
      }

      CurrentTime = GetTime (year, doy, hour, min, (float) sec);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if (st->YAxis == FpOverFc)
         {
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == SaturnLocalTime)
         {
            target = Saturn;

            GeographicCoords (CurrentTime, target, &dummy1,
                              &dummy2, &dummy3,
                              &dummy4, &LocalTime, &dummy5);

            if (LocalTime < 12.0)
               LocalTime += 24.0;

            xval = (float) LocalTime;
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

/*
   fclose (finput);
*/

   return (no_data);
}

int PlotLPData (st)
struct time_struc *st;
{
   char freq_ch;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag, index;
   float ss, edensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double NewQuality;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/nov2017data/LP_20Hz_data_total_without_NaN.txt";
/*
   char *filename = "/home/jbg/cassini/lprobe/newdata/LP_proxy_all.input";
*/
   FILE *finput;

   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*f %e",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      if (edensity < 0.0)
         continue;

      sprintf (name, "%4d-%02d-%02dT%02d-%02d-%06.3f",
               yr, mon, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      OldTime = CurrentTime;
      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) edensity;
            else
               ypts [index] = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) edensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

   fclose (finput);
   return (no_data);
}

int PlotLPData_Points (st)
struct time_struc *st;
{
   char freq_ch;
   int nsymbol = 850;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double NewQuality;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/nov2017data/LP_20Hz_data_total_without_NaN.txt";
/*
   char *filename = "/home/jbg/cassini/lprobe/newdata/LP_proxy_all.input";
*/
   FILE *finput;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*f %e",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      if (edensity < 0.0)
         continue;

      sprintf (name, "%4d-%02d-%02dT%02d-%02d-%06.3f",
               yr, mon, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

   fclose (finput);
   return (no_data);
}

int PlotLPDataNew (st)
struct time_struc *st;
{
   char freq_ch;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag, index;
   float ss, edensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double NewQuality;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/feb2017data/LP_sweep_data_total_without_NaN_Feb_2017.txt";
/*
   char *filename = "/home/jbg/cassini/lprobe/newdata/Ne_proxy_all_new.input";
*/
   FILE *finput;

   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*e %*f %e %*e %*f %*f",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      sprintf (name, "%4d-%02d-%02dT%02d-%02d-%06.3f",
               yr, mon, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      OldTime = CurrentTime;
      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) edensity;
            else
               ypts [index] = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) edensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

   fclose (finput);
   return (no_data);
}

int PlotLPDataNew_Points (st)
struct time_struc *st;
{
   char freq_ch;
   int nsymbol = 850;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double NewQuality;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/feb2017data/LP_sweep_data_total_without_NaN_Feb_2017.txt";
/*
   char *filename = "/home/jbg/cassini/lprobe/newdata/Ne_proxy_all_new.input";
*/
   FILE *finput;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*e %*f %e %*e %*f %*f",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      sprintf (name, "%4d-%02d-%02dT%02d-%02d-%06.3f",
               yr, mon, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

   fclose (finput);
   return (no_data);
}

int PlotLPDataNewest (st)
struct time_struc *st;
{
   char freq_ch;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag, index;
   float ss, edensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double NewQuality;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/newestdata/Ne_proxy_all_newest.input";
   FILE *finput;

   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*f %*f %e",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      sprintf (name, "%4d-%02d-%02dT%02d-%02d-%06.3f",
               yr, mon, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      OldTime = CurrentTime;
      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) edensity;
            else
               ypts [index] = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) edensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

   fclose (finput);
   return (no_data);
}

int PlotLPDataNewest_Points (st)
struct time_struc *st;
{
   char freq_ch;
   int nsymbol = 850;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double NewQuality;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/newestdata/Ne_proxy_all_newest.input";
   FILE *finput;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*f %*f %e",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      sprintf (name, "%4d-%02d-%02dT%02d-%02d-%06.3f",
               yr, mon, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

   fclose (finput);
   return (no_data);
}

int PlotLPDataNov2010 (st)
struct time_struc *st;
{
   char freq_ch;
   int red = 2, black = 1;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag, index;
   float ss, edensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double NewQuality;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/nov2010data/Ne_proxy_all_nov2010data.input";
   FILE *finput;

   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   pgsci_ (&red);

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*f %*f %e",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      sprintf (name, "%4d-%02d-%02dT%02d-%02d-%06.3f",
               yr, mon, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      OldTime = CurrentTime;
      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) edensity;
            else
               ypts [index] = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) edensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotLPDataNov2010_Points (st)
struct time_struc *st;
{
   char freq_ch;
   int red = 2, black = 1;
   int nsymbol = 850;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double NewQuality;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/nov2010data/Ne_proxy_all_nov2010data.input";
   FILE *finput;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

/*
   pgsci_ (&red);
*/

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*f %*f %e",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      sprintf (name, "%4d-%02d-%02dT%02d-%02d-%06.3f",
               yr, mon, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotLPDataMar2013 (st)
struct time_struc *st;
{
   char freq_ch;
   int red = 2, black = 1;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag, index;
   float ss, edensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double NewQuality;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/mar2013data/LP_Ne_sweep_all.dat";
   FILE *finput;

   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   pgsci_ (&red);

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %e",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      if (mon == 1)
         sprintf (name, "%4d-January-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 2)
         sprintf (name, "%4d-February-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 3)
         sprintf (name, "%4d-March-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 4)
         sprintf (name, "%4d-April-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 5)
         sprintf (name, "%4d-May-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 6)
         sprintf (name, "%4d-June-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 7)
         sprintf (name, "%4d-July-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 8)
         sprintf (name, "%4d-August-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 9)
         sprintf (name, "%4d-September-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 10)
         sprintf (name, "%4d-October-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 11)
         sprintf (name, "%4d-November-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 12)
         sprintf (name, "%4d-December-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      OldTime = CurrentTime;
      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) edensity;
            else
               ypts [index] = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) edensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotLPDataMar2013_Points (st)
struct time_struc *st;
{
   char freq_ch;
   int red = 2, black = 1;
   int nsymbol = 850;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double NewQuality;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/mar2013data/LP_Ne_sweep_all.dat";
   FILE *finput;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

/*
   pgsci_ (&red);
*/

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %e",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      if (mon == 1)
         sprintf (name, "%4d-January-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 2)
         sprintf (name, "%4d-February-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 3)
         sprintf (name, "%4d-March-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 4)
         sprintf (name, "%4d-April-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 5)
         sprintf (name, "%4d-May-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 6)
         sprintf (name, "%4d-June-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 7)
         sprintf (name, "%4d-July-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 8)
         sprintf (name, "%4d-August-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 9)
         sprintf (name, "%4d-September-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 10)
         sprintf (name, "%4d-October-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 11)
         sprintf (name, "%4d-November-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 12)
         sprintf (name, "%4d-December-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotLPDataMar2013_Ne20_Points (st)
struct time_struc *st;
{
   char freq_ch;
   int red = 2, black = 1;
   int nsymbol = 850;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double NewQuality;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/mar2013data/LP_Ne20_all.dat";
   FILE *finput;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

/*
   pgsci_ (&red);
*/

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %e",
                      &yr, &mon, &dy, &hr, &mn, &ss, &edensity);

      if (dflag < 0)
         break;

      if (mon == 1)
         sprintf (name, "%4d-January-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 2)
         sprintf (name, "%4d-February-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 3)
         sprintf (name, "%4d-March-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 4)
         sprintf (name, "%4d-April-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 5)
         sprintf (name, "%4d-May-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 6)
         sprintf (name, "%4d-June-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 7)
         sprintf (name, "%4d-July-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 8)
         sprintf (name, "%4d-August-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 9)
         sprintf (name, "%4d-September-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 10)
         sprintf (name, "%4d-October-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 11)
         sprintf (name, "%4d-November-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);
      else if (mon == 12)
         sprintf (name, "%4d-December-%02dT%02d-%02d-%06.3f",
                  yr, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotExportfUHData (st)
struct time_struc *st;
{
   char freq_ch;
   int red = 2, black = 1;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag, index;
   float NewQuality;
   float ss, edensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/data1/cassini/export/MasterList/MasterList_fuh_export.data";
   FILE *finput;

   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   pgsci_ (&red);

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %f %*f %e %*f %*f %*f %f",
                      &yr, &dy, &hr, &mn, &ss, &edensity, &NewQuality);

      if (dflag < 0)
         break;

      if ((double) NewQuality >= st->NewQuality)
         continue;

      OldTime = CurrentTime;
      CurrentTime = GetTime (year, dy, hr, mn, ss);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) edensity;
            else
               ypts [index] = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) edensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotExportfUHData_Points (st)
struct time_struc *st;
{
   char freq_ch;
   int red = 2, black = 1;
   int nsymbol = 850;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag;
   float NewQuality;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/data1/cassini/export/MasterList/MasterList_fuh_export.data";
   FILE *finput;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   pgsci_ (&red);

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %f %*f %e %*f %*f %*f %f",
                      &yr, &dy, &hr, &mn, &ss, &edensity, &NewQuality);

      if (dflag < 0)
         break;

      if ((double) NewQuality >= st->NewQuality)
         continue;

      CurrentTime = GetTime (yr, dy, hr, mn, ss);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotFUHLowRateAlternative (st)
struct time_struc *st;
{
   char freq_ch;
   int green = 3, black = 1;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag, index;
   int cl;
   int OldQuality;
   float NewQuality;
   float ss, edensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char *filename = "/opt/project/cassini/data/density/fuh/eew2017226_vertical.data";
   FILE *finput;

   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   pgsci_ (&green);

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %f %*f %*f %e %*f %*f %*f %d %c",
                      &yr, &dy, &hr, &mn, &ss, &edensity, &cl, &freq_ch);

      if (dflag < 0)
         break;

      if (cl < 0)
      {
         NewQuality = (double) cl;
         NewQuality /= -100.0;

         if (NewQuality >= st->NewQuality)
            continue;
      }
      else
      {
         OldQuality = cl;

         if (OldQuality > st->cl)
            continue;
      }

      OldTime = CurrentTime;
      CurrentTime = GetTime (yr, dy, hr, mn, ss);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) edensity;
            else
               ypts [index] = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) edensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotFUHLowRateAlternative_Points (st)
struct time_struc *st;
{
   char freq_ch;
   int green = 3, black = 1;
   int nsymbol = 850;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag;
   int cl;
   int OldQuality;
   float NewQuality;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char *filename = "/opt/project/cassini/data/density/fuh/eew2017226_vertical.data";
   FILE *finput;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   pgsci_ (&green);

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %f %*f %*f %e %*f %*f %*f %d %c",
                      &yr, &dy, &hr, &mn, &ss, &edensity, &cl, &freq_ch);

      if (dflag < 0)
         break;

      if (cl < 0)
      {
         NewQuality = (double) cl;
         NewQuality /= -100.0;

         if (NewQuality >= st->NewQuality)
            continue;
      }
      else
      {
         OldQuality = cl;

         if (OldQuality > st->cl)
            continue;
      }

      CurrentTime = GetTime (yr, dy, hr, mn, ss);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotExportfUHwbdData (st)
struct time_struc *st;
{
   char freq_ch;
   int red = 2, black = 1;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag, index;
   int OldQuality;
   float ss, edensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/data1/cassini/export/MasterList/MasterList_fuh_wideband_80kHz_export.data";
   FILE *finput;

   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   pgsci_ (&red);

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %f %*f %e %*f %*f %*f %d",
                      &yr, &dy, &hr, &mn, &ss, &edensity, &OldQuality);

      if (dflag < 0)
         break;

      if (OldQuality >= st->cl)
         continue;

      OldTime = CurrentTime;
      CurrentTime = GetTime (year, dy, hr, mn, ss);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) edensity;
            else
               ypts [index] = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) edensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotExportfUHwbdData_Points (st)
struct time_struc *st;
{
   char freq_ch;
   int red = 2, black = 1;
   int nsymbol = 850;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag;
   int OldQuality;
   float ss, edensity;
   float xpt;
   float xval, yval;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/data1/cassini/export/MasterList/MasterList_fuh_wideband_80kHz_export.data";
   FILE *finput;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   pgsci_ (&red);

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %f %*f %e %*f %*f %*f %d",
                      &yr, &dy, &hr, &mn, &ss, &edensity, &OldQuality);

      if (dflag < 0)
         break;

      if (OldQuality >= st->cl)
         continue;

      CurrentTime = GetTime (yr, dy, hr, mn, ss);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) edensity;
            else
               yval = (float) log10 ((double) edensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) edensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) edensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

   pgsci_ (&black);

   fclose (finput);
   return (no_data);
}

int PlotLPDataSweepfitNineIon (st)
struct time_struc *st;
{
   char freq_ch;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag, index;
   float ss, idensity;
   float xpts [MaxPoints], ypts [MaxPoints], xpt;
   double NewQuality;
   double DeltaTime, OldTime, CurrentTime, Start, Stop, second, DataGap;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/nov2017data/LP_sweepfit_nine_total_without_NaN.txt";
   FILE *finput;

   index = 0;
   OldTime = 0;
   DataGap = st->DataGapTime;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*e %*e %*f %e %*f",
                      &yr, &mon, &dy, &hr, &mn, &ss, &idensity);

      if (dflag < 0)
         break;

      if (idensity < 0.0)
         continue;

      sprintf (name, "%4d-%02d-%02dT%02d-%02d-%06.3f",
               yr, mon, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      OldTime = CurrentTime;
      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if (((CurrentTime - OldTime) > DataGap) && (index > 0))
      {
         pgline_ (&index, xpts, ypts);
         index = 0;
      }

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               ypts [index] = (float) idensity;
            else
               ypts [index] = (float) log10 ((double) idensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) idensity;
            temp1 /= Bmag;

            ypts [index] = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            ypts [index] = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
         {
            xpts [index] = GetTimePosition (st, CurrentTime);

            index++;

            if (index > MaxPoints)
            {
               fprintf (stderr, "We are going to need a bigger boat!\n");
               return (1);
            }
         }
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
         else if (st->XAxis == Ne)
         {
            xpts [index] = (float) log10 ((double) idensity);

            index++;
         }
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) idensity;
            temp1 /= Bmag;

            xpts [index] = (float) log10 (temp1);

            index++;
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xpts [index] = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xpts [index] = (float) log10 (TempLog);
            }

            index++;
         }
      }
   }

   if (index > 0)
      pgline_ (&index, xpts, ypts);

   fclose (finput);
   return (no_data);
}

int PlotLPDataSweepfitNineIon_Points (st)
struct time_struc *st;
{
   char freq_ch;
   int nsymbol = 850;
   int year, month, day, doy, hour, minute;
   int past, yr, mon, dy, hr, mn;
   int no_data = True, dflag;
   float ss, idensity;
   float xpt;
   float xval, yval;
   double NewQuality;
   double DeltaTime, CurrentTime, Start, Stop, second;
   double temp1, temp2, temp3, TempLog, Bmag;
   double sec;
   char name [80];
   char *filename = "/home/jbg/cassini/lprobe/nov2017data/LP_sweepfit_nine_total_without_NaN.txt";
   FILE *finput;

   if (!(finput = fopen (filename, "r")))
   {
      fprintf (stderr, "%s:  error opening %s\n", progname, filename);
      exit (1);
   }

   while (!feof (finput))
   {
      dflag = fscanf (finput, "%d %d %d %d %d %f %*e %*e %*f %e %*f",
                      &yr, &mon, &dy, &hr, &mn, &ss, &idensity);

      if (dflag < 0)
         break;

      if (idensity < 0.0)
         continue;

      sprintf (name, "%4d-%02d-%02dT%02d-%02d-%06.3f",
               yr, mon, dy, hr, mn, ss);

      if (parsetime (name, &year, &month, &day, &doy, &hour, &minute, &sec))
      {
         printf ("%s: error parsing %s\n", progname, name);
         exit (1);
      }

      CurrentTime = GetTime (year, doy, hour, minute, (float) sec);

      if ((CurrentTime >= st->st_total_ms) &&
          (CurrentTime <= st->sp_total_ms))
      {
         no_data = False;

         if (st->YAxis == Ne)
         {
            if (st->LinOrLogYAxis)
               yval = (float) idensity;
            else
               yval = (float) log10 ((double) idensity);
         }
         else if (st->YAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) idensity;
            temp1 /= Bmag;

            yval = (float) log10 (temp1);
         }
         else if ((st->YAxis == SaturnRad) || (st->YAxis == SaturnAlt) ||
                  (st->YAxis == EnceladusRad)  || (st->YAxis == EnceladusAlt))
            yval = (float) GetRadValue (st, CurrentTime);

         if (st->XAxis == Time)
            xval = GetTimePosition (st, CurrentTime);
         else if ((st->XAxis == SaturnRad) || (st->XAxis == SaturnAlt) ||
                  (st->XAxis == EnceladusRad)  || (st->XAxis == EnceladusAlt))
         {
            if (st->LinOrLog)
               xval = (float) GetRadValue (st, CurrentTime);
            else
            {
               TempLog = GetRadValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }
         else if (st->XAxis == Ne)
            xval = (float) log10 ((double) idensity);
         else if (st->XAxis == NeOverB)
         {
            Bmag = Z3Model (CurrentTime);
            temp1 = (double) idensity;
            temp1 /= Bmag;

            xval = (float) log10 (temp1);
         }
         else if ((st->XAxis == TheLValue)       ||
                  (st->XAxis == SaturnMLatitude) ||
                  (st->XAxis == SaturnKLatitude))
         {
            if (st->LinOrLog)
               xval = (float) GetLValue (st, CurrentTime);
            else
            {
               TempLog = GetLValue (st, CurrentTime);
               xval = (float) log10 (TempLog);
            }
         }

         pgpt1_ (&xval, &yval, &nsymbol);
      }
   }

   fclose (finput);
   return (no_data);
}

int main (int argc, char *argv[])
{
   register int m = 0, x;
   int magenta = 6, red = 2, black = 1;
   int cl, rows, num_files, DataSetIndex, VerifyIndex;
   int index, year, month, day, doy, hr, mn, past;
   int DataSetCode, DataSetTrue, EphemSetCode;
   int MnDyOrYr;
   int DataSetMasks [17] = {     fUHLR_MASK,      fPLR_MASK,
                                fPHR10_MASK,    fPHR80_MASK,
                               fuhHR80_MASK,    fUHLR2_MASK,
                                DMODEL_MASK,    LProbe_MASK,
                               LProbe2_MASK,   LProbe3_MASK,
                               LProbe4_MASK,   exfUHLR_MASK,
                             exfuhHR80_MASK,  LP_Proxy_MASK,
                               fUHLR_t_MASK, fuhHR80_t_MASK, fPHR80_dust_MASK  };
   float PointSize;
   double st_ms, sp_ms, sec;
   double xmin, xmax, AmpMin, AmpMax, OtherMin, OtherMax;
   char *dummy;
   char st_string [80], sp_string [80], temp [80];
   char qbuf [300];
   char DatabaseNames [34][200] = { SELECT_QUERY1, SELECT_QUERY2,
                                    SELECT_QUERY3, SELECT_QUERY4,
                                    SELECT_QUERY5, SELECT_QUERY6,
                                    SELECT_QUERY7, SELECT_QUERY8,
                                    SELECT_QUERY9, SELECT_QUERY0,
                                    SELECT_QUERYA, SELECT_QUERYB,
                                               "",            "",
                                               "",            "",
                                               "",            "",
                                               "",            "",
                                               "",            "",
                                               "",            "",
                                               "",            "",
                                    SELECT_QUERYC, SELECT_QUERYD,
                                    SELECT_QUERYE, SELECT_QUERYF,
                                    SELECT_QUERYG, SELECT_QUERYH,
                                    SELECT_QUERYI, SELECT_QUERYJ };
   char *name = "/home/jbg/data1/spice/cassini/ephemeris/CassiniKernels.list";
   struct time_struc st_time;
   struct file_structure files [MAXFILES];
   entry entries [MAX_ENTRIES];
   PGconn *pConn;
   PGresult *pRes1, *pRes2;

   if (strcmp (getenv ("REQUEST_METHOD"), "POST"))
   {
      printf ("Content-type: text/html%c%c", 10, 10);
      printf ("This script should be referenced with a METHOD of POST.\n");
      printf ("If you don't understand this, see this ");
      printf ("<A HREF=\"http://www.ncsa.uiuc.edu/SDG/Software/Mosaic/Docs/fill-out-forms/overview.html\">forms overview</A>.%c",10);
      exit (1);
   }

   if (strcmp (getenv ("CONTENT_TYPE"), "application/x-www-form-urlencoded"))
   {
      printf ("Content-type: text/html%c%c", 10, 10);
      printf ("This script can only be used to decode form results. \n");
      exit (1);
   }

   cl = atoi (getenv ("CONTENT_LENGTH"));

   for (x = 0; cl && (!feof (stdin)); x++)
   {
      m = x;
      entries [x].val = fmakeword (stdin, '&', &cl);
      plustospace (entries [x].val);
      unescape_url (entries [x].val);
      entries [x].name = makeword (entries [x].val,'=');
   }

   DataSetCode = 0;
   EphemSetCode = 0;

   for (index = 0; index < x; index++)
   {
      sprintf (temp, "%s", entries [index].val);

      if (!strcmp (entries [index].name, "StTime"))
      {
         if (parsetime (temp, &year, &month, &day, &doy, &hr, &mn, &sec))
         {
            printf ("Content-type: text/html%c%c", 10, 10);
            printf ("%s: error parsing %s\n", argv [0], temp);
            exit (1);
         }

         sprintf (st_string, "%04d-%02d-%02dT%02d:%02d:%f",
                  year, month, day, hr, mn, (float) sec);

         past = past_1958 (year, doy);

         st_ms = (double) past*86400000.0 +
                 (double) hr*3600000.0 +
                 (double) mn*60000.0 +
                 (double) sec*1000.0;

         st_time.st_past = past;
         st_time.st_year = year;
         st_time.st_day = doy;
         st_time.st_hr = hr;
         st_time.st_mn = mn;
         st_time.st_sec = (float) sec;
         st_time.st_total_ms = st_ms;
      }
      else if (!strcmp (entries [index].name, "SpTime"))
      {
         if (parsetime (temp, &year, &month, &day, &doy, &hr, &mn, &sec))
         {
            printf ("Content-type: text/html%c%c", 10, 10);
            printf ("%s: error parsing %s\n", argv [0], temp);
            exit (1);
         }

         sprintf (sp_string, "%04d-%02d-%02dT%02d:%02d:%f",
                  year, month, day, hr, mn, (float) sec);

         past = past_1958 (year, doy);

         sp_ms = (double) past*86400000.0 +
                 (double) hr*3600000.0 +
                 (double) mn*60000.0 +
                 (double) sec*1000.0;

         st_time.sp_past = past;
         st_time.sp_year = year;
         st_time.sp_day = doy;
         st_time.sp_hr = hr;
         st_time.sp_mn = mn;
         st_time.sp_sec = (float) sec;
         st_time.sp_total_ms = sp_ms;
      }
      else if (!strcmp (entries [index].name, "psorgif"))
      {
         if      (!strcmp (temp, "gif"))
            st_time.psorgif = False;
         else if (!strcmp (temp, "pps"))
            st_time.psorgif = True;
         else
         {
            printf ("Content-type: text/html%c%c", 10, 10);
            printf ("%s: unknown output type %s\n", argv [0], temp);
            exit (1);
         }
      }
      else if (!strcmp (entries [index].name, "minamp"))
      {
         for (VerifyIndex = 0; VerifyIndex < (int) strlen (temp); VerifyIndex++)
            if ((!(isdigit (temp [VerifyIndex]))) &&
                           (temp [VerifyIndex] != ' ') &&
                           (temp [VerifyIndex] != '+') &&
                           (temp [VerifyIndex] != '-') &&
                           (temp [VerifyIndex] != 'd') &&
                           (temp [VerifyIndex] != 'D') &&
                           (temp [VerifyIndex] != 'e') &&
                           (temp [VerifyIndex] != 'E') &&
                           (temp [VerifyIndex] != '.'))
            {
               printf ("Content-type: text/html%c%c", 10, 10);
               printf ("Invalid entry  %s\n", temp);
               return (0);
            }

         AmpMin = (double) strtod (temp, &dummy);
      }
      else if (!strcmp (entries [index].name, "maxamp"))
      {
         for (VerifyIndex = 0; VerifyIndex < (int) strlen (temp); VerifyIndex++)
            if ((!(isdigit (temp [VerifyIndex]))) &&
                           (temp [VerifyIndex] != ' ') &&
                           (temp [VerifyIndex] != '+') &&
                           (temp [VerifyIndex] != '-') &&
                           (temp [VerifyIndex] != 'd') &&
                           (temp [VerifyIndex] != 'D') &&
                           (temp [VerifyIndex] != 'e') &&
                           (temp [VerifyIndex] != 'E') &&
                           (temp [VerifyIndex] != '.'))
            {
               printf ("Content-type: text/html%c%c", 10, 10);
               printf ("Invalid entry  %s\n", temp);
               return (0);
            }

         AmpMax = (double) strtod (temp, &dummy);
      }
      else if (!strcmp (entries [index].name, "minnot"))
      {
         for (VerifyIndex = 0; VerifyIndex < (int) strlen (temp); VerifyIndex++)
            if ((!(isdigit (temp [VerifyIndex]))) &&
                           (temp [VerifyIndex] != ' ') &&
                           (temp [VerifyIndex] != '+') &&
                           (temp [VerifyIndex] != '-') &&
                           (temp [VerifyIndex] != 'd') &&
                           (temp [VerifyIndex] != 'D') &&
                           (temp [VerifyIndex] != 'e') &&
                           (temp [VerifyIndex] != 'E') &&
                           (temp [VerifyIndex] != '.'))
            {
               printf ("Content-type: text/html%c%c", 10, 10);
               printf ("Invalid entry  %s\n", temp);
               return (0);
            }

         OtherMin = (double) strtod (temp, &dummy);
      }
      else if (!strcmp (entries [index].name, "maxnot"))
      {
         for (VerifyIndex = 0; VerifyIndex < (int) strlen (temp); VerifyIndex++)
            if ((!(isdigit (temp [VerifyIndex]))) &&
                           (temp [VerifyIndex] != ' ') &&
                           (temp [VerifyIndex] != '+') &&
                           (temp [VerifyIndex] != '-') &&
                           (temp [VerifyIndex] != 'd') &&
                           (temp [VerifyIndex] != 'D') &&
                           (temp [VerifyIndex] != 'e') &&
                           (temp [VerifyIndex] != 'E') &&
                           (temp [VerifyIndex] != '.'))
            {
               printf ("Content-type: text/html%c%c", 10, 10);
               printf ("Invalid entry  %s\n", temp);
               return (0);
            }

         OtherMax = (double) strtod (temp, &dummy);
      }
      else if (!strcmp (entries [index].name, "quality1"))
      {
         if (!strcmp (temp, "qual00"))
            st_time.cl = 0;
         else if (!strcmp (temp, "qual01"))
            st_time.cl = 1;
         else if (!strcmp (temp, "qual02"))
            st_time.cl = 2;
         else if (!strcmp (temp, "qual03"))
            st_time.cl = 3;
         else
         {
            printf ("Content-type: text/html%c%c", 10, 10);
            printf ("%s: unknown quality level %s\n", argv [0], temp);
            exit (1);
         }
      }
      else if (!strcmp (entries [index].name, "quality2"))
      {
         for (VerifyIndex = 0; VerifyIndex < (int) strlen (temp); VerifyIndex++)
            if ((!(isdigit (temp [VerifyIndex]))) &&
                           (temp [VerifyIndex] != ' ') &&
                           (temp [VerifyIndex] != '+') &&
                           (temp [VerifyIndex] != '-') &&
                           (temp [VerifyIndex] != 'd') &&
                           (temp [VerifyIndex] != 'D') &&
                           (temp [VerifyIndex] != 'e') &&
                           (temp [VerifyIndex] != 'E') &&
                           (temp [VerifyIndex] != '.'))
            {
               printf ("Content-type: text/html%c%c", 10, 10);
               printf ("Invalid entry  %s\n", temp);
               return (0);
            }

         st_time.NewQuality = (double) strtod (temp, &dummy);
      }
      else if (!strcmp (entries [index].name, "gaptime"))
      {
         for (VerifyIndex = 0; VerifyIndex < (int) strlen (temp); VerifyIndex++)
            if ((!(isdigit (temp [VerifyIndex]))) &&
                           (temp [VerifyIndex] != ' ') &&
                           (temp [VerifyIndex] != '+') &&
                           (temp [VerifyIndex] != '-') &&
                           (temp [VerifyIndex] != 'd') &&
                           (temp [VerifyIndex] != 'D') &&
                           (temp [VerifyIndex] != 'e') &&
                           (temp [VerifyIndex] != 'E') &&
                           (temp [VerifyIndex] != '.'))
            {
               printf ("Content-type: text/html%c%c", 10, 10);
               printf ("Invalid entry  %s\n", temp);
               return (0);
            }

         st_time.DataGapTime = (double) strtod (temp, &dummy);
         st_time.DataGapTime *= 1000.0;
      }
      else if (!strcmp (entries [index].name, "dataset"))
      {
         if (!strcmp (temp, "fulow"))
            DataSetCode |= fUHLR_MASK;
         else if (!strcmp (temp, "fplow"))
            DataSetCode |= fPLR_MASK;
         else if (!strcmp (temp, "fphi10"))
            DataSetCode |= fPHR10_MASK;
         else if (!strcmp (temp, "fphi80"))
            DataSetCode |= fPHR80_MASK;
         else if (!strcmp (temp, "fuhi80"))
            DataSetCode |= fuhHR80_MASK;
         else if (!strcmp (temp, "fulow2"))
            DataSetCode |= fUHLR2_MASK;
         else if (!strcmp (temp, "dmodel"))
            DataSetCode |= DMODEL_MASK;
         else if (!strcmp (temp, "lprobe"))
            DataSetCode |= LProbe_MASK;
         else if (!strcmp (temp, "lprob2"))
            DataSetCode |= LProbe2_MASK;
         else if (!strcmp (temp, "lprob3"))
            DataSetCode |= LProbe3_MASK;
         else if (!strcmp (temp, "no2010"))
            DataSetCode |= LProbe4_MASK;
         else if (!strcmp (temp, "expfuh"))
            DataSetCode |= exfUHLR_MASK;
         else if (!strcmp (temp, "exwfuh"))
            DataSetCode |= exfuhHR80_MASK;
         else if (!strcmp (temp, "lproxy"))
            DataSetCode |= LP_Proxy_MASK;
         else if (!strcmp (temp, "fulowt"))
            DataSetCode |= fUHLR_t_MASK;
         else if (!strcmp (temp, "fuw80t"))
            DataSetCode |= fuhHR80_t_MASK;
         else if (!strcmp (temp, "fphi80d"))
            DataSetCode |= fPHR80_dust_MASK;
      }
      else if (!strcmp (entries [index].name, "ephem"))
      {
         if (!strcmp (temp, "srad"))
            EphemSetCode |= SaturnR_MASK;
         else if (!strcmp (temp, "slat"))
            EphemSetCode |= SaturnLat_MASK;
         else if (!strcmp (temp, "mlat"))
            EphemSetCode |= SaturnMLat_MASK;
         else if (!strcmp (temp, "slon"))
            EphemSetCode |= SaturnLon_MASK;
         else if (!strcmp (temp, "sloc"))
            EphemSetCode |= SaturnLT_MASK;
         else if (!strcmp (temp, "trad"))
            EphemSetCode |= TitanR_MASK;
         else if (!strcmp (temp, "tlat"))
            EphemSetCode |= TitanLat_MASK;
         else if (!strcmp (temp, "tlon"))
            EphemSetCode |= TitanLon_MASK;
         else if (!strcmp (temp, "tloc"))
            EphemSetCode |= TitanLT_MASK;
         else if (!strcmp (temp, "erad"))
            EphemSetCode |= EnceladusR_MASK;
         else if (!strcmp (temp, "elat"))
            EphemSetCode |= EnceladusLat_MASK;
         else if (!strcmp (temp, "elon"))
            EphemSetCode |= EnceladusLon_MASK;
         else if (!strcmp (temp, "eloc"))
            EphemSetCode |= EnceladusLT_MASK;
         else if (!strcmp (temp, "lval"))
            EphemSetCode |= SaturnLValue_MASK;
      }
      else if (!strcmp (entries [index].name, "xaxis"))
      {
         if (!strcmp (temp, "time"))
         {
            st_time.XAxis = Time;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "satr"))
         {
            st_time.XAxis = SaturnRad;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "srlo"))
         {
            st_time.XAxis = SaturnRad;
            st_time.LinOrLog = False;
         }
         else if (!strcmp (temp, "encr"))
         {
            st_time.XAxis = EnceladusRad;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "enlo"))
         {
            st_time.XAxis = EnceladusRad;
            st_time.LinOrLog = False;
         }
         else if (!strcmp (temp, "sata"))
         {
            st_time.XAxis = SaturnAlt;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "enca"))
         {
            st_time.XAxis = EnceladusAlt;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "elec"))
         {
            st_time.XAxis = Ne;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "eovb"))
         {
            st_time.XAxis = NeOverB;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "lval"))
         {
            st_time.XAxis = TheLValue;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "mlat"))
         {
            st_time.XAxis = SaturnMLatitude;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "slat"))
         {
            st_time.XAxis = SaturnKLatitude;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "sloc"))
         {
            st_time.XAxis = SaturnLocalTime;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "titr"))
         {
            st_time.XAxis = TitanRad;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "tnlo"))
         {
            st_time.XAxis = TitanRad;
            st_time.LinOrLog = False;
         }
         else if (!strcmp (temp, "tlat"))
         {
            st_time.XAxis = TitanLatitude;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "tloc"))
         {
            st_time.XAxis = TitanLocalTime;
            st_time.LinOrLog = True;
         }
         else if (!strcmp (temp, "tita"))
         {
            st_time.XAxis = TitanAlt;
            st_time.LinOrLog = True;
         }
      }
      else if (!strcmp (entries [index].name, "yaxis"))
      {
         st_time.LinOrLogYAxis = False;

         if (!strcmp (temp, "sata"))
            st_time.YAxis = SaturnAlt;
         else if (!strcmp (temp, "tita"))
            st_time.YAxis = EnceladusAlt;
         else if (!strcmp (temp, "elec"))
            st_time.YAxis = Ne;
         else if (!strcmp (temp, "elel"))
         {
            st_time.YAxis = Ne;
            st_time.LinOrLogYAxis = True;
         }
         else if (!strcmp (temp, "eovb"))
            st_time.YAxis = NeOverB;
         else if (!strcmp (temp, "bmag"))
            st_time.YAxis = BMag;
         else if (!strcmp (temp, "fpfc"))
            st_time.YAxis = FpOverFc;
         else if (!strcmp (temp, "fpcl"))
         {
            st_time.YAxis = FpOverFc;
            st_time.LinOrLogYAxis = True;
         }
      }
      else if (!strcmp (entries [index].name, "title"))
      {
         sprintf (st_time.title, "%s", temp);
      }
      else if (!strcmp (entries [index].name, "ptorline"))
      {
         if      (!strcmp (temp, "line"))
            st_time.PointsOrLines = False;
         else if (!strcmp (temp, "pnts"))
            st_time.PointsOrLines = True;
         else
         {
            printf ("Content-type: text/html%c%c", 10, 10);
            printf ("%s: unknown data presentation type %s\n", argv [0], temp);
            exit (1);
         }
      }
   }

   if (st_time.XAxis == Time)
   {
      GetTimeStats (&st_time, &MnDyOrYr, &xmin, &xmax);

      if (MnDyOrYr == Second)
      {
         if ((xmax - xmin) < 10.0)
         {
            MnDyOrYr = First;
            st_time.MnDyOrYr = MnDyOrYr;
         }
         else
            st_time.MnDyOrYr = MnDyOrYr;
      }
      else
         st_time.MnDyOrYr = MnDyOrYr;

      st_time.xmin = xmin;
      st_time.xmax = xmax;

      if (st_time.LinOrLogYAxis)
      {
         st_time.ymin = AmpMin;
         st_time.ymax = AmpMax;
      }
      else
      {
         st_time.ymin = log10 (AmpMin);
         st_time.ymax = log10 (AmpMax);
      }
   }
   else if ((st_time.XAxis == Ne) || (st_time.XAxis == NeOverB))
   {
      st_time.xmin = log10 (AmpMin);
      st_time.xmax = log10 (AmpMax);

      st_time.ymin = OtherMin;
      st_time.ymax = OtherMax;
   }
   else if ((st_time.XAxis == SaturnLocalTime) || (st_time.XAxis == TitanLocalTime))
   {
      st_time.xmin = 12.0;
      st_time.xmax = 36.0;

      st_time.ymin = log10 (AmpMin);
      st_time.ymax = log10 (AmpMax);
   }
   else
   {
      if (OtherMin == OtherMax)
      {
         printf ("Content-type: text/html%c%c", 10, 10);
         printf ("You must choose different values for horizontal axis mimimum and maximum\n");
         exit (-1);
      }

      if (st_time.LinOrLog)
      {
         st_time.xmin = OtherMin;
         st_time.xmax = OtherMax;
      }
      else
      {
         st_time.xmin = log10 (OtherMin);
         st_time.xmax = log10 (OtherMax);
      }

      st_time.ymin = log10 (AmpMin);
      st_time.ymax = log10 (AmpMax);
   }

   furnsh_ (name, strlen (name));

   st_time.ephemeris = EphemSetCode;

   st_time.SkipDropout = True;

   pConn = PQconnectdb ("host=sql-pw.physics.uiowa.edu dbname=cas_files user=pwreadonly");

   if (PQstatus (pConn) == CONNECTION_BAD)
   {
      printf ("Content-type: text/html%c%c", 10, 10);
      printf ("Couldn't connect to database %s on host %s as user %s\n\n",
              PQdb (pConn), PQhost (pConn), PQuser (pConn));

      perror ("");

      exit (-1);
   }

   if (st_time.XAxis == Time)
      InitPlotTime (&st_time);
   else
      InitPlotOther (&st_time);

   PointSize = 0.35;

   pgsch_ (&PointSize);

   for (DataSetIndex = 0; DataSetIndex < 17; DataSetIndex++)
   {
      DataSetTrue = DataSetCode;

      DataSetTrue &= DataSetMasks [DataSetIndex];
      DataSetTrue = (DataSetTrue >> DataSetIndex);

      if (DataSetTrue)
      {
         if (DataSetIndex == 7)
            pgsci_ (&magenta);
         else
            pgsci_ (&black);

         if (DataSetIndex == 5)
         {
            if (st_time.PointsOrLines)
               PlotFUHLowRateAlternative_Points (&st_time);
            else
               PlotFUHLowRateAlternative (&st_time);
         }
         else if (DataSetIndex == 6)
         {
            if (st_time.PointsOrLines)
               ProcessModel_Points (&st_time);
            else
               ProcessModel (&st_time);
         }
         else if (DataSetIndex == 7)
         {
            if (st_time.PointsOrLines)
               PlotLPData_Points (&st_time);
            else
               PlotLPData (&st_time);
         }
         else if (DataSetIndex == 8)
         {
            if (st_time.PointsOrLines)
               PlotLPDataNew_Points (&st_time);
            else
               PlotLPDataNew (&st_time);
         }
         else if (DataSetIndex == 9)
         {
            if (st_time.PointsOrLines)
               PlotLPDataSweepfitNineIon_Points (&st_time);
            else
               PlotLPDataSweepfitNineIon (&st_time);
         }
         else if (DataSetIndex == 10)
         {
            if (st_time.PointsOrLines)
               PlotLPDataMar2013_Ne20_Points (&st_time);
            else
               PlotLPDataMar2013_Ne20_Points (&st_time);
         }
         else if (DataSetIndex == 11)
         {
            if (st_time.PointsOrLines)
               PlotExportfUHData_Points (&st_time);
            else
               PlotExportfUHData (&st_time);
         }
         else if (DataSetIndex == 12)
         {
            if (st_time.PointsOrLines)
               PlotExportfUHwbdData_Points (&st_time);
            else
               PlotExportfUHwbdData (&st_time);
         }
         else
         {
            sprintf (qbuf, DatabaseNames [(2*DataSetIndex)], st_string, sp_string);

            pRes1 = PQexec (pConn, qbuf);

            if (PQresultStatus(pRes1) != PGRES_TUPLES_OK)
            {
               printf ("Content-type: text/html%c%c", 10, 10);
               printf ("Query failed (%s)\n", PQresultErrorMessage(pRes1));
               printf ("**No data found\n");
               exit (-1);
            }

            rows = PQntuples (pRes1);

            if (rows > 0)
            {
               sprintf (qbuf, DatabaseNames [((2*DataSetIndex) + 1)],
                        st_string, sp_string);

               pRes2 = PQexec (pConn, qbuf);

               if (PQresultStatus(pRes2) != PGRES_TUPLES_OK)
               {
                  printf ("Content-type: text/html%c%c", 10, 10);
                  printf ("Query failed (%s)\n", PQresultErrorMessage(pRes2));
                  printf ("**No data found\n");
                  exit (-1);
               }

               rows = PQntuples (pRes2);

               get_files (files, rows, &num_files, pRes2);

               PQclear (pRes1);
               PQclear (pRes2);
            }
            else
            {
               pgend_ ();
               printf ("Content-type: text/html%c%c", 10, 10);
               printf ("No data coverage of the requested time interval.\n");
               exit (1);
            }

            num_files = rows;
            st_time.datasets = DataSetIndex;

            if (DataSetIndex == 13)
            {
               if (st_time.PointsOrLines)
               {
                  if (ProcessDataProxy_Points (&st_time, files, num_files))
                     fprintf (stderr, "No data coverage of the requested time interval.\n");
               }
               else
               {
                  if (ProcessDataProxy (&st_time, files, num_files))
                     fprintf (stderr, "No data coverage of the requested time interval.\n");
               }
            }
            else
            {
               if ((DataSetIndex >= 14) && (DataSetIndex <= 15))
                  st_time.SaturnOrTitan = False;
               else
                  st_time.SaturnOrTitan = True;

               if (st_time.PointsOrLines)
               {
                  if (ProcessData_Points (&st_time, files, num_files))
                     fprintf (stderr, "No data coverage of the requested time interval.\n");
               }
               else
               {
                  if (ProcessData (&st_time, files, num_files))
                     fprintf (stderr, "No data coverage of the requested time interval.\n");
               }
            }
         }
      }
   }

   PointSize = 1.0;

   pgsch_ (&PointSize);

   pgend_ ();

   return (0);
}
