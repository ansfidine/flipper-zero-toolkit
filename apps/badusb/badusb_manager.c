#include "badusb_manager.h"

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG "BadUSB"

/* ── DuckyScript engine ─────────────────────────────────────────────── */

/* Parse modifier keys from string like "CTRL SHIFT ALT" */
static uint16_t ducky_parse_key(const char* key) {
    /* Function keys */
    if(!strcasecmp(key, "ENTER") || !strcasecmp(key, "RETURN")) return HID_KEYBOARD_RETURN;
    if(!strcasecmp(key, "ESC") || !strcasecmp(key, "ESCAPE")) return HID_KEYBOARD_ESCAPE;
    if(!strcasecmp(key, "BACKSPACE")) return HID_KEYBOARD_DELETE;
    if(!strcasecmp(key, "TAB")) return HID_KEYBOARD_TAB;
    if(!strcasecmp(key, "SPACE")) return HID_KEYBOARD_SPACEBAR;
    if(!strcasecmp(key, "DELETE") || !strcasecmp(key, "DEL")) return HID_KEYBOARD_DELETE;
    if(!strcasecmp(key, "INSERT")) return HID_KEYBOARD_INSERT;
    if(!strcasecmp(key, "HOME")) return HID_KEYBOARD_HOME;
    if(!strcasecmp(key, "END")) return HID_KEYBOARD_END;
    if(!strcasecmp(key, "PAGEUP")) return HID_KEYBOARD_PAGE_UP;
    if(!strcasecmp(key, "PAGEDOWN")) return HID_KEYBOARD_PAGE_DOWN;
    if(!strcasecmp(key, "UP")) return HID_KEYBOARD_UP_ARROW;
    if(!strcasecmp(key, "DOWN")) return HID_KEYBOARD_DOWN_ARROW;
    if(!strcasecmp(key, "LEFT")) return HID_KEYBOARD_LEFT_ARROW;
    if(!strcasecmp(key, "RIGHT")) return HID_KEYBOARD_RIGHT_ARROW;
    if(!strcasecmp(key, "CAPSLOCK")) return HID_KEYBOARD_CAPS_LOCK;
    if(!strcasecmp(key, "F1")) return HID_KEYBOARD_F1;
    if(!strcasecmp(key, "F2")) return HID_KEYBOARD_F2;
    if(!strcasecmp(key, "F3")) return HID_KEYBOARD_F3;
    if(!strcasecmp(key, "F4")) return HID_KEYBOARD_F4;
    if(!strcasecmp(key, "F5")) return HID_KEYBOARD_F5;
    if(!strcasecmp(key, "F6")) return HID_KEYBOARD_F6;
    if(!strcasecmp(key, "F7")) return HID_KEYBOARD_F7;
    if(!strcasecmp(key, "F8")) return HID_KEYBOARD_F8;
    if(!strcasecmp(key, "F9")) return HID_KEYBOARD_F9;
    if(!strcasecmp(key, "F10")) return HID_KEYBOARD_F10;
    if(!strcasecmp(key, "F11")) return HID_KEYBOARD_F11;
    if(!strcasecmp(key, "F12")) return HID_KEYBOARD_F12;
    if(!strcasecmp(key, "NUMLOCK")) return HID_KEYBOARD_CAPS_LOCK;

    /* Single characters handled via ASCII map */
    if(strlen(key) == 1) {
        char c = key[0];
        if(c >= 0x20 && c < 0x7F) {
            return HID_ASCII_TO_KEY(c);
        }
    }
    return HID_KEYBOARD_NONE;
}

/* Type a string using HID keyboard */
static void ducky_type_string(const char* str) {
    for(size_t i = 0; i < strlen(str); i++) {
        uint16_t key = HID_ASCII_TO_KEY(str[i]);
        if(key != HID_KEYBOARD_NONE) {
            uint16_t mod = key & 0xFF00;
            uint8_t keycode = key & 0xFF;
            if(mod) furi_hal_hid_kb_press(mod);
            furi_hal_hid_kb_press(keycode);
            furi_delay_ms(5);
            furi_hal_hid_kb_release(keycode);
            if(mod) furi_hal_hid_kb_release(mod);
            furi_delay_ms(5);
        }
    }
}

