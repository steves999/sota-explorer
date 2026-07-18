#pragma once
// Summit proximity matching against the on-flash ZL/VK summits CSV.
//
// CSV format (confirmed against the actual file we generated and
// uploaded — header row first):
//   SummitCode,SummitName,Latitude,Longitude,AltM
//
// Strategy: stream the file line-by-line from LittleFS rather than
// loading all ~535KB into RAM at once. At ~10,910 summits this is a
// few hundred ms of work on an ESP32-S3 — fine to run occasionally
// (e.g. once every few seconds when GPS fix changes meaningfully),
// not something to call every loop() iteration.

#include <Arduino.h>
#include <LittleFS.h>
#include "haversine.h"
#include "bearing.h"

// SOTA Activation Zone: official rule is a closed contour 25m below
// the summit (confirmed: this is the figure used by ZL and VK
// associations — some other associations use a different value, e.g.
// 75ft/~23m in parts of the US, so this constant would need revisiting
// if this code is ever reused outside ZL/VK).
//
// IMPORTANT LIMITATION: the real rule requires that the terrain between
// your position and the true summit never drops more than 25m — i.e.
// you can be within 25m vertically of the summit on a separate knoll
// across a deep saddle and still NOT be in the AZ. We have no terrain/
// contour data on this device, so we cannot detect that case. This
// check is a proxy only: vertical proximity AND tight horizontal
// proximity to the summit's recorded coordinate. It will be wrong on
// any summit with a nearby false top or knoll — known and accepted
// limitation, not silently hidden.
#define AZ_VERTICAL_LIMIT_M 25.0F
#define AZ_NOISE_MARGIN_M 5.0F        // GPS altitude jitter observed on bench
#define AZ_HORIZONTAL_LIMIT_M 100.0F  // proxy for "on this hill", not the real AZ shape

enum AZStatus {
  AZ_UNKNOWN,     // no fix, or altitude not valid
  AZ_OUT,         // clearly outside (beyond limit + margin, either axis)
  AZ_BORDERLINE,  // within the noise margin of the 25m threshold
  AZ_IN           // clearly within both vertical and horizontal proxy limits
};

struct SummitMatch {
  bool found;
  char summitCode[16];
  char summitName[48];
  float distanceKm;
  float bearingDeg;  // true bearing from fix position to summit, 0-360
  int altM;
};

// Estimates activation-zone status using GPS altitude vs the summit's
// recorded AltM, combined with horizontal distance as a rough proxy
// for "are we actually on this hill." See header comment above for the
// honest limitation: this cannot detect the knoll/saddle edge case
// that the real SOTA rule accounts for.
inline AZStatus estimateActivationZone(float fixAltM, float summitAltM, float distanceKm) {
  float vertDiff = summitAltM - fixAltM;  // positive = you're below the summit
  float horizM = distanceKm * 1000.0F;

  // Horizontal proxy gate first: if you're not even close to the
  // summit's coordinate, vertical proximity alone is meaningless (you
  // could be at the same altitude as the summit but on a different
  // hill entirely).
  if (horizM > AZ_HORIZONTAL_LIMIT_M + AZ_NOISE_MARGIN_M) {
    return AZ_OUT;
  }

  float lowerBound = AZ_VERTICAL_LIMIT_M - AZ_NOISE_MARGIN_M;
  float upperBound = AZ_VERTICAL_LIMIT_M + AZ_NOISE_MARGIN_M;

  if (vertDiff < 0) {
    // Fix altitude is ABOVE the summit's recorded altitude — well
    // within vertical range either way (could be GPS overshoot near
    // the true top, or you're literally on the highest point).
    if (horizM <= AZ_HORIZONTAL_LIMIT_M) return AZ_IN;
    return AZ_BORDERLINE;
  }
  if (vertDiff <= lowerBound && horizM <= AZ_HORIZONTAL_LIMIT_M) {
    return AZ_IN;
  }
  if (vertDiff <= upperBound) {
    return AZ_BORDERLINE;
  }
  return AZ_OUT;
}

inline const char *azStatusLabel(AZStatus s) {
  switch (s) {
    case AZ_IN: return "IN AZ";
    case AZ_BORDERLINE: return "AZ?";
    case AZ_OUT: return "OUT";
    default: return "--";
  }
}


// Parses one CSV line into the 5 expected fields. Returns false if the
// line doesn't have enough fields (e.g. blank line, malformed row) —
// callers should skip such lines rather than crash on them.
inline bool parseSummitLine(const String &line, char *codeOut, size_t codeLen,
                             char *nameOut, size_t nameLen,
                             float &lat, float &lon, int &altM) {
  // Simple comma-split. SummitName can't contain commas in our
  // filtered file since we generated it via Python's csv module,
  // which quotes fields containing commas — but our filter script
  // only kept SummitName values as-is from the source, and some
  // summit names DO contain commas if quoted in the original. To
  // keep the on-device parser simple and robust, we treat any
  // quoted field specially: if a field starts with '"', read until
  // the matching closing '"' rather than the next comma.
  int fieldIdx = 0;
  int pos = 0;
  int len = line.length();

  while (fieldIdx < 5 && pos < len) {
    String field;
    if (line[pos] == '"') {
      pos++; // skip opening quote
      int start = pos;
      while (pos < len && line[pos] != '"') pos++;
      field = line.substring(start, pos);
      pos++; // skip closing quote
      if (pos < len && line[pos] == ',') pos++; // skip trailing comma
    } else {
      int start = pos;
      while (pos < len && line[pos] != ',') pos++;
      field = line.substring(start, pos);
      if (pos < len) pos++; // skip comma
    }

    switch (fieldIdx) {
      case 0: strncpy(codeOut, field.c_str(), codeLen - 1); codeOut[codeLen-1] = '\0'; break;
      case 1: strncpy(nameOut, field.c_str(), nameLen - 1); nameOut[nameLen-1] = '\0'; break;
      case 2: lat = field.toFloat(); break;
      case 3: lon = field.toFloat(); break;
      case 4: altM = field.toInt(); break;
    }
    fieldIdx++;
  }

  return fieldIdx == 5;
}

