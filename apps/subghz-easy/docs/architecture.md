# Sub-GHz Easy CN — Architecture Document

## 1. Overview

A Flipper Zero Sub-GHz scanner + replay tool restricted to two China ISM bands:
- **315 MHz** — remote key fobs, garage doors, some IoT
- **433.92 MHz** — the most common ISM band in China (remotes, sensors, alarms)

**Target:** F7, API 87.1, SDK 1.4.3
**Screen:** 128×64 monochrome
**App type:** External FAP
**Category:** Sub-GHz

## 2. Key Design Constraints

| Constraint | Value |
|-----------|-------|
| Thread stack (worker) | 4096 bytes min |
| SPI bus lock window | Acquire → release on EVERY radio op |
| Expansion protocol | DISABLED while app runs (GPIO 15/16) |
| Screen refresh rate | UI driven by input loop + timer callbacks |
| Worker scan rate | 2 Hz (500ms intervals) |
| Capture timeout | 5000 ms auto-stop |
| Signal buffer | 2048 samples max |

## 3. Architecture

```
┌─────────────────────────────────────────┐
│  CPU (app thread — UI + input)          │
│  ├─ ViewPort draw callback              │
│  ├─ Input event handler                 │
│  └─ State machine (Screen_Main/Cap/Rep) │
├─────────────────────────────────────────┤
│  Worker Thread (RSSI scan)              │
│  ├─ acquire SPI → set freq → RX → RSSI  │
│  ├─ release SPI                         │
│  └─ sleep 500ms                         │
├─────────────────────────────────────────┤
│  Radio HAL                              │
│  ├─ CC1101 via SPI bus                  │
│  └─ GPIO 15/16                          │
├─────────────────────────────────────────┤
│  Expansion Protocol (DISABLED)          │
└─────────────────────────────────────────┘
```

## 4. Module Breakdown

### 4.1 App Struct (`SubGhzEasyApp`)
Every field grouped by concern. All radio state under `mutex`.

**Thread-safe (mutex-protected):**
- `frequency`, `rssi`, `has_signal`
- `signal_count`, `signals[]`
- `status[48]`
- `screen`, `worker_cmd`

**Thread-local (accessed by owner only):**
- `input_queue`, `view_port`, `gui`
- `worker` thread handle
- `expansion` handle
- `capture_timer` (timer runs in ISR context)

### 4.2 Worker Thread (`subghz_easy_worker_thread`)

```
while cmd != Stop:
    if cmd == Scan and not capturing/replaying:
        update_rssi()
    delay 500ms
```

`update_rssi()` SPI sequence:
```
spi_acquire(&handle_subghz)
→ idle() → set_frequency_and_path(freq) → rx()
spi_release(&handle_subghz)
delay 2ms (PLL settle)
rssi = get_rssi()
spi_acquire(&handle_subghz)
→ idle()
spi_release(&handle_subghz)
mutex_lock()
→ write rssi + has_signal
mutex_unlock()
```

### 4.3 Capture / Replay Module

**Capture flow:**
1. User presses OK on Capture screen
2. `start_capture()` sets `capturing = true`
3. SPI acquire → reset → idle → set_freq → start_async_rx → release
4. Timer starts (5000ms)
5. IRQ callback `capture_callback()` runs per sample in ISR context
6. Buffer full or timer fires → `stop_capture()`
7. SPI acquire → stop_async_rx → idle → release

**Replay flow:**
1. User presses OK on Replay screen
2. `start_replay()` sets `replaying = true`
3. SPI acquire → reset → idle → set_freq → start_async_tx → release
4. IRQ callback `replay_callback()` feeds LevelDuration to CC1101
5. Buffer empty → `stop_replay()`
6. SPI acquire → stop_async_tx → idle → release

### 4.4 File I/O (`.sub` format)

Interoperable with Flipper official SubGHz app.

```
Filetype: Flipper SubGhz Key File
Version: 1
Frequency: 433920000
Preset: FuriHalSubGhzPresetOok650Async
Protocol: RAW
RAW_Data: +500 -300 +400 ...
```

**Storage path:** `ext/apps_data/subghz_cn/`

### 4.5 UI State Machine

| Screen | Input | Action |
|--------|-------|--------|
| Main | Left/Right | `toggle_frequency()` (315 ↔ 433.92) |
| Main | OK | → Capture |
| Capture | OK | `start_capture()` / `stop_capture()` |
| Capture | Down | `save_signal()` |
| Capture | Back | → Main |
| Capture | OK (replay signal) | → Replay (note: flow via Main or direct load) |
| Replay | OK | `start_replay()` / `stop_replay()` |
| Replay | Back | → Main |

## 5. Thread Safety Rules

