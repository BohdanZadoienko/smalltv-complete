#pragma once
#include <stddef.h>
#include <string.h>
#include "src/qrcodegen/qrcodegen.h"

// 33 módulos + margem de 4 de cada lado, desenhados em pixels de 3x3.
// Dois buffers de 138 bytes durante a geração; nenhum framebuffer nem heap permanente.
static const int SETUP_QR_VERSION = 4;
static const size_t SETUP_QR_BYTES = qrcodegen_BUFFER_LEN_FOR_VERSION(SETUP_QR_VERSION);
static const size_t SETUP_WIFI_TEXT = 224;  // SSID (32) + senha (63), com todos os caracteres escapados

// Formato Wi-Fi do ZXing. Uma credencial nunca é truncada para outro valor.
static inline bool setupWifiPayload(char *out, size_t cap, const char *ssid, const char *pass) {
  if (!out || !cap) return false;
  out[0] = '\0';
  if (!ssid || !pass || !ssid[0] || strlen(ssid) > 32 || strlen(pass) < 8 || strlen(pass) > 63) return false;
  const char *parts[] = {"WIFI:T:WPA;S:", ssid, ";P:", pass, ";;"};
  size_t n = 0;
  for (int part = 0; part < 5; part++) {
    for (const char *p = parts[part]; *p; p++) {
      bool escape = (part == 1 || part == 3) && strchr("\\;,:\"", *p);
      if (n + (escape ? 2 : 1) >= cap) { out[0] = '\0'; return false; }
      if (escape) out[n++] = '\\';
      out[n++] = *p;
    }
  }
  out[n] = '\0';
  return true;
}

static inline bool setupQrEncode(const char *payload, uint8_t *qr) {
  uint8_t scratch[SETUP_QR_BYTES];
  return payload && qr && qrcodegen_encodeText(payload, scratch, qr, qrcodegen_Ecc_MEDIUM,
      1, SETUP_QR_VERSION, qrcodegen_Mask_AUTO, true);
}
