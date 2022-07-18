#include "gui/view.h"
#include "gui/gui.h"
#include "gui/lighttable.h"
#include "gui/map.h"

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
