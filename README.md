# SOTA Explorer — ZL1SGS
### A field instrument for SOTA, POTA, WWFF and HEMA activations

**Hardware:** LilyGo T-Display-S3 AMOLED Touch (ESP32-S3)  
**Callsign:** ZL1SGS — Steve Gorton, Auckland, New Zealand  
**Version:** v31

---

## What it does

A self-contained GPS/sensor field instrument that shows you:
- Nearest SOTA summits or WWFF/POTA parks, ranked by distance
- Distance, bearing, compass direction and altitude to each
- Whether you're in the activation zone (AZ) for the nearest summit
- Real-time GPS fix quality, grid square, satellite count
- Barometric pressure, temperature and humidity with 30-minute trend graphs
- Pressure-derived altitude with adjustable QNH
- UTC and local time (NZ/VK timezone aware, DST correct)
- Sunrise and sunset times calculated from GPS position
- Battery percentage

All data is stored on-device — no phone, no internet connection required in the field.

---

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | LilyGo T-Display-S3 AMOLED Touch |
| Processor | ESP32-S3 @ 240MHz |
| Display | RM67162 1.91" AMOLED touch, 536×240 |
| Flash | 16MB |
| GPS | ATGM336H — GPIO44 RX / GPIO43 TX @ 9600 baud |
| Barometric sensor | GY-BME280 — GPIO2 SCL / GPIO3 SDA, I2C addr 0x76 |
| Battery | 3.7V 3000mAh LiPo (BAT JST connector) |
| Case | Custom 3-part 3D printed PLA+ |

### Wiring

**GPS (ATGM336H):**
| T-Display pin | GPS pin |
|---------------|---------|
| GPIO43 (TX) | RX |
| GPIO44 (RX) | TX |
| 3.3V | VCC |
| GND | GND |

Note: TX/RX cross-over — board TX goes to GPS RX, board RX from GPS TX.

**BME280 (GY-BME280, I2C):**
| T-Display pin | BME280 pin |
|---------------|------------|
| GPIO2 (SCL) | SCL |
| GPIO3 (SDA) | SDA |
| 3.3V | VDD |
| GND | GND |

### BME280 notes
Module may return chip ID 0xFF rather than genuine Bosch 0x60. Pressure works
correctly after soft-reset and 300ms settle at boot. readPressureHpa() in
main.cpp reconfigures the chip and polls the status register before every read
to handle spontaneous resets from power glitches. Readings in the 700-800 range
are detected as mmHg and converted (x1.33322 to hPa).

PRESSURE_MIN_HPA = 870.0F — lower to ~700 for SOTA summits above 1300m
(e.g. ZL3/CB-001 Aoraki/Mt Cook at 2797m gives ~730 hPa).

### GPS antenna notes
The ATGM336H supports both active and passive antennas and has built-in
antenna detection. ANTENNA SHORT, ANTENNA OPEN and ANTENNA OK messages appear
in the NMEA stream via $GPTXT sentences. The IPEX connector on the GPS module
is fragile — avoid stressing it during assembly. Kapton tape over the backup
battery cell prevents accidental shorts from the IPEX connector rotating.
GPS ceramic patch antenna must face skyward (away from battery).

### Battery management
The LilyGo board includes a PMIC with low-voltage cutoff protecting the LiPo
from deep discharge damage. Safe to leave in deep sleep for extended periods.
Deep sleep current draw: ~26mA (GPS + BME280 remain powered from 3.3V rail).
3000mAh battery gives approximately 115 hours deep sleep, 15-20 hours active.

---

## Page Layout

```
Col:    0        1          2            3              4            5
Row 0: Home -> GPS Det -> Summit/Park -> BME Sensor -> Settings 1 -> Settings 2
                              |               |
Row 1:                   Temp/Humid -> Press Graph -> Altimeter
```

