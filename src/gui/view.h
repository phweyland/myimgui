#pragma once
#include "gui/gui.h"

typedef struct GLFWwindow GLFWwindow;

int dt_view_switch(dt_gui_view_t view);
void dt_view_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods);
void dt_view_mouse_scrolled(GLFWwindow *window, double xoff, double yoff);
void dt_view_mouse_position(GLFWwindow* window, double x, double y);
void dt_view_mouse_button(GLFWwindow* window, int button, int action, int mods);
