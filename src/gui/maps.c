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
  uint32_t index;
  uint64_t zxy[2];
  int zero;
  } ap_map_tile_job_t;

int ap_map_request_tile(const uint32_t index);

inline static double _win2map_x(const double wx)
{
  return d.map->xm + (double)wx * d.map->pixel_size;
}

inline static double _win2map_y(const double wy)
{
  return d.map->ym + (double)wy * d.map->pixel_size;
}

size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
  FILE *stream = (FILE *)userdata;
  if (!stream) {
    printf("No stream\n");
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
    else if(ti->prev == tl->mru || ti->zxy[0] == 0)
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
//  int k = pow(2,z);
  int k = 1 << z;
//  double r = 1.0 / k;
  int xa = min_x * k;
  int xb = ceil(max_x * k);
  int ya = min_y * k;
  int yb = ceil(max_y *k);
  xb = CLAMP(xb,0,k);
  yb = CLAMP(yb,0,k);
  int covered = 1;

  for(int y = ya; y < yb; ++y)
  {
    for(int x = xa; x < xb; ++x)
    {
      char text[20];
      snprintf(text, sizeof(text),"%d/%d/%d", z, x, y);
      uint32_t index = -1u;
      uint64_t zxy[2];
      zxy[0] = dt_token(text);
      zxy[1] = dt_token(text+8);
      ap_tile_t *tile = _tile_allocate(d.map, &zxy[0], &index);
      ap_map_request_tile(index);
      tile->x = x;
      tile->y = y;
      tile->z = z;
      if(d.map->region_cnt < d.map->region_max)
        d.map->region[d.map->region_cnt++] = index;
      else
        dt_log(s_log_map|s_log_err, "[map] tiles list too short");
      if(tile->thumb_status != s_thumb_loaded)
        covered = 0;
      else
        d.map->region_cov++;
    }
  }

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

  double max_tile_size = d.map->wd * ((double)TILE_SIZE / (double)d.center_wd);
  int z = 0;
  double r = 1.0 / (double)(1<<z);
  while (r > max_tile_size && z < MAX_ZOOM)
    r = 1.0 / (double)(1<<++z);

  d.map->region_cnt = d.map->region_cov = 0;
  if(!_append_region(z, min_x, max_x, min_y, max_y) && z > 0)
  {
    _append_region(--z, min_x, max_x, min_y, max_y);
    for(int i = 0; i < d.map->region_cnt / 2; i++)
    {
      uint32_t index = d.map->region[i];
      d.map->region[i] = d.map->region[d.map->region_cnt-1-i];
      d.map->region[d.map->region_cnt-1-i] = index;
    }
  }
  d.map->z = z;
  d.map->pixel_size = d.map->wd / (double)d.center_wd;
}


void map_mouse_scrolled(GLFWwindow* window, double xoff, double yoff)
{
  double x, y;
  glfwGetCursorPos(d.window, &x, &y);

  if(x >= d.center_x || x < (d.center_x + d.center_wd))
  {
    const double rate = (yoff > 0.0) ? 1.0f/1.2f : 1.2f;
    const double mx = _win2map_x(x);
    const double my = _win2map_y(y);
    d.map->xm = mx - rate * (mx - d.map->xm);
    d.map->xM = mx + rate * (d.map->xM - mx);
    d.map->ym = my - rate * (my - d.map->ym);
    d.map->yM = my + rate * (d.map->yM - my);
    d.map->wd = d.map->xM - d.map->xm;
    d.map->pixel_size = d.map->wd / (double)d.center_wd;
  }
  dt_gui_imgui_scrolled(window, xoff, yoff);
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
  dt_gui_imgui_mouse_button(window, button, action, mods);
}

