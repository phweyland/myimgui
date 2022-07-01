#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

int ap_gui_init_imgui();
void ap_gui_render_frame_imgui();
void ap_gui_cleanup_imgui();
void ap_gui_cleanup_vulkan();

#ifdef __cplusplus
}
#endif
