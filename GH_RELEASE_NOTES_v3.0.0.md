## Sub-GHz Easy CN v3.0 — Modular Rewrite

**Author:** Ansfidine Youssouf  
**Firmware:** Momentum SDK (API 87.1, Target 7)  
**Date:** June 2026  

### What's New in v3.0

Complete modular rebuild — from single-file monolith to 7 clean modules:

| Module | File | Responsibility |
|--------|------|----------------|
| Entry | `subghz_main.c` | App lifecycle, event loop, graceful shutdown |
| UI | `subghz_ui.c` | 128×64 Canvas draw + input dispatch + screen state machine |
| Radio | `subghz_radio.c` | SPI acquire/release, CC1101 init/shutdown |
| Worker | `subghz_worker.c` | 4096-byte background thread, 2 Hz RSSI scan |
| Capture | `subghz_capture.c` | Async RX + 5000ms auto-stop + ISR callback |
| Replay | `subghz_replay.c` | Async TX + LevelDuration ISR callback |
| File | `subghz_file.c` | `.sub` save format (Flipper SubGHz compatible) |

### Architecture Changes

- **Thread safety:** FuriMutex on all shared radio state
- **SPI guards:** Every HAL call wrapped with acquire/release
- **Expansion disable:** GPIO 15/16 freed on app entry
- **Worker thread:** Offloads RSSI from UI thread (no freezes)
- **Stack size:** 4096 bytes (worker), 4 KB (app)

### Features

- 315 MHz / 433.92 MHz toggle (China ISM bands only)
- Real-time RSSI display on Main screen
- One-button capture (OK) with auto-stop timer
- One-button replay (OK) from internal buffer
- Down-arrow save to `.sub` file on SD card
- Back button navigation across all screens

### Safety & Reliability

| Risk | Mitigation |
|------|-----------|
| SPI deadlock | Never hold mutex during HAL calls |
| Worker stack overflow | 4096-byte stack + minimal locals |
| GPIO conflict | `expansion_disable()` on startup |
| UI freeze | Worker thread handles all radio ops |
| Buffer overflow | Hard limit 2048 samples + auto-stop |
| Memory leak | Reverse-order free in shutdown path |
| Stuck capture | 5000ms timer fires automatically |

### Build

```bash
ufbt -c   # clean
ufbt      # compile
```

Output: `dist/subghz_cn_easy.fap` (12.8 KB)

### Docs

- `docs/architecture.md` — Full architecture document
- `docs/api.md` — Module API reference

### Legal

For authorized security testing and educational purposes only. ISM band transmission requires proper authorization in China.
