// SOTA Field Instrument — Milestone 2: Display bring-up (added to
// Milestone 1's working GPS + BME280 code)
//
// Display library: xinyuan-lilygo/LilyGo-AMOLED-Series @ 1.2.3 (confirmed
// real via `pio pkg search` on Steve's bench, June 2026 — not a guess).
// API confirmed by reading the actual Factory.ino example from the
// official repo: class is LilyGo_Class, instantiated as a global, and
// amoled.begin() auto-detects which AMOLED variant is attached rather
// than needing us to call a model-specific beginAMOLED_191() etc.
//
// This step ONLY confirms the display initializes and can show
// something. No LVGL UI layout yet — that's Milestone 3/4 territory
// once we've confirmed the basics work on real hardware.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <TinyGPSPlus.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <LittleFS.h>
#include "pins_config.h"
#include "maidenhead.h"
#include "summit_match.h"
#include "gui.h"
#include "settings.h"

LilyGo_Class amoled;
bool displayOk = false;

// Settings -- loaded from NVS on boot, saved whenever the user changes something
AppSettings appSettings;

Adafruit_BME280 bme;
bool bmeOk = false;
bool bmeGenuine = false;
float lastGoodPressureHpa = 0.0F;  // cache last valid reading
uint8_t bmeAddrFound = 0;

// Pressure trend: rolling buffer sampled every 5 min, enough history
// to compare "now" against "~15 min ago" for a simple rising/falling/
// steady indicator. SOTA-relevant timescale — short-term weather
// trend, not long-range forecasting.
#define PRESSURE_HISTORY_SIZE 6   // 6 samples x 5 min = 30 min window
float pressureHistory[PRESSURE_HISTORY_SIZE];
uint8_t pressureHistoryCount = 0;
uint8_t pressureHistoryIndex = 0;
float tempHistory[PRESSURE_HISTORY_SIZE];
float humidHistory[PRESSURE_HISTORY_SIZE];
uint32_t lastPressureSampleMillis = 0;
#define PRESSURE_SAMPLE_INTERVAL_MS (5UL * 60UL * 1000UL)  // 5 minutes
#define PRESSURE_MIN_HPA 870.0F
#define PRESSURE_MAX_HPA 1084.0F

TinyGPSPlus gps;
HardwareSerial GpsSerial(1);   // ESP32-S3 UART1
uint32_t gpsBaud = GPS_BAUD_DEFAULT;
uint32_t lastNmeaCharMillis = 0;
uint32_t lastStatusPrintMillis = 0;
uint32_t nmeaCharCount = 0;

// Summit match: only re-scan the CSV when the fix has moved far enough
// to matter (avoids redundant ~10,910-row flash scans every 10s when
// sitting still). 0.5km threshold is arbitrary but sensible for SOTA
// use — summit candidates rarely change meaningfully over less than
// that without you actually having moved to a different hill.
SummitMatch lastSummitMatch;
SummitMatchList lastSummitList;
bool haveSummitList = false;
float lastMatchedLat = 0.0F;
float lastMatchedLon = 0.0F;
bool haveSummitMatch = false;

// Mode switching: Summit mode scans summits.csv (SOTA+HEMA);
// Park mode scans parks.csv (WWFF+POTA). Only one file is active
// at a time, so scan time stays manageable regardless of total row
// counts across all programmes.
enum InstrumentMode { MODE_SUMMIT, MODE_PARK };
InstrumentMode currentMode = MODE_SUMMIT;
#define SUMMITS_CSV "/summits.csv"
#define PARKS_CSV   "/parks.csv"

// Returns the CSV path for the current mode.
inline const char* currentCsvPath() {
  return currentMode == MODE_SUMMIT ? SUMMITS_CSV : PARKS_CSV;
}

// ---------------------------------------------------------------------
// I2C scan — run once at boot so we know what's actually on the bus
// rather than assuming an address.
// ---------------------------------------------------------------------
void scanI2C() {
  Serial.println(F("--- I2C scan ---"));
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found device at 0x%02X\n", addr);
      found++;
      if (addr == BME280_ADDR_PRIMARY || addr == BME280_ADDR_SECONDARY) {
        bmeAddrFound = addr;
      }
    }
  }
  if (found == 0) {
    Serial.println(F("  No I2C devices found! Check wiring: "
                      "GPIO2=SCL, GPIO3=SDA, BME280 VCC->3V3, GND->G."));
  }
  Serial.println(F("--- end scan ---"));
}

