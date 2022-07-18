#pragma once
#include "gui/gui.h"

typedef struct GLFWwindow GLFWwindow;

int dt_view_switch(dt_gui_view_t view);
void dt_view_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods);
