
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

const char *map_source[] = {
  "OpenStreetMap",
  "GoogleMap",
  "GoogleSatellite",
  "GoogleHybrid",
  NULL
};

void _draw_tile(ImGuiWindow* window , ap_tile_t *tile, ap_map_bounds_t *b)
{
  uint32_t th = tile->thumbnail;
  const ImU32 col = ImGui::GetColorU32(d.debug ? ((tile->x % 2 == 0 && tile->y % 2 != 0) || (tile->x % 2 != 0 && tile->y % 2 == 0)) ? ImVec4(1,0.7,1,1) : ImVec4(1,1,0.7,1) : ImVec4(1,1,1,1));
  window->DrawList->AddImage(d.map->thumbs.thumb[th].dset, ImVec2(b->xm, b->ym), ImVec2(b->xM, b->yM), {0,0}, {1,1}, col);
  if(d.debug)
  {
    char text[100];
    snprintf(text, sizeof(text),"%s\nti %ld th %d", dt_token_str(tile->zxy), tile - d.map->tiles, th);
    window->DrawList->AddText(0, 0.0f, ImVec2(b->xm, (b->ym+b->yM)*0.5), 0xff111111u, text);
  }
}

void render_map()
{
  // right panel
  {
    ImGui::SetNextWindowPos(ImVec2(d.win_width - d.panel_wd, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(d.panel_wd, d.panel_ht), ImGuiCond_Always);
    ImGui::Begin("RightPanel", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    if(ImGui::CollapsingHeader("Preferences"))
    {
      const char* combo_preview_value = map_source[d.map->source];
      if (ImGui::BeginCombo("source", combo_preview_value, 0))
      {
        for (int n = 0; map_source[n]; n++)
        {
          const bool is_selected = (d.map->source == n);
          if (ImGui::Selectable(map_source[n], is_selected))
          {
            d.map->source = n;
            ap_map_source_set[n]();
            dt_rc_set_int(&d.rc, "map_source", n);
          }
          // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
          if (is_selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
    }

    if(ImGui::CollapsingHeader("Debug"))
    {
      ImGui::Text("z: %d", d.map->z);
      ImGui::Separator();
      ImGui::Text("Region covered/count: %d/%d / max: %d", d.map->region_cov, d.map->region_cnt, d.map->region_max);
      ImGui::Text("Tiles lru: %ld mru: %ld / max: %d", d.map->lru - d.map->tiles, d.map->mru - d.map->tiles, d.map->tiles_max);
      ImGui::Text("Tile req in: %d out: %d / nb: %d", d.map->thumbs.cache_req.in,d.map->thumbs.cache_req.out, d.map->thumbs.cache_req.nb);
      ImGui::Text("Thumbs lru: %ld mru: %ld / max: %d", d.map->thumbs.lru - d.map->thumbs.thumb, d.map->thumbs.mru - d.map->thumbs.thumb, d.map->thumbs.thumb_max);
      ImGui::Separator();
      ImGui::Text("Window on map: min %lf,%lf max %lf,%lf", d.map->xm, d.map->ym, d.map->xM, d.map->yM);
      ImGui::Text("Mouse win %.0lf,%.0lf map %lf,%lf", d.map->mwx, d.map->mwy, d.map->mmx, d.map->mmy);
      ImGui::Text("Tile %d %s th %d status %d", d.map->mtile, d.map->mtile != -1u ? dt_token_str(d.map->tiles[d.map->mtile].zxy) : "none",
                  d.map->mtile != -1u ? d.map->tiles[d.map->mtile].thumbnail : -1u, d.map->mtile != -1u ? d.map->tiles[d.map->mtile].thumb_status : -1u);
      ImGui::Separator();
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    }

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
    ap_map_get_region();
    // map origin on window
    double x0 = ap_map2win_x(0.0);
    double y0 = ap_map2win_y(0.0);
    double xm = ap_map2win_x(d.map->xm) + (double)d.center_x;
    double xM = xm + (double)d.center_wd;

    for(int i = 0; i < d.map->region_cnt; i++)
    {
      ap_tile_t *tile = &d.map->tiles[d.map->region[i]];
      if(tile->thumbnail  != -1u)
      {
        double tile_size = (double)((d.map->z > tile->z) ? 1 << (d.map->z - tile->z) : 1) * (double)TILE_SIZE;
        ap_map_bounds_t b;
        const int k = 1 << tile->z;
        double map_wd = (double)k * tile_size;
        b.xm = x0 + tile->x * tile_size + (double)d.center_x;
        b.ym = y0 + tile->y * tile_size + (double)d.center_y;
        b.yM = b.ym + tile_size;
        b.xM = b.xm + tile_size;

        if(b.xm >= xm && b.xM <= xM)
          _draw_tile(window, tile, &b);
        else
        {
          if((b.xM >= xm && b.xM + map_wd > xM) && (b.xm <= xM && b.xm - map_wd < xm))
          {
            _draw_tile(window, tile, &b);
          }
          if((b.xM < xm && b.xm + map_wd < xM) || (b.xM >= xm && b.xm + map_wd < xM))
          {
            b.xm += map_wd;
            b.xM += map_wd;
            _draw_tile(window, tile, &b);
          }
          else if((b.xm > xM && b.xM - map_wd > xm) || (b.xm <= xM && b.xM - map_wd > xm))
          {
            b.xm -= map_wd;
            b.xM -= map_wd;
            _draw_tile(window, tile, &b);
          }
        }
      }
    }
    if(d.debug)
    {
      ImGui::Text("m %lf,%lf wd %lf z %d ps %lf", d.map->xm, d.map->ym, d.map->wd, d.map->z, d.map->pixel_size);
    }

    ImGui::End(); // end center lighttable
  }
}

extern "C" int map_leave()
{
  dt_rc_set_int(&d.rc, "map_region_z", d.map->z);
  dt_rc_set_float(&d.rc, "map_region_xm", d.map->xm);
  dt_rc_set_float(&d.rc, "map_region_ym", d.map->ym);
  return 0;
}

extern "C" int map_enter()
{
  d.map->z = dt_rc_get_int(&d.rc, "map_region_z", 0);
  int k;
  if(d.map->z == 0)
  {
    k = 1 << d.map->z;
    while(k * TILE_SIZE < d.center_wd)
      k = (1 << ++d.map->z);
    k -= 1;
    d.map->wd = (double)d.center_wd / (double)(k * TILE_SIZE);
    d.map->xm = 0.5 * (1.0 - d.map->wd);
  }
  else
  {
    k = 1 << d.map->z;
    d.map->wd = (double)d.center_wd / (double)(k * TILE_SIZE);
    d.map->xm = dt_rc_get_float(&d.rc, "map_region_xm", 0.0);
    d.map->ym = dt_rc_get_float(&d.rc, "map_region_ym", 0.0);
  }

  d.map->xM = d.map->xm + d.map->wd;
  d.map->ht = (double)d.center_ht / (double)d.center_wd * d.map->wd;
  d.map->yM = d.map->ym + d.map->ht;
  d.map->pixel_size = d.map->wd / (double)d.center_wd;
  d.map->drag = 0;

  return 0;
}
