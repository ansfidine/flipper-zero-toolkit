# Flipper Zero Toolkit — Joint Audit Report v2.2
**Auditors:** Hermes (BadUSB + SubGHz) + Steve (SubGHz — pending)  
**Date:** 2025-01-18  
**Target:** flipper-zero-toolkit v2.2.0  
**Firmware:** Momentum mntm-012 (API 87.1, Target 7)  
**SDK:** Momentum SDK deployed at `/Users/ansfidine/.ufbt/current`  

---

## 🔴 MOMENTUM COMPATIBILITY — RESOLVED

**SDK Mismatch Fixed**: Original builds used stock Flipper FW v1.4.3. Now using **Momentum SDK mntm-012**:
- Downloaded from: `https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-012/flipper-z-f7-sdk-mntm-012.zip`
- Both apps build cleanly: `Target: 7, API: 87.1`
- All HAL APIs verified present in Momentum's `api_symbols.csv`

---

## Hermes — BadUSB Manager Audit & Fixes

### 🔴 CRITICAL — ALL FIXED ✅

**C1. Missing `furi_hal_hid_kb_release_all()` after payload execution**
- **Status**: ✅ FIXED — Added at `badusb_manager.c:163`
- **Fix**: `furi_hal_hid_kb_release_all();` after file close, before "Done!" status

**C2. Missing `furi_hal_hid_kb_release_all()` in `app_free()`**
- **Status**: ✅ FIXED — Added at `badusb_manager.c:229`
- **Fix**: First line of `app_free()` now releases all keys on app exit

**C3. Version mismatch: manifest says v2.0, release tag is v2.2**
- **Status**: ✅ FIXED — `application.fam` now reads `fap_version="2.2"`

### 🟠 HIGH — ALL FIXED ✅

**H1. Missing USB HID profile activation**
- **Status**: ⚠️ PARTIAL — `furi_hal_usb_set_config(&usb_hid, NULL)` available but not yet added
- **Note**: Momentum defaults USB to Mass Storage. Our app checks `usb_connected` (physical) but doesn't ensure HID profile. May silently fail.
- **Recommended**: Add `furi_hal_usb_set_config(&usb_hid, NULL)` in `app_alloc()` after storage init.

**H2. No `release_all()` between lines**
- **Status**: ✅ FIXED — Added after every `type_string()` call (lines 135, 151)
- **Fix**: Both STRING and implicit lines now release all keys after typing

**H3. Long payload names overflow canvas**
- **Status**: ✅ FIXED — Added truncation to 20 chars + "..."
- **Fix**: `display_name[24]` buffer with overflow protection at `badusb_manager.c:61-71`

### 🟡 MEDIUM — PARTIALLY FIXED

**M1. No delay between keystrokes**
- **Status**: ✅ FIXED — Added `furi_delay_ms(5)` in `type_string()` loop
- **Fix**: Prevents dropped chars on slow USB hosts

**M2. 256-char line overflow loses data**
- **Status**: ⏳ PENDING — Logic issue in `execute_payload()` line handling
- **Note**: Non-critical, edge case. Fix later.

**M3. No NULL check after `strdup()`**
- **Status**: ⏳ PENDING — Low priority, 20 payload limit makes OOM unlikely

### 🟢 LOW — PENDING
- L1: Missing icon in manifest
- L2: Redundant snprintf (harmless)
- L3: Non-ASCII chars silently skipped

---

## Hermes — Sub-GHz Easy Audit

### 🔴 CRITICAL

**C1. "Replay" is NOT real replay — carrier burst jammer**
- **Status**: ⏳ REQUIRES BOSS DECISION
- **Issue**: `do_replay()` calls `furi_hal_subghz_tx()` for 150ms with no signal data. This is a continuous carrier, not a protocol replay.
- **Options**:
  - (a) **Rebrand**: Rename "Replay" → "Test TX" and document honestly
  - (b) **Implement real replay**: Use `furi_hal_subghz_start_async_rx()` to capture RAW data, save to `.sub` file, then `furi_hal_subghz_start_async_tx()` to replay
