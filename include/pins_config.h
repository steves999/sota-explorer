#pragma once
// SOTA Field Instrument — Pin definitions
// LILYGO T-Display-S3 AMOLED (Touch), confirmed against official LILYGO
// pinout diagram (provided by Steve, June 2026) — NOT inferred from
// third-party blog posts or other board variants.
//
// Two separate, non-conflicting interfaces are in use. Touch controller
// (GPIO21) is NOT used by this firmware and is left alone.

// ---- I2C bus (BME280) ----
// Diagram: left header, labelled directly "I2C-SCL" / "I2C-SDA"
#define PIN_I2C_SCL   2
#define PIN_I2C_SDA   3

// ---- UART1 (ATGM336H GPS) ----
// Diagram: right header, labelled "U0TXD"/"U0RXD" at GPIO43/44, also
// broken out to the bottom-right JST-SH connector marked "TX:IO43 RX:IO44".
// Board TX (GPIO43) -> GPS RX
// Board RX (GPIO44) <- GPS TX
#define PIN_GPS_TX    43   // board transmits on this pin -> wire to GPS RX
#define PIN_GPS_RX    44   // board receives on this pin  <- wire to GPS TX

// ATGM336H default baud rate. CONFIRM ON BENCH — some ship at 9600,
// some at 38400. If no valid NMEA sentences appear at 9600, try 38400
// before assuming a wiring fault.
#define GPS_BAUD_DEFAULT   9600

// ---- BME280 ----
// I2C address varies by breakout batch — 0x76 is the GY-BME/P280 norm,
// 0x77 is the other common strap. Run an I2C scan on first boot rather
// than hardcoding; see setup() in main.cpp.
#define BME280_ADDR_PRIMARY   0x76
#define BME280_ADDR_SECONDARY 0x77

// ---- Display backlight / power (battery mode) ----
// Per official LILYGO guidance for this board family: GPIO15 must be
// driven HIGH for backlight when running on battery. Not used in
// Milestone 1 (no display code yet) — included here for Milestone 2.
#define PIN_DISPLAY_BACKLIGHT  15
