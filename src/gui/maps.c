#include "gui/maps.h"
#include "gui/render.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../ext/stb_image.h"
#include "core/log.h"
#include "core/core.h"
#include "core/queue.h"
#include "core/dlist.h"
#include "core/token.h"
#include <math.h>
#include <stdio.h>
#include <curl/curl.h>
#include <sys/stat.h>

typedef struct ap_map_tile_job_t
{
  uint64_t zxy[3];
  } ap_map_tile_job_t;

int ap_map_request_tile(const uint32_t index);
void ap_map_reset_tile_req();

inline static double _win2map_x(const double wx)
{
  return d.map->xm + (double)wx * d.map->pixel_size;
}

inline static double _win2map_y(const double wy)
{
  return d.map->ym + (double)wy * d.map->pixel_size;
}

double ap_map2win_x(const double mx)
{
  return (mx - d.map->xm) / d.map->pixel_size;
}

double ap_map2win_y(const double my)
{
  return (my - d.map->ym) / d.map->pixel_size;
}

size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
  FILE *stream = (FILE *)userdata;
  if (!stream)
  {
    dt_log(s_log_map|s_log_err, "[map] no stream");
    return 0;
  }
  size_t written = fwrite((FILE *)ptr, size, nmemb, stream);
  return written;
}

inline static ap_tile_t *_tile_allocate(ap_map_t *tl, const uint64_t *zxy, uint32_t *index)
{
  ap_tile_t *ti = NULL;
  for(ti = tl->mru; ti->prev != NULL; ti = ti->prev)
  {
    if(ti->zxy[0] == 0)
      break;
//    if(ti->thumbnail != -1u && ti->zxy[0] == zxy[0] && ti->zxy[1] == zxy[1])
    if(ti->zxy[0] == zxy[0] && ti->zxy[1] == zxy[1])
    {
      *index = ti - tl->tiles;
      if(ti == tl->lru) tl->lru = tl->lru->next; // move head
      tl->lru->prev = 0;
      if(tl->mru == ti) tl->mru = ti->prev;      // going to remove mru, need to move
      DLIST_RM_ELEMENT(ti);                      // disconnect old head
      tl->mru = DLIST_APPEND(tl->mru, ti);       // append to end and move tail
      return ti;
    }
    else if(ti->zxy[0] == 0)
      break;
}
  // allocate tile from lru list
  ti = tl->lru;
  tl->lru = tl->lru->next;             // move head
  if(tl->mru == ti) tl->mru = ti->prev;// going to remove mru, need to move
  DLIST_RM_ELEMENT(ti);                // disconnect old head
  tl->mru = DLIST_APPEND(tl->mru, ti); // append to end and move tail
  ti->zxy[0] = zxy[0];
  ti->zxy[1] = zxy[1];
  ti->thumbnail = -1u;
  ti->thumb_status = s_thumb_unavailable;
  *index = ti - tl->tiles;
  return ti;
}

static int _append_region(int z, double min_x, double max_x, double min_y, double max_y)
{
  int k = 1 << z;
  int xa = floor(min_x * (double)k);
  int xb = ceil(max_x * (double)k);
  int ya = min_y * k;
  int yb = ceil(max_y * k);
  xa = CLAMP(xa,-k,k);
  xb = CLAMP(xb,0,k+k);
  yb = CLAMP(yb,0,k);
  int covered = 1;

  for(int y = ya; y < yb; ++y)
  {
    int i = 0;
    for(int x = xa; x < xb && i < k; ++x)
    {
      char text[20];
      const int cx = x < 0 ? (x + k) : x > k-1 ? (x - k) : x;
      snprintf(text, sizeof(text),"%d/%d/%d", z, cx, y);
      uint32_t index = -1u;
      uint64_t zxy[2];
      zxy[0] = dt_token(text);
      zxy[1] = dt_token(text+8);
      ap_tile_t *tile = _tile_allocate(d.map, &zxy[0], &index);
      tile->x = cx;
      tile->y = y;
      tile->z = z;
      if(d.map->region_cnt < d.map->region_max-1)
        d.map->region[d.map->region_cnt++] = index;
      else
        dt_log(s_log_map|s_log_err, "[map] tiles list too short");
      if(tile->thumbnail == -1u)
        covered = 0;
      else
        d.map->region_cov++;
      i++;
    }
  }

  return covered;
}

static int compare_region(const void *a, const void *b, void *arg)
{
  const uint32_t *ia = a, *ib = b;
  return d.map->tiles[ia[0]].z - d.map->tiles[ib[0]].z;
}

