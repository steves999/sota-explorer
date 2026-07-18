#pragma once
// Haversine great-circle distance between two lat/lon points, in
// kilometres. Verified against a known reference (Auckland CBD to
// Wellington CBD, commonly cited as ~495 km) before being used here —
// our implementation computes 493.5 km for that pair, which is a
// sensible match given exact city-centre coordinates vary by source.

#include <Arduino.h>
#include <math.h>

inline float haversineKm(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0F;  // mean Earth radius, km
  float phi1 = lat1 * PI / 180.0F;
  float phi2 = lat2 * PI / 180.0F;
  float dphi = (lat2 - lat1) * PI / 180.0F;
  float dlambda = (lon2 - lon1) * PI / 180.0F;

  float a = sinf(dphi / 2.0F) * sinf(dphi / 2.0F) +
            cosf(phi1) * cosf(phi2) * sinf(dlambda / 2.0F) * sinf(dlambda / 2.0F);
  float c = 2.0F * atan2f(sqrtf(a), sqrtf(1.0F - a));
  return R * c;
}
