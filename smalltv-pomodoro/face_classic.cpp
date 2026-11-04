#include "ui_kit.h"

static void classicClock(void *context) {
  const UiState &s=*(const UiState*)context;
  G->fillRect(0,0,240,133,BG);
  char b[40];dateLabel(s,b,sizeof b);textC(120,8,1,MUTED,b);
  snprintf(b,sizeof b,"%02d:%02d",displayHour(s),s.now.tm_min);
  textC(120,36,5,s.accent,s.timeOk?b:"--:--");
  textC(120,92,textW(s.city?s.city:"",2)>228?1:2,TEXT,s.city?s.city:"");
  textC(120,116,1,MUTED,(s.weatherOn&&s.wx.ok)?weatherText(s.wx.code):"Set weather location");
}

static void classicWeather(void *context) {
  const UiState &s=*(const UiState*)context;
  G->fillRect(100,140,140,100,BG);
  if(!(s.weatherOn&&s.wx.ok))return;
  char b[24];snprintf(b,sizeof b,"%d %c",(int)lroundf(s.wx.t),s.fahr?'F':'C');textC(171,148,3,TEXT,b);
  snprintf(b,sizeof b,"Humidity %d%%",s.wx.hum);textC(171,181,1,MUTED,b);
  snprintf(b,sizeof b,"Low %d  High %d",(int)lroundf(s.wx.tmin),(int)lroundf(s.wx.tmax));textC(171,201,1,MUTED,b);
}

static void classicIcon(void *context) {
  const UiState &s=*(const UiState*)context;
  G->fillRect(0,140,100,100,BG);
  if(s.weatherOn&&s.wx.ok)weatherIcon(48,183,1.6f,s.wx.code,s.wx.day,BG);
}

void themeClassic(const UiState &s,bool full) {
  static uint32_t previous=0;
  uint32_t key=s.wx.version*65537UL+s.now.tm_min+s.now.tm_hour*61UL+
    (s.weatherOn?0x1000000UL:0)+(s.weatherGif?0x2000000UL:0);
  if(!full&&key==previous)return;
  previous=key;
  uiPaint(full,0,0,240,133,classicClock,(void*)&s);
  uiPaint(full,100,140,140,100,classicWeather,(void*)&s);
  if(!s.weatherGif)uiPaint(full,0,140,100,100,classicIcon,(void*)&s);
}
