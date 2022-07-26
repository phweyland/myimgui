#include "gui/maps.h"
#include "core/log.h"
#include "core/core.h"
#include "core/queue.h"

#include <math.h>
#include <stdio.h>

#define TILE_SERVER   "https://a.tile.openstreetmap.org/" // the tile map server url

void ap_map_tile_subdir(ap_tile_t *t)
{
  const int len = snprintf(t->subdir, sizeof(t->subdir), "%d/%d/", t->z, t->x);
  if(len >= sizeof(t->subdir))
    dt_log(s_log_map|s_log_err, "tile subdir too small");
}

void ap_map_tile_dir(ap_tile_t *t)
{
  const int len = snprintf(t->dir, sizeof(t->dir), "tiles/%s", t->subdir);
  if(len >= sizeof(t->dir))
    dt_log(s_log_map|s_log_err, "tile dir too small");
}

void ap_map_tile_file(ap_tile_t *t)
{
  const int len = snprintf(t->file, sizeof(t->file), "%d.png", t->y);
  if(len >= sizeof(t->file))
    dt_log(s_log_map|s_log_err, "tile file too small");
}

void ap_map_tile_path(ap_tile_t *t)
{
  const int len = snprintf(t->path, sizeof(t->path), "%s%s", t->dir, t->file);
  if(len >= sizeof(t->path))
    dt_log(s_log_map|s_log_err, "tile path too small");
}

void ap_map_tile_url(ap_tile_t *t)
{
  const int len = snprintf(t->url, sizeof(t->url), "%s%s%s", TILE_SERVER, t->subdir, t->file);
  if(len >= sizeof(t->url))
    dt_log(s_log_map|s_log_err, "tile url too small");
}

void ap_map_tile_label(ap_tile_t *t)
{
  const int len = snprintf(t->label, sizeof(t->label), "%s%d", t->subdir, t->y);
  if(len >= sizeof(t->label))
    dt_log(s_log_map|s_log_err, "tile label too small");
}

void ap_map_tile_bounds(ap_tile_t *t)
{
  double n = 1.0 / pow(2.0, t->z);
  t->b.b00 = t->x * n;
  t->b.b01 = (1.0 + t->y) * n;
  t->b.b10 = (1.0 + t->x) * n;
  t->b.b11 = t->y * n;
}

size_t ap_map_curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
  FILE *stream = (FILE *)userdata;
  if (!stream) {
      printf("No stream\n");
      return 0;
  }
  size_t written = fwrite((FILE *)ptr, size, nmemb, stream);
  return written;
}

inline static double _win2map(const double wx)
{
  return d.map->xm + (double)wx * d.map->pixel_size;
}

inline static double _map2win(const double mx)
{
  return (mx - d.map->xm) / d.map->pixel_size;
}

static int _append_region(int z, double min_x, double max_x, double min_y, double max_y)
{
  int k = pow(2,z);
//  double r = 1.0 / k;
  int xa = min_x * k;
  int xb = ceil(max_x * k);
  int ya = min_y * k;
  int yb = ceil(max_y *k);
  xb = CLAMP(xb,0,k);
  yb = CLAMP(yb,0,k);
  int covered = 1;
//  printf("mx %lf Mx %lf my %lf My %lf- k %d xa %d xb %d ya %d yb %d\n",
//        min_x,  max_x, min_y,  max_y , k, xa, xb, ya, yb);

  for (int y = ya; y < yb; ++y)
  {
    for (int x = xa; x < xb; ++x)
    {
//      double coord[3] = {z,x,y};
//      std::shared_ptr<Tile> tile = request_tile(coord);
//      m_region.push_back({coord,tile});
//      if (tile == nullptr || tile->state != TileState::Loaded)
      printf("%d/%d/%d ", z, x, y);
        covered = 0;
    }
    printf("\n");
  }
printf("\n");
  return covered;
}

void ap_map_get_region()
{
    d.map->wd = d.map->xM - d.map->xm;
    d.map->ht = d.map->yM - d.map->ym;
    double min_x = CLAMP(d.map->xm, 0.0, 1.0);
    double min_y = CLAMP(d.map->ym, 0.0, 1.0);
    double max_x = CLAMP(d.map->xM, 0.0, 1.0);
    double max_y = CLAMP(d.map->yM, 0.0, 1.0);

    double tile_size = d.map->wd * (TILE_SIZE / (double)d.center_wd);
    int z = 0;
    double r = 1.0 / pow(2,z);
    while (r > tile_size && z < MAX_ZOOM)
        r = 1.0 / pow(2,++z);

    _append_region(z, min_x, max_x, min_y, max_y);
    d.map->z = z;
    d.map->pixel_size = d.map->wd / (double)d.center_wd;

/*    m_region.clear();
    if (!append_region(z, min_x, min_y, size_x, size_y) && z > 0) {
        append_region(--z, min_x, min_y, size_x, size_y);
        std::reverse(m_region.begin(),m_region.end());
    }
    return m_region;*/
}


