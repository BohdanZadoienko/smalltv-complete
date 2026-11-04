#pragma once
#include <stdint.h>
#include <stddef.h>

// Decode one scalar. Invalid input becomes one visible replacement glyph;
// continuation bytes never become unrelated CP437 characters.
inline uint32_t utf8Scalar(const char *&p) {
  uint8_t a = (uint8_t)*p++;
  if (a < 0x80) return a;
  int n = a >= 0xC2 && a <= 0xDF ? 1 : a >= 0xE0 && a <= 0xEF ? 2 : a >= 0xF0 && a <= 0xF4 ? 3 : 0;
  if (!n) return '?';
  uint32_t cp = a & (0x7F >> (n + 1));
  for (int i = 0; i < n; ++i) {
    uint8_t b = (uint8_t)*p;
    if ((b & 0xC0) != 0x80) return '?';
    ++p; cp = (cp << 6) | (b & 0x3F);
  }
  if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) || (n == 3 && cp < 0x10000) ||
      cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return '?';
  return cp;
}

inline uint32_t utf8Character(const char *&p) {
  uint32_t cp = utf8Scalar(p);
  if ((uint8_t)*p == 0xCC && (uint8_t)p[1] == 0x88) {
    uint32_t composed = cp == 'a' ? 0xE4 : cp == 'o' ? 0xF6 : cp == 'u' ? 0xFC :
                        cp == 'A' ? 0xC4 : cp == 'O' ? 0xD6 : cp == 'U' ? 0xDC : 0;
    if (composed) { p += 2; return composed; }
  }
  return cp;
}

inline size_t utf8Characters(const char *p) {
  size_t n = 0;
  while (*p) { utf8Character(p); ++n; }
  return n;
}

inline bool utf8HasUnicode(const char *p) {
  while (*p) if ((uint8_t)*p++ >= 0x80) return true;
  return false;
}

inline void utf8Copy(char *out, const char *in, size_t capacity) {
  if (!capacity) return;
  size_t used = 0;
  while (*in) {
    const char *next = in; utf8Character(next);
    size_t n = (size_t)(next - in);
    if (used + n >= capacity) break;
    while (in != next) out[used++] = *in++;
  }
  out[used] = 0;
}

inline uint8_t utf8Cp437(uint32_t cp) {
  if (cp < 128) return (uint8_t)cp;
  switch (cp) {
    case 0x00C4: return 142; case 0x00D6: return 153; case 0x00DC: return 154;
    case 0x00E4: return 132; case 0x00F6: return 148; case 0x00FC: return 129;
    case 0x00DF: case 0x1E9E: return 225;
    case 0x00B0: return 248; case 0x00B1: return 241; case 0x00B2: return 253;
    case 0x00E9: return 130; case 0x00E2: return 131; case 0x00E0: return 133;
    case 0x00E5: return 134; case 0x00E7: return 135; case 0x00EA: return 136;
    case 0x00EB: return 137; case 0x00E8: return 138; case 0x00EF: return 139;
    case 0x00EE: return 140; case 0x00EC: return 141; case 0x00C5: return 143;
    case 0x00C9: return 144; case 0x00E6: return 145; case 0x00C6: return 146;
    case 0x00F4: return 147; case 0x00F2: return 149; case 0x00FB: return 150;
    case 0x00F9: return 151; case 0x00FF: return 152; case 0x00A2: return 155;
    case 0x00A3: return 156; case 0x00A5: return 157; case 0x00E1: return 160;
    case 0x00ED: return 161; case 0x00F3: return 162; case 0x00FA: return 163;
    case 0x00F1: return 164; case 0x00D1: return 165; case 0x00BF: return 168;
    case 0x00A1: return 173; case 0x00AB: return 174; case 0x00BB: return 175;
    case 0x00B5: return 230; case 0x00F7: return 246;
    default: return '?';
  }
}