bool initBme() {
  if (bmeAddrFound == 0) return false;

  // Soft reset before begin() to clear any stuck state
  Wire.beginTransmission(bmeAddrFound);
  Wire.write(0xE0); Wire.write(0xB6);
  Wire.endTransmission();
  delay(10);

  if (!bme.begin(bmeAddrFound, &Wire)) {
    Serial.printf("BME280 at 0x%02X: begin() failed\n", bmeAddrFound);
    return false;
  }

  // Check chip ID
  Wire.beginTransmission(bmeAddrFound);
  Wire.write(0xD0);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)bmeAddrFound, (uint8_t)1);
  uint8_t chipId = Wire.available() ? Wire.read() : 0;
  Serial.printf("BME280 chip ID: 0x%02X (expect 0x60)\n", chipId);
  bmeGenuine = (chipId == 0x60 || chipId == 0x58);

  bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                  Adafruit_BME280::SAMPLING_X4,
                  Adafruit_BME280::SAMPLING_X4,
                  Adafruit_BME280::SAMPLING_X4,
                  Adafruit_BME280::FILTER_X4,
                  Adafruit_BME280::STANDBY_MS_125);
  delay(300);
  float p = bme.readPressure() / 100.0F;
  Serial.printf("BME280 OK at 0x%02X  startup: %.2f hPa\n", bmeAddrFound, p);
  return true;
}

// Forward declarations
char getPressureTrendArrow();
float getPressureTrendRate();
float pressureAltitudeM(float pressHpa, float qnhHpa);
int readBatteryPct();
void onSettingsChanged(const AppSettings &s);
void goToSleep();

// Read pressure in hPa. Reconfigures the chip every call to recover from
// spontaneous resets (power glitches). Detects and converts mmHg output
// from chips with non-standard compensation (757.88 mmHg = ~1010 hPa).
// Returns 0.0 if reading is implausible.
float readPressureHpa() {
  // Reconfigure before every read -- fast and handles self-reset chips
  bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                  Adafruit_BME280::SAMPLING_X4,
                  Adafruit_BME280::SAMPLING_X4,
                  Adafruit_BME280::SAMPLING_X4,
                  Adafruit_BME280::FILTER_X4,
                  Adafruit_BME280::STANDBY_MS_125);

  // Poll status register until measurement ready (max 500ms)
  uint32_t deadline = millis() + 500;
  while (millis() < deadline) {
    delay(10);
    Wire.beginTransmission(bmeAddrFound);
    Wire.write(0xF3);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)bmeAddrFound, (uint8_t)1);
    if (Wire.available() && (Wire.read() & 0x08) == 0) break;
  }

  float p = bme.readPressure() / 100.0F;

  // If in mmHg range (700-800), convert to hPa -- some clone compensation
  // tables output mmHg. 757.88 mmHg = 1010.42 hPa, consistent with
  // Auckland sea-level pressure.
  if (p >= 700.0F && p < PRESSURE_MIN_HPA) {
    p = p * 1.33322F;
    Serial.printf("[BME280] mmHg detected, converted to %.2f hPa\n", p);
  }

  if (p >= PRESSURE_MIN_HPA && p <= PRESSURE_MAX_HPA) return p;

  Serial.printf("[BME280] implausible: %.2f hPa\n", p);
  return 0.0F;
}

void printBme() {
  if (!bmeOk) return;
  float tempC = bme.readTemperature();
  float pressHpa = readPressureHpa();
  float humidity = bme.readHumidity();
  char trend = getPressureTrendArrow();
  int batt = readBatteryPct();
  Serial.printf("[BME280] temp=%.2fC  pressure=%.2fhPa [%c]  humidity=%.1f%%  battery=%d%%\n",
                tempC, pressHpa, trend, humidity, batt);
}

