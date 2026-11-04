# Validation — 4 November 2026

Hardware: one GeekMagic SmallTV Ultra, ESP8266/4 MB, ST7789 240 × 240. It originally reported vendor Ultra-V9.0.51 and was running smalltv-ultra-mod 1.0.2 immediately before this update.

Firmware image tested: SHA-256 `715c2c58a3435e5e5765063e758d709d40690312b1baf6b1b3050457b836407c`, 683,696 bytes. ESP8266 image checksum and separate RSA signature passed. Build: 39,648 bytes static RAM; 679,551 bytes application flash. Source code: `7ff285e03f078c6eec20809745770b4968e46e1d`.

| Check | Result |
|---|---|
| Upgrade from running 1.0.2 | HTTP 200; new firmware booted |
| Wi-Fi, timezone, brightness, 4M1M filesystem | Imported; original address retained; no format |
| Mac services already installed at login | Continued sending real Claude and Codex data without modification |
| Current weather for a configured German city | Device received valid current weather, daily range and timezone |
| Individual Claude, Codex, forecast and Classic faces | Selected and reported active by device |
| Timed AI screen rotation | Device advanced between selected faces at the configured interval |
| Combined components | Saved and read back successfully |
| JPEG upload and fixed album item | Accepted and reported as active |
| Animated GIF and Classic weather GIF | Accepted; decoder allocated; device stayed reachable |
| Automatic album advance | Device advanced from GIF to JPEG |
| Temporary media cleanup | Test-only uploads removed |
| Power restart | User removed power for 10 seconds; new boot observed; city, timezone and selected Claude/Codex rotation persisted; both real feeds resumed |
| Physical screen appearance | User reported the selected screen looks correct |
| Recovery access via three short power cycles | Source-reviewed; not physically exercised |

The ESP8266 has little spare RAM while the GIF decoder is allocated: roughly 6 KB was free in these short checks, versus about 30 KB without it. This is not a long-duration stability or exhaustive media-compatibility test.

Host checks passed: address/undefined-behavior sanitizers for UTF-8, usage/display payload parsing, rotation, reset countdowns and freshness; real renderer incremental/full redraw comparisons and transitions for all 17 faces; four Mac bridge tests; existing upload-security tests. Browser checks used both a mock form and the installed device's actual pages. No inference/model calls were made to obtain quota data.

The upstream Linux 32-bit media fuzzing suite was retained but not run on this Mac. CI is supplied as a template because the publishing credential does not include workflow permission. No automated GitHub run is claimed.

## 2.0.1 changes awaiting installation

Adds category submenus inside the existing Settings tab, integrates the full display form using the panel theme, and redirects the previous /display URL to the same Settings submenu. Adds a persisted remaining-limit flag; usage ingestion remains in percentages used, while the renderer and Settings preview can show 100 minus used. Native tests cover both modes, endpoints and invalid configuration types; renderer tests toggle the mode during incremental drawing. Browser checks cover category navigation, the legacy link, save/reload, remaining values and a 390-pixel phone layout without horizontal overflow.