/* Press and release a key with optional modifiers */
static void ducky_press_key(uint16_t keycode, uint16_t mods) {
    if(mods) furi_hal_hid_kb_press(mods);
    if(keycode != HID_KEYBOARD_NONE) {
        furi_hal_hid_kb_press(keycode);
        furi_delay_ms(10);
        furi_hal_hid_kb_release(keycode);
    }
    if(mods) furi_hal_hid_kb_release(mods);
    furi_delay_ms(10);
}

/* Execute a DuckyScript payload from file */
bool badusb_execute_payload(BadUsbApp* app, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open payload: %s", path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        snprintf(app->result_msg, sizeof(app->result_msg), "File not found");
        return false;
    }

    /* Release all keys first - safety measure */
    furi_hal_hid_kb_release_all();

    /* Get file size via storage_common_stat */
    FileInfo file_info;
    FS_Error stat_err = storage_common_stat(storage, path, &file_info);
    if(stat_err != FSE_OK || file_info.size == 0 || file_info.size > 32768) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        snprintf(app->result_msg, sizeof(app->result_msg), 
                 (stat_err != FSE_OK) ? "Cannot stat file" : (file_info.size == 0 ? "Empty file" : "File too large"));
        return false;
    }
    uint64_t file_size = file_info.size;

    char* buf = malloc(file_size + 1);
    size_t total_read = 0;
    storage_file_seek(file, 0, true);
    while(total_read < file_size) {
        size_t chunk = storage_file_read(file, buf + total_read, file_size - total_read);
        if(chunk == 0) break;
        total_read += chunk;
    }
    buf[total_read] = '\0';
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    /* Parse lines from buffer */
    char* line = buf;
    uint32_t default_delay = 0;
    bool comment_mode = false;
    int line_num = 0;

    while(line && *line) {
        /* Find end of current line */
        char* eol = strchr(line, '\n');
        if(eol) *eol = '\0';

        /* Strip CR */
        size_t len = strlen(line);
        while(len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' '))
            line[--len] = '\0';

        line_num++;

        /* Skip empty lines */
        if(len == 0) {
            line = eol ? eol + 1 : NULL;
            continue;
        }

        /* Skip comments */
        if(strncasecmp(line, "REM ", 4) == 0 || strncasecmp(line, "REM\t", 4) == 0 ||
           strncmp(line, "//", 2) == 0) {
            line = eol ? eol + 1 : NULL;
            continue;
        }
        if(strcasecmp(line, "REM") == 0) {
            comment_mode = true;
            line = eol ? eol + 1 : NULL;
            continue;
        }
        if(comment_mode) {
            if(strcasecmp(line, "END_REM") == 0) comment_mode = false;
            line = eol ? eol + 1 : NULL;
            continue;
        }

        /* DELAY */
        if(strncasecmp(line, "DELAY ", 6) == 0) {
            uint32_t ms = atoi(line + 6);
            if(ms > 0) furi_delay_ms(ms);
            line = eol ? eol + 1 : NULL;
            continue;
        }

        /* DEFAULT_DELAY / DEFAULTDELAY */
        if(strncasecmp(line, "DEFAULT_DELAY ", 14) == 0 ||
           strncasecmp(line, "DEFAULTDELAY ", 13) == 0) {
            default_delay = atoi(strchr(line, ' ') + 1);
            line = eol ? eol + 1 : NULL;
            continue;
        }

        /* STRING */
        if(strncasecmp(line, "STRINGLN ", 9) == 0) {
            ducky_type_string(line + 9);
            ducky_press_key(HID_KEYBOARD_RETURN, 0);
            if(default_delay) furi_delay_ms(default_delay);
            line = eol ? eol + 1 : NULL;
            continue;
        }
        if(strncasecmp(line, "STRING ", 7) == 0) {
            ducky_type_string(line + 7);
            if(default_delay) furi_delay_ms(default_delay);
            line = eol ? eol + 1 : NULL;
            continue;
        }

        /* Single keys */
        if(strcasecmp(line, "ENTER") == 0 || strcasecmp(line, "RETURN") == 0) {
            ducky_press_key(HID_KEYBOARD_RETURN, 0);
        } else if(strcasecmp(line, "TAB") == 0) {
            ducky_press_key(HID_KEYBOARD_TAB, 0);
        } else if(strcasecmp(line, "SPACE") == 0) {
            ducky_press_key(HID_KEYBOARD_SPACEBAR, 0);
        } else if(strcasecmp(line, "ESCAPE") == 0 || strcasecmp(line, "ESC") == 0) {
            ducky_press_key(HID_KEYBOARD_ESCAPE, 0);
        } else if(strcasecmp(line, "BACKSPACE") == 0) {
            ducky_press_key(HID_KEYBOARD_DELETE, 0);
        } else {
            /* Try modifier + key combo: GUI r, CTRL SHIFT ESC, etc */
            uint16_t mods = 0;
            const char* ptr = line;
            while(ptr && *ptr) {
                if(strncasecmp(ptr, "CTRL ", 5) == 0 || strncasecmp(ptr, "CONTROL ", 8) == 0) {
                    mods |= KEY_MOD_LEFT_CTRL;
                    ptr = strchr(ptr, ' ') + 1;
                } else if(strncasecmp(ptr, "SHIFT ", 6) == 0) {
                    mods |= KEY_MOD_LEFT_SHIFT;
                    ptr = strchr(ptr, ' ') + 1;
                } else if(strncasecmp(ptr, "ALT ", 4) == 0) {
                    mods |= KEY_MOD_LEFT_ALT;
                    ptr = strchr(ptr, ' ') + 1;
                } else if(strncasecmp(ptr, "GUI ", 4) == 0 ||
                          strncasecmp(ptr, "WINDOWS ", 8) == 0 ||
                          strncasecmp(ptr, "COMMAND ", 8) == 0 ||
                          strncasecmp(ptr, "CMD ", 4) == 0) {
                    mods |= KEY_MOD_LEFT_GUI;
                    ptr = strchr(ptr, ' ') + 1;
                } else {
                    break;
                }
            }
            if(mods) {
                uint16_t keycode = ducky_parse_key(ptr);
                ducky_press_key(keycode, mods);
            } else {
                /* Single key */
                uint16_t key = ducky_parse_key(line);
                if(key != HID_KEYBOARD_NONE) {
                    ducky_press_key(key, 0);
                } else {
                    FURI_LOG_W(TAG, "Unknown cmd line %d: %s", line_num, line);
                }
            }
        }
        if(default_delay) furi_delay_ms(default_delay);
        line = eol ? eol + 1 : NULL;
    }

    /* Safety: release all keys after execution */
    furi_hal_hid_kb_release_all();
    free(buf);

    snprintf(app->result_msg, sizeof(app->result_msg), "Done (%d lines)", line_num);
    return true;
}
/* ── Scan payload directory ─────────────────────────────────────────── */