double pmin_x; double pmin_y; double pmax_x;

void ap_map_get_region()
{
  double min_x = d.map->xm;
  double min_y = CLAMP(d.map->ym, 0.0, 1.0);
  double max_x = d.map->xM;
  double max_y = CLAMP(d.map->yM, 0.0, 1.0);

  if(min_x != pmin_x || min_y != pmin_y || max_x != pmax_x)
  {
    pmin_x = min_x; pmin_y = min_y; pmax_x = max_x;
    ap_map_reset_tile_req();
    for(int i = 0; i < d.map->region_cnt; i++)
      if(d.map->tiles[d.map->region[i]].thumb_status == s_thumb_downloading)
        d.map->tiles[d.map->region[i]].thumb_status = s_thumb_unavailable;
  }

  d.map->region_cnt = d.map->region_cov = 0;
  if(!_append_region(d.map->z, min_x, max_x, min_y, max_y) && d.map->z > 0)
  {
    _append_region(d.map->z - 1, min_x, max_x, min_y, max_y);
    qsort_r(d.map->region, d.map->region_cnt, sizeof(d.map->region[0]), compare_region, NULL);
  }

  for(int i = 0; i < d.map->region_cnt; i++)
    ap_map_request_tile(d.map->region[i]);
}

static void _map_update_mouse_data(double x, double y)
{
  d.map->mwx = x - d.center_x;
  d.map->mwy = y - d.center_y;
  d.map->mmx = _win2map_x(d.map->mwx);
  d.map->mmy = _win2map_y(d.map->mwy);
  const int k = 1 << d.map->z;
  const int tx = d.map->mmx * k;
  const int ty = d.map->mmy * k;
  d.map->mtile = -1u;
  for(ap_tile_t *tile = d.map->tiles; tile < &d.map->tiles[d.map->tiles_max] && tile->zxy[0] != 0; tile++)
  {
    if(tile->thumbnail != -1u && tile->z == d.map->z && tile->x == tx && tile->y == ty)
      d.map->mtile = tile - d.map->tiles;
  }
}

void map_mouse_scrolled(GLFWwindow* window, double xoff, double yoff)
{
  double x, y;
  glfwGetCursorPos(d.window, &x, &y);

  if(x >= d.center_x && x < (d.center_x + d.center_wd) &&
//    (yoff > 0.0 || (d.map->z > 0 && d.map->wd < 1.0)) && (yoff < 0.0 || d.map->z < MAX_ZOOM))
    (yoff > 0.0 || (d.map->z > 0 && d.map->wd < 1.0)))
  {
    d.map->z = (yoff > 0.0) ? ++d.map->z : --d.map->z;
    const double rate = (yoff > 0.0) ? 0.5f : 2.0f;
    d.map->mwx = x - d.center_x;
    d.map->mwy = y - d.center_y;
    const double mx = _win2map_x(d.map->mwx);
    const double my = _win2map_y(d.map->mwy);
    d.map->xm = mx - rate * (mx - d.map->xm);
    d.map->xM = mx + rate * (d.map->xM - mx);
    d.map->ym = my - rate * (my - d.map->ym);
    d.map->yM = my + rate * (d.map->yM - my);
    d.map->wd = d.map->xM - d.map->xm;
    d.map->ht = d.map->yM - d.map->ym;
    d.map->pixel_size = d.map->wd / (double)d.center_wd;
  }
  dt_gui_imgui_scrolled(window, xoff, yoff);
}

void map_mouse_button(GLFWwindow* window, int button, int action, int mods)
{
  double x, y;
  glfwGetCursorPos(d.window, &x, &y);

  if(action == GLFW_PRESS  && (x >= d.center_x && x < (d.center_x + d.center_wd)))
  {
    d.map->prevx = x;
    d.map->prevy = y;
    d.map->drag = 1;
    _map_update_mouse_data(x, y);
  }
  else if(action == GLFW_RELEASE)
    d.map->drag = 0;
  dt_gui_imgui_mouse_button(window, button, action, mods);
}

