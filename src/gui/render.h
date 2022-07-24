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
void dt_gui_imgui_keyboard(GLFWwindow *w, int key, int scancade, int action, int mods);
void dt_gui_imgui_mouse_button(GLFWwindow *window, int button, int action, int mods);
void dt_gui_imgui_mouse_position(GLFWwindow *window, double xoff, double yoff);
void dt_gui_imgui_scrolled(GLFWwindow *window, double xoff, double yoff);
#ifdef __cplusplus
}
#endif