// Streams the summits CSV from LittleFS, finds the nearest summit to
// (fixLat, fixLon). Returns a SummitMatch with found=false if the file
// couldn't be opened or no valid rows were found.
inline SummitMatch findNearestSummit(float fixLat, float fixLon,
                                      const char *csvPath = "/summits.csv") {
  SummitMatch best;
  best.found = false;
  best.distanceKm = 1e9F;
  best.summitCode[0] = '\0';
  best.summitName[0] = '\0';
  best.altM = 0;

  File f = LittleFS.open(csvPath, "r");
  if (!f) {
    return best;  // found stays false — caller should check this
  }

  // Skip header row.
  if (f.available()) {
    f.readStringUntil('\n');
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    char code[16];
    char name[48];
    float lat, lon;
    int altM;

    if (!parseSummitLine(line, code, sizeof(code), name, sizeof(name), lat, lon, altM)) {
      continue;  // malformed row — skip rather than abort the whole scan
    }

    float d = haversineKm(fixLat, fixLon, lat, lon);
    if (d < best.distanceKm) {
      best.distanceKm = d;
      best.found = true;
      strncpy(best.summitCode, code, sizeof(best.summitCode) - 1);
      best.summitCode[sizeof(best.summitCode) - 1] = '\0';
      strncpy(best.summitName, name, sizeof(best.summitName) - 1);
      best.summitName[sizeof(best.summitName) - 1] = '\0';
      best.altM = altM;
    }
  }

  f.close();
  return best;
}

#define SUMMIT_TOP_N 5

struct SummitMatchList {
  SummitMatch items[SUMMIT_TOP_N];
  int count;  // how many slots are actually populated (may be < SUMMIT_TOP_N
              // if the file has fewer than SUMMIT_TOP_N valid rows)
};

// Streams the CSV ONCE, maintaining a small sorted (ascending distance)
// array of the SUMMIT_TOP_N closest summits via simple insertion —
// O(N) candidates each doing an O(SUMMIT_TOP_N) insert, which is
// trivial overhead compared to the ~10,910-row file read/parse that
// already dominates the ~6.5s scan time measured on real hardware.
// Reuses parseSummitLine — no duplicated parsing logic.
inline SummitMatchList findNearestSummits(float fixLat, float fixLon,
                                           const char *csvPath = "/summits.csv") {
  SummitMatchList list;
  list.count = 0;
  for (int i = 0; i < SUMMIT_TOP_N; i++) {
    list.items[i].found = false;
    list.items[i].distanceKm = 1e9F;
    list.items[i].bearingDeg = 0.0F;
  }

  File f = LittleFS.open(csvPath, "r");
  if (!f) {
    return list;  // count stays 0 — caller should check this
  }

  if (f.available()) {
    f.readStringUntil('\n');  // header row
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    char code[16];
    char name[48];
    float lat, lon;
    int altM;

    if (!parseSummitLine(line, code, sizeof(code), name, sizeof(name), lat, lon, altM)) {
      continue;
    }

    float d = haversineKm(fixLat, fixLon, lat, lon);

    // Only bother inserting if this candidate beats the current worst
    // entry in our top-N (or we haven't filled all N slots yet).
    if (list.count < SUMMIT_TOP_N || d < list.items[SUMMIT_TOP_N - 1].distanceKm) {
      // Find insertion position (simple linear scan — N is tiny, 5).
      int insertAt = list.count < SUMMIT_TOP_N ? list.count : SUMMIT_TOP_N - 1;
      for (int i = 0; i < SUMMIT_TOP_N; i++) {
        if (i >= list.count || d < list.items[i].distanceKm) {
          insertAt = i;
          break;
        }
      }

      // Shift everything after insertAt down by one (dropping the
      // last element if the array is already full).
      int lastFilled = (list.count < SUMMIT_TOP_N) ? list.count : SUMMIT_TOP_N - 1;
      for (int i = lastFilled; i > insertAt; i--) {
        list.items[i] = list.items[i - 1];
      }

      SummitMatch &slot = list.items[insertAt];
      slot.found = true;
      slot.distanceKm = d;
      slot.bearingDeg = calcBearing(fixLat, fixLon, lat, lon);
      slot.altM = altM;
      strncpy(slot.summitCode, code, sizeof(slot.summitCode) - 1);
      slot.summitCode[sizeof(slot.summitCode) - 1] = '\0';
      // "NoName" is the HEMA scraper's placeholder for unnamed summits --
      // display the summit code instead, which is more useful in the field.
      const char *displayName = (strncmp(name, "NoName", 6) == 0) ? code : name;
      strncpy(slot.summitName, displayName, sizeof(slot.summitName) - 1);
      slot.summitName[sizeof(slot.summitName) - 1] = '\0';

      if (list.count < SUMMIT_TOP_N) list.count++;
    }
  }

  f.close();
  return list;
}
