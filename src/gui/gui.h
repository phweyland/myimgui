#pragma once
#include "../db/rc.h"

// forward declare
typedef struct GLFWwindow GLFWwindow;

typedef struct apdt_t
{
  GLFWwindow *window;
  int theme;
  dt_rc_t rc;            // config file
} apdt_t;

extern apdt_t apdt;
extern const char *ap_name;

int ap_gui_init();
void ap_gui_cleanup();
