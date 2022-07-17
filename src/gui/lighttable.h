#pragma once
#include "gui/render.h"

static inline void
lighttable_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS)
  {
    if(key == GLFW_KEY_E)
    {
      if(d.img.current_imgid != -1u)
      {
        ap_gui_image_edit(d.img.current_imgid);
      }
    }
#define RATE(X)\
    else if(key == GLFW_KEY_ ## X )\
    {\
      const uint32_t *sel = dt_db_selection_get();\
      for(int i=0;i<d.img.selection_cnt;i++)\
        d.img.images[sel[i]].rating = X;\
    }
    RATE(1)
    RATE(2)
    RATE(3)
    RATE(4)
    RATE(5)
    RATE(0)
#undef RATE
#define LABEL(X)\
    else if(key == GLFW_KEY_F ## X)\
    {\
      const uint32_t *sel = dt_db_selection_get();\
      for(int i=0;i<d.img.selection_cnt;i++)\
        d.img.images[sel[i]].labels ^= 1<<(X-1);\
    }
    LABEL(1)
    LABEL(2)
    LABEL(3)
    LABEL(4)
    LABEL(5)
#undef LABEL
  }
}
