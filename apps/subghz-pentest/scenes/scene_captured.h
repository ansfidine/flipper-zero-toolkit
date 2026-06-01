#pragma once

#include <gui/scene_manager.h>

void subghz_pentest_scene_captured_on_enter(void* context);
bool subghz_pentest_scene_captured_on_event(void* context, SceneManagerEvent event);
void subghz_pentest_scene_captured_on_exit(void* context);