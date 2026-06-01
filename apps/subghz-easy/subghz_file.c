/*
 * Module: subghz_file.c
 * Purpose: Save/load .sub files compatible with Flipper SubGHz app
 */

#include "subghz_easy.h"

/* ── Save captured signal to .sub file ─────────────────────────────────── */
/** file_save_signal
 *  Format: Flipper SubGHz Key File v1, RAW protocol
 *  Path: ext/apps_data/subghz_cn/sig_<freq>_<NNN>.sub
 *  Status: "Saved!" or "Save err" on failure */
void file_save_signal(SubGhzEasyApp* app) {
    furi_assert(app);
    if(app->signal_count == 0) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        strcpy(app->status, "Nothing!");
        furi_mutex_release(app->mutex);
        return;
    }

    if(!storage_dir_exists(app->storage, SUBGHZ_FOLDER)) {
        storage_simply_mkdir(app->storage, SUBGHZ_FOLDER);
    }

    char path[128];
    int idx = 1;
    do {
        snprintf(path, sizeof(path), "%s/sig_%lu_%03d.sub",
                 SUBGHZ_FOLDER, app->capture_freq / 1000000, idx);
        idx++;
    } while(storage_file_exists(app->storage, path));

    FuriString* buf = furi_string_alloc();
    furi_string_printf(buf, "Filetype: Flipper SubGhz Key File\n");
    furi_string_cat_printf(buf, "Version: 1\n");
    furi_string_cat_printf(buf, "Frequency: %lu\n", app->capture_freq);
    furi_string_cat_printf(buf, "Preset: FuriHalSubGhzPresetOok650Async\n");
    furi_string_cat_printf(buf, "Protocol: RAW\n");
    furi_string_cat_printf(buf, "RAW_Data: ");

    for(uint16_t i = 0; i < app->signal_count; i++) {
        int32_t us = app->signals[i].duration;
        if(!app->signals[i].level) us = -us;
        furi_string_cat_printf(buf, "%ld ", us);
        if((i + 1) % 12 == 0 && i < app->signal_count - 1) {
            furi_string_cat_printf(buf, "\nRAW_Data: ");
        }
    }
    furi_string_cat_printf(buf, "\n");

    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        strcpy(app->status, "Save err");
        furi_mutex_release(app->mutex);
        storage_file_free(file);
        furi_string_free(buf);
        return;
    }

    const char* data = furi_string_get_cstr(buf);
    storage_file_write(file, data, strlen(data));
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(buf);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strcpy(app->status, "Saved!");
    furi_mutex_release(app->mutex);
}
