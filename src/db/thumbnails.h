#pragma once

#include "db/alloc.h"
#include "core/queue.h"
#include "core/threads.h"

#include <vulkan/vulkan.h>

// render out a lot of images from the same
// graph object that is re-initialised to accomodate
// the needs of the images. the results are kept in a
// separate memory block and can be displayed in the gui.
//
// this stores a list of thumbnails, the correspondence
// to imgid is done in the dt_image_t struct.
//

typedef struct ap_img_t ap_img_t;
typedef struct ap_image_t ap_image_t;

typedef enum ap_thumb_state_t
{
    s_thumb_unavailable = 0, // thumb not available
    s_thumb_downloading,     // thumb si being saved on disk
    s_thumb_ondisk,          // thumb is saved to disk, but not loaded into memory
    s_thumb_loaded,          // thumb has been loaded into memory
    s_thumb_dead,            // cannot be saved on disk
}
ap_thumb_state_t;

typedef struct dt_thumbnail_t
{
  VkDescriptorSet        dset;
  VkImage                image;
  VkImageView            image_view;
  dt_vkmem_t            *mem;
  uint64_t               offset;
  struct dt_thumbnail_t *prev;    // dlist for lru cache
  struct dt_thumbnail_t *next;
  uint32_t               wd;
  uint32_t               ht;
  uint32_t               owner;
}
dt_thumbnail_t;

typedef struct dt_thumbnails_t
{
  int                   thumb_wd;
  int                   thumb_ht;

  dt_vkalloc_t          alloc;
  uint32_t              memory_type_bits;
  VkDeviceMemory        vkmem;
  VkDescriptorPool      dset_pool;
  VkDescriptorSetLayout dset_layout;
  VkFormat              format;
  dt_thumbnail_t       *thumb;
  int                   thumb_max;

  threads_mutex_t       lru_lock;
  dt_thumbnail_t       *lru;   // least recently used thumbnail, delete this first
  dt_thumbnail_t       *mru;   // most  recently used thumbnail, append here

  char                  cachedir[1024];

  // cache request queue
  ap_fifo_t cache_req;
  int cache_req_abort;
  threads_mutex_t       req_lock;
  void                  (*reset_owner)(const uint32_t index);
}
dt_thumbnails_t;

// init a thumbnail cache
VkResult dt_thumbnails_init(
    dt_thumbnails_t *tn,
    const int wd,            // max width of thumbnail
    const int ht,            // max height of thumbnail
    const int cnt,           // max number of thumbnails
    const size_t heap_size,  // max heap size in bytes (allocated on GPU)
    VkFormat img_format);

// free all resources
void dt_thumbnails_cleanup(dt_thumbnails_t *tn);

// load one image thumb
VkResult dt_thumbnails_load_one(dt_thumbnails_t *tn, dt_thumbnail_t *th, const char *imgfilename, VkResult _thumbnails_read());
dt_thumbnail_t *dt_thumbnails_allocate(dt_thumbnails_t *tn, uint32_t *index);
void dt_thumbnails_move_to_mru(dt_thumbnails_t *tn, const uint32_t index);

// init images thumbnails cache
VkResult dt_thumbnails_vkdt_init();
// update thumbnails for a list of image ids.
void dt_thumbnails_vkdt_load_list(dt_thumbnails_t *tn, uint32_t beg, uint32_t end);
// load one bc1 thumbnail for a given filename. fills thumb_index and returns
// VK_SUCCESS if all went well.
VkResult dt_thumbnails_vkdt_load_one(dt_thumbnails_t *tn, const char *filename, uint32_t *thumb_index);
// start thumbnail generation job
int ap_thumbnails_vkdt_start_job();
// reset the thumbnail generation
void ap_thumbnails_vkdt_reset();
