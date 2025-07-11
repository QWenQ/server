/*
   Copyright (c) 2004, 2011, Oracle and/or its affiliates.
   Copyright (c) 2017, Monty Program Ab.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1335  USA */

/*
  This is a private header of sql-common library, containing
  declarations for my_time.c
*/

#ifndef _my_time_h_
#define _my_time_h_
#include "mysql_time.h"
#include "my_decimal_limits.h"

C_MODE_START

extern MYSQL_PLUGIN_IMPORT ulonglong log_10_int[20];
extern uchar days_in_month[];

#if SIZEOF_VOIDP == 4
/* 32 bit system, using old timestamp */
#define MY_TIME_T_MAX LONG_MAX
#define MY_TIME_T_MIN LONG_MIN
#define TIMESTAMP_MAX_YEAR 2038
#define TIMESTAMP_MAX_MONTH 1
#define TIMESTAMP_MAX_DAY 19
#define TIMESTAMP_MAX_VALUE INT_MAX32
#define TIMESTAMP_MIN_VALUE 0
#else
/* 64 bit system. Use 4 byte unsigned timestamp */
#define MY_TIME_T_MAX ((longlong) UINT_MAX32)
#define MY_TIME_T_MIN 0
#define TIMESTAMP_MAX_YEAR 2106
#define TIMESTAMP_MIN_VALUE 0
#define TIMESTAMP_MAX_VALUE ((longlong) UINT_MAX32)
#define TIMESTAMP_MAX_MONTH 2
#define TIMESTAMP_MAX_DAY 7
#endif /* SIZEOF_VOIDP */

#define TIMESTAMP_MIN_YEAR (1900 + YY_PART_YEAR - 1)
/* two-digit years < this are 20..; >= this are 19.. */
#define YY_PART_YEAR	   70

/*
  check for valid times only if the range of time_t is greater than
  the range of my_time_t
*/
#if SIZEOF_TIME_T > 4 || defined(TIME_T_UNSIGNED)
# define IS_TIME_T_VALID_FOR_TIMESTAMP(x) \
  ((ulonglong) (x) <= TIMESTAMP_MAX_VALUE &&     \
      ((x) >= TIMESTAMP_MIN_VALUE)
#else
# define IS_TIME_T_VALID_FOR_TIMESTAMP(x) \
    ((x) >= TIMESTAMP_MIN_VALUE)
#endif

/* Flags to str_to_datetime */

#define C_TIME_NO_ZERO_IN_DATE  (1UL << 23) /* == MODE_NO_ZERO_IN_DATE */
#define C_TIME_NO_ZERO_DATE     (1UL << 24) /* == MODE_NO_ZERO_DATE    */
#define C_TIME_INVALID_DATES    (1UL << 25) /* == MODE_INVALID_DATES   */

#define MYSQL_TIME_WARN_TRUNCATED    1U
#define MYSQL_TIME_WARN_OUT_OF_RANGE 2U
#define MYSQL_TIME_WARN_EDOM         4U
#define MYSQL_TIME_WARN_ZERO_DATE    8U
#define MYSQL_TIME_NOTE_TRUNCATED    16U

#define MYSQL_TIME_WARN_WARNINGS (MYSQL_TIME_WARN_TRUNCATED|\
                                  MYSQL_TIME_WARN_OUT_OF_RANGE|\
                                  MYSQL_TIME_WARN_EDOM|\
                                  MYSQL_TIME_WARN_ZERO_DATE)
#define MYSQL_TIME_WARN_NOTES    (MYSQL_TIME_NOTE_TRUNCATED)

#define MYSQL_TIME_WARN_HAVE_WARNINGS(x) MY_TEST((x) & MYSQL_TIME_WARN_WARNINGS)
#define MYSQL_TIME_WARN_HAVE_NOTES(x) MY_TEST((x) & MYSQL_TIME_WARN_NOTES)

/* Useful constants */
#define SECONDS_IN_24H 86400L

/* Limits for the INTERVAL data type */

 /* Number of hours between '0001-01-01 00h' and '9999-12-31 23h' */
#define TIME_MAX_INTERVAL_HOUR             87649415
#define TIME_MAX_INTERVAL_HOUR_CHAR_LENGTH 8

