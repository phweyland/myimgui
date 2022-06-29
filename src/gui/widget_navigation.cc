#include "widget_navigation.hh"
#include "imgui.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
extern "C" {
#include "../db/rc.h"
#include "../gui/gui.h"
#include "../core/log.h"
}

void ap_navigation_init(ap_navigation_widget_t *w, const int type)
{
  memset(w, 0, sizeof(*w));
  w->type = type;
  snprintf(w->sep, sizeof(w->sep), "%s", type == 0 ? "/" : "|");
  const char *node = type == 0 ? dt_rc_get(&apdt.rc, "gui/last_folder", "")
                               : dt_rc_get(&apdt.rc, "gui/last_tag", "");
  if(node[0])
    snprintf(w->node, sizeof(w->node), "%s", node);
  w->count = -1u;
}

static void _update_node(ap_navigation_widget_t *w)
{
  if(w->subnodes)
    free(w->subnodes);
  w->subnodes = 0;
  w->count = -1u;
}

void ap_navigation_open(ap_navigation_widget_t *w)
{
  if(w->count == -1u)
  {
    clock_t beg = clock();
    w->count = ap_db_get_subnodes(w->node, w->type, &w->subnodes);
    clock_t end = clock();
    dt_log(s_log_perf, "[db] ran get nodes in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);
    w->selected = 0;
  }

  if (ImGui::Button("Open"))
    ap_gui_switch_collection(w->node, w->type);
  char current[256];
  snprintf(current, sizeof(current), "%s", w->node[0] ? w->node : "root");
  ImGui::SameLine();
  ImGui::Text(current);

  ImGui::BeginChild("nodelist", ImVec2(0, ImGui::GetFontSize() * 20.0f), 0);

  char name[260];
  int selected = 0;
  snprintf(name, sizeof(name), "%s", "..");
  if(ImGui::Selectable(name, selected, 0))
  {// go up one node
    int len = strnlen(w->node, sizeof(w->node));
    char *c = w->node + len;
    while(c >= w->node && *c != w->sep[0])
      *(c--) = 0;
    if(c >= w->node && *c == w->sep[0])
      *c = 0;
    _update_node(w);
  }
  // display list of nodes
  ap_nodes_t *node = w->subnodes;
  for(int i = 0; i < w->count; i++)
  {
    snprintf(name, sizeof(name), "%s", node->name);
    selected = node->name == w->selected;
    if(ImGui::Selectable(name, selected, 0))
    {
      w->selected = node->name; // mark as selected
      // change node by appending to the string..
      int len = strnlen(w->node, sizeof(w->node));
      if(len)
      {
        char *c = w->node;
        snprintf(c+len, sizeof(w->node)-len-1, "%s%s", w->sep, node->name);
      }
      else
        snprintf(w->node, sizeof(w->node), "%s", node->name);
      _update_node(w);
    }
    node++;
  }
  ImGui::EndChild();
}
