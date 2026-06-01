# Sub-GHz Easy CN — Public API Reference

## Module: `subghz_worker`

### `FuriThread* subghz_worker_alloc(SubGhzEasyApp* app)`
**Purpose:** Create and initialize the background RSSI worker.
**Parameters:**
- `app` — parent app context (must have `mutex` and `frequency` set)
**Returns:** Allocated `FuriThread` with 4096-byte stack.
**Preconditions:** `app != NULL`, `app->mutex != NULL`
**Postconditions:** Thread is allocated but NOT started. Caller must `furi_thread_start()`.
**Thread safety:** Safe from UI thread only.

### `void subghz_worker_free(FuriThread* worker)`
**Purpose:** Free worker thread resources.
**Parameters:**
- `worker` — pointer returned by `subghz_worker_alloc`
**Preconditions:** Thread has been joined or never started.

### `int32_t subghz_worker_thread(void* context)`
**Internal.** Runs the scan loop. See architecture doc §4.2.

---

## Module: `subghz_radio`

### `void radio_init(Expansion* expansion)`
**Purpose:** Disable expansion protocol and initialize CC1101.
**Preconditions:** App records opened.
**Postconditions:** `expansion_disable()` called, CC1101 in idle state.
**Side effects:** Blocks if SPI bus held by another device. Fails silently on SPI error (CC1101 unresponsive).

### `void radio_shutdown(SubGhzEasyApp* app)`
**Purpose:** Put CC1101 to sleep and re-enable expansion.
**Preconditions:** `app->expansion` valid.
**Postconditions:** CC1101 sleeping, expansion enabled.
**Thread safety:** Must NOT be called while worker or replay active. Call `stop_capture/stop_replay` first.

### `float scan_frequency_rssi(uint32_t freq)`
**Purpose:** Single-shot RSSI read at specified frequency.
**Parameters:**
- `freq` — Hz (must be 315000000 or 433920000)
**Returns:** RSSI in dBm, clamped [-127, 0]. Returns -127 on invalid freq.
**SPI sequence:** Acquire → idle → set_freq → rx → release → delay 2ms → get_rssi → acquire → idle → release.
**Thread safety:** Not thread-safe internally; caller must serialize.

---

## Module: `subghz_capture`

### `void capture_start(SubGhzEasyApp* app)`
**Purpose:** Begin async signal capture.
**Preconditions:** `!capturing`, `!replaying`
**Postconditions:** `capturing = true`, `capture_timer` armed, async RX callback registered.
**SPI sequence:** Acquire → reset → idle → set_freq → start_async_rx → release → timer_start.
**Thread safety:** UI thread only.

### `void capture_stop(SubGhzEasyApp* app)`
**Purpose:** Stop capture and clean up.
**Preconditions:** `capturing == true`
**Postconditions:** `capturing = false`, timer stopped, async RX stopped, CC1101 idle.
**SPI sequence:** Acquire → stop_async_rx → idle → release.
**Thread safety:** UI thread only (called from input handler or timer callback).

### `void capture_callback(bool level, uint32_t duration, void* context)`
**Interrupt context.** Appends sample to `signals[]`. Stops capture if buffer full.
**Thread safety:** ISR context. No locks held. Only writes to `signals[]` and `signal_count` which have no other writers during capture.

---

## Module: `subghz_replay`

### `void replay_start(SubGhzEasyApp* app)`
**Purpose:** Begin async signal replay.
**Preconditions:** `!replaying`, `signal_count > 0`
**Postconditions:** `replaying = true`, async TX callback registered.
**SPI sequence:** Acquire → reset → idle → set_freq → start_async_tx → release.

### `void replay_stop(SubGhzEasyApp* app)`
**Purpose:** Stop replay.
**Preconditions:** `replaying == true`
**Postconditions:** `replaying = false`, async TX stopped, CC1101 idle.
**SPI sequence:** Acquire → stop_async_tx → idle → release.

### `LevelDuration replay_callback(void* context)`
**Interrupt context.** Feeds next `LevelDuration` into CC1101. Resets when buffer exhausted.

---

## Module: `subghz_file`

### `void file_save_signal(SubGhzEasyApp* app)`
**Purpose:** Persist captured signal to `.sub` file.
**Preconditions:** `signal_count > 0`
**Postconditions:** New file `apps_data/subghz_cn/sig_<freq>_<NNN>.sub` created.
**Format:** Flipper SubGHz Key File v1, RAW protocol.
**Error handling:** Silently sets `status` on write failure.

---

## Module: `subghz_ui`

### `void ui_draw_main(Canvas* canvas, SubGhzEasyApp* app)`
**Draws:** Title bar, frequency, RSSI, signal indicator, status, help.
**Thread safety:** Reads `frequency`, `rssi`, `has_signal`, `status` under `mutex_lock()`.

### `void ui_draw_capture(Canvas* canvas, SubGhzEasyApp* app)`
**Draws:** Title, capture freq, sample count, REC indicator, status, help.

### `void ui_draw_replay(Canvas* canvas, SubGhzEasyApp* app)`
**Draws:** Title, timing count, freq, PLAY status, help.

### `void ui_input_handler(InputEvent* event, SubGhzEasyApp* app)`
**Dispatches:** Key events to screen-specific handlers. Posts message to input queue.

---

## Constants

```c
#define SIGNAL_SAMPLES_MAX     2048
#define SUBGHZ_FOLDER          EXT_PATH("apps_data/subghz_cn")
#define FREQ_315               315000000
#define FREQ_433               433920000
#define CC1101_PLL_SETTLE_MS   2
#define CAPTURE_TIMEOUT_MS     5000
#define WORKER_STACK_SIZE      4096
#define WORKER_SCAN_INTERVAL   500  // ms between scans
```

---

## Error Codes (none returned — silent internal state)

App uses `status[48]` for user-facing error reporting:
- `"Ready"` — initial state
- `"REC..."` — capture in progress
- `"Done: N"` — capture complete with N samples
- `"TX..."` — replay in progress
- `"Done"` — replay complete
- `"No signal!"` — replay attempted with empty buffer
- `"Nothing!"` — save attempted with empty buffer
- `"Save err"` — file write failed
- `"Saved!"` — file write success

No numeric error codes. All state is visible in UI.