### 5.1 Mutex Lock Rules
**MUST hold `mutex` when:**
- Reading/writing `frequency`, `rssi`, `has_signal`
- Reading/writing `status[]
- Reading `signal_count` from worker context

**NEVER hold `mutex` when:**
- Calling ANY `furi_hal_subghz_*` (deadlock risk with SPI)
- Calling notification_message (blocks)
- Yielding / delaying

### 5.2 SPI Bus Rules
**MUST acquire before:**
- `furi_hal_subghz_reset()`, `idle()`, `sleep()`
- `furi_hal_subghz_set_frequency_and_path()`
- `furi_hal_subghz_rx()`, `tx()`
- `furi_hal_subghz_start_async_rx/tx()`
- `furi_hal_subghz_stop_async_rx/tx()`
- `furi_hal_subghz_get_rssi()` **ONLY IF** acquire already held

**MUST release after:**
- Same sequence, with matching release call
- Never hold across a sleep/timer start

## 6. Expansion Protocol Guard

On app entry:
```
app->expansion = furi_record_open(RECORD_EXPANSION);
expansion_disable(app->expansion);
```

On app exit:
```
expansion_enable(app->expansion);
furi_record_close(RECORD_EXPANSION);
```

**Why:** SubGHz CC1101 shares GPIO pins 15/16 with the expansion module interface. If expansion protocol is active, radio SPI/I2C operations will silently fail or crash.

## 7. API Reference

### 7.1 Radio Ops

```c
void radio_init(SubGhzEasyApp* app)
    Acquire SPI, reset, idle, release.

void radio_shutdown(SubGhzEasyApp* app)
    Acquire SPI, idle, sleep, release.

void worker_update_rssi(SubGhzEasyApp* app)
    Full SPI sequence + RSSI read + mutex write.
```

### 7.2 Capture Ops

```c
void start_capture(SubGhzEasyApp* app)
    Precondition: !capturing, !replaying
    Postcondition: capturing = true, timer armed

void stop_capture(SubGhzEasyApp* app)
    Precondition: capturing
    Postcondition: capturing = false, async RX stopped, timer stopped
```

### 7.3 Replay Ops

```c
void start_replay(SubGhzEasyApp* app)
    Precondition: !replaying, signal_count > 0
    Postcondition: replaying = true, async TX started

void stop_replay(SubGhzEasyApp* app)
    Precondition: replaying
    Postcondition: replaying = false, async TX stopped
```

### 7.4 File I/O

```c
void save_signal(SubGhzEasyApp* app)
    Precondition: signal_count > 0
    Postcondition: .sub file written to apps_data/subghz_cn/
    Side effect: status[] updated
```

## 8. State Machine Diagram

```
┌──────────┐  OK    ┌──────────┐  OK (rec)  ┌──────────┐
│  Main    │ ──────▶│ Capture  │ ──────────▶│ Replay   │
│  L/R=freq│        │  OK=rec │  Done      │  OK=play │
└──────────┘  Back   └──────────┘  Back      └──────────┘
     │                 │  Down=save
     │ Back=exit       │  Back=Main
     │                 │
```

States:
- `Screen_Main` → freq readout + status + signal indicator
- `Screen_Capture` → sample count + REC status + save
- `Screen_Replay` → playback control + timing count

## 9. Crash Prevention Checklist

| Risk | Mitigation | Line Hint |
|------|-----------|-----------|
| Worker stack overflow | Stack = 4096 bytes | `furi_thread_set_stack_size(..., 4096)` |
| SPI bus contention | Acquire/release every HAL call | All `furi_hal_subghz_*` wrapped |
| GPIO 15/16 conflict | expansion_disable on startup | `app->expansion = open; disable` |
| UI freeze on radio op | Worker thread offloads RSSI | Worker thread, not UI thread |
| Buffer overflow | Stop when signal_count == capacity | `SIGNAL_SAMPLES_MAX` |
| Memory leak on exit | Free all in reverse allocation order | `free(signals); free(app)` |
| Stuck capture | 5000ms timer auto-stop | `furi_timer_start(timer, 5000)` |
| Worker hang | WorkerCmd_Stop + thread_join | `furi_thread_join()` before cleanup |

## 10. Build Info

```bash
ufbt -c   # clean build
ufbt      # compile
```

Output:
- `~/.ufbt/build/subghz_cn_easy.fap`
- `dist/subghz_cn_easy.fap`

## 11. Changelog

| Version | Date | Notes |
|---------|------|-------|
| 3.0 | Jun 2026 | Worker thread, SPI guards, expansion_disable, 4096 stack |
| 2.0 | Jun 2026 | Icons, 128×64 UI alignment, 315/433.92 only toggle |
| 1.0 | Jun 2026 | Initial — UI thread blocked, no SPI guards |
