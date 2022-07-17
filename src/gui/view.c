#include "gui/view.h"
#include "gui/gui.h"
#include "gui/lighttable.h"

void dt_view_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  switch(d.view_mode)
  {
  case s_view_lighttable:
    lighttable_keyboard(window, key, scancode, action, mods);
    break;
  case s_view_map:
//    map_keyboard(window, key, scancode, action, mods);
    break;
  default:;
  }
}
