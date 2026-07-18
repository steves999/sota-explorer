#pragma once
// Maidenhead grid square locator calculation.
// Standard algorithm — see https://en.wikipedia.org/wiki/Maidenhead_Locator_System
// Produces 6-character precision (e.g. "RE66ko"), the conventional
// precision used in amateur radio / SOTA contexts.

#include <Arduino.h>

inline String calculateMaidenhead(double lat, double lon) {
  // Maidenhead is defined on a 0-360 / 0-180 shifted coordinate space.
  // Longitude: -180..180 -> 0..360. Latitude: -90..90 -> 0..180.
  double adjLon = lon + 180.0;
  double adjLat = lat + 90.0;

  // Field (first pair): 18 zones of 20 deg longitude, 10 deg latitude,
  // letters A-R.
  int fieldLon = (int)(adjLon / 20.0);
  int fieldLat = (int)(adjLat / 10.0);
  char fieldLonChar = 'A' + fieldLon;
  char fieldLatChar = 'A' + fieldLat;

  double remLon = adjLon - (fieldLon * 20.0);
  double remLat = adjLat - (fieldLat * 10.0);

  // Square (second pair): 10x10 grid within the field, digits 0-9.
  int squareLon = (int)(remLon / 2.0);
  int squareLat = (int)(remLat / 1.0);

  remLon = remLon - (squareLon * 2.0);
  remLat = remLat - (squareLat * 1.0);

  // Subsquare (third pair): 24x24 grid within the square, letters a-x
  // (lowercase by convention).
  int subLon = (int)(remLon / (2.0 / 24.0));
  int subLat = (int)(remLat / (1.0 / 24.0));
  char subLonChar = 'a' + subLon;
  char subLatChar = 'a' + subLat;

  char buf[7];
  buf[0] = fieldLonChar;
  buf[1] = fieldLatChar;
  buf[2] = '0' + squareLon;
  buf[3] = '0' + squareLat;
  buf[4] = subLonChar;
  buf[5] = subLatChar;
  buf[6] = '\0';

  return String(buf);
}