/* Number of full days between '0001-01-01' and '9999-12-31'*/
#define TIME_MAX_INTERVAL_DAY              3652058 /*87649415/24*/
#define TIME_MAX_INTERVAL_DAY_CHAR_LENGTH  7

/* Limits for the TIME data type */
#define TIME_MAX_HOUR 838
#define TIME_MAX_MINUTE 59
#define TIME_MAX_SECOND 59
#define TIME_MAX_SECOND_PART 999999
#define TIME_SECOND_PART_FACTOR (TIME_MAX_SECOND_PART+1)
#define TIME_SECOND_PART_DIGITS 6
#define TIME_MAX_VALUE (TIME_MAX_HOUR*10000 + TIME_MAX_MINUTE*100 + TIME_MAX_SECOND)
#define TIME_MAX_VALUE_SECONDS (TIME_MAX_HOUR * 3600L + \
                                TIME_MAX_MINUTE * 60L + TIME_MAX_SECOND)

/*
  Structure to return status from
    str_to_datetime(), str_to_time().
*/
typedef struct st_mysql_time_status
{
  int warnings;
  uint precision;
  uint nanoseconds;
} MYSQL_TIME_STATUS;

static inline void my_time_status_init(MYSQL_TIME_STATUS *status)
{
  status->warnings= 0;
  status->precision= 0;
  status->nanoseconds= 0;
}

struct my_timeval
{
  longlong tv_sec;
  ulong tv_usec;
};


my_bool check_date(const MYSQL_TIME *ltime, my_bool not_zero_date,
                   ulonglong flags, int *was_cut);
my_bool str_to_DDhhmmssff(const char *str, size_t length, MYSQL_TIME *l_time,
                          ulong max_hour, MYSQL_TIME_STATUS *status);
my_bool str_to_datetime_or_date_or_time(const char *str, size_t length,
                                        MYSQL_TIME *to, ulonglong flag,
                                        MYSQL_TIME_STATUS *status,
                                        ulong time_max_hour,
                                        ulong time_err_hour);
my_bool
str_to_datetime_or_date_or_interval_hhmmssff(const char *str, size_t length,
                                             MYSQL_TIME *to, ulonglong flag,
                                             MYSQL_TIME_STATUS *status,
                                             ulong time_max_hour,
                                             ulong time_err_hour);
my_bool
str_to_datetime_or_date_or_interval_day(const char *str, size_t length,
                                        MYSQL_TIME *to, ulonglong flag,
                                        MYSQL_TIME_STATUS *status,
                                        ulong time_max_hour,
                                        ulong time_err_hour);
my_bool str_to_datetime_or_date(const char *str, size_t length, MYSQL_TIME *to,
                                ulonglong flags, MYSQL_TIME_STATUS *status);

longlong number_to_datetime_or_date(longlong nr, ulong sec_part,
                                    MYSQL_TIME *time_res,
                                    ulonglong flags, int *was_cut);
int number_to_time_only(my_bool neg, ulonglong nr, ulong sec_part,
                        ulong max_hour, MYSQL_TIME *to, int *was_cut);

