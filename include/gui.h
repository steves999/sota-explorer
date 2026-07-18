#pragma once
// Milestone 4 UI: swipeable 3-page tileview across the 536x240 AMOLED
// (confirmed resolution via LilyGo's own published spec).
//
//   Page 0 (home, always shown on boot):
//     [ GPS strip: grid/sats/HDOP ......... fix-quality pill ]
//     [ Summit code/name/alt/dist (large) ...... AZ status pill ]
//     [ temp ........... pressure+trend ........... UTC time ]
//
//   Page 1 (swipe right from home): GPS detail -- lat/lon/altitude
//   Page 2 (swipe right again): 5 nearest summits, ranked by distance
//
// Navigation: touch swipe only, via lv_tileview (per Steve's choice).
// Home page always shown first on boot -- tileview defaults to tile
// (0,0) on creation, and we also explicitly set it in guiInit() to be
// sure regardless of creation-order quirks.
//
// NOTE -- unverified at time of writing: LV_USE_TILEVIEW must be
// enabled in lv_conf.h for this to compile. Steve opted to find out
// via compile error rather than pre-check the file, which is a
// reasonable call since the failure mode is a clear compiler error,
// not a silent runtime issue.
//
// LVGL objects are created ONCE in guiInit() and only their text/color
// are updated afterwards in guiUpdate() -- rebuilding the tree every
// cycle would be wasteful and is not how LVGL is meant to be driven.

#include <lvgl.h>
#include "maidenhead.h"
#include "bearing.h"
#include "summit_match.h"
#include "timezone.h"
#include "sunrise.h"
#include "settings.h"

// Long-press brightness cycle: quick field shortcut cycling dim/medium/full.
// Also updates the slider on the settings page if it's visible.
static const uint8_t BRIGHTNESS_LEVELS[] = {255, 128, 38};
static const int     BRIGHTNESS_COUNT    = 3;
static int           g_brightCycleIdx    = 0;

// Callback registered by main.cpp for brightness control
static void (*g_setBrightnessCallback)(uint8_t) = nullptr;

#define COLOR_BG        0x0A0A0A
#define COLOR_GOOD_BG   0x1D9E75
#define COLOR_GOOD_TEXT 0x04342C
#define COLOR_WARN_BG   0xBA7517
#define COLOR_WARN_TEXT 0x412402
#define COLOR_BAD_BG    0x993C1D
#define COLOR_BAD_TEXT  0xFAECE7
#define COLOR_TEXT_MAIN 0xFFFFFF
#define COLOR_TEXT_SUB  0xCCCCCC
#define COLOR_TEXT_DIM  0x888888
#define COLOR_TEXT_FAINT 0x666666
#define COLOR_LINE      0x2A2A2A

enum FixQuality { FIX_GOOD, FIX_OK, FIX_BAD };

inline FixQuality classifyFixQuality(bool fixValid, int sats, float hdop) {
  if (!fixValid) return FIX_BAD;
  if (hdop < 1.5F && sats >= 8) return FIX_GOOD;
  if (hdop < 3.0F) return FIX_OK;
  return FIX_BAD;
}

inline void setPill(lv_obj_t *pill, lv_obj_t *pillLabel, const char *text,
                     uint32_t bgColor, uint32_t textColor) {
  lv_obj_set_style_bg_color(pill, lv_color_hex(bgColor), LV_PART_MAIN);
  lv_label_set_text(pillLabel, text);
  lv_obj_set_style_text_color(pillLabel, lv_color_hex(textColor), LV_PART_MAIN);
}

// ===== Page 0: home =====
static lv_obj_t *gpsInfoLabel;
static lv_obj_t *fixPill;
static lv_obj_t *fixPillLabel;
static lv_obj_t *modeBtn;       // tappable mode toggle in top strip
static lv_obj_t *modeBtnLabel;  // shows "SUMMITS" or "PARKS"
static lv_obj_t *summitCodeLabel;
static lv_obj_t *summitNameLabel;
static lv_obj_t *summitDetailLabel;
static lv_obj_t *azPill;
static lv_obj_t *azPillLabel;
static lv_obj_t *tempLabel;
static lv_obj_t *pressureLabel;
static lv_obj_t *timeLabel;

// Callback registered by main.cpp at init time -- called when the
// mode toggle button is tapped. Kept as a static so the LVGL event
// handler (which must be a free function, not a lambda) can reach it.
static void (*g_switchModeCallback)() = nullptr;

// ===== Page 1: GPS detail =====
static lv_obj_t *detailLatLabel;
static lv_obj_t *detailLonLabel;
static lv_obj_t *detailAltLabel;
static lv_obj_t *detailGridLabel;
static lv_obj_t *detailSatsLabel;
static lv_obj_t *g_sunriseLabel;
static lv_obj_t *g_sunsetLabel;

// ===== Page 2: nearest summits/parks list =====
static lv_obj_t *listPageTitle;
static lv_obj_t *summitListLabels[SUMMIT_TOP_N];

// ===== Page 3: BME280 sensor detail =====
static lv_obj_t *bmeDetailTempLabel;
static lv_obj_t *bmeDetailHumidityLabel;
static lv_obj_t *bmeDetailPressureLabel;
static lv_obj_t *bmeDetailTrendLabel;
static lv_obj_t *bmeDetailPressAltLabel;
static lv_obj_t *bmeDetailBattLabel;

// ===== Row 1, tile (2,1): Temperature + humidity trend =====
static lv_obj_t *graphTempLabel;
static lv_obj_t *graphHumidLabel;
static lv_obj_t *graphTempLine;
static lv_obj_t *graphHumidLine;
static lv_point_t graphTempPoints[6];
static lv_point_t graphHumidPoints[6];

// ===== Row 1, tile (3,1): Pressure trend graph =====
static lv_obj_t *graphPressLabel;
static lv_obj_t *graphRateLabel;
static lv_obj_t *graphLine;
static lv_point_t graphPoints[6];

// ===== Row 1, tile (1,1): Altimeter =====
static lv_obj_t *altAltLabel;        // large pressure altitude
static lv_obj_t *altGpsLabel;        // GPS altitude for comparison
static lv_obj_t *altQnhLabel;        // current QNH value
static lv_obj_t *altPlusBtn;
static lv_obj_t *altMinusBtn;
static lv_obj_t *altPlusBtnLabel;
static lv_obj_t *altMinusBtnLabel;

// ===== Page 4: Settings page 1 (brightness + auto-dim) =====
static lv_obj_t *settingDimSwitch;
static lv_obj_t *settingDimDelayLabel;
static lv_obj_t *settingBrightnessLabel;
static lv_obj_t *settingBrightnessSlider;  // global so lambda can reference it safely

// ===== Page 4b: Settings page 2 (proximity + rescan + timezone) =====
static lv_obj_t *settingProxSwitch;
static lv_obj_t *settingProxThreshLabel;
static lv_obj_t *settingRescanLabel;
static lv_obj_t *settingTimezoneLabel;

// Callback: called when a setting changes, passes updated AppSettings to main.cpp
static void (*g_settingsChangedCallback)(const AppSettings &) = nullptr;

// Gear button in home top strip
static lv_obj_t *gearBtn;
static lv_obj_t *gearBtnLabel;

// LVGL event handler for the mode toggle button -- must be a plain
// function (not a lambda) to satisfy lv_obj_add_event_cb's signature.
// Local copy of settings -- updated by settings page, saved to NVS via callback
static AppSettings g_settings = defaultSettings();

// Forward declare so event handlers can reference tileview
static lv_obj_t *tileview;

// Navigate to settings page when gear button tapped.
// This build (lvgl@8.4.0) has NO tileview setter -- confirmed by reading
// the actual lv_tileview.h: only lv_tileview_create, lv_tileview_add_tile,
// and lv_tileview_get_tile_act exist. Navigation is done by scrolling the
// tileview to the tile's position. Each tile is 536px wide (the screen width),
// so tile N is at x = N * 536.
#define TILEVIEW_TILE_W 536
#define SETTINGS_TILE_IDX 4

static void gearBtnCb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_scroll_to_x(tileview, SETTINGS_TILE_IDX * TILEVIEW_TILE_W, LV_ANIM_ON);
  }
}

