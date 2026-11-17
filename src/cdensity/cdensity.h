#define SELECT_QUERY1 "select path_to_dev, path_to_file, file_name, begin_time from cas_fuh where end_time > '%s' and begin_time < '%s'"
#define SELECT_QUERY2 "select path_to_dev, path_to_file, file_name, begin_time from cas_fuh where end_time > '%s' and begin_time < '%s' order by begin_time"

#define SELECT_QUERY3 "select path_to_dev, path_to_file, file_name, begin_time from cas_fpe where end_time > '%s' and begin_time < '%s'"
#define SELECT_QUERY4 "select path_to_dev, path_to_file, file_name, begin_time from cas_fpe where end_time > '%s' and begin_time < '%s' order by begin_time"

#define SELECT_QUERY5 "select path_to_dev, path_to_file, file_name, begin_time from cwb10k_fpe where end_time > '%s' and begin_time < '%s'"
#define SELECT_QUERY6 "select path_to_dev, path_to_file, file_name, begin_time from cwb10k_fpe where end_time > '%s' and begin_time < '%s' order by begin_time"

#define SELECT_QUERY7 "select path_to_dev, path_to_file, file_name, begin_time from cwb80k_fpe where end_time > '%s' and begin_time < '%s'"
#define SELECT_QUERY8 "select path_to_dev, path_to_file, file_name, begin_time from cwb80k_fpe where end_time > '%s' and begin_time < '%s' order by begin_time"

#define SELECT_QUERY9 "select path_to_dev, path_to_file, file_name, begin_time from cwb80k_fuh where end_time > '%s' and begin_time < '%s'"
#define SELECT_QUERY0 "select path_to_dev, path_to_file, file_name, begin_time from cwb80k_fuh where end_time > '%s' and begin_time < '%s' order by begin_time"

#define SELECT_QUERYA "select path_to_dev, path_to_file, file_name, begin_time from css_mag where end_time > '%s' and begin_time < '%s'"
#define SELECT_QUERYB "select path_to_dev, path_to_file, file_name, begin_time from css_mag where end_time > '%s' and begin_time < '%s' order by begin_time"

#define SELECT_QUERYC "select path_to_dev, path_to_file, file_name, begin_time from cas_lp_proxy where end_time > '%s' and begin_time < '%s'"
#define SELECT_QUERYD "select path_to_dev, path_to_file, file_name, begin_time from cas_lp_proxy where end_time > '%s' and begin_time < '%s' order by begin_time"

#define SELECT_QUERYE "select path_to_dev, path_to_file, file_name, begin_time from cas_fuh_t where end_time > '%s' and begin_time < '%s'"
#define SELECT_QUERYF "select path_to_dev, path_to_file, file_name, begin_time from cas_fuh_t where end_time > '%s' and begin_time < '%s' order by begin_time"

#define SELECT_QUERYG "select path_to_dev, path_to_file, file_name, begin_time from cwb80k_fuh_t where end_time > '%s' and begin_time < '%s'"
#define SELECT_QUERYH "select path_to_dev, path_to_file, file_name, begin_time from cwb80k_fuh_t where end_time > '%s' and begin_time < '%s' order by begin_time"

#define SELECT_QUERYI "select path_to_dev, path_to_file, file_name, begin_time from cwb80k_dust where end_time > '%s' and begin_time < '%s'"
#define SELECT_QUERYJ "select path_to_dev, path_to_file, file_name, begin_time from cwb80k_dust where end_time > '%s' and begin_time < '%s' order by begin_time"

#define MAXCHARS        80
#define MAXFILES      3000
#define MAX_ENTRIES    100
#define MaxPoints   224000

#define Zeroeth  0
#define First    1
#define Second   2
#define Third    3

#define True  1
#define False 0

#define PI   3.141592654
#define DPR 57.295779506
#define RPD  0.0174532925

#define Cassini    -82

#define Sun         10
#define Saturn     699
#define Enceladus  602
#define Titan      606

#define RSaturn    60268.0
#define REnceladus   254.0
#define RTitan      2575.0

#define Gauss2nT 100000.0

#define SingleMinute 60000.0
#define FourSeconds   4000.0

#define Time              0
#define SaturnRad         1
#define EnceladusRad      2
#define SaturnAlt         3
#define EnceladusAlt      4
#define Ne                5
#define NeOverB           6
#define BMag              7
#define TheLValue         8
#define SaturnMLatitude   9
#define FpOverFc         10
#define SaturnLocalTime  11
#define TitanRad         12
#define TitanAlt         13
#define TitanLatitude    14
#define TitanLocalTime   15
#define SaturnKLatitude  16

#define SaturnR_MASK           0x00001
#define SaturnLat_MASK         0x00002
#define SaturnMLat_MASK        0x00004
#define SaturnLon_MASK         0x00008
#define SaturnLT_MASK          0x00010
#define SaturnLValue_MASK      0x00020

#define EnceladusR_MASK        0x00040
#define EnceladusLat_MASK      0x00080
#define EnceladusMLat_MASK     0x00100
#define EnceladusLon_MASK      0x00200
#define EnceladusLValue_MASK   0x00400
#define EnceladusLT_MASK       0x00800

#define TitanR_MASK            0x01000
#define TitanLat_MASK          0x02000
#define TitanMLat_MASK         0x04000
#define TitanLon_MASK          0x08000
#define TitanLValue_MASK       0x10000
#define TitanLT_MASK           0x20000

#define fUHLR_MASK        0x00001
#define fPLR_MASK         0x00002
#define fPHR10_MASK       0x00004
#define fPHR80_MASK       0x00008
#define fuhHR80_MASK      0x00010
#define fUHLR2_MASK       0x00020
#define DMODEL_MASK       0x00040
#define LProbe_MASK       0x00080
#define LProbe2_MASK      0x00100
#define LProbe3_MASK      0x00200
#define LProbe4_MASK      0x00400
#define exfUHLR_MASK      0x00800
#define exfuhHR80_MASK    0x01000
#define LP_Proxy_MASK     0x02000
#define fUHLR_t_MASK      0x04000
#define fuhHR80_t_MASK    0x08000
#define fPHR80_dust_MASK  0x10000

struct time_struc
{
   int st_past;
   int sp_past;
   int st_year;
   int sp_year;
   int st_day;
   int sp_day;
   int st_hr;
   int sp_hr;
   int st_mn;
   int sp_mn;
   int observer;
   int target;
   int psorgif;
   int x;
   int y;
   int ephemeris;
   int datasets;
   int MnDyOrYr;
   int XAxis;
   int YAxis;
   int LinOrLog;
   int LinOrLogYAxis;
   int SkipDropout;
   int PointsOrLines;
   int SaturnOrTitan;
   int cl;
   float st_sec;
   float sp_sec;
   double NewQuality;
   double DataGapTime;
   double xmin;
   double xmax;
   double ymin;
   double ymax;
   double st_total_ms;
   double sp_total_ms;
   char title [200];
};

struct oa_struc
{
   int year;
   int day;
   int hr;
   int mn;
   float sec;
   int observer;
   int target;
   double radius;
};

struct file_structure
{
   char line [MAXCHARS];
   int year;
   int day;
   int hr;
   int mn;
   int num_recs;
   float sec;
   double ms;
   double st_ms;
   double sp_ms;
};

typedef struct
{
   char *name;
   char *val;
}
entry;
