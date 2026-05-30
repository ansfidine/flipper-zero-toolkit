#pragma once

#include <gui/scene_manager.h>

void gpio_sensor_bridge_scene_sensor_on_enter(void* context);
bool gpio_sensor_bridge_scene_sensor_on_event(void* context, SceneManagerEvent event);
void gpio_sensor_bridge_scene_sensor_on_exit(void* context);