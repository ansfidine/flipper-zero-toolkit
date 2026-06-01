/*
 * Sub-GHz Easy CN — Main Header
 * Architecture v3.0, Modular rebuild
 *
 * Boss: Ansfidine Youssouf
 * License: MIT
 */

#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal_subghz.h>
#include <furi_hal_spi.h>
#include <storage/storage.h>
#include <lib/toolbox/level_duration.h>
#include <expansion/expansion.h>

#define TAG "SubGHz_CNEasy"
#define SIGNAL_SAMPLES_MAX 2048
#define SUBGHZ_FOLDER EXT_PATH("apps_data/subghz_cn")

#define FREQ_315 315000000
#define FREQ_433 433920000
#define PLL_SETTLE_MS 2
#define CAPTURE_TIMEOUT_MS 5000
#define WORKER_STACK_SIZE 4096

/* ── Screen enum (UI state machine) ─────────────────────────────────────── */
typedef enum {
    Screen_Main,
    Screen_Capture,
    Screen_Replay,
} Screen;

/* ── Worker command enum (background thread control) ────────────────────── */
typedef enum {
    WorkerCmd_Idle,
    WorkerCmd_Scan,
    WorkerCmd_Stop,
} WorkerCmd;

/* ── Signal timing (one pulse — level + duration in µs) ────────────── */
typedef struct {
    bool level;
    uint32_t duration;
} SignalTiming;

/* ── App context (app-thread + worker-thread shared) ─────────────────── */
typedef struct {
    /* ── UI (app thread) ───────────────────────────────────────────── */
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    Screen screen;
    Storage* storage;

    /* ── Radio state (worker writes, app thread + UI reads) ────────── */
    /* Lock: mutex */
    uint32_t frequency;
    float rssi;
    bool has_signal;

    /* ── Signal capture (app thread writes, replay callback reads) ─── */
    /* No lock needed — capture/replay never overlap */
    SignalTiming* signals;
    uint16_t signal_count;
    uint16_t signal_capacity;
    bool capturing;
    uint32_t capture_freq;
    FuriTimer* capture_timer;

    /* ── Replay (app thread writes, replay callback reads) ─────────── */
    /* No lock needed — capture/replay never overlap */
    bool replaying;
    uint16_t replay_idx;

    /* ── Thread safety (all radio state under this lock) ───────────── */
    FuriMutex* mutex;
    FuriThread* worker;
    WorkerCmd worker_cmd;

    /* ── Expansion guard (GPIO 15/16 release) ───────────────────────── */
    Expansion* expansion;

    /* ── Status message (mutex-protected) ────────────────────────── */
    char status[48];
} SubGhzEasyApp;

/* ══════════════════════════════════════════════════════════════════════════ */
/* Module: radio (subghz_radio.c)                                           */
/* ══════════════════════════════════════════════════════════════════════════ */

/** Disable expansion protocol and initialize CC1101 to idle state.
 *  Must be called before any worker or radio activity.
 *  Preconditions: app->expansion must be valid.
 *  Postconditions: expansion disabled, CC1101 idle, SPI released. */
void radio_init(SubGhzEasyApp* app);

/** Put CC1101 to sleep and re-enable expansion protocol.
 *  Must be called after all radio activity stops.
 *  Preconditions: !capturing, !replaying, worker stopped.
 *  Postconditions: expansion enabled, CC1101 sleeping. */
void radio_shutdown(SubGhzEasyApp* app);

/** Single-shot RSSI scan at current frequency.
 *  Full SPI sequence: acquire, set_freq, rx, release, settle, get_rssi, acquire, idle, release.
 *  Returns: RSSI in dBm, clamped [-127.0, 0.0]. Returns -127.0 on error.
 *  Thread safety: caller must ensure no other radio op in progress. */
float scan_frequency_rssi(SubGhzEasyApp* app);

/* ══════════════════════════════════════════════════════════════════════════ */
/* Module: worker (subghz_worker.c)                                         */
/* ══════════════════════════════════════════════════════════════════════════ */

/** Allocate worker thread with 4096-byte stack.
 *  Returns: allocated FuriThread (not started). */
FuriThread* worker_alloc(SubGhzEasyApp* app);

/** Start the worker thread (enter scan loop). */
void worker_start(FuriThread* worker);

/** Signal worker to stop and wait for thread exit.
 *  Postconditions: worker thread is fully joined and safe to free. */
void worker_stop(SubGhzEasyApp* app);

/** Worker thread entry point (internal — do not call directly). */
int32_t worker_thread_entry(void* context);

/* ══════════════════════════════════════════════════════════════════════════ */
/* Module: capture (subghz_capture.c)                                       */
/* ══════════════════════════════════════════════════════════════════════════ */

/** Begin async signal capture.
 *  Precondition: !capturing && !replaying
 *  Postcondition: capturing = true, timer armed */
void capture_start(SubGhzEasyApp* app);

/** Stop async signal capture.
 *  Postcondition: capturing = false, async RX stopped */
void capture_stop(SubGhzEasyApp* app);

/** Timer callback — auto-stop after CAPTURE_TIMEOUT_MS (internal). */
void capture_timeout_cb(void* context);

/** IRQ callback — append sample to buffer (ISR context, internal). */
void capture_isr_cb(bool level, uint32_t duration, void* context);

/* ══════════════════════════════════════════════════════════════════════════ */
/* Module: replay (subghz_replay.c)                                         */
/* ══════════════════════════════════════════════════════════════════════════ */

/** Begin async signal replay.
 *  Precondition: !replaying && signal_count > 0
 *  Postcondition: replaying = true */
void replay_start(SubGhzEasyApp* app);

/** Stop async signal replay.
 *  Postcondition: replaying = false, async TX stopped */
void replay_stop(SubGhzEasyApp* app);

/** IRQ callback — feed next sample (ISR context, internal). */
LevelDuration replay_isr_cb(void* context);

/* ══════════════════════════════════════════════════════════════════════════ */
/* Module: file (subghz_file.c)                                           */
/* ══════════════════════════════════════════════════════════════════════════ */

/** Save captured signal to .sub file on SD card.
 *  Precondition: signal_count > 0
 *  Path: apps_data/subghz_cn/sig_<freq>_<NNN>.sub
 *  Sets status to "Saved!" or "Save err" */
void file_save_signal(SubGhzEasyApp* app);

/* ══════════════════════════════════════════════════════════════════════════ */
/* Module: UI (subghz_ui.c)                                               */
/* ══════════════════════════════════════════════════════════════════════════ */

/** Canvas draw callback (attached to ViewPort). */
void ui_draw(Canvas* canvas, void* ctx);

/** Input event callback (attached to ViewPort, posts to input_queue). */
void ui_input(InputEvent* event, void* ctx);

/** Toggle frequency between FREQ_315 and FREQ_433 (mutex-safe). */
void frequency_toggle(SubGhzEasyApp* app);

/* ══════════════════════════════════════════════════════════════════════════ */
/* Module: main (subghz_main.c) — entry point                             */
/* ══════════════════════════════════════════════════════════════════════════ */

int32_t subghz_cn_easy_app(void* p);
