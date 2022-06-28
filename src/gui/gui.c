#include "../core/log.h"
#include "gui.h"
#include "render.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <time.h>

const char *ap_name = "apdt";
apdt_t apdt;

void ap_gui_window_resize()
{
  const int pwd = apdt.panel_width_frac * apdt.win_width;
  apdt.center_x = apdt.border_frac * apdt.win_width;
  apdt.center_y = apdt.border_frac * apdt.win_width;
  apdt.scale = -1.0f;
  apdt.panel_wd = dt_rc_get_int(&apdt.rc, "gui/right_panel_width", pwd);
  apdt.center_wd = apdt.win_width * (1.0f - 2.0f * apdt.border_frac) - apdt.panel_wd;
  apdt.center_ht = apdt.win_height - 2*apdt.border_frac * apdt.win_width;
  apdt.panel_ht = apdt.win_height;
}

int ap_gui_init()
{
  if(!glfwInit())
  {
    const char* description;
    glfwGetError(&description);
    dt_log(s_log_gui|s_log_err, "glfwInit() failed: %s", description);
    return 1;
  }
  dt_log(s_log_gui, "glfwGetVersionString() : %s", glfwGetVersionString() );
  if(!glfwVulkanSupported())
  {
    const char* description;
    glfwGetError(&description);
    dt_log(s_log_gui|s_log_err, "glfwVulkanSupported(): no vulkan support found: %s", description);
    glfwTerminate(); // for correctness, if in future we don't just exit(1)
    return 1;
  }

  char configfile[512];
  snprintf(configfile, sizeof(configfile), "%s/.config/%s/config.rc", getenv("HOME"), ap_name);
  dt_rc_init(&apdt.rc);
  dt_rc_read(&apdt.rc, configfile);

  apdt.panel_width_frac = 0.2f;
  apdt.border_frac = 0.02f;
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  // start "full screen"
  apdt.win_width  = mode->width;  //1920;
  apdt.win_height = mode->height; //1080;
  ap_gui_init_imgui();

  return 0;
}

void ap_gui_cleanup()
{
  char configfile[512];
  snprintf(configfile, sizeof(configfile), "%s/.config/%s/config.rc", getenv("HOME"), ap_name);
  dt_rc_write(&apdt.rc, configfile);
  dt_rc_cleanup(&apdt.rc);
  glfwDestroyWindow(apdt.window);
  glfwTerminate();
}

void ap_gui_switch_collection(const char *node, const int type)
{
  if(apdt.images)
    free(apdt.images);
  clock_t beg = clock();
  apdt.image_cnt = ap_db_get_images(node, type, &apdt.images);
  clock_t end = clock();
  dt_log(s_log_perf, "[db] ran get images in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);
}
