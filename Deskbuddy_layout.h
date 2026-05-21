#pragma once

#include "Deskbuddy_types.h"

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 320;
constexpr int TOPBAR_H = 34;
constexpr int NAV_H    = 44;

constexpr int TOPBAR_BTN_SZ = 23;
constexpr int TOPBAR_BTN_GAP = 11;
constexpr int TOPBAR_BTN_MR = 5;

inline int topBarMoonBtnX() { return SCREEN_W - TOPBAR_BTN_SZ - TOPBAR_BTN_MR; }
inline int topBarDimBtnX() { return topBarMoonBtnX() - TOPBAR_BTN_SZ - TOPBAR_BTN_GAP; }
inline int topBarWifiForgetBtnX() { return topBarDimBtnX() - TOPBAR_BTN_SZ - TOPBAR_BTN_GAP; }
inline int topBarUpdateBtnX() { return topBarWifiForgetBtnX() - TOPBAR_BTN_SZ - TOPBAR_BTN_GAP; }

constexpr int HOME_GRID_Y1 = 120;
constexpr int HOME_GRID_Y2 = 198;
constexpr int HOME_WIDGET_H = 70;

constexpr int HOME_TIMER_X = 124;
constexpr int HOME_TIMER_Y = HOME_GRID_Y1;
constexpr int HOME_TIMER_W = 108;
constexpr int HOME_TIMER_H = HOME_WIDGET_H;
constexpr int TIMER_MENU_X = 20;
constexpr int TIMER_MENU_Y = 68;
constexpr int TIMER_MENU_W = 200;
constexpr int TIMER_MENU_H = 194;
constexpr int TIMER_DONE_X = 26;
constexpr int TIMER_DONE_Y = 92;
constexpr int TIMER_DONE_W = 188;
constexpr int TIMER_DONE_H = 108;

constexpr int WIFI_FORGET_DLG_X = 16;
constexpr int WIFI_FORGET_DLG_Y = 88;
constexpr int WIFI_FORGET_DLG_W = 208;
constexpr int WIFI_FORGET_DLG_H = 144;

constexpr int WEATHER_DETAIL_X = 12;
constexpr int WEATHER_DETAIL_Y = 48;
constexpr int WEATHER_DETAIL_W = 216;
constexpr int WEATHER_DETAIL_H = 218;

constexpr int PAGE_ROW1_Y = 42;
constexpr int PAGE_ROW2_Y = 120;
constexpr int PAGE_ROW3_Y = 198;
constexpr int PAGE_WIDGET_H = HOME_WIDGET_H;