// ---------------------------------------------------------------------
// Pressure trend: sample every 5 min into a small ring buffer, compare
// oldest vs newest sample to give a simple rising/falling/steady
// indicator. Threshold of 1.0 hPa over the window is a common rule-of-
// thumb sensitivity for noticeable short-term weather change — not a
// precise meteorological standard, just a sensible field-use threshold.
// ---------------------------------------------------------------------
void samplePressureTrend() {
  if (!bmeOk) return;
  float pressHpa = readPressureHpa();
  pressureHistory[pressureHistoryIndex] = pressHpa;
  tempHistory[pressureHistoryIndex]     = bme.readTemperature();
  humidHistory[pressureHistoryIndex]    = bme.readHumidity();
  pressureHistoryIndex = (pressureHistoryIndex + 1) % PRESSURE_HISTORY_SIZE;
  if (pressureHistoryCount < PRESSURE_HISTORY_SIZE) {
    pressureHistoryCount++;
  }
  if (displayOk) {
    updatePressureGraph(pressureHistory, PRESSURE_HISTORY_SIZE,
                        pressureHistoryCount, pressureHistoryIndex);
    updateTempHumidGraph(tempHistory, humidHistory, PRESSURE_HISTORY_SIZE,
                         pressureHistoryCount, pressureHistoryIndex);
  }
}

// Returns: '^' rising, 'v' falling, '-' steady, '?' not enough data yet.
char getPressureTrendArrow() {
  if (pressureHistoryCount < 2) return '?';

  // Oldest sample is the one PRESSURE_HISTORY_COUNT steps behind the
  // current write index (wrapping).
  uint8_t oldestIdx = (pressureHistoryIndex + PRESSURE_HISTORY_SIZE - pressureHistoryCount)
                      % PRESSURE_HISTORY_SIZE;
  uint8_t newestIdx = (pressureHistoryIndex + PRESSURE_HISTORY_SIZE - 1)
                      % PRESSURE_HISTORY_SIZE;

  float oldest = pressureHistory[oldestIdx];
  float newest = pressureHistory[newestIdx];
  float delta = newest - oldest;

  const float THRESHOLD_HPA = 1.0F;
  if (delta > THRESHOLD_HPA) return '^';
  if (delta < -THRESHOLD_HPA) return 'v';
  return '-';
}

// Returns the pressure change rate in hPa/hr over the history window,
// or NAN if fewer than 2 samples are available yet.
float getPressureTrendRate() {
  if (pressureHistoryCount < 2) return NAN;
  uint8_t oldestIdx = (pressureHistoryIndex + PRESSURE_HISTORY_SIZE - pressureHistoryCount)
                      % PRESSURE_HISTORY_SIZE;
  uint8_t newestIdx = (pressureHistoryIndex + PRESSURE_HISTORY_SIZE - 1)
                      % PRESSURE_HISTORY_SIZE;
  float deltaHpa = pressureHistory[newestIdx] - pressureHistory[oldestIdx];
  float windowHours = ((pressureHistoryCount - 1) * PRESSURE_SAMPLE_INTERVAL_MS)
                      / 3600000.0F;
  return deltaHpa / windowHours;
}

// Pressure-derived altitude using the international barometric formula.
// QNH (sea-level reference pressure) defaults to ISA standard 1013.25 hPa —
// useful as a rough cross-check against GPS altitude on a summit, but will
// be off by ~8m per hPa of QNH error, so treat as indicative only unless
// you have a current QNH from a nearby METAR/airfield.
float pressureAltitudeM(float pressHpa, float qnhHpa = 1013.25F) {
  return 44330.0F * (1.0F - powf(pressHpa / qnhHpa, 0.1903F));
}

// ---------------------------------------------------------------------
// GPS: feed every byte to TinyGPSPlus, and separately echo raw NMEA to
// the console so we can SEE traffic is flowing even before a fix.
// ---------------------------------------------------------------------
void pollGps() {
  while (GpsSerial.available() > 0) {
    char c = GpsSerial.read();
    nmeaCharCount++;
    lastNmeaCharMillis = millis();
    gps.encode(c);
    // Echo raw stream so it's visually obvious data is arriving.
    Serial.write(c);
  }
}

