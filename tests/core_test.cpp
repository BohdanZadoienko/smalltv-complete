#include <cassert>
#include <cstring>
#include <cstdio>
#include "../smalltv-pomodoro/utf8_text.h"
#include "../smalltv-pomodoro/ai_usage.h"
#include "../smalltv-pomodoro/usage_payload.h"
#include "../smalltv-pomodoro/display_payload.h"
int main(){
 assert(utf8Characters("München")==7);
 assert(utf8Characters("Düsseldorf")==10);
 assert(utf8Characters("Größe")==5);
 assert(utf8Characters("Mu\xcc\x88nchen")==7);
 const char *p="ÄÖÜäöüß";const int expected[]={142,153,154,132,148,129,225};
 for(int glyph:expected)assert(utf8Cp437(utf8Character(p))==glyph);
 assert(!*p);
 char cut[4];utf8Copy(cut,"aüx",sizeof cut);assert(!strcmp(cut,"aü"));
 const char *bad="\xe0\x80\x80";assert(utf8Character(bad)=='?');
 const char *shortUtf="\xf0\x9f";assert(utf8Character(shortUtf)=='?');
 const char *nul="\xc3";assert(utf8Character(nul)=='?');
 ScreenPlaylist list;list.count=3;list.screens[0]=0;list.screens[1]=13;list.screens[2]=200;list.delaySeconds=5;list.rotate=true;
 assert(list.active(4999)==0);assert(list.active(5000)==13);assert(list.active(10000)==200);assert(list.active(15000)==0);
 assert(list.active(35000)==13);list.rotate=false;assert(list.active(40000)==0);
 list.rotate=true;list.changedAt=0xffffff00UL;list.cursor=0;assert(list.active(0x00001288UL)==13);
 UsageReading reading;
 const char *ok="{\"ok\":true,\"s\":12,\"w\":95,\"sr\":120,\"wr\":10080,\"pl\":\"plus\"}";
 assert(!parseUsagePayload(ok,strlen(ok),reading,100));
 assert(reading.view(60100).sessionReset==119);assert(reading.view(299999).valid);assert(!reading.view(300100).valid);
 auto previous=reading;
 for(const char *invalid:{"{}","{\"ok\":1}","{\"ok\":true,\"s\":101,\"w\":2,\"sr\":1,\"wr\":2}","{\"ok\":true,\"s\":1,\"w\":2,\"sr\":-1,\"wr\":2}","{\"ok\":true,\"s\":\"1\",\"w\":2,\"sr\":1,\"wr\":2}"}){
  assert(parseUsagePayload(invalid,strlen(invalid),reading,500));assert(reading.receivedAt==previous.receivedAt);
 }
 const char *absent="{\"ok\":false}";assert(!parseUsagePayload(absent,strlen(absent),reading,1000));assert(!reading.view(1000).valid);
 JsonDocument display;
 deserializeJson(display,"{\"rotate\":true,\"delay\":5,\"parts\":15,\"albumAuto\":false,\"screens\":[13,14,15,16,200]}");
 ScreenPlaylist config;assert(parseDisplayPayload(display.as<JsonVariantConst>(),config,17,123));
 assert(config.count==5&&config.changedAt==123&&config.screens[4]==200&&!config.albumAuto);
 auto saved=config;
 for(const char *invalid:{"{}","{\"rotate\":true,\"delay\":4,\"parts\":15,\"albumAuto\":true,\"screens\":[0]}","{\"rotate\":true,\"delay\":15,\"parts\":0,\"albumAuto\":true,\"screens\":[0]}","{\"rotate\":true,\"delay\":15,\"parts\":15,\"albumAuto\":true,\"screens\":[17]}","{\"rotate\":true,\"delay\":15,\"parts\":15,\"albumAuto\":true,\"screens\":[0,0]}","{\"rotate\":true,\"delay\":15,\"parts\":15,\"albumAuto\":true,\"screens\":[]}"}){
  deserializeJson(display,invalid);assert(!parseDisplayPayload(display.as<JsonVariantConst>(),config,17,999));
  assert(config.changedAt==saved.changedAt&&config.count==saved.count&&config.screens[4]==200);
 }
 printf("UTF-8, German glyphs, truncation, timer wraparound, rotation, usage validation and freshness: PASS\n");
}