// Settings event handler -- called when any setting widget is interacted with
static void settingsCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_VALUE_CHANGED) return;

  lv_obj_t *obj = lv_event_get_target(e);

  if (obj == settingProxSwitch) {
    g_settings.proximityAlert = lv_obj_has_state(settingProxSwitch, LV_STATE_CHECKED);
  } else if (obj == settingProxThreshLabel) {
    g_settings.proximityThreshIdx =
      (g_settings.proximityThreshIdx + 1) % PROX_THRESHOLD_COUNT;
    char buf[20];
    snprintf(buf, sizeof(buf), "%dm", PROX_THRESHOLDS[g_settings.proximityThreshIdx]);
    lv_label_set_text(settingProxThreshLabel, buf);
  } else if (obj == settingRescanLabel) {
    g_settings.rescanThreshIdx =
      (g_settings.rescanThreshIdx + 1) % RESCAN_THRESHOLD_COUNT;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2fkm",
             RESCAN_THRESHOLDS_CM[g_settings.rescanThreshIdx] / 100.0F);
    lv_label_set_text(settingRescanLabel, buf);
  } else if (obj == settingDimSwitch) {
    g_settings.autoDim = lv_obj_has_state(settingDimSwitch, LV_STATE_CHECKED);
  } else if (obj == settingDimDelayLabel) {
    g_settings.dimDelayIdx = (g_settings.dimDelayIdx + 1) % DIM_DELAY_COUNT;
    char buf[12];
    int secs = DIM_DELAYS[g_settings.dimDelayIdx];
    if (secs == 0) snprintf(buf, sizeof(buf), "off");
    else snprintf(buf, sizeof(buf), "%dmin", secs / 60);
    lv_label_set_text(settingDimDelayLabel, buf);
  } else if (obj == settingTimezoneLabel) {
    g_settings.timezoneIdx = (g_settings.timezoneIdx + 1) % TZ_OPTION_COUNT;
    lv_label_set_text(settingTimezoneLabel,
                      TZ_OPTIONS[g_settings.timezoneIdx].label);
  }

  if (g_settingsChangedCallback) {
    g_settingsChangedCallback(g_settings);
  }
}

// Long-press event handler for brightness cycling
static void homeLongPressCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_LONG_PRESSED && g_setBrightnessCallback) {
    g_brightCycleIdx = (g_brightCycleIdx + 1) % BRIGHTNESS_COUNT;
    g_setBrightnessCallback(BRIGHTNESS_LEVELS[g_brightCycleIdx]);
  }
}

// Proximity alert overlay -- a full-screen flash shown briefly when
// within PROXIMITY_ALERT_M of the nearest summit/park.
#define PROXIMITY_ALERT_M 500.0F
static lv_obj_t *proximityAlert      = nullptr;
static uint32_t  proximityAlertUntil = 0;  // millis() timestamp to hide it

static void (*g_sleepCallback)() = nullptr;