static uint8_t badusb_scan_payloads(BadUsbApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    if(!storage_dir_open(dir, BADUSB_PAYLOAD_DIR)) {
        FURI_LOG_W(TAG, "No /badusb directory found");
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return 0;
    }

    app->payload_count = 0;
    FileInfo info;
    char filename[128];

    while(storage_dir_read(dir, &info, filename, sizeof(filename))) {
        if(app->payload_count >= BADUSB_MAX_PAYLOADS) break;
        /* Only .txt files */
        size_t len = strlen(filename);
        if(len < 5 || strcasecmp(filename + len - 4, ".txt") != 0) continue;

        /* Build full path */
        snprintf(app->payloads[app->payload_count].path,
                 BADUSB_MAX_PATH_LEN, "%s/%s", BADUSB_PAYLOAD_DIR, filename);

        /* Display name without .txt */
        strncpy(app->payloads[app->payload_count].name, filename,
                sizeof(app->payloads[app->payload_count].name) - 1);
        app->payloads[app->payload_count].name[len - 4] = '\0';

        app->payload_count++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Found %d payloads", app->payload_count);
    return app->payload_count;
}

/* ── Scene handlers ─────────────────────────────────────────────────── */

/* ── Menu scene ─────────────────────────────────────────────────────── */
void badusb_scene_menu_on_enter(void* context) {
    BadUsbApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "BadUSB Manager v1.0");

    submenu_add_item(app->submenu, "Run Payload", 0, NULL, NULL);
    submenu_add_item(app->submenu, "About", 1, NULL, NULL);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbViewSubmenu);
}

