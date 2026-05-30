#pragma once

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

/* ── BadUSB Manager — DuckyScript payload runner ────────────────────────── */

/* Max payloads we can list */
#define BADUSB_MAX_PAYLOADS 32
/* Max path length */
#define BADUSB_MAX_PATH_LEN 256
/* Max line length for DuckyScript parsing */
#define BADUSB_MAX_LINE_LEN 512
/* Payload directory on SD card */
#define BADUSB_PAYLOAD_DIR EXT_PATH("badusb")
/* Default keyboard layout for Momentum */
#define BADUSB_LAYOUT_PATH EXT_PATH("/badusb/assets/en-us.kl")

/* View IDs */
typedef enum {
    BadUsbViewSubmenu,
    BadUsbViewWidget,
    BadUsbViewDialogEx,
} BadUsbView;

/* Scene IDs */
typedef enum {
    BadUsbSceneMenu,
    BadUsbScenePayloadList,
    BadUsbSceneConfirm,
    BadUsbSceneRunning,
    BadUsbSceneResult,
    BadUsbSceneAbout,
    BadUsbSceneCount,
} BadUsbScene;

/* Custom events */
typedef enum {
    BadUsbEventConfirmOk,
    BadUsbEventConfirmCancel,
    BadUsbEventPayloadSelected,
} BadUsbEvent;

/* Payload entry */
typedef struct {
    char name[64];
    char path[BADUSB_MAX_PATH_LEN];
} BadUsbPayload;

/* DuckyScript key definitions */
typedef struct {
    uint16_t keycode;
    uint16_t modmask;
} DuckyKey;

/* App context */
typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    DialogEx* dialog_ex;

    /* Payload list */
    BadUsbPayload payloads[BADUSB_MAX_PAYLOADS];
    uint8_t payload_count;
    uint8_t selected_payload;

    /* Execution state */
    bool usb_connected;
    bool running;
    bool success;
    char result_msg[128];

    /* HID config */
    uint16_t vid;
    uint16_t pid;
} BadUsbApp;

/* Scene handlers */
void badusb_scene_menu_on_enter(void* context);
bool badusb_scene_menu_on_event(void* context, SceneManagerEvent event);
void badusb_scene_menu_on_exit(void* context);

void badusb_scene_payload_list_on_enter(void* context);
bool badusb_scene_payload_list_on_event(void* context, SceneManagerEvent event);
void badusb_scene_payload_list_on_exit(void* context);

void badusb_scene_confirm_on_enter(void* context);
bool badusb_scene_confirm_on_event(void* context, SceneManagerEvent event);
void badusb_scene_confirm_on_exit(void* context);

void badusb_scene_running_on_enter(void* context);
bool badusb_scene_running_on_event(void* context, SceneManagerEvent event);
void badusb_scene_running_on_exit(void* context);

void badusb_scene_result_on_enter(void* context);
bool badusb_scene_result_on_event(void* context, SceneManagerEvent event);
void badusb_scene_result_on_exit(void* context);

void badusb_scene_about_on_enter(void* context);
bool badusb_scene_about_on_event(void* context, SceneManagerEvent event);
void badusb_scene_about_on_exit(void* context);

/* DuckyScript engine */
bool badusb_execute_payload(BadUsbApp* app, const char* path);