void map_mouse_position(GLFWwindow* window, double x, double y)
{
  if(x >= d.center_x && x < (d.center_x + d.center_wd))
  {
    if(d.map->drag)
    {
      const double dx = (d.map->prevx - x) * d.map->wd / (double)d.center_wd;
      const double dy = (d.map->prevy - y) * d.map->ht / (double)d.center_ht;
      d.map->xm += dx; d.map->xM += dx;
      const double cor = d.map->xm > 1.0 ? -1.0 : d.map->xM < 0.0 ? 1.0 : 0.0;

      d.map->xm += cor;
      d.map->xM += cor;
      d.map->ym += dy; d.map->yM += dy;
      d.map->prevx = x;
      d.map->prevy = y;
    }
    _map_update_mouse_data(x, y);
  }
  dt_gui_imgui_mouse_position(window, x, y);
}

static int _thumbnails_map_get_size(const char *filename, uint32_t *wd, uint32_t *ht)
{
  int wdl;
  int htl;
  unsigned char *image_data = stbi_load(filename, &wdl, &htl, NULL, 0);
  if (image_data == NULL)
  {
    fprintf(stderr, "[map] %s: can't open file (size)!\n", filename);
    return 1;
  }
  *wd = wdl;
  *ht = htl;
  stbi_image_free(image_data);
  return 0;
}

static int _thumbnails_map_read(const char *filename, void *mapped)
{
  int wd;
  int ht;
  unsigned char *image_data = stbi_load(filename, &wd, &ht, NULL, 4);
  if (image_data == NULL)
  {
    fprintf(stderr, "[map] %s: can't open file (data)!\n", filename);
    return 1;
  }
  memcpy(mapped, image_data, sizeof(uint8_t)*4*wd*ht);
  stbi_image_free(image_data);
  return 0;
}

VkResult ap_map_load_one(dt_thumbnails_t *tn, const char *filename, uint32_t *thumb_index, const uint32_t tile_index)
{
  if(*thumb_index != -1u)
    dt_log(s_log_map|s_log_err, "[map] allocate thumb while already allocated");
  uint32_t wd;
  uint32_t ht;
  if(_thumbnails_map_get_size(filename, &wd, &ht))
    return VK_INCOMPLETE;
  dt_thumbnail_t *th = dt_thumbnails_allocate(tn, thumb_index);
  th->wd = wd; th->ht = ht;
  th->owner = tile_index;

  return dt_thumbnails_load_one(tn, th, filename, _thumbnails_map_read);
}

int ap_map_request_tile(uint32_t index)
{
  ap_tile_t *tile = &d.map->tiles[index];

  // loaded
  if(tile->thumbnail != -1u)
    return 0;

  char path[512] = {0};
  snprintf(path, sizeof(path), "%s/%s.png", d.map->thumbs.cachedir, dt_token_str(tile->zxy));
  struct stat statbuf = {0};

  // thumbnail in cache
  if(!stat(path, &statbuf))
  {
    if(!ap_map_load_one(&d.map->thumbs, path, &tile->thumbnail, index))
    {
      tile->thumb_status = s_thumb_loaded;
      return 0;
    }
    return 1;
  }

 	char *p = strrchr(path, '/');
  p[0] = '\0';
  if(stat(path, &statbuf))
  {
    // folder doesn't exist, creates it
    char *ps = strrchr(path, '/');
    ps[0] = '\0';
    mkdir(path, 0755);
    ps[0] = '/';
    mkdir(path, 0755);
  }

  // if not yet downloaded, gets the tile
  if(tile->thumb_status != s_thumb_downloading)
  {
    tile->thumb_status = s_thumb_downloading;
    ap_map_tile_job_t job;
    job.zxy[0] = tile->zxy[0];
    job.zxy[1] = tile->zxy[1];
    job.zxy[2] = 0;
    threads_mutex_lock(&d.map->thumbs.req_lock);
    if(ap_fifo_push(&d.map->thumbs.cache_req, &job) != 0)
    {
      tile->thumb_status = s_thumb_unavailable;
      threads_mutex_unlock(&d.map->thumbs.req_lock);
      return 1;
    }
    threads_mutex_unlock(&d.map->thumbs.req_lock);
  }
  return 0;
}