### Navigation
- Row 0: swipe left/right through all 6 pages
- Row 1: swipe down from Summit/Park List (2,0) or BME Sensor (3,0)
- Within row 1: Temp/Humid <-> Pressure Graph <-> Altimeter
- Settings: Settings 1 swipe right -> Settings 2, swipe left to return

### Home page (0,0)
- SUMMITS/PARKS pill top-left, large touch zone 268x120px
  - Tap: switch between SOTA/HEMA summits and WWFF/POTA parks mode
  - Long-press: deep sleep (press RST button to wake)
- Fix quality pill: green (HDOP<1.5, sats>=8) / amber / red NO FIX
- Gear icon top-right -> Settings 1
- Summit/park code (28px), name (18px), distance/bearing/altitude
- AZ pill: IN AZ / AZ? / OUT (hidden in park mode)
- Proximity alert: green flash overlay when within threshold distance
- UTC HH:MM:SS left / Local time right (both 22px)
- Long-press home tile body: cycles brightness full/medium/dim

### GPS Detail (1,0)
- Lat/lon, altitude, grid square, sats, HDOP
- Sunrise/sunset in local time (NOAA algorithm from GPS position)

### Summit/Park List (2,0)
- 5 nearest ranked by distance with bearing
- Title updates: NEAREST SUMMITS or NEAREST PARKS with mode
- Swipe down -> Temp/Humid trend page

### BME Sensor Detail (3,0)
- Temp, humidity, pressure + trend arrow + rate (hPa/hr)
- Pressure altitude vs GPS altitude comparison
- Battery percentage
- Swipe down -> Pressure trend graph

### Temp + Humidity Trend (2,1)
- Temperature (orange line) top half, humidity (blue line) bottom half
- 30-minute rolling history, auto-scaled to actual range
- 6 samples x 5 minute intervals
- -30min / now axis labels

### Pressure Trend Graph (3,1)
- 30-minute rolling pressure history, auto-scaled line chart
- Current pressure (28px) + trend rate (hPa/hr)
- -30min / now axis labels

### Altimeter (4,1)
- Large pressure altitude (48px font)
- GPS altitude shown for comparison
- QNH adjustment: tap +/- 1 hPa, long-press +/- 5 hPa/step repeating
- QNH persisted to NVS across reboots

### Settings 1 (4,0)
- Full-width brightness slider (15% minimum to prevent screen blackout)
- Auto-dim toggle + delay selection (off/2min/5min/10min)
- Swipe right -> Settings 2

### Settings 2 (5,0)
- Proximity alert toggle + distance threshold (200m/500m/1km/2km)
- Rescan distance threshold (0.25/0.5/1/2km)
- Timezone (NZ / VK1-VK8, all DST aware)

---

## Source Files

| File | Purpose |
|------|---------|
| src/main.cpp | Setup, loop, GPS/BME280 read cycles, all callbacks |
| include/gui.h | All LVGL UI: pages, widgets, guiInit(), guiUpdate() |
| include/summit_match.h | LittleFS CSV streaming, nearest-N finder |
| include/settings.h | AppSettings struct, NVS load/save via Preferences |
| include/timezone.h | NZ + VK DST rules, toLocalTime(), tzAbbrev() |
| include/sunrise.h | NOAA solar algorithm, calcSunTimes() |
| include/bearing.h | calcBearing(), compass8() |
| include/haversine.h | haversineKm() |
| include/maidenhead.h | latLonToGrid() |
| include/pins_config.h | Hardware pin definitions |

---

## Data Files (LittleFS)

Stored in data/ folder, flashed separately with uploadfs target.

| File | Content | Approx size |
|------|---------|-------------|
| summits.csv | SOTA + HEMA ZL/VK (~15,500 rows) | ~835 KB |
| parks.csv | WWFF/VKFF/ZLFF + POTA AU/NZ (~18,700 rows) | ~1.2 MB |

CSV format: SummitCode, SummitName, Latitude, Longitude, AltM, Type
Type values: SOTA, HEMA, WWFF, POTA

