#include "gui/view.h"
#include "gui/gui.h"
#include "gui/lighttable.h"
#include "gui/maps.h"

int lighttable_enter();
int lighttable_leave();
int map_enter();
int map_leave();

int dt_view_switch(dt_gui_view_t view)
{
  int err = 0;
  switch(d.view_mode)
  {
    case s_view_lighttable:
      err = lighttable_leave();
      break;
    case s_view_map:
      err = map_leave();
      break;
    default:;
  }
  if(err) return err;
  dt_gui_view_t old_view = d.view_mode;
  d.view_mode = view;
  switch(d.view_mode)
  {
    case s_view_lighttable:
      err = lighttable_enter();
      break;
    case s_view_map:
      err = map_enter();
      break;
    default:;
  }
  // TODO: reshuffle this stuff so we don't have to re-enter the old view?
  if(err)
  {
    d.view_mode = old_view;
    return err;
  }
  return 0;
}

void dt_view_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  switch(d.view_mode)
  {
  case s_view_lighttable:
    lighttable_keyboard(window, key, scancode, action, mods);
    break;
  case s_view_map:
    map_keyboard(window, key, scancode, action, mods);
    break;
  default:;
  }
}

void dt_view_mouse_scrolled(GLFWwindow *window, double xoff, double yoff)
{
  switch(d.view_mode)
  {
  case s_view_lighttable:
    lighttable_mouse_scrolled(window, xoff, yoff);
    break;
  case s_view_map:
    map_mouse_scrolled(window, xoff, yoff);
    break;
  default:;
  }
}

void dt_view_mouse_button(GLFWwindow *window, int button, int action, int mods)
{
  // unfortunately this is always true:
  // if(dt_gui_imgui_want_mouse()) return;
  switch(d.view_mode)
  {
  case s_view_lighttable:
    lighttable_mouse_button(window, button, action, mods);
    break;
  case s_view_map:
    map_mouse_button(window, button, action, mods);
    break;
  default:;
  }
}

void dt_view_mouse_position(GLFWwindow *window, double x, double y)
{
  switch(d.view_mode)
  {
  case s_view_lighttable:
    lighttable_mouse_position(window, x, y);
    break;
  case s_view_map:
    map_mouse_position(window, x, y);
    break;
  default:;
  }
}