void map_work(uint32_t item, void *arg)
{
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 10000;

  CURL* curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

  while(1)
  {
    if(d.map->thumbs.cache_req_abort)
      return;
    nanosleep(&ts, NULL);
    ap_map_tile_job_t job;
    threads_mutex_lock(&d.map->thumbs.req_lock);
    int res = ap_fifo_pop(&d.map->thumbs.cache_req, &job);
    threads_mutex_unlock(&d.map->thumbs.req_lock);
    if(!res)
    {
      double beg = dt_time();
      char url[256];
      char path[512];
      char real_path[512];
      snprintf(url, sizeof(url), TILE_SERVER, dt_token_str(job.zxy));
      snprintf(path, sizeof(path), "%s/%s.dld", d.map->thumbs.cachedir, dt_token_str(job.zxy));
      snprintf(real_path, sizeof(real_path), "%s/%s.png", d.map->thumbs.cachedir, dt_token_str(job.zxy));
      struct stat statbuf = {0};
      if(stat(real_path, &statbuf))
      {
        FILE *fp = fopen(path, "wb");
        if(fp)
        {
          curl_easy_setopt(curl, CURLOPT_URL, url);
          curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
          CURLcode cc = curl_easy_perform(curl);
          fclose(fp);
          if (cc != CURLE_OK)
          {
            dt_log(s_log_map|s_log_err, "[map] failed to download tile %s", url);
            long rc = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rc);
            if (!((rc == 200 || rc == 201) && rc != CURLE_ABORTED_BY_CALLBACK))
              dt_log(s_log_map|s_log_err, "[map] response code %s", rc);
          }
          else
          {
            rename(path, real_path);
            double end = dt_time();
            dt_log(s_log_perf, "[map] tile created in %3.0fms", 1000.0*(end-beg));
          }
        }
      }
    }
  }
}

typedef struct map_job_t
{
  uint32_t gid;
}
map_job_t;

static void map_free(void *arg)
{
  // task is done, every thread will call this
  map_job_t *j = arg;
  // only first thread frees
  if(j->gid == 0)
  {
    pthread_mutex_destroy(&d.map->thumbs.req_lock);
  }
}

int ap_map_start_tile_job()
{
  map_job_t job[MAX_THREADS];
  int taskid = -1;
  for(int k=0;k<MAX_THREADS;k++)
  {
    if(k == 0)
      threads_mutex_init(&d.map->thumbs.req_lock, 0);
    job[k].gid = k;
    taskid = threads_task(1, -1, &job[k], map_work, map_free);
    assert(taskid != -1); // only -1 is fatal
  }
  return 0;
}

void ap_map_reset_tile_req()
{
  threads_mutex_lock(&d.map->thumbs.req_lock);
  ap_fifo_empty(&d.map->thumbs.cache_req);
  threads_mutex_unlock(&d.map->thumbs.req_lock);
}

void ap_map_cleanup()
{
  d.map->thumbs.cache_req_abort = 1;
  ap_fifo_clean(&d.map->thumbs.cache_req);
  free(d.map);
}

static void _map_tile_reset_owner(const uint32_t index)
{
  d.map->tiles[index].thumbnail = -1u;
  d.map->tiles[index].zxy[0] = -1u;
}

void ap_map_tiles_init()
{
  d.map = (ap_map_t *)malloc(sizeof(ap_map_t));
  memset(d.map, 0, sizeof(*d.map));

  dt_thumbnails_init(&d.map->thumbs, TILE_SIZE, TILE_SIZE, 1000, 1ul<<29, "apdt-tiles", VK_FORMAT_R8G8B8A8_UNORM);
  d.map->thumbs.reset_owner = _map_tile_reset_owner;

  d.map->tiles_max = 1000;
  // init tiles collection
  d.map->tiles = malloc(sizeof(ap_tile_t)*d.map->tiles_max);
  memset(d.map->tiles, 0, sizeof(ap_tile_t)*d.map->tiles_max);
  // init lru list
  d.map->lru = d.map->tiles;
  d.map->mru = d.map->tiles + d.map->tiles_max-1;
  d.map->tiles[0].next = d.map->tiles+1;
  d.map->tiles[d.map->tiles_max-1].prev = d.map->tiles+d.map->tiles_max-2;
  d.map->tiles[0].thumbnail = d.map->tiles[d.map->tiles_max-1].thumbnail = -1u;
  for(int k=1;k<d.map->tiles_max-1;k++)
  {
    d.map->tiles[k].next = d.map->tiles+k+1;
    d.map->tiles[k].prev = d.map->tiles+k-1;
    d.map->tiles[k].thumbnail = -1u;
  }

  d.map->region_max = 200;
  d.map->region_cnt = 0;
  d.map->region = (uint32_t *)malloc(sizeof(uint32_t)*d.map->region_max);

  // job stuff
  ap_fifo_init(&d.map->thumbs.cache_req, sizeof(ap_map_tile_job_t), 200);
  d.map->thumbs.cache_req_abort = 0;
  ap_map_start_tile_job();
}
