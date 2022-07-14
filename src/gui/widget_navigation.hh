#pragma once
//#include <glib.h>
extern "C" {
#include "db/database.h"
}
// store some state
struct ap_navigation_widget_t
{
  int counter;      // entries number
  char node[512];   // current working folder/tag
  ap_nodes_t *subnodes; // list of subnodes
  int count;        // nb subnodes
  char *selected;   // selected
  char sep[4];      // '/' or '|'
  int type;         // 0: folder; 1: tags
};

void ap_navigation_init(ap_navigation_widget_t *w, const int type);

void ap_navigation_open(ap_navigation_widget_t *w);