bool badusb_scene_menu_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case 0: /* Run Payload */
            scene_manager_next_scene(app->scene_manager, BadUsbScenePayloadList);
            return true;
        case 1: /* About */
            scene_manager_next_scene(app->scene_manager, BadUsbSceneAbout);
            return true;
        }
    }
    return false;
}

void badusb_scene_menu_on_exit(void* context) {
    UNUSED(context);
}

/* ── Payload list scene ─────────────────────────────────────────────── */
void badusb_scene_payload_list_on_enter(void* context) {
    BadUsbApp* app = context;
    badusb_scan_payloads(app);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select Payload");

    if(app->payload_count == 0) {
        submenu_add_item(app->submenu, "No payloads found", 0, NULL, NULL);
        submenu_add_item(app->submenu, "Add .txt to /badusb/", 1, NULL, NULL);
    } else {
        for(uint8_t i = 0; i < app->payload_count; i++) {
            submenu_add_item(app->submenu, app->payloads[i].name, i, NULL, NULL);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbViewSubmenu);
}

bool badusb_scene_payload_list_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < app->payload_count) {
            app->selected_payload = event.event;
            scene_manager_next_scene(app->scene_manager, BadUsbSceneConfirm);
            return true;
        }
    }
    return false;
}

void badusb_scene_payload_list_on_exit(void* context) {
    UNUSED(context);
}

/* ── Confirm scene ──────────────────────────────────────────────────── */
void badusb_scene_confirm_on_enter(void* context) {
    BadUsbApp* app = context;
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_header(app->dialog_ex, "Run Payload?", 64, 12, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog_ex,
                       app->payloads[app->selected_payload].name, 64, 32,
                       AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "Cancel");
    dialog_ex_set_right_button_text(app->dialog_ex, "Run");
    dialog_ex_set_result_callback(app->dialog_ex, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbViewDialogEx);
}

bool badusb_scene_confirm_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BadUsbEventConfirmOk) {
            scene_manager_next_scene(app->scene_manager, BadUsbSceneRunning);
            return true;
        }
    }
    return false;
}

void badusb_scene_confirm_on_exit(void* context) {
    UNUSED(context);
}

/* ── Running scene ───────────────────────────────────────────────────── */
void badusb_scene_running_on_enter(void* context) {
    BadUsbApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignTop,
                              FontPrimary, "Executing...");
    widget_add_string_element(app->widget, 64, 24, AlignCenter, AlignTop,
                              FontSecondary, app->payloads[app->selected_payload].name);
    widget_add_string_element(app->widget, 64, 40, AlignCenter, AlignTop,
                              FontSecondary, "Press BACK to abort");

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbViewWidget);

    /* Switch to HID mode and execute */
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(&usb_hid, &((FuriHalUsbHidConfig){
        .vid = app->vid ?: HID_VID_DEFAULT,
        .pid = app->pid ?: HID_PID_DEFAULT,
        .manuf = "Logitech",
        .product = "USB Receiver",
    }));

    furi_delay_ms(500);

    /* Check if USB is connected */
    app->usb_connected = furi_hal_hid_is_connected();

    if(app->usb_connected) {
        app->success = badusb_execute_payload(app, app->payloads[app->selected_payload].path);
    } else {
        snprintf(app->result_msg, sizeof(app->result_msg), "USB not connected!");
        app->success = false;
    }

    /* Switch back to CDC mode */
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(&usb_cdc_single, NULL);

    scene_manager_next_scene(app->scene_manager, BadUsbSceneResult);
}

bool badusb_scene_running_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void badusb_scene_running_on_exit(void* context) {
    UNUSED(context);
}

/* ── Result scene ────────────────────────────────────────────────────── */
void badusb_scene_result_on_enter(void* context) {
    BadUsbApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignTop,
                              FontPrimary, app->success ? "Complete" : "Failed");
    widget_add_string_element(app->widget, 64, 24, AlignCenter, AlignTop,
                              FontSecondary, app->result_msg);

    if(!app->usb_connected) {
        widget_add_string_element(app->widget, 64, 40, AlignCenter, AlignTop,
                                  FontSecondary, "Connect USB and try again");
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbViewWidget);
}

