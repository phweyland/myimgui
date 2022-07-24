#pragma once
#include "gui/render.h"
#include "gui/gui.h"

static inline void lighttable_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS)
  {
    if(key == GLFW_KEY_E && mods & GLFW_MOD_CONTROL)
    {
      if(d.img.current_imgid != -1u)
      {
        ap_gui_image_edit(d.img.current_imgid);
      }
    }
    else if(key == GLFW_KEY_A && mods & GLFW_MOD_CONTROL)
      ap_gui_selection_all();
    else if(key == GLFW_KEY_D && mods & GLFW_MOD_CONTROL)
      dt_gui_selection_clear();
    else if(key == GLFW_KEY_HOME)
      d.scrollpos = s_scroll_top;
    else if(key == GLFW_KEY_END)
      d.scrollpos = s_scroll_bottom;
    else if(key == GLFW_KEY_SPACE)
      d.scrollpos = s_scroll_current;
    else if(key == GLFW_KEY_M)
      dt_view_switch(s_view_map);

#define RATE(X)\
    else if(key == GLFW_KEY_ ## X )\
    {\
      ap_db_set_images_rating(d.img.selection, d.img.selection_cnt, X);\
    }
    RATE(1)
    RATE(2)
    RATE(3)
    RATE(4)
    RATE(5)
    RATE(0)
    else if(key == GLFW_KEY_X)
    {
      ap_db_set_images_rating(d.img.selection, d.img.selection_cnt, -1);
    }
#undef RATE
#define LABEL(X)\
    else if(key == GLFW_KEY_F ## X)\
    {\
      ap_db_toggle_images_labels(d.img.selection, d.img.selection_cnt, 1<<(X-1));\
    }
    LABEL(1)
    LABEL(2)
    LABEL(3)
    LABEL(4)
    LABEL(5)
#undef LABEL
  }
}

static inline void lighttable_mouse_scrolled(GLFWwindow* window, double xoff, double yoff)
{
  dt_gui_imgui_scrolled(window, xoff, yoff);
}

static inline void lighttable_mouse_button(GLFWwindow* window, int button, int action, int mods)
{
  dt_gui_imgui_mouse_button(window, button, action, mods);
}

static inline void lighttable_mouse_position(GLFWwindow* window, double x, double y)
{
  dt_gui_imgui_mouse_position(window, x, y);
}
