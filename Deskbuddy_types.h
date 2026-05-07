#pragma once

/** Arduino: enum'lar #include'lardan once Deskbuddy.ino basinda kalir. */

enum HomeWidgetType {
  HOME_WIDGET_HUMIDITY = 0,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR,
  HOME_WIDGET_KP,
  HOME_WIDGET_UV,
  HOME_WIDGET_WIND,
  HOME_WIDGET_SUN,
  HOME_WIDGET_FINANCE,
  HOME_WIDGET_BUDDY
};

constexpr int HOME_SLOT_COUNT = 4;

/** Open-Meteo WMO weather_code */
enum WxKind {
  WX_UNK,
  WX_CLEAR_DAY,
  WX_CLEAR_NIGHT,
  WX_PARTLY,
  WX_CLOUD,
  WX_FOG,
  WX_DRIZZLE,
  WX_RAIN,
  WX_SNOW,
  WX_SHOWER,
  WX_THUNDER
};

enum Page {
  PAGE_HOME = 0,
  PAGE_WEATHER = 1,
  PAGE_NOTES = 2,
  PAGE_STATUS = 3
};
