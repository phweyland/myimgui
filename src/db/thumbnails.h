#pragma once

#include "alloc.h"
#include "../core/threads.h"

#include <vulkan/vulkan.h>

// render out a lot of images from the same
// graph object that is re-initialised to accomodate
// the needs of the images. the results are kept in a
// separate memory block and can be displayed in the gui.
//
// this stores a list of thumbnails, the correspondence
// to imgid is done in the dt_image_t struct.
//

typedef struct ap_col_t ap_col_t;
typedef struct ap_image_t ap_image_t;

typedef struct dt_thumbnail_t
{
  VkDescriptorSet        dset;
  VkImage                image;
  VkImageView            image_view;
  dt_vkmem_t            *mem;
  uint64_t               offset;
  struct dt_thumbnail_t *prev;    // dlist for lru cache
  struct dt_thumbnail_t *next;
  uint32_t               imgid;   // index into images->image[] or -1u
  uint32_t               wd;
  uint32_t               ht;
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
  dt_thumbnail_t       *thumb;
  int                   thumb_max;

  // threads_mutex_t       lru_lock; // currently not needed, only using lru cache in gui thread
  dt_thumbnail_t       *lru;   // least recently used thumbnail, delete this first
  dt_thumbnail_t       *mru;   // most  recently used thumbnail, append here

  char                  cachedir[1024];
}
dt_thumbnails_t;

// init a thumbnail cache
VkResult dt_thumbnails_init(
    dt_thumbnails_t *tn,     // struct to be constructed
    const int wd,            // max width of thumbnail
    const int ht,            // max height of thumbnail
    const int cnt,           // max number of thumbnails
    const size_t heap_size); // max heap size in bytes (allocated on GPU)

// free all resources
void dt_thumbnails_cleanup(dt_thumbnails_t *tn);

// load one bc1 thumbnail for a given filename. fills thumb_index and returns
// VK_SUCCESS if all went well.
VkResult dt_thumbnails_load_one(dt_thumbnails_t *tn, ap_image_t *img);

// update thumbnails for a list of image ids.
void dt_thumbnails_load_list(dt_thumbnails_t *tn, ap_col_t *col, uint32_t beg, uint32_t end);