ulonglong TIME_to_ulonglong_datetime(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong_date(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong_time(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong(const MYSQL_TIME *);
double TIME_to_double(const MYSQL_TIME *my_time);

int check_time_range(struct st_mysql_time *my_time, uint dec, int *warning);
my_bool check_datetime_range(const MYSQL_TIME *ltime);


long calc_daynr(uint year,uint month,uint day);
uint calc_days_in_year(uint year);
uint year_2000_handling(uint year);

void my_init_time(void);


/*
  Function to check sanity of a TIMESTAMP value

  DESCRIPTION
    Check if a given MYSQL_TIME value fits in TIMESTAMP range.
    This function doesn't make precise check, but rather a rough
    estimate.

  RETURN VALUES
    TRUE   The value seems sane
    FALSE  The MYSQL_TIME value is definitely out of range
*/

static inline my_bool validate_timestamp_range(const MYSQL_TIME *t)
{
  if ((t->year > TIMESTAMP_MAX_YEAR || t->year < TIMESTAMP_MIN_YEAR) ||
      (t->year == TIMESTAMP_MIN_YEAR && (t->month < 12 || t->day < 31)))
    return FALSE;

  return TRUE;
}

/* Can't include mysqld_error.h, it needs mysys to build, thus hardcode 2 error values here. */
#ifndef ER_WARN_DATA_OUT_OF_RANGE
#define ER_WARN_DATA_OUT_OF_RANGE 1264
#define ER_WARN_INVALID_TIMESTAMP 1299
#endif

my_time_t 
my_system_gmt_sec(const MYSQL_TIME *t, long *my_timezone, uint *error_code);

void set_zero_time(MYSQL_TIME *tm, enum enum_mysql_timestamp_type time_type);

/*
  Required buffer length for my_time_to_str, my_date_to_str,
  my_datetime_to_str and TIME_to_string functions. Note, that the
  caller is still responsible to check that given TIME structure
  has values in valid ranges, otherwise size of the buffer could
  be not enough. We also rely on the fact that even wrong values
  sent using binary protocol fit in this buffer.
*/
#define MAX_DATE_STRING_REP_LENGTH 30
#define AUTO_SEC_PART_DIGITS DECIMAL_NOT_SPECIFIED

int my_interval_DDhhmmssff_to_str(const MYSQL_TIME *, char *to, uint digits);
int my_time_to_str(const MYSQL_TIME *l_time, char *to, uint digits);
int my_date_to_str(const MYSQL_TIME *l_time, char *to);
int my_datetime_to_str(const MYSQL_TIME *l_time, char *to, uint digits);
int my_TIME_to_str(const MYSQL_TIME *l_time, char *to, uint digits);

int my_timeval_to_str(const struct my_timeval *tm, char *to, uint dec);

static inline longlong sec_part_shift(longlong second_part, uint digits)
{
  return second_part / (longlong)log_10_int[TIME_SECOND_PART_DIGITS - digits];
}
static inline longlong sec_part_unshift(longlong second_part, uint digits)
{
  return second_part * (longlong)log_10_int[TIME_SECOND_PART_DIGITS - digits];
}

/* Date/time rounding and truncation functions */
static inline long my_time_fraction_remainder(long nr, uint decimals)
{
  return nr % (long) log_10_int[TIME_SECOND_PART_DIGITS - decimals];
}
static inline void my_datetime_trunc(MYSQL_TIME *ltime, uint decimals)
{
  ltime->second_part-= my_time_fraction_remainder(ltime->second_part, decimals);
}
static inline void my_time_trunc(MYSQL_TIME *ltime, uint decimals)
{
  ltime->second_part-= my_time_fraction_remainder(ltime->second_part, decimals);
  if (!ltime->second_part && ltime->neg &&
      !ltime->hour && !ltime->minute && !ltime->second)
    ltime->neg= FALSE;
}
#ifdef _WIN32
#define suseconds_t long
#endif

#define hrtime_to_my_time(X) ((my_time_t)hrtime_to_time(X))

/* 
  Available interval types used in any statement.

  'interval_type' must be sorted so that simple intervals comes first,
  ie year, quarter, month, week, day, hour, etc. The order based on
  interval size is also important and the intervals should be kept in a
  large to smaller order. (get_interval_value() depends on this)
 
  Note: If you change the order of elements in this enum you should fix 
  order of elements in 'interval_type_to_name' and 'interval_names' 
  arrays 
  
  See also interval_type_to_name, get_interval_value, interval_names, append_interval
*/

enum interval_type
{
  INTERVAL_YEAR, INTERVAL_QUARTER, INTERVAL_MONTH, INTERVAL_WEEK, INTERVAL_DAY,
  INTERVAL_HOUR, INTERVAL_MINUTE, INTERVAL_SECOND, INTERVAL_MICROSECOND,
  INTERVAL_YEAR_MONTH, INTERVAL_DAY_HOUR, INTERVAL_DAY_MINUTE,
  INTERVAL_DAY_SECOND, INTERVAL_HOUR_MINUTE, INTERVAL_HOUR_SECOND,
  INTERVAL_MINUTE_SECOND, INTERVAL_DAY_MICROSECOND, INTERVAL_HOUR_MICROSECOND,
  INTERVAL_MINUTE_MICROSECOND, INTERVAL_SECOND_MICROSECOND, INTERVAL_LAST
};

C_MODE_END

#endif /* _my_time_h_ */