void printGpsStatus() {
  Serial.println(F("--- GPS status ---"));
  Serial.printf("  Chars processed total: %lu\n", (unsigned long)nmeaCharCount);
  Serial.printf("  Failed checksums: %lu\n", (unsigned long)gps.failedChecksum());
  Serial.printf("  Passed checksums: %lu\n", (unsigned long)gps.passedChecksum());

  if (nmeaCharCount == 0) {
    Serial.println(F("  No characters received yet from GPS module."));
    Serial.println(F("  Check: wiring (board TX43->GPS RX, board RX44->GPS TX),"));
    Serial.printf("  and try GPS_BAUD_DEFAULT=38400 in pins_config.h if this "
                  "persists (currently %lu).\n", (unsigned long)gpsBaud);
  } else if (gps.passedChecksum() == 0) {
    Serial.println(F("  Characters arriving but no valid NMEA sentences yet."));
    Serial.println(F("  Possible wrong baud rate — try 38400 if at 9600, or "
                      "vice versa."));
  }

  if (gps.location.isValid()) {
    Serial.printf("  FIX: lat=%.6f lon=%.6f age=%lums sats=%d hdop=%.1f\n",
                  gps.location.lat(), gps.location.lng(),
                  gps.location.age(), gps.satellites.value(),
                  gps.hdop.hdop());
    String grid = calculateMaidenhead(gps.location.lat(), gps.location.lng());
    Serial.printf("  Maidenhead grid: %s\n", grid.c_str());
  } else {
    Serial.printf("  No fix yet. Satellites in view: %d. "
                  "Cold fix can take several minutes, longer indoors.\n",
                  gps.satellites.isValid() ? gps.satellites.value() : 0);
  }

  if (gps.altitude.isValid()) {
    Serial.printf("  Altitude: %.1fm\n", gps.altitude.meters());
    if (haveSummitMatch && lastSummitMatch.found) {
      AZStatus az = estimateActivationZone(gps.altitude.meters(),
                                            (float)lastSummitMatch.altM,
                                            lastSummitMatch.distanceKm);
      Serial.printf("  Activation zone (%s, summit alt %dm): %s "
                    "(vertical diff %.1fm — proxy only, see code comment "
                    "re: terrain/saddle limitation)\n",
                    lastSummitMatch.summitCode, lastSummitMatch.altM,
                    azStatusLabel(az), (float)lastSummitMatch.altM - gps.altitude.meters());
    }
  }
  if (gps.date.isValid() && gps.time.isValid()) {
    Serial.printf("  UTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                  gps.date.year(), gps.date.month(), gps.date.day(),
                  gps.time.hour(), gps.time.minute(), gps.time.second());
  }
  Serial.println(F("--- end GPS status ---"));
}

// ---------------------------------------------------------------------
// Display init — auto-detect, confirmed pattern from the real
// Factory.ino example (not guessed). Wire.begin() for our I2C sensors
// happens AFTER this since amoled.begin() likely sets up its own I2C
// for the touch controller (GPIO21, separate from our GPIO2/3 bus) —
// if there's any conflict we'll see it immediately in testing rather
// than assume either way.
// ---------------------------------------------------------------------
bool initDisplay() {
  bool rslt = amoled.begin();
  if (!rslt) {
    Serial.println(F("Display NOT detected — amoled.begin() returned false."));
    return false;
  }
  Serial.print(F("Display OK — Board Name: "));
  Serial.println(amoled.getName());
  beginLvglHelper(amoled);
  return true;
}

// ---------------------------------------------------------------------
// Milestone 4: gathers current state and pushes it to the real LVGL UI
// (gui.h) — three-band layout with colour-coded GPS-quality and AZ
// pills, agreed with Steve: summit/AZ info dominant, GPS quality
// secondary.
// ---------------------------------------------------------------------
void updateDisplayLabel() {
  if (!displayOk) return;

  bool fixValid = gps.location.isValid();
  bool altValid = gps.altitude.isValid();

  float pressHpa = 0.0F;
  if (bmeOk) {
    float p = readPressureHpa();
    if (p > 0.0F) {
      lastGoodPressureHpa = p;
      pressHpa = p;
    } else if (lastGoodPressureHpa > 0.0F) {
      pressHpa = lastGoodPressureHpa;
    }
  }
  bool dateValid = gps.date.isValid();
  guiUpdate(
    fixValid,
    fixValid ? gps.location.lat() : 0.0,
    fixValid ? gps.location.lng() : 0.0,
    gps.satellites.isValid() ? gps.satellites.value() : 0,
    gps.hdop.isValid() ? gps.hdop.hdop() : 99.9F,
    bmeOk,
    bmeGenuine,
    (lastGoodPressureHpa > 0.0F),  // pressure usable = have at least one good reading
    bmeOk ? bme.readTemperature() : 0.0F,
    pressHpa,
    getPressureTrendArrow(),
    bmeOk ? bme.readHumidity() : 0.0F,
    getPressureTrendRate(),
    bmeOk ? pressureAltitudeM(pressHpa, 1013.25F) : 0.0F,
    haveSummitMatch, lastSummitMatch,
    altValid, altValid ? gps.altitude.meters() : 0.0F,
    dateValid ? gps.date.year()  : 2025,
    dateValid ? gps.date.month() : 1,
    dateValid ? gps.date.day()   : 1,
    gps.time.isValid() ? gps.time.hour()   : 0,
    gps.time.isValid() ? gps.time.minute() : 0,
    gps.time.isValid() ? gps.time.second() : 0,
    haveSummitList, lastSummitList,
    currentMode == MODE_PARK,
    readBatteryPct(),
    appSettings.proximityAlert,
    proximityThreshM(appSettings),
    currentTimezone(appSettings)
  );
}

