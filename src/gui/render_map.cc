
#include "imgui.h"
#include "imgui_internal.h"
#include <GLFW/glfw3.h>

extern "C" {
#include "core/core.h"
#include "core/token.h"
#include "gui/gui.h"
#include "gui/view.h"
#include "gui/maps.h"
#include "core/token.h"
}

void render_map()
{
  // right panel
  {
    ImGui::SetNextWindowPos(ImVec2(d.win_width - d.panel_wd, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(d.panel_wd, d.panel_ht), ImGuiCond_Always);
    ImGui::Begin("RightPanel", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);


    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImVec2 window_size = ImGui::GetWindowSize();
    if(window_size.x != d.panel_wd)
    {
      d.panel_wd = window_size.x;
      dt_rc_set_int(&d.rc, "gui/right_panel_width", d.panel_wd);
      d.swapchainrebuild = 1;
    }
    ImGui::End();  // end right panel
  }
  // Center view
  {
    ImGuiStyle &style = ImGui::GetStyle();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground;
    ImGui::SetNextWindowPos(ImVec2(d.center_x,  d.center_y),  ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(d.center_wd, d.center_ht), ImGuiCond_Always);
    ImGui::Begin("Map", 0, window_flags);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
  //  const float rat = (float)d.center_ht / (float)d.center_wd;

//    d.map->lxmin = d.map->cx - 0.5f * d.map->wd;
//    d.map->lxmax = d.map->cx + 0.5f * d.map->wd;
//    d.map->lymin = d.map->cy - 0.5f * d.map->wd * rat;
//    d.map->lymax = d.map->cy + 0.5f * d.map->wd * rat;

    ap_map_get_region();
    // map origin on window
    double x = -d.map->xm / d.map->pixel_size;
    double y = -d.map->ym / d.map->pixel_size;
    for(int i = 0; i < d.map->region_cnt; i++)
    {
      ap_tile_t *tile = &d.map->tiles[d.map->region[i]];
      if(tile->thumb_status == s_thumb_loaded)
      {
        double tile_size = (double)d.center_wd / d.map->wd / (double)(1<<tile->z); // pixels
        ap_map_bounds_t b;
        b.xm = x + tile->x * tile_size + (double)d.center_x;
        b.ym = y + tile->y * tile_size + (double)d.center_y;
        b.xM = b.xm + tile_size;
        b.yM = b.ym + tile_size;
        int tid = tile->thumbnail;
  //      printf("tile %s status %d thumb %d min %lf,%lf max %lf,%lf\n", dt_token_str(tile->zxy), tile->thumb_status, tid, b.xm, b.ym, b.xM, b.yM);
        const ImU32 col = ImGui::GetColorU32(d.debug ? ((tile->x % 2 == 0 && tile->y % 2 != 0) || (tile->x % 2 != 0 && tile->y % 2 == 0)) ? ImVec4(1,0,1,1) : ImVec4(1,1,0,1) : ImVec4(1,1,1,1));
        window->DrawList->AddImage(d.map->thumbs.thumb[tid].dset, ImVec2(b.xm, b.ym), ImVec2(b.xM, b.yM), {0,0}, {1,1}, col);
        if(d.debug)
          window->DrawList->AddText(0, 0.0f, ImVec2((b.xm+b.xM)*0.5, (b.ym+b.yM)*0.5), 0xff111111u, dt_token_str(tile->zxy));
      }
    }
//    printf("pos %f %f size %f %f limits xm %f xM %f ym %f yM %f\n", (float)d.center_x, (float)d.center_y, (float)d.center_wd, (float)d.center_ht,
//              d.map->lxmin, d.map->lxmax, d.map->lymin, d.map->lymax);

//    printf("pos %f %f size %f %f limits xm %f xM %f ym %f yM %f\n", pos.x, pos.y, size.x, size.y, limits.X.Min, limits.X.Max, limits.Y.Min, limits.Y.Max);
//    printf("pos %f %f size %f %f limits xm   xM   ym   yM  \n", (float)d.center_x, (float)d.center_y, (float)d.center_wd, (float)d.center_ht);
    if(d.debug)
    {
      ImGui::Text("m %lf,%lf M %lf,%lf wd %lf k %d   x %lf y %lf ps %lf", d.map->xm, d.map->ym, d.map->xM, d.map->yM, d.map->wd, d.map->z, x, y, d.map->pixel_size);
    }

    ImGui::End(); // end center lighttable
  }
}

extern "C" int map_leave()
{
  return 0;
}

extern "C" int map_enter()
{
  d.map->xm = 0.0;
  d.map->wd = 1.0;
  d.map->xM = d.map->xm + d.map->wd;
  double ym = (double)d.center_ht / (double)d.center_wd * d.map->wd * 0.5;
  d.map->ym = 0.5 - ym;
  d.map->yM = 0.5 + ym;
  d.map->pixel_size = d.map->wd / (double)d.center_wd;
  d.map->drag = 0;
  printf("xm %lf xM %lf ym %lf yM %lf wd %lf ps %lf\n", d.map->xm, d.map->xM, d.map->ym, d.map->yM, d.map->wd, d.map->pixel_size);
  return 0;
}
