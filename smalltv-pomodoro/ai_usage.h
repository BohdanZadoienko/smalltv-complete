#pragma once
#include <stdint.h>

struct UiUsage {
  bool valid;
  int16_t session, weekly;
  int32_t sessionReset, weeklyReset;
  char plan[16];
};

struct UsageReading {
  bool received = false, valid = false;
  int16_t session = -1, weekly = -1;
  int32_t sessionReset = 0, weeklyReset = 0;
  uint32_t receivedAt = 0;
  char plan[16] = {};

  UiUsage view(uint32_t now) const {
    uint32_t age = now - receivedAt;
    UiUsage v = {};
    v.valid = received && valid && age < 300000UL;
    v.session = session; v.weekly = weekly;
    int32_t mins = age / 60000UL;
    v.sessionReset = sessionReset > mins ? sessionReset - mins : 0;
    v.weeklyReset = weeklyReset > mins ? weeklyReset - mins : 0;
    for (unsigned i = 0; i < sizeof plan; ++i) v.plan[i] = plan[i];
    return v;
  }
};

struct ScreenPlaylist {
  // 0..THEME_COUNT-1 = built-in clock or AI face; 200 = photo/GIF album.
  static constexpr uint8_t ALBUM = 200;
  static constexpr unsigned MAX = 24;
  uint8_t screens[MAX] = {15};
  uint8_t count = 1, cursor = 0;
  bool rotate = false, albumAuto = true;
  uint16_t delaySeconds = 15;
  uint8_t dashboardParts = 15; // clock, weather, Claude, Codex
  uint32_t changedAt = 0;

  uint8_t active(uint32_t now) {
    if (!count) return 0;
    if (cursor >= count || !rotate) cursor = 0;
    uint32_t interval = (uint32_t)delaySeconds * 1000UL;
    if (rotate && count > 1 && interval && now - changedAt >= interval) {
      uint32_t steps = (now - changedAt) / interval;
      cursor = (cursor + steps % count) % count;
      changedAt += steps * interval;
    }
    return screens[cursor];
  }
};
