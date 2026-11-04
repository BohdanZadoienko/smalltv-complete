#pragma once
#include <ArduinoJson.h>
#include <string.h>
#include "ai_usage.h"

inline const char *parseUsagePayload(const char *body,size_t length,UsageReading &reading,uint32_t now) {
  if (!length || length>1024) return "usage payload must be 1..1024 bytes";
  JsonDocument doc;
  if (deserializeJson(doc,body,length)) return "invalid usage JSON";
  if (!doc["ok"].is<bool>()) return "ok must be a boolean";
  if (!doc["ok"].as<bool>()) {
    reading.valid=false;reading.received=true;reading.receivedAt=now;return nullptr;
  }
  for (const char *key : {"s","w","sr","wr"}) if (!doc[key].is<int>()) return "missing numeric usage field";
  int s=doc["s"],w=doc["w"],sr=doc["sr"],wr=doc["wr"];
  if (s< -1 || s>100 || w< -1 || w>100 || (s<0&&w<0)) return "invalid usage percentage";
  if (sr<0 || wr<0 || sr>525600 || wr>525600) return "invalid reset interval";
  UsageReading next;
  next.valid=next.received=true;next.receivedAt=now;
  next.session=s;next.weekly=w;next.sessionReset=sr;next.weeklyReset=wr;
  const char *plan=doc["pl"]|"";
  for (unsigned i=0; i<sizeof next.plan-1 && plan[i];++i) {
    char c=plan[i];next.plan[i]=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'?c:'?';
  }
  reading=next;return nullptr;
}