Generate with:
    python filter_all_programmes.py

---

## NVS Settings (namespace "sota")

| Key | Type | Default | Notes |
|-----|------|---------|-------|
| proxAlert | bool | true | Proximity alert on/off |
| proxThresh | int | 1 | Index: {200,500,1000,2000}m |
| rescanThresh | int | 1 | Index: {25,50,100,200}x0.01km |
| autoDim | bool | false | Auto-dim on/off |
| dimDelay | int | 1 | Index: {0,120,300,600}s |
| timezone | int | 0 | Index into TZ_OPTIONS (NZ=0) |
| qnh | float | 1013.25 | QNH hPa for altimeter |
| brightness2 | int | 255 | Raw 0-255 (key "brightness2" avoids clash with old index key) |

---

## Build and Flash

```bash
# First time or after CSV data changes:
pio run --target uploadfs
pio run --target upload --target monitor

# Firmware only (most builds):
pio run --target upload --target monitor

# Monitor only without reflashing:
pio device monitor

# Filter to GPS antenna status only (PowerShell):
pio device monitor | Select-String "ANTENNA"

# Force bootloader if stuck in crash loop:
# Hold BOOT button, tap RESET, release BOOT, then:
pio run --target upload
```

---

## Serial Monitor Reference

Key output lines:

```
BME280 chip ID: 0x60           genuine Bosch (0xFF = clone, may still work)
BME280 OK at 0x76              sensor initialised successfully
[BME280] mmHg detected...      unit conversion applied (normal for this module)
[BME280] temp=xx pressure=xx   sensor reading every 2 seconds
[BAT] raw ADC=xxxx pct=xx%     battery level
ANTENNA OK / SHORT / OPEN      GPS antenna connection status
FIX: lat=xx lon=xx sats=x      GPS fix details (in status block every 10s)
[Summit scan] took xxxx ms     nearest summit search completed
```

NMEA sentence quick reference:
- $GPGSV: satellites in view. Field 3 = total count, then groups of (PRN, elevation, azimuth, signal strength). Signal strength blank = no signal from that satellite.
- $GNRMC: A = active fix, V = void/no fix
- $GNGGA: position, altitude, satellites in use, HDOP
- $GNGSA: fix type 1=no fix 2=2D 3=3D, plus DOP values
- $GPTXT: antenna status messages

---

## Known Issues / To Do

- [ ] BME280 self-resets intermittently under power glitches — 100uF cap on VDD/GND may help
- [ ] Battery % formula is empirical and needs calibration against known voltages
- [ ] HEMA summits not yet included in summits.csv — scraper script available (scrape_hema.py)
- [ ] PRESSURE_MIN_HPA needs lowering to ~700 for high-altitude SOTA activations above 1300m
- [ ] GPS antenna IPEX connection is fragile — handle carefully during case assembly

---

## Case Design

3-part 3D printed PLA+ design:
- LilyGo display housing based on Jeepers model #888766 on Printables
- Custom battery box base (LiPo tray)
- Custom battery box lid with display housing mount hole
- RST and BOOT buttons accessible via recessed labelled holes in case wall
- USB-C port accessible for charging and programming
- GPS antenna window: thin PLA section over ceramic patch, antenna faces skyward
- BME280 vent holes in end wall for temperature/humidity/pressure airflow
- Labelled ZL1SGS / SOTA Explorer on lid

---

## Activation Programmes Supported

| Programme | Region | Data file |
|-----------|--------|-----------|
| SOTA | ZL + VK | summits.csv |
| HEMA | ZL + VK | summits.csv |
| WWFF / ZLFF / VKFF | ZL + VK | parks.csv |
| POTA | ZL + VK | parks.csv |

---

## Author

Steve Gorton — ZL1SGS  
Auckland, New Zealand  
Retiring November 2026  
Australia motorcycle circumnavigation planned February 2027

73 de ZL1SGS
