#pragma once
#include <ArduinoJson.h>
#include "ai_usage.h"

inline bool parseDisplayPayload(JsonVariantConst root, ScreenPlaylist &result, unsigned themeCount, uint32_t now) {
  ScreenPlaylist next;
  if (!root.is<JsonObjectConst>() || !root["rotate"].is<bool>() || !root["delay"].is<int>() ||
      !root["parts"].is<int>() || !root["albumAuto"].is<bool>() || !root["screens"].is<JsonArrayConst>()) return false;
  int delay=root["delay"],parts=root["parts"];
  JsonArrayConst screens=root["screens"];
  if (delay<5 || delay>3600 || parts<1 || parts>15 || !screens.size() || screens.size()>ScreenPlaylist::MAX) return false;
  next.count=0;
  for (JsonVariantConst x:screens) {
    if (!x.is<int>()) return false;
    int v=x.as<int>();
    if (v<0 || (v>=static_cast<int>(themeCount) && v!=ScreenPlaylist::ALBUM)) return false;
    for (unsigned i=0;i<next.count;++i) if(next.screens[i]==v) return false;
    next.screens[next.count++]=v;
  }
  next.rotate=root["rotate"];next.delaySeconds=delay;next.dashboardParts=parts;next.albumAuto=root["albumAuto"];
  next.changedAt=now;next.cursor=0;result=next;return true;
}
