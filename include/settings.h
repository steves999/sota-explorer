#pragma once
// Persistent settings via ESP32 NVS (Preferences library).
// All settings live in one namespace "sota" with short keys
// (NVS key limit: 15 characters).
//
// Defaults are applied on first boot or whenever a key is absent.
// Saving writes all settings in one open/close cycle to minimise
// NVS wear (NVS has ~10,000 erase cycles per page).

#include <Preferences.h>
#include "timezone.h"

// Proximity alert threshold options (metres)
static const int PROX_THRESHOLDS[]  = {200, 500, 1000, 2000};
static const int PROX_THRESHOLD_COUNT = 4;

// Rescan distance threshold options (km * 100 to avoid floats in NVS)
static const int RESCAN_THRESHOLDS_CM[] = {25, 50, 100, 200};  // 0.25/0.5/1.0/2.0 km
static const int RESCAN_THRESHOLD_COUNT = 4;

// Auto-dim delay options (seconds, 0 = disabled)
static const int DIM_DELAYS[]  = {0, 120, 300, 600};  // off/2min/5min/10min
static const int DIM_DELAY_COUNT = 4;

// Timezone options in display order
struct TZOption {
  TZone  tz;
  const char *label;
};
static const TZOption TZ_OPTIONS[] = {
  {TZ_NZ,  "NZ (NZST/NZDT)"},
  {TZ_ACT, "VK1 ACT"},
  {TZ_NSW, "VK2 NSW"},
  {TZ_VIC, "VK3 VIC"},
  {TZ_SA,  "VK5 SA"},
  {TZ_QLD, "VK4 QLD"},
  {TZ_WA,  "VK6 WA"},
  {TZ_NT,  "VK8 NT"},
  {TZ_TAS, "VK7 TAS"},
};
static const int TZ_OPTION_COUNT = 9;

struct AppSettings {
  bool    proximityAlert;
  int     proximityThreshIdx;
  int     rescanThreshIdx;
  bool    autoDim;
  int     dimDelayIdx;
  int     timezoneIdx;
  float   qnhHpa;
  uint8_t brightness;   // raw 0-255
};

inline AppSettings defaultSettings() {
  AppSettings s;
  s.proximityAlert     = true;
  s.proximityThreshIdx = 1;
  s.rescanThreshIdx    = 1;
  s.autoDim            = false;
  s.dimDelayIdx        = 1;
  s.timezoneIdx        = 0;
  s.qnhHpa             = 1013.25F;
  s.brightness         = 255;
  return s;
}

inline AppSettings loadSettings() {
  Preferences prefs;
  prefs.begin("sota", true);
  AppSettings s = defaultSettings();
  s.proximityAlert     = prefs.getBool("proxAlert",   s.proximityAlert);
  s.proximityThreshIdx = prefs.getInt("proxThresh",   s.proximityThreshIdx);
  s.rescanThreshIdx    = prefs.getInt("rescanThresh", s.rescanThreshIdx);
  s.autoDim            = prefs.getBool("autoDim",     s.autoDim);
  s.dimDelayIdx        = prefs.getInt("dimDelay",     s.dimDelayIdx);
  s.timezoneIdx        = prefs.getInt("timezone",     s.timezoneIdx);
  s.qnhHpa             = prefs.getFloat("qnh",        s.qnhHpa);
  s.brightness         = (uint8_t)prefs.getInt("brightness2", (int)s.brightness);
  prefs.end();
  return s;
}

inline void saveSettings(const AppSettings &s) {
  Preferences prefs;
  prefs.begin("sota", false);
  prefs.putBool("proxAlert",   s.proximityAlert);
  prefs.putInt("proxThresh",   s.proximityThreshIdx);
  prefs.putInt("rescanThresh", s.rescanThreshIdx);
  prefs.putBool("autoDim",     s.autoDim);
  prefs.putInt("dimDelay",     s.dimDelayIdx);
  prefs.putInt("timezone",     s.timezoneIdx);
  prefs.putFloat("qnh",        s.qnhHpa);
  prefs.putInt("brightness2",  s.brightness);  // new key avoids conflict with old index
  prefs.end();
}

// Convenience accessors
inline float rescanThreshKm(const AppSettings &s) {
  return RESCAN_THRESHOLDS_CM[s.rescanThreshIdx] / 100.0F;
}
inline int proximityThreshM(const AppSettings &s) {
  return PROX_THRESHOLDS[s.proximityThreshIdx];
}
inline int dimDelaySeconds(const AppSettings &s) {
  return DIM_DELAYS[s.dimDelayIdx];
}
inline uint8_t brightnessValue(const AppSettings &s) {
  return s.brightness;
}
inline TZone currentTimezone(const AppSettings &s) {
  return TZ_OPTIONS[s.timezoneIdx].tz;
}
inline const char* currentTimezoneLabel(const AppSettings &s) {
  return TZ_OPTIONS[s.timezoneIdx].label;
}
