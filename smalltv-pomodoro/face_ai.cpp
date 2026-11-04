#include "ui_kit.h"
#include <string.h>
#include <initializer_list>

static const uint16_t CLAUDE = uiRgb(217,119,87), CODEX = uiRgb(16,163,127);

static void resetLabel(char *out, size_t n, int32_t mins) {
  if (mins >= 1440) snprintf(out,n,"Reset %ldd %ldh",(long)(mins/1440),(long)(mins%1440/60));
  else snprintf(out,n,"Reset %ldh %ldm",(long)(mins/60),(long)(mins%60));
}

static void usageRow(const char *name, const UiUsage &u, int y, int h, uint16_t brand) {
  G->fillRoundRect(6,y,228,h,9,CARD);
  text(14,y+8,1,brand,name);
  if (!u.valid) { text(14,y+28,1,MUTED,"Waiting for Mac usage"); return; }
  bool compact = h < 100;
  int slot = 0;
  for (int i=0;i<2;++i) {
    int pct = i ? u.weekly : u.session;
    if (pct < 0) continue;
    int line = y + 25 + slot++ * (compact ? 29 : 66);
    uint16_t color = pct >= 90 ? WORK : brand;
    char b[32]; snprintf(b,sizeof b,"%s  %d%% used",i?"7 days":"5 hours",pct);
    text(14,line,compact?1:2,TEXT,b);
    int by = line + (compact?10:20);
    G->fillRoundRect(14,by,212,compact?4:8,2,CARD2);
    if (pct) G->fillRoundRect(14,by,212*pct/100,compact?4:8,2,color);
    resetLabel(b,sizeof b,i?u.weeklyReset:u.sessionReset);
    text(14,by+(compact?7:13),compact?1:2,MUTED,b);
  }
}

struct AiDraw { const UiState *s; uint8_t kind; };
static void aiDraw(void *ctx) {
  AiDraw &d = *(AiDraw*)ctx; const UiState &s = *d.s;
  G->fillRect(0,0,240,240,BG);
  if (d.kind != 2) {
    textC(120,10,2,d.kind==0?CLAUDE:CODEX,d.kind==0?"Claude Code":"OpenAI Codex");
    usageRow("SUBSCRIPTION LIMITS",d.kind==0?s.claude:s.codex,40,186,d.kind==0?CLAUDE:CODEX);
    return;
  }
  int y=6; uint8_t parts=s.dashboardParts;
  if (parts & 1) {
    char b[12]; snprintf(b,sizeof b,"%02d:%02d",displayHour(s),s.now.tm_min);
    textC(120,y,3,TEXT,s.timeOk?b:"--:--"); y+=29;
  }
  if (parts&2) {
    if (s.weatherOn && s.wx.ok) {
      char b[32]; snprintf(b,sizeof b,"%d%c  %d%% humidity",(int)lroundf(s.wx.t),s.fahr?'F':'C',s.wx.hum);
      textC(120,y,1,MUTED,b); y+=12;
      textC(120,y,1,MUTED,s.city?s.city:""); y+=14;
    } else { textC(120,y,1,MUTED,"Set weather location");y+=14; }
  }
  int n=((parts&4)?1:0)+((parts&8)?1:0);
  int height=n?(234-y-(n-1)*5)/n:0;
  if (parts&4) {usageRow("CLAUDE CODE",s.claude,y,height,CLAUDE);y+=height+5;}
  if (parts&8) usageRow("OPENAI CODEX",s.codex,y,height,CODEX);
}

static void aiFace(const UiState &s,bool full,uint8_t kind) {
  uint32_t key=2166136261UL;
  auto add=[&key](uint32_t x){key=(key^x)*16777619UL;};
  add(s.timeOk);add(s.now.tm_hour);add(s.now.tm_min);add(s.dashboardParts);add(s.weatherOn);add(s.wx.version);
  for (const UiUsage *u : {&s.claude,&s.codex}) {
    add(u->valid);add(u->session);add(u->weekly);add(u->sessionReset);add(u->weeklyReset);
  }
  static uint32_t previous[3]={};
  if (!full && key==previous[kind]) return;
  previous[kind]=key;
  AiDraw d={&s,kind};uiPaint(full,0,0,240,240,aiDraw,&d);
}

void themeClaude(const UiState &s,bool full){aiFace(s,full,0);}
void themeCodex(const UiState &s,bool full){aiFace(s,full,1);}
void themeAiDashboard(const UiState &s,bool full){aiFace(s,full,2);}
