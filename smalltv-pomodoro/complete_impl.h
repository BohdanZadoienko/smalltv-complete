// Included by main.cpp after its media and request-guard implementations.
#include "usage_payload.h"
#include "display_payload.h"

static const char *DISPLAY_PATH="/display.json";
static int completeScreen=-1;
static uint32_t completeAlbumAt=0;


static void completePlaylistJson(JsonDocument &doc) {
  doc["rotate"]=displayPlaylist.rotate;doc["delay"]=displayPlaylist.delaySeconds;
  doc["parts"]=displayPlaylist.dashboardParts;doc["albumAuto"]=displayPlaylist.albumAuto;
  doc["photo"]=completeSelectedPhoto;doc["weatherGif"]=completeWeatherGif;
  doc["enabled"]=displayPlaylistEnabled;doc["active"]=completeScreen;
  JsonArray a=doc["screens"].to<JsonArray>();
  for(unsigned i=0;i<displayPlaylist.count;++i)a.add(displayPlaylist.screens[i]);
}

static bool completeSavePlaylist() {
  if(!fsOk)return false;
  JsonDocument doc;completePlaylistJson(doc);
  File f=LittleFS.open("/display.new","w");
  if(!f)return false;
  bool ok=serializeJson(doc,f)>0;f.close();
  if(ok)ok=LittleFS.rename("/display.new",DISPLAY_PATH);
  return ok;
}

void completeBegin() {
  displayPlaylist.screens[0]=THEME_AI_DASHBOARD;
  if(!fsOk)return;
  File f=LittleFS.open(DISPLAY_PATH,"r");
  if(!f || f.size()>2048)return;
  JsonDocument doc;
  if(!deserializeJson(doc,f)) {
    ScreenPlaylist next;
    if(parseDisplayPayload(doc.as<JsonVariantConst>(),next,THEME_COUNT,millis())) {
      displayPlaylist=next;displayPlaylistEnabled=doc["enabled"]|true;
      const char *weatherGif=doc["weatherGif"]|"";
      if(strlen(weatherGif)<128 && mediaPath(String(weatherGif)) && isGif(String(weatherGif)))completeWeatherGif=weatherGif;
      const char *photo=doc["photo"]|"";
      if(strlen(photo)<128 && mediaPath(String(photo)))completeSelectedPhoto=photo;
    }
  }
  f.close();
}

void completeMigrateNetwork() {
  if(!fsOk || LittleFS.exists("/complete-migrated"))return;
  File f=LittleFS.open("/config.json","r");
  if(!f || f.size()>16384)return;
  JsonDocument doc;
  if(deserializeJson(doc,f)){f.close();return;}f.close();
  JsonObjectConst net=doc["wifi"][0];
  const char *ssid=net["ssid"]|"",*pass=net["pass"]|"";
  if(!*ssid || strlen(ssid)>32 || strlen(pass)>64)return;
  // The currently installed fork keeps Wi-Fi in LittleFS, this base uses the SDK.
  WiFi.persistent(true);WiFi.mode(WIFI_STA);WiFi.begin(ssid,pass);WiFi.persistent(false);
  const char *tz=doc["clock"]["tzPosix"]|"CET-1CEST,M3.5.0,M10.5.0/3";
  utf8Copy(cfg.tz,tz,sizeof cfg.tz);
  int brightness=doc["brightness"]|80;cfg.brightness=constrain(brightness,5,100);
  cfg.hour12=0;cfg.lang=1;cfg.alexa=0;cfg.theme=THEME_AI_DASHBOARD;
  saveSettings();
  File marker=LittleFS.open("/complete-migrated","w");if(marker){marker.print("1");marker.close();}
}

static bool completeGifSize(const String &path) {
  File f=fsOk?LittleFS.open(path,"r"):File();uint8_t b[10];
  if(!f || f.read(b,10)!=10){if(f)f.close();return false;}f.close();
  int w=b[6]|(b[7]<<8),h=b[8]|(b[9]<<8);
  return !memcmp(b,"GIF8",4)&&w>0&&h>0&&w<=80&&h<=80;
}

static void completeStartWeatherGif() {
  if(completeWeatherGifActive||completeWeatherGifFailed||!completeWeatherGif.length())return;
  completeWeatherGifFailed=true;
  if(!completeGifSize(completeWeatherGif)||!validGif(completeWeatherGif))return;
  gif=new(std::nothrow) AnimatedGIF();if(!gif)return;
  gif->begin(GIF_PALETTE_RGB565_LE);
  if(!gif->open(completeWeatherGif.c_str(),mediaOpen,mediaClose,gifRead,gifSeek,gifDraw)){
    gif->close();delete gif;gif=nullptr;return;
  }
  gifOffX=8+(80-gif->getCanvasWidth())/2;gifOffY=143+(80-gif->getCanvasHeight())/2;nextFrameAt=0;
  completeWeatherGifActive=true;
}

