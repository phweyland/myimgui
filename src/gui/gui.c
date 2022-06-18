#include "../core/log.h"
#include "gui.h"
#include "render.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdlib.h>

const char *ap_name = "apdt";
apdt_t apdt;

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