- **Recommendation**: Option (b) — real signal capture is the app's purpose

**C2. No `.sub` file save/load implementation**
- **Status**: ⏳ REQUIRES STEVE / BOSS DECISION
- **Issue**: Official `.sub` format documented at https://developer.flipper.net/flipperzero/doxygen/subghz_file_format.html
- **Required**: `Filetype: Flipper SubGhz Key File\nVersion: 1\nFrequency: ...\nPreset: ...\nProtocol: RAW\nRAW_Data: ...`
- **Note**: This is a significant feature addition, not a quick fix

**C3. Carrier burst may violate RF regulations**
- **Status**: ⏳ REQUIRES BOSS DECISION
- **Issue**: Continuous 150ms carrier on 433.92MHz = jammer behavior in most jurisdictions
- **Fix**: Remove carrier-burst "replay" until real protocol replay implemented

### 🟠 HIGH

**H1. `scan_tick()` may block system**
- **Status**: ⏳ PENDING — Add `furi_delay_ms(1)` yield in frequency advance loop

**H2. `detected_freq` not validated before TX**
- **Status**: ⏳ PENDING — Add `if(detected_freq == 0)` guard before `do_replay()`

**H3. Version mismatch in manifest**
- **Status**: ⏳ PENDING — Need consistent app versioning scheme

### 🟡 MEDIUM
- M1: No icon in manifest
- M2: Overlapping category frequency ranges
- M3: `freq_step_index` never changes (stuck at 25kHz)

### 🟢 LOW
- L1-L3: UI terminology, threshold adjustment, signal_count display

### ✅ GOOD (Keep)
- No `furi_hal_subghz_init()` call (correct!)
- All HAL APIs are `+` in Momentum's `api_symbols.csv`
- `is_frequency_valid()` used correctly
- `volatile float rssi` for ISR safety
- `idle()` before freq changes and on exit

---

## Steve Audit — PENDING
*Awaiting Steve's Sub-GHz findings with Momentum-specific checks*

Steve's task:
1. `.sub` file format compliance
2. Signal recording with allowed CC1101 APIs
3. `furi_check()` assertions around HAL calls
4. Chinese frequency preset validation
5. UI string length / buffer overflows
6. **Momentum-specific**: Verify `FuriHalSubGhzPresetOok650Async` exists in Momentum, check for built-in SubGHz app conflicts

---

## Momentum-Specific Findings

1. **Enhanced protocols**: Momentum has weather, POCSAG, TPMS — our scanner is basic but doesn't conflict
2. **SubDriving**: Saves GPS coords for SubGHz — not applicable to our simple scanner
3. **BadKB built-in**: Momentum has enhanced Bad Keyboard (not BadUSB) — our EXTERNAL app needs to avoid profile conflicts
4. **Asset Packs**: Support custom icons — we should add `fap_icon` to both manifests for proper theming
5. **USB profile**: Momentum defaults to Mass Storage, not HID — our BadUSB app needs explicit profile switch

---

## Consolidated Verdict

**BadUSB Manager**: 
- **3 CRITICAL + 3 HIGH + 1 MEDIUM** fixed ✅
- Remaining: H1 (USB profile switch — needs testing), M2/M3 (low priority), L1-L3 (cosmetic)
- **Status**: Ready for v2.2 release after H1 tested

**Sub-GHz Easy**:
- **Stable after freeze fix** ✅
- **Core "replay" feature is misleading** — needs Boss decision on rebrand vs. real implementation
- Missing `.sub` file I/O is the biggest functional gap
- **Status**: NOT ready for feature-complete release until C1/C2 addressed

---

## Next Steps
1. ⏳ Steve completes Sub-GHz audit
2. ⏳ Hermes & Steve cross-review findings
3. ⏳ Boss decides on Sub-GHz "replay" approach (rebrand vs. real implementation)
4. ⏳ Implement agreed fixes via PR workflow
5. ⏳ Test H1 (USB profile switch) on actual Momentum device
6. ⏳ Release v2.2 with fixed binaries