bool completeScheduleView() {
  uint32_t now=millis();
  uint8_t selected=displayPlaylist.active(now);
  if(selected!=completeScreen) {
    stopAlbum();completeScreen=selected;completeAlbumAt=0;completeWeatherGifFailed=false;
    if(selected!=ScreenPlaylist::ALBUM){cfg.theme=selected;facePackUnload();uiInvalidate();}
  }
  if(selected==ScreenPlaylist::ALBUM) {
    if(album.empty()) {
      static bool shown=false;
      if(!shown){uiError("Upload a photo or GIF");shown=true;}
      return true;
    }
    if(!inAlbum || (displayPlaylist.albumAuto && now-completeAlbumAt>=(uint32_t)cfg.albumDelay*1000UL)) {
      const String &item=(!displayPlaylist.albumAuto && completeSelectedPhoto.length() && LittleFS.exists(completeSelectedPhoto)) ? completeSelectedPhoto : album[albumIdx++%album.size()];
      if(!inAlbum || item!=curFile)showAlbumItem(item);
      completeAlbumAt=now;
    }
  } else {
    if(inAlbum)stopAlbum();
    if(selected==THEME_CLASSIC)completeStartWeatherGif();
  }
  return true;
}

bool completeShowEmptyAlbum() {
  static bool shown=false;
  bool empty=displayPlaylistEnabled && completeScreen==ScreenPlaylist::ALBUM && album.empty();
  if(empty && !shown)uiError("Upload a photo or GIF");
  shown=empty;return empty;
}

static void completeMeter(JsonObject out,const UsageReading &reading) {
  UiUsage v=reading.view(millis());
  out["fresh"]=v.valid;out["valid"]=v.valid;out["error"]=!v.valid;
  out["s"]=v.session;out["w"]=v.weekly;out["sr"]=v.sessionReset;out["wr"]=v.weeklyReset;out["pl"]=v.plan;
}

static void completeReceiveUsage(UsageReading &reading) {
  // Same intentionally open, data-only endpoints as the installed usage firmware.
  // Configuration, media, reset and OTA retain the base's authentication/CSRF guards.
  if(!hostOk()){deny();return;}
  String body=server.arg("plain");
  const char *error=parseUsagePayload(body.c_str(),body.length(),reading,millis());
  if(error){server.send(400,"text/plain",error);return;}
  server.send(200,"application/json","{\"ok\":true}");
}

void completeWeb() {
  server.on("/display",HTTP_GET,[]{
    if(!readOk())return;
    server.sendHeader("Content-Encoding","gzip");server.sendHeader("Cache-Control","no-cache");
    server.send_P(200,"text/html",(const char*)COMPLETE_DISPLAY_GZ,sizeof COMPLETE_DISPLAY_GZ);
  });
  server.on("/api/display",HTTP_GET,[]{
    if(!readOk())return;
    JsonDocument d;completePlaylistJson(d);String body;serializeJson(d,body);server.send(200,"application/json",body);
  });
  server.on("/api/display",HTTP_POST,[]{
    if(!guard())return;
    String body=server.arg("plain");
    if(body.length()>2048){server.send(413,"text/plain","display configuration too large");return;}
    JsonDocument doc;ScreenPlaylist next;
    if(deserializeJson(doc,body)||!parseDisplayPayload(doc.as<JsonVariantConst>(),next,THEME_COUNT,millis())){
      server.send(400,"text/plain","Choose 1..24 distinct screens, a delay of 5..3600 seconds, and at least one dashboard component.");return;
    }
    const char *weatherGif=doc["weatherGif"]|"";
    if(strlen(weatherGif)>127 || (*weatherGif && (!mediaPath(String(weatherGif))||!isGif(String(weatherGif))||!completeGifSize(String(weatherGif))))) {
      server.send(400,"text/plain","Weather animation must be an uploaded GIF no larger than 80x80 pixels.");return;
    }
    String oldGif=completeWeatherGif;completeWeatherGif=weatherGif;
    ScreenPlaylist old=displayPlaylist;bool oldEnabled=displayPlaylistEnabled;
    displayPlaylist=next;displayPlaylistEnabled=true;
    if(!completeSavePlaylist()){
      displayPlaylist=old;displayPlaylistEnabled=oldEnabled;completeWeatherGif=oldGif;server.send(507,"text/plain","Cannot save display settings");return;
    }
    completeScreen=-1;viewUntil=0;stopAlbum();uiInvalidate();
    server.send(200,"application/json","{\"ok\":true}");
  });
  server.on("/api/usage",HTTP_POST,[]{completeReceiveUsage(usageClaude);});
  server.on("/api/usage2",HTTP_POST,[]{completeReceiveUsage(usageCodex);});
  server.on("/api/status",HTTP_GET,[]{
    if(!readOk())return;
    JsonDocument doc;
    // Preserve compatibility with the already installed Mac daemons.
    doc["fw"]="smalltv-ultra-mod";doc["variant"]="smalltv-complete";doc["version"]=FW_VERSION;
    doc["connected"]=WiFi.status()==WL_CONNECTED;doc["ip"]=WiFi.localIP().toString();
    doc["heap"]=ESP.getFreeHeap();doc["uptime"]=millis()/1000;doc["theme"]=cfg.theme;
    doc["screen"]=inAlbum?"album":cfg.theme==THEME_CLAUDE?"usage":cfg.theme==THEME_CODEX?"codex":cfg.theme==THEME_AI_DASHBOARD?"all":"clock";
    completeMeter(doc["claude"].to<JsonObject>(),usageClaude);completeMeter(doc["codex"].to<JsonObject>(),usageCodex);
    String body;serializeJson(doc,body);server.send(200,"application/json",body);
  });
}