void map_mouse_scrolled(GLFWwindow* window, double xoff, double yoff)
{
  double x, y;
  glfwGetCursorPos(d.window, &x, &y);

  if(x >= d.center_x || x < (d.center_x + d.center_wd))
  {
    const double rate = (yoff > 0.0) ? 1.0f/1.2f : 1.2f;
    const double mx = _win2map(x);
    const double my = _win2map(y);
    d.map->xm = mx - rate * (mx - d.map->xm);
    d.map->xM = mx + rate * (d.map->xM - mx);
    d.map->ym = my - rate * (my - d.map->ym);
    d.map->yM = my + rate * (d.map->yM - my);
    d.map->wd = d.map->xM - d.map->xm;
    d.map->pixel_size = d.map->wd / (double)d.center_wd;
//    printf("rate %lf mx %lf my %lf\n", rate, mx, my);
  }
}

void map_mouse_button(GLFWwindow* window, int button, int action, int mods)
{
  double x, y;
  glfwGetCursorPos(d.window, &x, &y);

  if(action == GLFW_PRESS  && (x >= d.center_x || x < (d.center_x + d.center_wd)))
  {
    d.map->prevx = x;
    d.map->prevy = y;
    d.map->drag = 1;
  }
  else if(action == GLFW_RELEASE)
    d.map->drag = 0;
//  printf("map mouse button %lf %lf\n", x, y);
}

void map_mouse_position(GLFWwindow* window, double x, double y)
{
//  printf("map mouse posl %lf %lf\n", x, y);
  if(d.map->drag)
  {
    const double dx = (d.map->prevx - x) * d.map->wd / (double)d.center_wd;
    const double dy = (d.map->prevy - y) * d.map->ht / (double)d.center_ht;
    d.map->xm += dx; d.map->xM += dx;
    d.map->ym += dy; d.map->yM += dy;
    d.map->prevx = x;
    d.map->prevy = y;
//    printf("x %lf,%lf y %lf,%lf\n", d.map->xm, d.map->xM, d.map->ym, d.map->yM);
  }
}
typedef struct map_job_t
{
  int taskid;
} map_job_t;

int ap_map_request_tile(uint32_t index)
{
  int res = 0;/*
  if(d.img.images[index].thumbnail == 0)
  {
    if(!d.img.images[index].thumb_coming)
    {
      d.img.images[index].thumb_coming = 1;
      res = ap_fifo_push(&d.thumbs.img_th, &index);
      if(res)
        d.img.images[index].thumb_coming = 0;
    }
  }
  else
    d.img.images[index].thumb_coming = 0; */
  return res;
}

void map_work(uint32_t item, void *arg)
{
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 10000;
printf("map work started\n");
  while(1)
  {
    if(d.map->thumbs.cache_req_abort)
      return;
    nanosleep(&ts, NULL);
    uint32_t index;
    int res = ap_fifo_pop(&d.map->thumbs.cache_req, &index);
    if(!res)
    {
      printf("map work got tile to load\n");
    }
  }
}

int ap_map_start_tile_job()
{
  static map_job_t j;
  j.taskid = threads_task(1, -1, &j, map_work, NULL);
  return j.taskid;
}

void ap_map_reset_tile()
{
  ap_fifo_empty(&d.map->thumbs.cache_req);
}

void ap_map_tiles_cleanup()
{
  d.map->thumbs.cache_req_abort = 1;
  ap_fifo_clean(&d.map->thumbs.cache_req);
}

void ap_map_tiles_init()
{
  d.map = (ap_map_t *)malloc(sizeof(ap_map_t));
  memset(d.map, 0, sizeof(*d.map));

  VkResult res = dt_thumbnails_init(&d.map->thumbs, TILE_SIZE, TILE_SIZE, 500, 1ul<<29, "apdt-tiles", VK_FORMAT_R8_UNORM);

//TODO set proper queue element size
  ap_fifo_init(&d.map->thumbs.cache_req, sizeof(uint32_t), 30);
  d.map->thumbs.cache_req_abort = 0;
  ap_map_start_tile_job();
}