// ---------------------------------------------------------------------
// LittleFS smoke test: mount the filesystem and list what's in it.
// This is ONLY a verification step before we write real CSV-parsing/
// summit-matching code — confirms the upload actually worked and the
// file is readable before we build anything on top of it.
// ---------------------------------------------------------------------
void testLittleFS() {
  Serial.println(F("--- LittleFS test ---"));
  if (!LittleFS.begin(false)) {  // false = don't auto-format on failure
    Serial.println(F("  LittleFS mount FAILED. Filesystem image may not "
                      "have been uploaded yet (run 'pio run --target "
                      "uploadfs'), or the partition table has no LittleFS "
                      "region."));
    Serial.println(F("--- end LittleFS test ---"));
    return;
  }
  Serial.println(F("  LittleFS mounted OK."));

  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println(F("  Could not open root directory."));
  } else {
    File file = root.openNextFile();
    int count = 0;
    while (file) {
      Serial.printf("  Found: %s  (%d bytes)\n", file.name(), file.size());
      count++;
      file = root.openNextFile();
    }
    if (count == 0) {
      Serial.println(F("  No files found — filesystem is empty."));
    }
  }
  Serial.println(F("--- end LittleFS test ---"));
}

// Called from the GUI mode toggle button. Switches between Summit and
// Park mode, clears the current match so a rescan is forced immediately
// on the next status cycle, and updates the display mode label.
void switchMode() {
  currentMode = (currentMode == MODE_SUMMIT) ? MODE_PARK : MODE_SUMMIT;
  // Clear match state so updateSummitMatch() rescans immediately
  haveSummitMatch = false;
  haveSummitList  = false;
  lastMatchedLat  = 0.0F;
  lastMatchedLon  = 0.0F;
  Serial.printf("[Mode] Switched to %s mode — will rescan %s\n",
                currentMode == MODE_SUMMIT ? "Summit" : "Park",
                currentCsvPath());
}

// Brightness control -- called from the GUI long-press handler.
// amoled is declared in main.cpp so this function must live here.
void setBrightness(uint8_t level) {
  amoled.setBrightness(level);
  Serial.printf("[Brightness] Set to %d\n", level);
}

// Called whenever the user changes a setting in the settings page.
// Saves to NVS and applies any immediate effects (brightness, etc.)
void onSettingsChanged(const AppSettings &s) {
  appSettings = s;
  saveSettings(s);
  // Apply brightness immediately
  amoled.setBrightness(brightnessValue(s));
  Serial.printf("[Settings] Saved. prox=%s/%dm rescan=%.2fkm tz=%s\n",
                s.proximityAlert ? "on" : "off",
                proximityThreshM(s),
                rescanThreshKm(s),
                currentTimezoneLabel(s));
}

// Deep sleep -- triggered by long-press on the mode button.
// Shows a brief message, dims the display, then enters ESP32 deep sleep.
// Wake source: RESET button (external reset pin), which triggers a full
// reboot. GPS will do a warm start and reacquire in ~30 seconds.
// Settings are preserved in NVS across sleep cycles.
void goToSleep() {
  Serial.println("[Sleep] Entering deep sleep. Press RESET to wake.");
  Serial.flush();
  showSleepMessage();
  delay(800);  // let the user see "Sleeping..."
  amoled.setBrightness(0);
  delay(200);
  esp_deep_sleep_start();
  // Execution never reaches here -- RESET causes a full reboot
}


// Formula is empirical (community-derived, undocumented divider ratio for
// the 1.91" AMOLED board). Returns 0-100 percentage estimate, or -1 if invalid.
// IMPORTANT: verify displayed value against a multimeter on first field use.
#define PIN_BAT_VOLT 4
int readBatteryPct() {
  int raw = analogRead(PIN_BAT_VOLT);
  if (raw <= 0) return -1;
  int pct = (raw * 1000 / 6206) - 300;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  Serial.printf("[BAT] raw ADC=%d  pct=%d%%\n", raw, pct);
  return pct;
}