static void modeBtnEventCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED && g_switchModeCallback) {
    g_switchModeCallback();
    // Update button label immediately so user sees feedback before the
    // scan completes (scan can take 10-15s in park mode -- without this
    // the button appears to do nothing until the scan finishes)
    bool nowPark = (lv_label_get_text(modeBtnLabel) &&
                    strcmp(lv_label_get_text(modeBtnLabel), "SUMMITS") == 0);
    lv_label_set_text(modeBtnLabel, nowPark ? "PARKS" : "SUMMITS");
    lv_obj_set_style_text_color(modeBtnLabel,
      lv_color_hex(nowPark ? 0xBA7517 : COLOR_GOOD_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_color(modeBtn,
      lv_color_hex(nowPark ? 0x3A2A00 : 0x2A3A2A), LV_PART_MAIN);
    // Clear stale summit display immediately
    lv_label_set_text(summitCodeLabel, "--");
    lv_label_set_text(summitNameLabel, nowPark ? "Scanning parks..." : "Scanning summits...");
    lv_label_set_text(summitDetailLabel, "");
    setPill(azPill, azPillLabel, "--", COLOR_LINE, COLOR_TEXT_DIM);
  } else if (code == LV_EVENT_LONG_PRESSED && g_sleepCallback) {
    g_sleepCallback();
  }
}

inline void buildHomePage(lv_obj_t *tile) {
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_pad_all(tile, 8, LV_PART_MAIN);

  lv_obj_t *topRow = lv_obj_create(tile);
  lv_obj_remove_style_all(topRow);
  lv_obj_set_size(topRow, 520, 28);
  lv_obj_align(topRow, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_border_side(topRow, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
  lv_obj_set_style_border_color(topRow, lv_color_hex(COLOR_LINE), LV_PART_MAIN);
  lv_obj_set_style_border_width(topRow, 1, LV_PART_MAIN);

  // Mode toggle button -- leftmost in the top strip, tappable.
  // Uses lv_btn (which is clickable by default) rather than lv_label
  // since labels need LV_OBJ_FLAG_CLICKABLE added manually.
  modeBtn = lv_btn_create(topRow);
  lv_obj_remove_style_all(modeBtn);
  lv_obj_set_size(modeBtn, 72, 20);
  lv_obj_set_style_bg_color(modeBtn, lv_color_hex(0x2A3A2A), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(modeBtn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(modeBtn, 6, LV_PART_MAIN);
  lv_obj_align(modeBtn, LV_ALIGN_LEFT_MID, 0, -4);
  lv_obj_add_event_cb(modeBtn, modeBtnEventCb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(modeBtn, modeBtnEventCb, LV_EVENT_LONG_PRESSED, NULL);
  // Expand touch area well beyond visible pill -- top-left corner of screen
  lv_obj_set_ext_click_area(modeBtn, 30);
  modeBtnLabel = lv_label_create(modeBtn);
  lv_obj_set_style_text_color(modeBtnLabel, lv_color_hex(COLOR_GOOD_BG), LV_PART_MAIN);
  lv_obj_set_style_text_font(modeBtnLabel, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_label_set_text(modeBtnLabel, "SUMMITS");
  lv_obj_center(modeBtnLabel);

  gpsInfoLabel = lv_label_create(topRow);
  lv_obj_set_style_text_color(gpsInfoLabel, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_align(gpsInfoLabel, LV_ALIGN_LEFT_MID, 78, -4);  // after mode button

  fixPill = lv_obj_create(topRow);
  lv_obj_remove_style_all(fixPill);
  lv_obj_set_size(fixPill, 80, 18);
  lv_obj_set_style_radius(fixPill, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(fixPill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(fixPill, LV_ALIGN_RIGHT_MID, -86, -4);
  fixPillLabel = lv_label_create(fixPill);
  lv_obj_set_style_text_font(fixPillLabel, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_center(fixPillLabel);

  // Gear button -- navigates to settings page (tile 4,0)
  // Stored user_data is set in guiInit() once the settings tile exists
  gearBtn = lv_btn_create(topRow);
  lv_obj_remove_style_all(gearBtn);
  lv_obj_set_size(gearBtn, 28, 20);
  lv_obj_set_style_bg_color(gearBtn, lv_color_hex(0x333333), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(gearBtn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(gearBtn, 4, LV_PART_MAIN);
  lv_obj_align(gearBtn, LV_ALIGN_RIGHT_MID, 0, -4);
  gearBtnLabel = lv_label_create(gearBtn);
  lv_obj_set_style_text_color(gearBtnLabel, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(gearBtnLabel, LV_SYMBOL_SETTINGS);
  lv_obj_center(gearBtnLabel);

  lv_obj_t *midRow = lv_obj_create(tile);
  lv_obj_remove_style_all(midRow);
  lv_obj_set_size(midRow, 520, 160);
  lv_obj_align(midRow, LV_ALIGN_CENTER, 0, 2);

  lv_obj_t *summitTextCol = lv_obj_create(midRow);
  lv_obj_remove_style_all(summitTextCol);
  lv_obj_set_size(summitTextCol, 380, 160);
  lv_obj_align(summitTextCol, LV_ALIGN_LEFT_MID, 0, 0);

  summitCodeLabel = lv_label_create(summitTextCol);
  lv_obj_set_style_text_color(summitCodeLabel, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_MAIN);
  lv_obj_set_style_text_font(summitCodeLabel, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_align(summitCodeLabel, LV_ALIGN_TOP_LEFT, 0, 10);

  summitNameLabel = lv_label_create(summitTextCol);
  lv_obj_set_style_text_color(summitNameLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_set_style_text_font(summitNameLabel, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_width(summitNameLabel, 380);
  lv_label_set_long_mode(summitNameLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(summitNameLabel, LV_ALIGN_TOP_LEFT, 0, 50);

  summitDetailLabel = lv_label_create(summitTextCol);
  lv_obj_set_style_text_color(summitDetailLabel, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_set_style_text_font(summitDetailLabel, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(summitDetailLabel, LV_ALIGN_TOP_LEFT, 0, 90);

  azPill = lv_obj_create(midRow);
  lv_obj_remove_style_all(azPill);
  lv_obj_set_size(azPill, 90, 36);
  lv_obj_set_style_radius(azPill, 10, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(azPill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(azPill, LV_ALIGN_RIGHT_MID, 0, 0);
  azPillLabel = lv_label_create(azPill);
  lv_obj_set_style_text_font(azPillLabel, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_center(azPillLabel);

  // Bottom strip: UTC time (left) and local time with zone abbrev (right).
  // BME280 data removed from home page -- it has its own dedicated page now.
  lv_obj_t *botRow = lv_obj_create(tile);
  lv_obj_remove_style_all(botRow);
  lv_obj_set_size(botRow, 520, 44);
  lv_obj_align(botRow, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_border_side(botRow, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
  lv_obj_set_style_border_color(botRow, lv_color_hex(COLOR_LINE), LV_PART_MAIN);
  lv_obj_set_style_border_width(botRow, 1, LV_PART_MAIN);

  tempLabel = lv_label_create(botRow);  // UTC time
  lv_obj_set_style_text_color(tempLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_set_style_text_font(tempLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_obj_align(tempLabel, LV_ALIGN_LEFT_MID, 0, 0);

  pressureLabel = lv_label_create(botRow);  // local time
  lv_obj_set_style_text_color(pressureLabel, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_MAIN);
  lv_obj_set_style_text_font(pressureLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_obj_align(pressureLabel, LV_ALIGN_RIGHT_MID, 0, 0);

  // timeLabel no longer used on home page -- kept as NULL to avoid
  // dangling pointer; guiUpdate checks before setting text.
  timeLabel = NULL;

  lv_label_set_text(gpsInfoLabel, "No fix");
  setPill(fixPill, fixPillLabel, "NO FIX", COLOR_BAD_BG, COLOR_BAD_TEXT);
  lv_label_set_text(summitCodeLabel, "--");
  lv_label_set_text(summitNameLabel, "Waiting for fix...");
  lv_label_set_text(summitDetailLabel, "");
  setPill(azPill, azPillLabel, "--", COLOR_LINE, COLOR_TEXT_DIM);
  lv_label_set_text(tempLabel, "UTC --:--:--");
  lv_label_set_text(pressureLabel, "LOCAL --:--:--");

  // Long-press anywhere on the home tile cycles brightness
  lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(tile, homeLongPressCb, LV_EVENT_LONG_PRESSED, NULL);

  // Proximity alert: full-screen semi-transparent green overlay, hidden by default
  proximityAlert = lv_obj_create(tile);
  lv_obj_set_size(proximityAlert, 536, 240);
  lv_obj_align(proximityAlert, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(proximityAlert, lv_color_hex(COLOR_GOOD_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(proximityAlert, LV_OPA_50, LV_PART_MAIN);
  lv_obj_set_style_border_width(proximityAlert, 0, LV_PART_MAIN);
  lv_obj_t *alertLabel = lv_label_create(proximityAlert);
  lv_obj_set_style_text_color(alertLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(alertLabel, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_label_set_text(alertLabel, "NEARBY!");
  lv_obj_center(alertLabel);
  lv_obj_add_flag(proximityAlert, LV_OBJ_FLAG_HIDDEN);

  // Large invisible touch zone: top-left 268x120px of the screen.
  // Extends mode toggle tap/long-press target well beyond the visible pill.
  // Created last so it sits on top of other widgets in the event chain.
  lv_obj_t *modeTouchZone = lv_btn_create(tile);
  lv_obj_remove_style_all(modeTouchZone);
  lv_obj_set_size(modeTouchZone, 268, 120);
  lv_obj_set_style_bg_opa(modeTouchZone, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_align(modeTouchZone, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_event_cb(modeTouchZone, modeBtnEventCb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(modeTouchZone, modeBtnEventCb, LV_EVENT_LONG_PRESSED, NULL);
}

inline void buildDetailPage(lv_obj_t *tile) {
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_pad_all(tile, 16, LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(tile);
  lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(title, "GPS DETAIL");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  detailGridLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(detailGridLabel, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_MAIN);
  lv_obj_align(detailGridLabel, LV_ALIGN_TOP_LEFT, 0, 30);

  detailLatLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(detailLatLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_align(detailLatLabel, LV_ALIGN_TOP_LEFT, 0, 70);

  detailLonLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(detailLonLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_align(detailLonLabel, LV_ALIGN_TOP_LEFT, 0, 100);

  detailAltLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(detailAltLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_align(detailAltLabel, LV_ALIGN_TOP_LEFT, 0, 130);

  detailSatsLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(detailSatsLabel, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_align(detailSatsLabel, LV_ALIGN_TOP_LEFT, 0, 170);

  // Sunrise/sunset on the GPS detail page -- computed from GPS position
  lv_obj_t *sunTitle = lv_label_create(tile);
  lv_obj_set_style_text_color(sunTitle, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(sunTitle, "SUN");
  lv_obj_align(sunTitle, LV_ALIGN_TOP_RIGHT, -80, 0);

  // Reuse detailSatsLabel area -- put sun times on right column
  // sunrise label
  static lv_obj_t *sunriseLabel;
  sunriseLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(sunriseLabel, lv_color_hex(0xFFCC44), LV_PART_MAIN);
  lv_obj_align(sunriseLabel, LV_ALIGN_TOP_RIGHT, 0, 30);

  static lv_obj_t *sunsetLabel;
  sunsetLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(sunsetLabel, lv_color_hex(0xFF7722), LV_PART_MAIN);
  lv_obj_align(sunsetLabel, LV_ALIGN_TOP_RIGHT, 0, 60);

  lv_label_set_text(sunriseLabel, "Rise: --:--");
  lv_label_set_text(sunsetLabel,  "Set:  --:--");

  // Store for guiUpdate access
  g_sunriseLabel = sunriseLabel;
  g_sunsetLabel  = sunsetLabel;

  lv_label_set_text(detailGridLabel, "Grid: --");
  lv_label_set_text(detailLatLabel, "Lat: --");
  lv_label_set_text(detailLonLabel, "Lon: --");
  lv_label_set_text(detailAltLabel, "Alt: --");
  lv_label_set_text(detailSatsLabel, "--");
}

inline void buildSummitListPage(lv_obj_t *tile) {
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_pad_all(tile, 12, LV_PART_MAIN);

  listPageTitle = lv_label_create(tile);
  lv_obj_set_style_text_color(listPageTitle, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(listPageTitle, "NEAREST SUMMITS");
  lv_obj_align(listPageTitle, LV_ALIGN_TOP_LEFT, 0, 0);

  // Each row: one label spanning the width, fixed-height rows since
  // the list only ever holds SUMMIT_TOP_N=5 short lines.
  int rowHeight = 36;
  int startY = 26;
  for (int i = 0; i < SUMMIT_TOP_N; i++) {
    summitListLabels[i] = lv_label_create(tile);
    lv_obj_set_style_text_color(summitListLabels[i], lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
    lv_obj_set_width(summitListLabels[i], 512);
    lv_obj_align(summitListLabels[i], LV_ALIGN_TOP_LEFT, 0, startY + i * rowHeight);
    lv_label_set_text(summitListLabels[i], "--");
  }
}

inline void buildBmePage(lv_obj_t *tile) {
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_pad_all(tile, 16, LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(tile);
  lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(title, "SENSOR DETAIL");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  bmeDetailTempLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(bmeDetailTempLabel, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_MAIN);
  lv_obj_align(bmeDetailTempLabel, LV_ALIGN_TOP_LEFT, 0, 30);

  bmeDetailHumidityLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(bmeDetailHumidityLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_align(bmeDetailHumidityLabel, LV_ALIGN_TOP_LEFT, 0, 70);

  bmeDetailPressureLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(bmeDetailPressureLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_align(bmeDetailPressureLabel, LV_ALIGN_TOP_LEFT, 0, 100);

  bmeDetailTrendLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(bmeDetailTrendLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_align(bmeDetailTrendLabel, LV_ALIGN_TOP_LEFT, 0, 130);

  bmeDetailPressAltLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(bmeDetailPressAltLabel, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_align(bmeDetailPressAltLabel, LV_ALIGN_TOP_LEFT, 0, 170);

  // Battery voltage -- GPIO4 ADC, empirical formula, unverified for 1.91" board
  // displayed with caveat; Steve to verify against multimeter on first field use
  bmeDetailBattLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(bmeDetailBattLabel, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_align(bmeDetailBattLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lv_label_set_text(bmeDetailTempLabel, "Temp: --");
  lv_label_set_text(bmeDetailHumidityLabel, "Humidity: --");
  lv_label_set_text(bmeDetailPressureLabel, "Pressure: --");
  lv_label_set_text(bmeDetailTrendLabel, "Trend: --");
  lv_label_set_text(bmeDetailPressAltLabel, "Press.Alt: -- (ISA QNH)");
  lv_label_set_text(bmeDetailBattLabel, "Battery: --");
}

// Settings page 1 (tile 4,0): Brightness + Auto-dim
// Settings page 2 (tile 4,1): Proximity alert + Rescan + Timezone
// Both are full-screen, no scrolling needed.

// Brightness slider event handler -- must be a plain function, not a lambda,
// for reliable operation on Xtensa/ESP32 toolchain.
static void brightnessSlidercb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  int val = lv_slider_get_value(lv_event_get_target(e));
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", val * 100 / 255);
  lv_label_set_text(settingBrightnessLabel, buf);
  if (g_setBrightnessCallback) g_setBrightnessCallback((uint8_t)val);
  g_settings.brightness = (uint8_t)val;
  if (g_settingsChangedCallback) g_settingsChangedCallback(g_settings);
}

inline void buildSettingsPage1(lv_obj_t *tile) {
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_pad_all(tile, 0, LV_PART_MAIN);

  // Title
  lv_obj_t *title = lv_label_create(tile);
  lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_label_set_text(title, LV_SYMBOL_SETTINGS "  SETTINGS  1/2");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  // -- Brightness: label + percentage + full-width slider --
  lv_obj_t *bLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(bLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_set_style_text_font(bLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_label_set_text(bLabel, "Brightness");
  lv_obj_align(bLabel, LV_ALIGN_TOP_LEFT, 16, 44);

  settingBrightnessLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(settingBrightnessLabel, lv_color_hex(COLOR_GOOD_BG), LV_PART_MAIN);
  lv_obj_set_style_text_font(settingBrightnessLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_obj_align(settingBrightnessLabel, LV_ALIGN_TOP_RIGHT, -16, 44);
  lv_label_set_text(settingBrightnessLabel, "100%");

  settingBrightnessSlider = lv_slider_create(tile);
  lv_obj_set_size(settingBrightnessSlider, 504, 36);
  lv_obj_align(settingBrightnessSlider, LV_ALIGN_TOP_MID, 0, 84);
  lv_slider_set_range(settingBrightnessSlider, 38, 255);
  lv_slider_set_value(settingBrightnessSlider, 255, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(settingBrightnessSlider, lv_color_hex(COLOR_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_color(settingBrightnessSlider, lv_color_hex(COLOR_GOOD_BG), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(settingBrightnessSlider, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_KNOB);
  lv_obj_set_style_pad_all(settingBrightnessSlider, 10, LV_PART_KNOB);
  lv_obj_add_event_cb(settingBrightnessSlider, brightnessSlidercb, LV_EVENT_VALUE_CHANGED, NULL);

  // Divider
  lv_obj_t *div = lv_obj_create(tile);
  lv_obj_remove_style_all(div);
  lv_obj_set_size(div, 504, 1);
  lv_obj_set_style_bg_color(div, lv_color_hex(COLOR_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(div, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 140);

  // -- Auto-dim row --
  lv_obj_t *dLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(dLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_set_style_text_font(dLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_label_set_text(dLabel, "Auto-dim");
  lv_obj_align(dLabel, LV_ALIGN_TOP_LEFT, 16, 158);

  settingDimSwitch = lv_switch_create(tile);
  lv_obj_set_size(settingDimSwitch, 72, 36);
  lv_obj_align(settingDimSwitch, LV_ALIGN_TOP_RIGHT, -130, 152);
  lv_obj_add_event_cb(settingDimSwitch, settingsCb, LV_EVENT_VALUE_CHANGED, NULL);

  settingDimDelayLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(settingDimDelayLabel, lv_color_hex(COLOR_GOOD_BG), LV_PART_MAIN);
  lv_obj_set_style_text_font(settingDimDelayLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_obj_add_flag(settingDimDelayLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(settingDimDelayLabel, 24);
  lv_obj_add_event_cb(settingDimDelayLabel, settingsCb, LV_EVENT_CLICKED, NULL);
  lv_obj_align(settingDimDelayLabel, LV_ALIGN_TOP_RIGHT, -16, 158);

  // Scroll hint
  lv_obj_t *hint = lv_label_create(tile);
  lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_label_set_text(hint, LV_SYMBOL_RIGHT "  more settings");
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

inline void buildSettingsPage2(lv_obj_t *tile) {
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_pad_all(tile, 0, LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(tile);
  lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_label_set_text(title, LV_SYMBOL_SETTINGS "  SETTINGS  2/2");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  // -- Proximity alert --
  lv_obj_t *l = lv_label_create(tile);
  lv_obj_set_style_text_color(l, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_label_set_text(l, "Proximity alert");
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 16, 44);

  settingProxSwitch = lv_switch_create(tile);
  lv_obj_set_size(settingProxSwitch, 72, 36);
  lv_obj_align(settingProxSwitch, LV_ALIGN_TOP_RIGHT, -130, 38);
  lv_obj_add_event_cb(settingProxSwitch, settingsCb, LV_EVENT_VALUE_CHANGED, NULL);

  settingProxThreshLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(settingProxThreshLabel, lv_color_hex(COLOR_GOOD_BG), LV_PART_MAIN);
  lv_obj_set_style_text_font(settingProxThreshLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_obj_add_flag(settingProxThreshLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(settingProxThreshLabel, 24);
  lv_obj_add_event_cb(settingProxThreshLabel, settingsCb, LV_EVENT_CLICKED, NULL);
  lv_obj_align(settingProxThreshLabel, LV_ALIGN_TOP_RIGHT, -16, 44);

  // Divider
  lv_obj_t *div = lv_obj_create(tile);
  lv_obj_remove_style_all(div);
  lv_obj_set_size(div, 504, 1);
  lv_obj_set_style_bg_color(div, lv_color_hex(COLOR_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(div, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 100);

  // -- Rescan distance --
  l = lv_label_create(tile);
  lv_obj_set_style_text_color(l, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_label_set_text(l, "Rescan distance");
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 16, 118);

  settingRescanLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(settingRescanLabel, lv_color_hex(COLOR_GOOD_BG), LV_PART_MAIN);
  lv_obj_set_style_text_font(settingRescanLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_obj_add_flag(settingRescanLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(settingRescanLabel, 24);
  lv_obj_add_event_cb(settingRescanLabel, settingsCb, LV_EVENT_CLICKED, NULL);
  lv_obj_align(settingRescanLabel, LV_ALIGN_TOP_RIGHT, -16, 118);

  // Divider
  div = lv_obj_create(tile);
  lv_obj_remove_style_all(div);
  lv_obj_set_size(div, 504, 1);
  lv_obj_set_style_bg_color(div, lv_color_hex(COLOR_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(div, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 174);

  // -- Timezone --
  l = lv_label_create(tile);
  lv_obj_set_style_text_color(l, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_label_set_text(l, "Timezone");
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 16, 192);

  settingTimezoneLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(settingTimezoneLabel, lv_color_hex(COLOR_GOOD_BG), LV_PART_MAIN);
  lv_obj_set_style_text_font(settingTimezoneLabel, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_add_flag(settingTimezoneLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(settingTimezoneLabel, 24);
  lv_obj_add_event_cb(settingTimezoneLabel, settingsCb, LV_EVENT_CLICKED, NULL);
  lv_obj_align(settingTimezoneLabel, LV_ALIGN_TOP_RIGHT, -16, 196);

  // Scroll hint back up
  lv_obj_t *hint = lv_label_create(tile);
  lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_label_set_text(hint, LV_SYMBOL_LEFT "  brightness / dim");
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

// Keep buildSettingsPage as alias for page 1 for compatibility
inline void buildSettingsPage(lv_obj_t *tile) { buildSettingsPage1(tile); }

inline void applySettingsToWidgets(const AppSettings &s) {
  if (s.proximityAlert) lv_obj_add_state(settingProxSwitch, LV_STATE_CHECKED);
  else                  lv_obj_clear_state(settingProxSwitch, LV_STATE_CHECKED);

  char buf[24];
  snprintf(buf, sizeof(buf), "%dm", PROX_THRESHOLDS[s.proximityThreshIdx]);
  lv_label_set_text(settingProxThreshLabel, buf);

  snprintf(buf, sizeof(buf), "%.2fkm",
           RESCAN_THRESHOLDS_CM[s.rescanThreshIdx] / 100.0F);
  lv_label_set_text(settingRescanLabel, buf);

  if (s.autoDim) lv_obj_add_state(settingDimSwitch, LV_STATE_CHECKED);
  else           lv_obj_clear_state(settingDimSwitch, LV_STATE_CHECKED);

  int secs = DIM_DELAYS[s.dimDelayIdx];
  if (secs == 0) snprintf(buf, sizeof(buf), "off");
  else           snprintf(buf, sizeof(buf), "%dmin", secs / 60);
  lv_label_set_text(settingDimDelayLabel, buf);

  snprintf(buf, sizeof(buf), "%d%%", s.brightness * 100 / 255);
  lv_label_set_text(settingBrightnessLabel, buf);
  lv_slider_set_value(settingBrightnessSlider, s.brightness, LV_ANIM_OFF);

  lv_label_set_text(settingTimezoneLabel, TZ_OPTIONS[s.timezoneIdx].label);
}

// Call just before entering deep sleep -- clears the screen and shows
// a brief message so the user knows the long-press registered.
inline void showSleepMessage() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_t *msg = lv_label_create(scr);
  lv_obj_set_style_text_color(msg, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_set_style_text_font(msg, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_label_set_text(msg, "Sleeping...");
  lv_obj_center(msg);
  lv_timer_handler();  // force one render pass to show the message
}

// Altimeter +/- button callback
static void altBtnCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED &&
      lv_event_get_code(e) != LV_EVENT_LONG_PRESSED_REPEAT) return;
  lv_obj_t *obj = lv_event_get_target(e);
  float step = (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED_REPEAT) ? 5.0F : 1.0F;
  if (obj == altPlusBtn)  g_settings.qnhHpa += step;
  if (obj == altMinusBtn) g_settings.qnhHpa -= step;
  g_settings.qnhHpa = fmaxf(900.0F, fminf(1100.0F, g_settings.qnhHpa));
  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f hPa", g_settings.qnhHpa);
  lv_label_set_text(altQnhLabel, buf);
  if (g_settingsChangedCallback) g_settingsChangedCallback(g_settings);
}

inline void buildTempHumidPage(lv_obj_t *tile) {
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_pad_all(tile, 12, LV_PART_MAIN);

  // --- Temperature chart (top half) ---
  lv_obj_t *tempTitle = lv_label_create(tile);
  lv_obj_set_style_text_color(tempTitle, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(tempTitle, "TEMPERATURE");
  lv_obj_align(tempTitle, LV_ALIGN_TOP_LEFT, 0, 0);

  graphTempLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(graphTempLabel, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_MAIN);
  lv_obj_set_style_text_font(graphTempLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_obj_align(graphTempLabel, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_label_set_text(graphTempLabel, "-- °C");

  graphTempLine = lv_line_create(tile);
  lv_obj_set_style_line_color(graphTempLine, lv_color_hex(0xE07040), LV_PART_MAIN);  // warm orange
  lv_obj_set_style_line_width(graphTempLine, 2, LV_PART_MAIN);
  lv_obj_set_style_line_rounded(graphTempLine, true, LV_PART_MAIN);
  lv_obj_set_size(graphTempLine, 512, 80);
  lv_obj_align(graphTempLine, LV_ALIGN_TOP_MID, 0, 26);
  for (int i = 0; i < 6; i++) { graphTempPoints[i].x = i * 100; graphTempPoints[i].y = 40; }
  lv_line_set_points(graphTempLine, graphTempPoints, 6);

  // Divider
  lv_obj_t *div = lv_obj_create(tile);
  lv_obj_remove_style_all(div);
  lv_obj_set_size(div, 512, 1);
  lv_obj_set_style_bg_color(div, lv_color_hex(COLOR_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(div, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 114);

  // --- Humidity chart (bottom half) ---
  lv_obj_t *humTitle = lv_label_create(tile);
  lv_obj_set_style_text_color(humTitle, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(humTitle, "HUMIDITY");
  lv_obj_align(humTitle, LV_ALIGN_TOP_LEFT, 0, 122);

  graphHumidLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(graphHumidLabel, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_MAIN);
  lv_obj_set_style_text_font(graphHumidLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_obj_align(graphHumidLabel, LV_ALIGN_TOP_RIGHT, 0, 122);
  lv_label_set_text(graphHumidLabel, "-- %");

  graphHumidLine = lv_line_create(tile);
  lv_obj_set_style_line_color(graphHumidLine, lv_color_hex(0x4090E0), LV_PART_MAIN);  // cool blue
  lv_obj_set_style_line_width(graphHumidLine, 2, LV_PART_MAIN);
  lv_obj_set_style_line_rounded(graphHumidLine, true, LV_PART_MAIN);
  lv_obj_set_size(graphHumidLine, 512, 80);
  lv_obj_align(graphHumidLine, LV_ALIGN_TOP_MID, 0, 148);
  for (int i = 0; i < 6; i++) { graphHumidPoints[i].x = i * 100; graphHumidPoints[i].y = 40; }
  lv_line_set_points(graphHumidLine, graphHumidPoints, 6);

  // Axis labels
  lv_obj_t *old1 = lv_label_create(tile);
  lv_obj_set_style_text_color(old1, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(old1, "-30min");
  lv_obj_align(old1, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lv_obj_t *now1 = lv_label_create(tile);
  lv_obj_set_style_text_color(now1, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(now1, "now");
  lv_obj_align(now1, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

inline void buildPressureGraphPage(lv_obj_t *tile) {
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_pad_all(tile, 12, LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(tile);
  lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(title, "PRESSURE TREND");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  graphPressLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(graphPressLabel, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_MAIN);
  lv_obj_set_style_text_font(graphPressLabel, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_align(graphPressLabel, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_label_set_text(graphPressLabel, "-- hPa");

  graphRateLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(graphRateLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_align(graphRateLabel, LV_ALIGN_TOP_RIGHT, 0, 36);
  lv_label_set_text(graphRateLabel, "-- hPa/hr");

  // Line chart: drawn as an lv_line spanning the full width
  // Points are updated in guiUpdate when pressure data is available
  graphLine = lv_line_create(tile);
  lv_obj_set_style_line_color(graphLine, lv_color_hex(COLOR_GOOD_BG), LV_PART_MAIN);
  lv_obj_set_style_line_width(graphLine, 2, LV_PART_MAIN);
  lv_obj_set_style_line_rounded(graphLine, true, LV_PART_MAIN);
  lv_obj_align(graphLine, LV_ALIGN_BOTTOM_MID, 0, -8);
  // Initialise with flat line as placeholder
  for (int i = 0; i < 6; i++) {
    graphPoints[i].x = i * 100;
    graphPoints[i].y = 80;
  }
  lv_line_set_points(graphLine, graphPoints, 6);
  lv_obj_set_size(graphLine, 512, 120);

  // Horizontal axis labels
  lv_obj_t *old = lv_label_create(tile);
  lv_obj_set_style_text_color(old, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(old, "-30min");
  lv_obj_align(old, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lv_obj_t *now = lv_label_create(tile);
  lv_obj_set_style_text_color(now, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(now, "now");
  lv_obj_align(now, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

inline void buildAltimeterPage(lv_obj_t *tile) {
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_pad_all(tile, 12, LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(tile);
  lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_label_set_text(title, "ALTIMETER");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  // Large pressure altitude
  altAltLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(altAltLabel, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_MAIN);
  lv_obj_set_style_text_font(altAltLabel, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_align(altAltLabel, LV_ALIGN_CENTER, 0, -20);
  lv_label_set_text(altAltLabel, "---- m");

  // GPS altitude for comparison
  altGpsLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(altGpsLabel, lv_color_hex(COLOR_TEXT_DIM), LV_PART_MAIN);
  lv_obj_align(altGpsLabel, LV_ALIGN_CENTER, 0, 40);
  lv_label_set_text(altGpsLabel, "GPS: -- m");

  // QNH display and +/- buttons at bottom
  altMinusBtn = lv_btn_create(tile);
  lv_obj_remove_style_all(altMinusBtn);
  lv_obj_set_size(altMinusBtn, 50, 36);
  lv_obj_set_style_bg_color(altMinusBtn, lv_color_hex(0x333333), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(altMinusBtn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(altMinusBtn, 6, LV_PART_MAIN);
  lv_obj_align(altMinusBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(altMinusBtn, altBtnCb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(altMinusBtn, altBtnCb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
  lv_obj_set_ext_click_area(altMinusBtn, 15);
  altMinusBtnLabel = lv_label_create(altMinusBtn);
  lv_obj_set_style_text_font(altMinusBtnLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_obj_set_style_text_color(altMinusBtnLabel, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_MAIN);
  lv_label_set_text(altMinusBtnLabel, "-");
  lv_obj_center(altMinusBtnLabel);

  altQnhLabel = lv_label_create(tile);
  lv_obj_set_style_text_color(altQnhLabel, lv_color_hex(COLOR_TEXT_SUB), LV_PART_MAIN);
  lv_obj_set_style_text_font(altQnhLabel, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_align(altQnhLabel, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_label_set_text(altQnhLabel, "1013.2 hPa");

  altPlusBtn = lv_btn_create(tile);
  lv_obj_remove_style_all(altPlusBtn);
  lv_obj_set_size(altPlusBtn, 50, 36);
  lv_obj_set_style_bg_color(altPlusBtn, lv_color_hex(0x333333), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(altPlusBtn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(altPlusBtn, 6, LV_PART_MAIN);
  lv_obj_align(altPlusBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(altPlusBtn, altBtnCb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(altPlusBtn, altBtnCb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
  lv_obj_set_ext_click_area(altPlusBtn, 15);
  altPlusBtnLabel = lv_label_create(altPlusBtn);
  lv_obj_set_style_text_font(altPlusBtnLabel, &lv_font_montserrat_22, LV_PART_MAIN);
  lv_obj_set_style_text_color(altPlusBtnLabel, lv_color_hex(COLOR_TEXT_MAIN), LV_PART_MAIN);
  lv_label_set_text(altPlusBtnLabel, "+");
  lv_obj_center(altPlusBtnLabel);
}

// Call from setup() after beginLvglHelper(). Pass switchMode() from
// main.cpp as the callback so the mode toggle button can trigger it.
inline void guiInit(void (*switchModeCb)() = nullptr,
                    void (*setBrightnessCb)(uint8_t) = nullptr,
                    void (*settingsChangedCb)(const AppSettings &) = nullptr,
                    const AppSettings &initialSettings = AppSettings(),
                    void (*sleepCb)() = nullptr) {
  g_switchModeCallback      = switchModeCb;
  g_setBrightnessCallback    = setBrightnessCb;
  g_settingsChangedCallback  = settingsChangedCb;
  g_settings                 = initialSettings;
  g_sleepCallback            = sleepCb;
  // Force dark background at every level — screen root, tileview
  // container, and each tile. Without all three, LVGL's default white
  // theme bleeds through wherever a tile hasn't explicitly painted over
  // it (which is what caused the white background Steve saw on first run).
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  tileview = lv_tileview_create(scr);
  lv_obj_set_style_bg_color(tileview, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(tileview, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

  // Tile (0,0) = home, swipe RIGHT to GPS detail, swipe DOWN to pressure graph
  lv_obj_t *homeTile = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
  buildHomePage(homeTile);

  lv_obj_t *detailTile = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
  buildDetailPage(detailTile);

  lv_obj_t *listTile = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT | LV_DIR_BOTTOM);
  buildSummitListPage(listTile);

  lv_obj_t *bmeTile = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT | LV_DIR_BOTTOM);
  buildBmePage(bmeTile);

  lv_obj_t *settingsTile = lv_tileview_add_tile(tileview, 4, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
  buildSettingsPage1(settingsTile);

  // Settings page 2 at (5,0) -- swipe RIGHT from settings 1, or LEFT back
  lv_obj_t *settings2Tile = lv_tileview_add_tile(tileview, 5, 0, LV_DIR_LEFT);
  buildSettingsPage2(settings2Tile);

  applySettingsToWidgets(g_settings);

  // Row 1: accessed by swiping DOWN from BME sensor page (3,0)
  // Pressure graph at (3,1) -- swipe UP back to BME, RIGHT to altimeter
  // Tile (2,1) = Temp/humidity trend; swipe UP back to summit list, RIGHT to pressure graph
  lv_obj_t *tempHumidTile = lv_tileview_add_tile(tileview, 2, 1, LV_DIR_TOP | LV_DIR_RIGHT);
  buildTempHumidPage(tempHumidTile);

  // Tile (3,1) = Pressure trend; swipe LEFT to temp/humidity, UP back to BME, RIGHT to altimeter
  lv_obj_t *graphTile = lv_tileview_add_tile(tileview, 3, 1, LV_DIR_TOP | LV_DIR_LEFT | LV_DIR_RIGHT);
  buildPressureGraphPage(graphTile);

  // Altimeter at (4,1) -- swipe LEFT back to graph, UP back to settings 1
  lv_obj_t *altTile = lv_tileview_add_tile(tileview, 4, 1, LV_DIR_LEFT | LV_DIR_TOP);
  buildAltimeterPage(altTile);

  // Expand touch area into top-right corner of screen
  lv_obj_set_ext_click_area(gearBtn, 30);

  // No explicit "jump to home" call is needed or possible here: Steve
  // confirmed via his own build's real lv_tileview.h (not docs, which
  // gave two wrong answers in a row across different LVGL versions)
  // that this pinned lvgl@8.4.0 only declares lv_tileview_get_tile_act
  // -- there is NO setter function in this version at all. lv_tileview
  // defaults to tile (0,0) on creation regardless of LVGL version, and
  // since homeTile was added first at (0,0), that default already
  // gives us the behaviour Steve wants (home shown first on boot).
}

inline void guiUpdate(bool fixValid, double lat, double lon, int sats, float hdop,
                       bool bmeOk_, bool bmeGenuine_, bool bmePressureOk_,
                       float tempC, float pressHpa, char pressureTrend,
                       float humidity, float trendRateHpaHr, float pressAltM,
                       bool haveSummit, const SummitMatch &summit,
                       bool haveAlt, float fixAltM,
                       int utcYear, int utcMonth, int utcDay,
                       int hh, int mm, int ss,
                       bool haveSummitList, const SummitMatchList &summitList,
                       bool parkMode,
                       int battPct,
                       bool proximityAlertEnabled,
                       int proximityThresholdM,
                       TZone tz) {
  // --- Mode toggle button label ---
  lv_label_set_text(modeBtnLabel, parkMode ? "PARKS" : "SUMMITS");
  lv_obj_set_style_text_color(modeBtnLabel,
    lv_color_hex(parkMode ? 0xBA7517 : COLOR_GOOD_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_color(modeBtn,
    lv_color_hex(parkMode ? 0x3A2A00 : 0x2A3A2A), LV_PART_MAIN);
  lv_label_set_text(listPageTitle, parkMode ? "NEAREST PARKS" : "NEAREST SUMMITS");

  // --- Home page: top strip ---
  String grid;
  if (fixValid) {
    grid = calculateMaidenhead(lat, lon);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s  %dsats", grid.c_str(), sats);
    lv_label_set_text(gpsInfoLabel, buf);
  } else {
    lv_label_set_text(gpsInfoLabel, "No fix");
  }

  FixQuality fq = classifyFixQuality(fixValid, sats, hdop);
  switch (fq) {
    case FIX_GOOD: setPill(fixPill, fixPillLabel, "GOOD FIX", COLOR_GOOD_BG, COLOR_GOOD_TEXT); break;
    case FIX_OK:   setPill(fixPill, fixPillLabel, "OK FIX", COLOR_WARN_BG, COLOR_WARN_TEXT); break;
    case FIX_BAD:  setPill(fixPill, fixPillLabel, "NO FIX", COLOR_BAD_BG, COLOR_BAD_TEXT); break;
  }

  // --- Home page: middle band ---
  AZStatus homeAz = AZ_UNKNOWN;
  if (haveSummit && summit.found) {
    lv_label_set_text(summitCodeLabel, summit.summitCode);
    lv_label_set_text(summitNameLabel, summit.summitName);
    char detail[80];
    snprintf(detail, sizeof(detail), "%dm  %.1fkm %s  %d\xC2\xB0",
             summit.altM, summit.distanceKm,
             compass8(summit.bearingDeg), (int)roundf(summit.bearingDeg));
    lv_label_set_text(summitDetailLabel, detail);

    if (haveAlt && !parkMode) {
      homeAz = estimateActivationZone(fixAltM, (float)summit.altM, summit.distanceKm);
    }
    if (parkMode) {
      // Parks don't have an activation zone concept -- hide the pill
      lv_obj_add_flag(azPill, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(azPill, LV_OBJ_FLAG_HIDDEN);
      switch (homeAz) {
        case AZ_IN:         setPill(azPill, azPillLabel, "IN AZ", COLOR_GOOD_BG, COLOR_GOOD_TEXT); break;
        case AZ_BORDERLINE: setPill(azPill, azPillLabel, "AZ ?", COLOR_WARN_BG, COLOR_WARN_TEXT); break;
        case AZ_OUT:         setPill(azPill, azPillLabel, "OUT", COLOR_BAD_BG, COLOR_BAD_TEXT); break;
        default:             setPill(azPill, azPillLabel, "--", COLOR_LINE, COLOR_TEXT_DIM); break;
      }
    }
  } else {
    lv_label_set_text(summitCodeLabel, "--");
    lv_label_set_text(summitNameLabel, fixValid ? "Scanning..." : "Waiting for fix...");
    lv_label_set_text(summitDetailLabel, "");
    setPill(azPill, azPillLabel, "--", COLOR_LINE, COLOR_TEXT_DIM);
  }

  // --- Home page: bottom strip (UTC + local time) ---
  if (hh >= 0) {
    char utcbuf[20];
    snprintf(utcbuf, sizeof(utcbuf), "UTC %02d:%02d:%02d", hh, mm, ss);
    lv_label_set_text(tempLabel, utcbuf);

    int localH, localM, localS;
    toLocalTime(hh, mm, ss, utcYear, utcMonth, utcDay,
                tz, localH, localM, localS);
    const char *abbrev = tzAbbrev(tz, utcYear, utcMonth, utcDay);
    char locbuf[20];
    snprintf(locbuf, sizeof(locbuf), "%s %02d:%02d:%02d", abbrev, localH, localM, localS);
    lv_label_set_text(pressureLabel, locbuf);
  } else {
    lv_label_set_text(tempLabel, "UTC --:--:--");
    lv_label_set_text(pressureLabel, "LOCAL --:--:--");
  }

  // --- GPS detail page ---
  if (fixValid) {
    char gbuf[32];
    snprintf(gbuf, sizeof(gbuf), "Grid: %s", grid.c_str());
    lv_label_set_text(detailGridLabel, gbuf);

    char latbuf[32];
    snprintf(latbuf, sizeof(latbuf), "Lat: %.6f", lat);
    lv_label_set_text(detailLatLabel, latbuf);

    char lonbuf[32];
    snprintf(lonbuf, sizeof(lonbuf), "Lon: %.6f", lon);
    lv_label_set_text(detailLonLabel, lonbuf);

    if (haveAlt) {
      char altbuf[32];
      snprintf(altbuf, sizeof(altbuf), "Alt: %.1fm", fixAltM);
      lv_label_set_text(detailAltLabel, altbuf);
    } else {
      lv_label_set_text(detailAltLabel, "Alt: --");
    }

    char satbuf[48];
    snprintf(satbuf, sizeof(satbuf), "%d satellites, HDOP %.1f", sats, hdop);
    lv_label_set_text(detailSatsLabel, satbuf);

    // Sunrise/sunset computed from current GPS position and UTC date
    if (utcYear > 2020) {
      SunTimes sun = calcSunTimes(utcYear, utcMonth, utcDay, (float)lat, (float)lon);
      if (sun.isValid) {
        int localH, localM, localS;
        // Sunrise
        int srH = (int)(sun.sunriseUtcMin / 60) % 24;
        int srM = (int)sun.sunriseUtcMin % 60;
        toLocalTime(srH, srM, 0, utcYear, utcMonth, utcDay,
                    tz, localH, localM, localS);
        char srbuf[20];
        snprintf(srbuf, sizeof(srbuf), "Rise %02d:%02d", localH, localM);
        lv_label_set_text(g_sunriseLabel, srbuf);
        // Sunset
        int ssH = (int)(sun.sunsetUtcMin / 60) % 24;
        int ssM = (int)sun.sunsetUtcMin % 60;
        toLocalTime(ssH, ssM, 0, utcYear, utcMonth, utcDay,
                    tz, localH, localM, localS);
        char ssbuf[20];
        snprintf(ssbuf, sizeof(ssbuf), "Set  %02d:%02d", localH, localM);
        lv_label_set_text(g_sunsetLabel, ssbuf);
      } else {
        lv_label_set_text(g_sunriseLabel, "Rise: --");
        lv_label_set_text(g_sunsetLabel,  "Set:  --");
      }
    }
  } else {
    lv_label_set_text(detailGridLabel, "Grid: --");
    lv_label_set_text(detailLatLabel, "Lat: --");
    lv_label_set_text(detailLonLabel, "Lon: --");
    lv_label_set_text(detailAltLabel, "Alt: --");
    lv_label_set_text(detailSatsLabel, "No fix");
  }

  // --- Summit/Park list page ---
  if (haveSummitList && summitList.count > 0) {
    for (int i = 0; i < SUMMIT_TOP_N; i++) {
      if (i < summitList.count) {
        const SummitMatch &m = summitList.items[i];
        char row[96];
        snprintf(row, sizeof(row), "%d. %s  %.1fkm %s %d\xC2\xB0  %s",
                  i + 1, m.summitCode, m.distanceKm,
                  compass8(m.bearingDeg), (int)roundf(m.bearingDeg),
                  m.summitName);
        lv_label_set_text(summitListLabels[i], row);
      } else {
        lv_label_set_text(summitListLabels[i], "");
      }
    }
  } else {
    const char *scanMsg = parkMode ? "Scanning parks..." : "Scanning summits...";
    for (int i = 0; i < SUMMIT_TOP_N; i++) {
      lv_label_set_text(summitListLabels[i], i == 0 ? scanMsg : "");
    }
  }

  // --- BME280 sensor detail page ---
  if (bmeOk_) {
    char tbuf[32];
    snprintf(tbuf, sizeof(tbuf), "Temp: %.1f\xC2\xB0" "C", tempC);
    lv_label_set_text(bmeDetailTempLabel, tbuf);

    char hbuf[32];
    snprintf(hbuf, sizeof(hbuf), "Humidity: %.0f%%", humidity);
    lv_label_set_text(bmeDetailHumidityLabel, hbuf);

    char pbuf[40];
    if (!bmePressureOk_) {
      snprintf(pbuf, sizeof(pbuf), "Pressure: -- hPa");
    } else if (!bmeGenuine_) {
      snprintf(pbuf, sizeof(pbuf), "Pressure: %.1f hPa %c (?ID)", pressHpa, pressureTrend);
    } else {
      snprintf(pbuf, sizeof(pbuf), "Pressure: %.1f hPa %c", pressHpa, pressureTrend);
    }
    lv_label_set_text(bmeDetailPressureLabel, pbuf);

    char tbuf2[48];
    if (!isnan(trendRateHpaHr)) {
      snprintf(tbuf2, sizeof(tbuf2), "Trend: %+.1f hPa/hr", trendRateHpaHr);
    } else {
      snprintf(tbuf2, sizeof(tbuf2), "Trend: building history...");
    }
    lv_label_set_text(bmeDetailTrendLabel, tbuf2);

    // Pressure altitude cross-check vs GPS altitude — useful on a summit.
    // ISA QNH (1013.25 hPa) assumed; will be off by ~8m per hPa of error.
    char abuf[64];
    if (haveAlt) {
      snprintf(abuf, sizeof(abuf), "Press.Alt: %.0fm  GPS: %.0fm (ISA)",
               pressAltM, fixAltM);
    } else {
      snprintf(abuf, sizeof(abuf), "Press.Alt: %.0fm (ISA QNH)", pressAltM);
    }
    lv_label_set_text(bmeDetailPressAltLabel, abuf);
  } else {
    lv_label_set_text(bmeDetailTempLabel, "Temp: --");
    lv_label_set_text(bmeDetailHumidityLabel, "Humidity: --");
    lv_label_set_text(bmeDetailPressureLabel, "Pressure: --");
    lv_label_set_text(bmeDetailTrendLabel, "Trend: --");
    lv_label_set_text(bmeDetailPressAltLabel, "BME280 not available");
  }

  // Battery voltage -- shown on BME page regardless of BME status
  // Formula is empirical (community-derived for GPIO4 on 1.91" board,
  // undocumented divider ratio). Verify against multimeter on first field use.
  if (battPct >= 0) {
    char bbuf[32];
    snprintf(bbuf, sizeof(bbuf), "Battery: ~%d%%", battPct);
    lv_label_set_text(bmeDetailBattLabel, bbuf);
  } else {
    lv_label_set_text(bmeDetailBattLabel, "Battery: --");
  }

  // --- Proximity alert ---
  if (proximityAlertEnabled && haveSummit && summit.found &&
      summit.distanceKm * 1000.0F < (float)proximityThresholdM) {
    if (lv_obj_has_flag(proximityAlert, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_clear_flag(proximityAlert, LV_OBJ_FLAG_HIDDEN);
      proximityAlertUntil = millis() + 3000;
    }
  }
  // Hide alert when timer expires
  if (!lv_obj_has_flag(proximityAlert, LV_OBJ_FLAG_HIDDEN) &&
      millis() > proximityAlertUntil) {
    lv_obj_add_flag(proximityAlert, LV_OBJ_FLAG_HIDDEN);
  }

  // --- Pressure trend graph page (3,1) ---
  if (bmeOk_) {
    char pbuf[20];
    snprintf(pbuf, sizeof(pbuf), "%.1f hPa", pressHpa);
    lv_label_set_text(graphPressLabel, pbuf);

    if (!isnan(trendRateHpaHr)) {
      char rbuf[20];
      snprintf(rbuf, sizeof(rbuf), "%+.1f hPa/hr", trendRateHpaHr);
      lv_label_set_text(graphRateLabel, rbuf);
    } else {
      lv_label_set_text(graphRateLabel, "-- hPa/hr");
    }
  }

  // --- Temp + humidity trend page (2,1) ---
  if (bmeOk_) {
    char tbuf[12], hbuf[12];
    snprintf(tbuf, sizeof(tbuf), "%.1f°C", tempC);
    snprintf(hbuf, sizeof(hbuf), "%.1f%%", humidity);
    lv_label_set_text(graphTempLabel, tbuf);
    lv_label_set_text(graphHumidLabel, hbuf);
  }

  // --- Altimeter page (1,1) ---
  if (bmeOk_ && bmePressureOk_ && pressHpa > 0.0F) {
    float pressAlt = 44330.0F * (1.0F - powf(pressHpa / g_settings.qnhHpa, 0.1903F));
    char abuf[20];
    snprintf(abuf, sizeof(abuf), "%.0f m", pressAlt);
    lv_label_set_text(altAltLabel, abuf);
    char qbuf[16];
    snprintf(qbuf, sizeof(qbuf), "%.1f hPa", g_settings.qnhHpa);
    lv_label_set_text(altQnhLabel, qbuf);
  } else {
    lv_label_set_text(altAltLabel, "---- m");
  }

  if (haveAlt) {
    char gbuf[20];
    snprintf(gbuf, sizeof(gbuf), "GPS: %.0f m", fixAltM);
    lv_label_set_text(altGpsLabel, gbuf);
  } else {
    lv_label_set_text(altGpsLabel, "GPS: -- m");
  }
}

// Updates the pressure trend line chart from the rolling history buffer.
// Call whenever a new pressure sample is added (every 5 minutes).
inline void updatePressureGraph(const float *history, int historySize,
                                 int count, int index) {
  if (count < 2) return;
  int oldest = (index + historySize - count) % historySize;
  float vals[6];
  float minVal = 1e9F, maxVal = -1e9F;
  for (int i = 0; i < count; i++) {
    int idx = (oldest + i) % historySize;
    vals[i] = history[idx];
    if (vals[i] < minVal) minVal = vals[i];
    if (vals[i] > maxVal) maxVal = vals[i];
  }
  float range = fmaxf(maxVal - minVal, 2.0F);
  float padding = range * 0.2F;
  float yMin = minVal - padding;
  float yMax = maxVal + padding;
  int chartW = 512;
  int chartH = 110;
  int xStep = (count > 1) ? chartW / (count - 1) : chartW;
  for (int i = 0; i < count; i++) {
    graphPoints[i].x = i * xStep;
    float norm = (vals[i] - yMin) / (yMax - yMin);
    graphPoints[i].y = (lv_coord_t)(chartH * (1.0F - norm));
  }
  lv_line_set_points(graphLine, graphPoints, count);
}

// Updates the temperature and humidity trend line charts.
// Called from main.cpp whenever a new 5-minute sample is added.
inline void updateTempHumidGraph(const float *temps, const float *humids,
                                  int historySize, int count, int index) {
  if (count < 2) return;
  int oldest = (index + historySize - count) % historySize;

  // Plot temps
  {
    float minVal = 1e9F, maxVal = -1e9F;
    float vals[6];
    for (int i = 0; i < count; i++) {
      vals[i] = temps[(oldest + i) % historySize];
      if (vals[i] < minVal) minVal = vals[i];
      if (vals[i] > maxVal) maxVal = vals[i];
    }
    float range = fmaxf(maxVal - minVal, 1.0F);
    float pad = range * 0.15F;
    float yMin = minVal - pad, yMax = maxVal + pad;
    int xStep = (count > 1) ? 512 / (count - 1) : 512;
    for (int i = 0; i < count; i++) {
      graphTempPoints[i].x = i * xStep;
      float norm = (vals[i] - yMin) / (yMax - yMin);
      graphTempPoints[i].y = (lv_coord_t)(76 * (1.0F - norm));
    }
    lv_line_set_points(graphTempLine, graphTempPoints, count);
  }

  // Plot humids
  {
    float minVal = 1e9F, maxVal = -1e9F;
    float vals[6];
    for (int i = 0; i < count; i++) {
      vals[i] = humids[(oldest + i) % historySize];
      if (vals[i] < minVal) minVal = vals[i];
      if (vals[i] > maxVal) maxVal = vals[i];
    }
    float range = fmaxf(maxVal - minVal, 2.0F);
    float pad = range * 0.15F;
    float yMin = minVal - pad, yMax = maxVal + pad;
    int xStep = (count > 1) ? 512 / (count - 1) : 512;
    for (int i = 0; i < count; i++) {
      graphHumidPoints[i].x = i * xStep;
      float norm = (vals[i] - yMin) / (yMax - yMin);
      graphHumidPoints[i].y = (lv_coord_t)(76 * (1.0F - norm));
    }
    lv_line_set_points(graphHumidLine, graphHumidPoints, count);
  }
}
