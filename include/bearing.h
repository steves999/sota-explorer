#pragma once
// Bearing calculation from fix position to a target lat/lon.
// Returns true bearing in degrees 0-360 (0=North, 90=East etc).
// Desk-checked in Python against known cardinal points before porting.

#include <Arduino.h>
#include <math.h>

inline float calcBearing(float lat1, float lon1, float lat2, float lon2) {
  float lat1r = lat1 * DEG_TO_RAD;
  float lat2r = lat2 * DEG_TO_RAD;
  float dlon  = (lon2 - lon1) * DEG_TO_RAD;
  float x = sinf(dlon) * cosf(lat2r);
  float y = cosf(lat1r) * sinf(lat2r) - sinf(lat1r) * cosf(lat2r) * cosf(dlon);
  float b = atan2f(x, y) * RAD_TO_DEG;
  return fmodf(b + 360.0F, 360.0F);
}

// 8-point compass label for a bearing in degrees.
// Each sector is 45° wide, centred on 0/45/90/135/180/225/270/315.
inline const char* compass8(float deg) {
  static const char* pts[8] = {"N","NE","E","SE","S","SW","W","NW"};
  int idx = (int)((deg + 22.5F) / 45.0F) % 8;
  return pts[idx];
}