// last scan. Prints scan duration to serial — we genuinely don't know
// yet how long a ~10,910-row LittleFS streaming scan takes on this
// hardware, so this measurement is the real answer, not a guess.
// ---------------------------------------------------------------------
void updateSummitMatch() {
  if (!gps.location.isValid()) return;

  float curLat = gps.location.lat();
  float curLon = gps.location.lng();

  bool needRescan = !haveSummitMatch;
  if (haveSummitMatch) {
    float movedKm = haversineKm(curLat, curLon, lastMatchedLat, lastMatchedLon);
    if (movedKm > rescanThreshKm(appSettings)) {
      needRescan = true;
    }
  }

  if (!needRescan) return;

  uint32_t t0 = millis();
  SummitMatchList listResult = findNearestSummits(curLat, curLon, currentCsvPath());
  uint32_t elapsedMs = millis() - t0;

  Serial.printf("[%s scan] took %lums — ",
                currentMode == MODE_SUMMIT ? "Summit" : "Park",
                (unsigned long)elapsedMs);
  if (listResult.count > 0) {
    Serial.printf("nearest: %s \"%s\" alt=%dm dist=%.2fkm (top-%d list also updated)\n",
                  listResult.items[0].summitCode, listResult.items[0].summitName,
                  listResult.items[0].altM, listResult.items[0].distanceKm,
                  listResult.count);
  } else {
    Serial.println(F("no match found (file missing or empty?)"));
  }

  lastSummitList = listResult;
  haveSummitList = listResult.count > 0;

  // Single-best-match state derives from the same scan — items[0] is
  // always the closest, so this is free (no second file pass needed).
  if (listResult.count > 0) {
    lastSummitMatch = listResult.items[0];
    haveSummitMatch = true;
  } else {
    haveSummitMatch = false;
  }

  lastMatchedLat = curLat;
  lastMatchedLon = curLon;
}

void setup() {
  Serial.begin(115200);
  delay(500); // let USB-CDC settle
  Serial.println();
  Serial.println(F("=== SOTA Field Instrument — Milestone 2 bring-up ==="));

  testLittleFS();

  appSettings = loadSettings();
  Serial.printf("[Settings] Loaded: prox=%s/%dm rescan=%.2fkm tz=%s bright=%d\n",
                appSettings.proximityAlert ? "on" : "off",
                proximityThreshM(appSettings),
                rescanThreshKm(appSettings),
                currentTimezoneLabel(appSettings),
                brightnessValue(appSettings));

  displayOk = initDisplay();
  if (displayOk) {
    guiInit(switchMode, setBrightness, onSettingsChanged, appSettings, goToSleep);
    amoled.setBrightness(brightnessValue(appSettings));
    lv_timer_handler(); // force one render pass immediately
  }

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  scanI2C();
  bmeOk = initBme();

  GpsSerial.begin(gpsBaud, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  Serial.printf("GPS UART started at %lu baud (RX=GPIO%d, TX=GPIO%d)\n",
                (unsigned long)gpsBaud, PIN_GPS_RX, PIN_GPS_TX);
  Serial.println(F("Waiting for sensor data... (raw NMEA will echo below)"));
  Serial.println();
}

void loop() {
  if (displayOk) {
    lv_timer_handler();
  }

  pollGps();

  uint32_t now = millis();

  // BME280 reading every 2s -- refresh sampling config first to recover
  // from any spontaneous chip reset (caused by 3.3V rail glitches).
  // The 10ms delay after setSampling gives the chip one measurement cycle
  // before we read -- not enough for a full settling but enough to reject
  // the immediate post-reset garbage value via readPressureHpa() validation.
  static uint32_t lastBme = 0;
  if (now - lastBme >= 2000) {
    lastBme = now;
    printBme();
  }

  // Pressure trend sample every 5 min
  if (now - lastPressureSampleMillis >= PRESSURE_SAMPLE_INTERVAL_MS) {
    lastPressureSampleMillis = now;
    samplePressureTrend();
  }

  // Full status block every 10s
  if (now - lastStatusPrintMillis >= 10000) {
    lastStatusPrintMillis = now;
    printGpsStatus();
    updateSummitMatch();
    updateDisplayLabel();
  }
}
