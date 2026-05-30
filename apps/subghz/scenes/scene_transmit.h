#pragma once

#include "../subghz_pentest.h"

void subghz_pentest_scene_transmit_on_enter(void* context);
bool subghz_pentest_scene_transmit_on_event(void* context, SceneManagerEvent event);
void subghz_pentest_scene_transmit_on_exit(void* context);