void map_mouse_position(GLFWwindow* window, double x, double y)
{
  if(d.map->drag)
  {
    const double dx = (d.map->prevx - x) * d.map->wd / (double)d.center_wd;
    const double dy = (d.map->prevy - y) * d.map->ht / (double)d.center_ht;
    d.map->xm += dx; d.map->xM += dx;
    d.map->ym += dy; d.map->yM += dy;
    d.map->prevx = x;
    d.map->prevy = y;
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

VkResult ap_map_load_one(dt_thumbnails_t *tn, const char *filename, uint32_t *thumb_index)
{
  dt_thumbnail_t *th = dt_thumbnails_allocate(tn, thumb_index);
  if(_thumbnails_map_get_size(filename, &th->wd, &th->ht))
    return VK_INCOMPLETE;

  return dt_thumbnails_load_one(tn, th, filename, _thumbnails_map_read);
}

int ap_map_request_tile(uint32_t index)
{
  ap_tile_t *tile = &d.map->tiles[index];
  if(tile->thumb_status == s_thumb_loaded || tile->thumb_status == s_thumb_downloading)
    return 0;
  else if(tile->thumb_status == s_thumb_unavailable || tile->thumb_status == s_thumb_ondisk)
  { // not loaded
    char path[512] = {0};
    snprintf(path, sizeof(path), "%s/%s.png", d.map->thumbs.cachedir, dt_token_str(tile->zxy));
    struct stat statbuf = {0};
    if(!stat(path, &statbuf))
    {
      if(!ap_map_load_one(&d.map->thumbs, path, &tile->thumbnail))
      {
        tile->thumb_status = s_thumb_loaded;
        return 0;
      }
    }
  }
  if(tile->thumb_status == s_thumb_unavailable)
  {
    char dir[512];
    uint64_t zxy[2];
    int zero = 0; // not sure if variables are contiguous...
    zxy[0] = tile->zxy[0];
    zxy[1] = tile->zxy[1];
    char *ps = (char *)zxy;
    char *pe = strstr(ps, "/");
    pe[0] = '\0';
    snprintf(dir, sizeof(dir), "%s/%s", d.map->thumbs.cachedir, ps);
    mkdir(dir, 0755);
    ps = pe + 1;
    pe = strstr(ps, "/");
    pe[0] = '\0';
    snprintf(&dir[strlen(dir)], sizeof(dir) - strlen(dir), "/%s", ps);
    mkdir(dir, 0755);

    tile->thumb_status = s_thumb_downloading;
    ap_map_tile_job_t job;
    job.index = index;
    job.zxy[0] = tile->zxy[0];
    job.zxy[1] = tile->zxy[1];
    job.zero = 0;
    if(ap_fifo_push(&d.map->thumbs.cache_req, &job) != 0)
    {
      tile->thumb_status = s_thumb_unavailable;
      return 1;
    }
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
      ap_tile_t *tile = &d.map->tiles[job.index];
      char url[256];
      char path[512];
      char real_path[512];
      snprintf(url, sizeof(url), TILE_SERVER, dt_token_str(job.zxy));
      snprintf(path, sizeof(path), "%s/%s.dld", d.map->thumbs.cachedir, dt_token_str(job.zxy));
      snprintf(real_path, sizeof(real_path), "%s/%s.png", d.map->thumbs.cachedir, dt_token_str(job.zxy));

      FILE *fp = fopen(path, "wb");
      if(fp)
      {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        CURLcode cc = curl_easy_perform(curl);
        fclose(fp);
        if (cc != CURLE_OK)
        {
          dt_log(s_log_map|s_log_err, "[map] failed to load tile %s", url);
          long rc = 0;
          curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rc);
          if (!((rc == 200 || rc == 201) && rc != CURLE_ABORTED_BY_CALLBACK))
            dt_log(s_log_map|s_log_err, "[map] response code %s", rc);
          if(job.zxy[0] == tile->zxy[0] && job.zxy[1] == tile->zxy[1])
            tile->thumb_status = s_thumb_dead;
        }
        else
        {
          if(job.zxy[0] == tile->zxy[0] && job.zxy[1] == tile->zxy[1])
          {
            rename(path, real_path);
            tile->thumb_status = s_thumb_ondisk;
          }
          double end = dt_time();
          dt_log(s_log_perf, "[map] tile created in %3.0fms", 1000.0*(end-beg));
        }
      }
      else
      {
        dt_log(s_log_map|s_log_err, "[map] failed to open or create tile %s", path);
        tile->thumb_status = s_thumb_dead;
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
    {
      threads_mutex_init(&d.map->thumbs.req_lock, 0);
    }
    job[k].gid = k;
    taskid = threads_task(1, -1, &job[k], map_work, map_free);
    assert(taskid != -1); // only -1 is fatal
  }
  return 0;
}

void ap_map_reset_tile()
{
  ap_fifo_empty(&d.map->thumbs.cache_req);
}

void ap_map_cleanup()
{
  d.map->thumbs.cache_req_abort = 1;
  ap_fifo_clean(&d.map->thumbs.cache_req);
  free(d.map);
}

void ap_map_tiles_init()
{
  d.map = (ap_map_t *)malloc(sizeof(ap_map_t));
  memset(d.map, 0, sizeof(*d.map));

  dt_thumbnails_init(&d.map->thumbs, TILE_SIZE, TILE_SIZE, 200, 1ul<<29, "apdt-tiles", VK_FORMAT_R8G8B8A8_UNORM);
  d.map->tiles_max = 200;
  // init tiles collection
  d.map->tiles = malloc(sizeof(ap_tile_t)*d.map->tiles_max);
  memset(d.map->tiles, 0, sizeof(ap_tile_t)*d.map->tiles_max);
  // init lru list
  d.map->lru = d.map->tiles;
  d.map->mru = d.map->tiles + d.map->tiles_max-1;
  d.map->tiles[0].next = d.map->tiles+1;
  d.map->tiles[d.map->tiles_max-1].prev = d.map->tiles+d.map->tiles_max-2;
  for(int k=1;k<d.map->tiles_max-1;k++)
  {
    d.map->tiles[k].next = d.map->tiles+k+1;
    d.map->tiles[k].prev = d.map->tiles+k-1;
  }

  d.map->region_max = 200;
  d.map->region_cnt = 0;
  d.map->region = (uint32_t *)malloc(sizeof(uint32_t)*d.map->region_max);

  // job stuff
  ap_fifo_init(&d.map->thumbs.cache_req, sizeof(ap_map_tile_job_t), 30);
  d.map->thumbs.cache_req_abort = 0;
  ap_map_start_tile_job();
}