bool badusb_scene_result_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void badusb_scene_result_on_exit(void* context) {
    UNUSED(context);
}

/* ── About scene ─────────────────────────────────────────────────────── */
void badusb_scene_about_on_enter(void* context) {
    BadUsbApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 4, AlignCenter, AlignTop,
                              FontPrimary, "BadUSB Manager v1.0");
    widget_add_string_element(app->widget, 0, 18, AlignLeft, AlignTop,
                              FontSecondary, "DuckyScript payload runner");
    widget_add_string_element(app->widget, 0, 29, AlignLeft, AlignTop,
                              FontSecondary, "for Flipper Zero");
    widget_add_string_element(app->widget, 0, 40, AlignLeft, AlignTop,
                              FontSecondary, "Put .txt payloads in");
    widget_add_string_element(app->widget, 0, 51, AlignLeft, AlignTop,
                              FontSecondary, "/badusb/ on SD card");
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbViewWidget);
}

bool badusb_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void badusb_scene_about_on_exit(void* context) {
    UNUSED(context);
}

/* ── App lifecycle ───────────────────────────────────────────────────── */

static bool badusb_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BadUsbApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool badusb_back_event_callback(void* context) {
    furi_assert(context);
    BadUsbApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

int32_t badusb_manager_app(void* p) {
    UNUSED(p);

    BadUsbApp* app = malloc(sizeof(BadUsbApp));
    app->vid = HID_VID_DEFAULT;
    app->pid = HID_PID_DEFAULT;

    /* Setup view dispatcher */
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    /* view_dispatcher_enable_queue is deprecated, queue is always enabled */
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, badusb_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, badusb_back_event_callback);

    /* Setup scene manager */
    static const AppSceneOnEnterCallback on_enter[] = {
        [BadUsbSceneMenu] = badusb_scene_menu_on_enter,
        [BadUsbScenePayloadList] = badusb_scene_payload_list_on_enter,
        [BadUsbSceneConfirm] = badusb_scene_confirm_on_enter,
        [BadUsbSceneRunning] = badusb_scene_running_on_enter,
        [BadUsbSceneResult] = badusb_scene_result_on_enter,
        [BadUsbSceneAbout] = badusb_scene_about_on_enter,
    };
    static const AppSceneOnEventCallback on_event[] = {
        [BadUsbSceneMenu] = badusb_scene_menu_on_event,
        [BadUsbScenePayloadList] = badusb_scene_payload_list_on_event,
        [BadUsbSceneConfirm] = badusb_scene_confirm_on_event,
        [BadUsbSceneRunning] = badusb_scene_running_on_event,
        [BadUsbSceneResult] = badusb_scene_result_on_event,
        [BadUsbSceneAbout] = badusb_scene_about_on_event,
    };
    static const AppSceneOnExitCallback on_exit[] = {
        [BadUsbSceneMenu] = badusb_scene_menu_on_exit,
        [BadUsbScenePayloadList] = badusb_scene_payload_list_on_exit,
        [BadUsbSceneConfirm] = badusb_scene_confirm_on_exit,
        [BadUsbSceneRunning] = badusb_scene_running_on_exit,
        [BadUsbSceneResult] = badusb_scene_result_on_exit,
        [BadUsbSceneAbout] = badusb_scene_about_on_exit,
    };
    static const SceneManagerHandlers scene_handlers = {
        .on_enter_handlers = on_enter,
        .on_event_handlers = on_event,
        .on_exit_handlers = on_exit,
        .scene_num = BadUsbSceneCount,
    };
    app->scene_manager = scene_manager_alloc(&scene_handlers, app);

    /* Allocate views */
    app->submenu = submenu_alloc();
    app->widget = widget_alloc();
    app->dialog_ex = dialog_ex_alloc();

    view_dispatcher_add_view(app->view_dispatcher, BadUsbViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, BadUsbViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, BadUsbViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Enter main scene */
    scene_manager_next_scene(app->scene_manager, BadUsbSceneMenu);

    view_dispatcher_run(app->view_dispatcher);

    /* Cleanup */
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    widget_free(app->widget);
    dialog_ex_free(app->dialog_ex);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}