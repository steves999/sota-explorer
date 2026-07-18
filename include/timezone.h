#pragma once
// Local time computation from GPS UTC for NZ and VK (Australian) zones.
// DST rules desk-checked in Python against published 2026 transition dates
// before porting -- confirmed correct for NZ (last Sun Sep / first Sun Apr)
// and AU DST states (first Sun Oct / first Sun Apr).
//
// GPS provides UTC date+time. We compute local offset in minutes, then
// add to UTC. The transition window (2:00am local) is approximated by
// checking UTC date only -- worst case is up to ~14h error right at the
// transition midnight boundary, which is acceptable for a field instrument.
//
// Standard offsets (minutes from UTC):
//   NZ (NZST)     +720  (UTC+12)   DST: +780 (UTC+13)
//   ACT/NSW/VIC/TAS/QLD  +600  (UTC+10)  DST (not QLD): +660 (UTC+11)
//   SA/NT         +570  (UTC+9:30) DST (not NT):  +630 (UTC+10:30)
//   WA            +480  (UTC+8)    no DST

#include <Arduino.h>

enum TZone {
  TZ_NZ,   // New Zealand (NZST/NZDT)
  TZ_ACT,  // Australian Capital Territory
  TZ_NSW,  // New South Wales
  TZ_VIC,  // Victoria
  TZ_TAS,  // Tasmania
  TZ_SA,   // South Australia
  TZ_QLD,  // Queensland (no DST)
  TZ_WA,   // Western Australia (no DST)
  TZ_NT,   // Northern Territory (no DST)
};

// Compile-time zone selection -- change this before building for Australia.
// TZ_NZ is the default for ZL use.
#ifndef LOCAL_TIMEZONE
#define LOCAL_TIMEZONE TZ_NZ
#endif

// Returns day-of-week for a given date (Zeller's congruence).
// Returns 0=Sun, 1=Mon ... 6=Sat.
inline int dayOfWeek(int year, int month, int day) {
  if (month < 3) { month += 12; year--; }
  int k = year % 100;
  int j = year / 100;
  int h = (day + (13*(month+1))/5 + k + k/4 + j/4 + 5*j) % 7;
  // Zeller gives 0=Sat, 1=Sun ... convert to 0=Sun
  return (h + 6) % 7;
}

// Returns the date (day-of-month) of the nth Sunday (1-based) in month/year.
inline int nthSunday(int year, int month, int n) {
  // Find day-of-week of the 1st of the month
  int dow = dayOfWeek(year, month, 1);
  // Days until first Sunday (0 if already Sunday)
  int firstSun = (dow == 0) ? 1 : (8 - dow);
  return firstSun + (n - 1) * 7;
}

// Returns the date (day-of-month) of the last Sunday in month/year.
inline int lastSunday(int year, int month) {
  // Days in month
  int days;
  if (month == 2) {
    days = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  } else if (month == 4 || month == 6 || month == 9 || month == 11) {
    days = 30;
  } else {
    days = 31;
  }
  int dow = dayOfWeek(year, month, days);
  return days - dow;  // dow=0 means last day is Sunday, so subtract 0
}

// Returns true if NZ is currently in DST (NZDT, UTC+13).
// DST: last Sunday September -> first Sunday April.
inline bool nzInDst(int year, int month, int day) {
  int startDay = lastSunday(year, 9);   // last Sun Sep
  int endDay   = nthSunday(year, 4, 1); // first Sun Apr
  if (month > 9 || (month == 9 && day >= startDay)) return true;
  if (month < 4 || (month == 4 && day < endDay))    return true;
  return false;
}

// Returns true if AU DST states (ACT/NSW/VIC/TAS/SA) are in DST.
// DST: first Sunday October -> first Sunday April.
inline bool auInDst(int year, int month, int day) {
  int startDay = nthSunday(year, 10, 1); // first Sun Oct
  int endDay   = nthSunday(year, 4, 1);  // first Sun Apr
  if (month > 10 || (month == 10 && day >= startDay)) return true;
  if (month < 4  || (month == 4  && day < endDay))    return true;
  return false;
}

// Returns UTC offset in minutes for the given zone and UTC date.
inline int utcOffsetMinutes(TZone tz, int year, int month, int day) {
  switch (tz) {
    case TZ_NZ:  return nzInDst(year, month, day)  ? 780 : 720;
    case TZ_ACT:
    case TZ_NSW:
    case TZ_VIC:
    case TZ_TAS: return auInDst(year, month, day)  ? 660 : 600;
    case TZ_SA:  return auInDst(year, month, day)  ? 630 : 570;
    case TZ_QLD: return 600;  // no DST
    case TZ_WA:  return 480;  // no DST
    case TZ_NT:  return 570;  // no DST
    default:     return 0;
  }
}

inline const char* tzAbbrev(TZone tz, int year, int month, int day) {
  switch (tz) {
    case TZ_NZ:  return nzInDst(year, month, day) ? "NZDT" : "NZST";
    case TZ_ACT:
    case TZ_NSW:
    case TZ_VIC:
    case TZ_TAS: return auInDst(year, month, day) ? "AEDT" : "AEST";
    case TZ_SA:  return auInDst(year, month, day) ? "ACDT" : "ACST";
    case TZ_QLD: return "AEST";
    case TZ_WA:  return "AWST";
    case TZ_NT:  return "ACST";
    default:     return "UTC";
  }
}

// Applies UTC offset (minutes) to UTC hh:mm:ss + date, returns local time.
// Handles day rollover/rollback.
inline void toLocalTime(int utcH, int utcM, int utcS,
                         int utcYear, int utcMonth, int utcDay,
                         TZone tz,
                         int &localH, int &localM, int &localS) {
  int offsetMin = utcOffsetMinutes(tz, utcYear, utcMonth, utcDay);
  int totalMin = utcH * 60 + utcM + offsetMin;
  // Handle negative or overflow (midnight wrap)
  totalMin = ((totalMin % 1440) + 1440) % 1440;
  localH = totalMin / 60;
  localM = totalMin % 60;
  localS = utcS;  // seconds unchanged
}
