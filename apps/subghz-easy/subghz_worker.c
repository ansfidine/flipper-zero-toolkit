/*
 * Module: subghz_worker.c
 * Purpose: Background RSSI worker thread (2 Hz, mutex-protected)
 *
 * Boss: Ansfidine Youssouf
 */

#include "subghz_easy.h"

/* ── Worker thread entry point ─────────────────────────────────────────── */
/** worker_thread_entry
 *  Loop: check cmd → if Scan and radio free → update_rssi() → sleep 500ms
 *  Exits when worker_cmd == WorkerCmd_Stop.
 *  Stack: 4096 bytes (set at allocation time in main.c). */
int32_t worker_thread_entry(void* context) {
    SubGhzEasyApp* app = context;
    furi_assert(app);

    while(true) {
        WorkerCmd cmd;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        cmd = app->worker_cmd;
        furi_mutex_release(app->mutex);

        if(cmd == WorkerCmd_Stop) break;

        if(cmd == WorkerCmd_Scan) {
            bool active;
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            active = app->capturing || app->replaying;
            furi_mutex_release(app->mutex);

            if(!active) {
                float rssi = scan_frequency_rssi(app);

                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->rssi = rssi;
                app->has_signal = (rssi > -67.0f);
                furi_mutex_release(app->mutex);
            }
        }

        furi_delay_ms(500);
    }
    return 0;
}

/* ── Allocate worker ──────────────────────────────────────────────────── */
/** worker_alloc
 *  Sets name, stack size, context, callback. Returns unfired thread.
 *  Caller: must call worker_start() afterward. */
FuriThread* worker_alloc(SubGhzEasyApp* app) {
    FuriThread* worker = furi_thread_alloc();
    furi_thread_set_name(worker, "SubGhzEasyWorker");
    furi_thread_set_stack_size(worker, WORKER_STACK_SIZE);
    furi_thread_set_context(worker, app);
    furi_thread_set_callback(worker, worker_thread_entry);
    return worker;
}

/* ── Start worker (begin RTOS scheduling) ────────────────────────────── */
void worker_start(FuriThread* worker) {
    furi_assert(worker);
    furi_thread_start(worker);
}

/* ── Stop + join worker (graceful thread drain) ────────────────────────── */
/** worker_stop
 *  Sets cmd = Stop, joins thread, frees handle.
 *  Must be called NO LATER than shutdown (after capture/replay stopped). */
void worker_stop(SubGhzEasyApp* app) {
    furi_assert(app && app->worker);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->worker_cmd = WorkerCmd_Stop;
    furi_mutex_release(app->mutex);

    furi_thread_join(app->worker);
    furi_thread_free(app->worker);
    app->worker = NULL;
}
