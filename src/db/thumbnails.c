#include "../core/core.h"
#include "../core/log.h"
#include "../core/dlist.h"
#include "../core/vk.h"
#include "thumbnails.h"
#include "../gui/gui.h"
#include <sys/stat.h>
#include <zlib.h>
#include <dirent.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
//#include <unistd.h>

#if 0
void
debug_test_list(
    dt_thumbnails_t *t)
{
  dt_thumbnail_t *l = t->lru;
  assert(!l->prev);
  assert(l->next);
  int len = 1;
  while(l->next)
  {
    dt_thumbnail_t *n = l->next;
    assert(n->prev == l);
    l = n;
    len++;
  }
  assert(t->thumb_max == len + 1);
  assert(t->mru == l);
}
#endif

VkResult
dt_thumbnails_init(
    dt_thumbnails_t *tn,
    const int wd,
    const int ht,
    const int cnt,
    const size_t heap_size)
{
  memset(tn, 0, sizeof(*tn));

  // TODO: getenv(XDG_CACHE_HOME)
  const char *home = getenv("HOME");
  snprintf(tn->cachedir, sizeof(tn->cachedir), "%s/.cache/vkdt", home);
  int err1 = mkdir(tn->cachedir, 0755);
  if(err1 && errno != EEXIST)
  {
    dt_log(s_log_err|s_log_db, "could not create thumbnail cache directory!");
    return VK_INCOMPLETE;
  }

  tn->thumb_wd = wd,
  tn->thumb_ht = ht,
  tn->thumb_max = cnt;

  tn->thumb = malloc(sizeof(dt_thumbnail_t)*tn->thumb_max);
  memset(tn->thumb, 0, sizeof(dt_thumbnail_t)*tn->thumb_max);
  // need at least one extra slot to catch free block (if contiguous, else more)
  dt_vkalloc_init(&tn->alloc, tn->thumb_max + 10, heap_size);

  // init lru list
  tn->lru = tn->thumb + 1; // [0] is special: busy bee
  tn->mru = tn->thumb + tn->thumb_max-1;
  tn->thumb[1].next = tn->thumb+2;
  tn->thumb[tn->thumb_max-1].prev = tn->thumb+tn->thumb_max-2;
  for(int k=2;k<tn->thumb_max-1;k++)
  {
    tn->thumb[k].next = tn->thumb+k+1;
    tn->thumb[k].prev = tn->thumb+k-1;
  }
  dt_log(s_log_db, "allocating %3.1f MB for thumbnails", heap_size/(1024.0*1024.0));

  // init thumbnail generation queue
  tn->img_th_req = tn->img_th_done = 0;
  tn->img_th_nb = 500;
  tn->img_ths = calloc(tn->img_th_nb, sizeof(ap_image_t));
  tn->img_th_abort = 0;
  ap_start_vkdt_thumbnail_job(tn, &tn->img_th_abort);

  // alloc dummy image to get memory type bits and something to display
  VkFormat format = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {
      .width  = 24,
      .height = 24,
      .depth  = 1
    },
    .mipLevels             = 1,
    .arrayLayers           = 1,
    .samples               = VK_SAMPLE_COUNT_1_BIT,
    .tiling                = VK_IMAGE_TILING_OPTIMAL,
    .usage                 =
        VK_IMAGE_ASPECT_COLOR_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT,
    .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices   = 0,
    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkResult err;
  VkImage img;
  err = vkCreateImage(apdt.device, &images_create_info, NULL, &img);
  check_vk_result(err);
  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(apdt.device, img, &mem_req);
  tn->memory_type_bits = mem_req.memoryTypeBits;
  vkDestroyImage(apdt.device, img, VK_NULL_HANDLE);
  VkMemoryAllocateInfo mem_alloc_info = {
    .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize  = heap_size,
    .memoryTypeIndex = qvk_get_memory_type(tn->memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
  };
  err = vkAllocateMemory(apdt.device, &mem_alloc_info, 0, &tn->vkmem);
  check_vk_result(err);
  // create descriptor pool (keep at least one for each type)
  VkDescriptorPoolSize pool_sizes[] = {{
    .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = tn->thumb_max,
  }};

  VkDescriptorPoolCreateInfo pool_info = {
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .poolSizeCount = LENGTH(pool_sizes),
    .pPoolSizes    = pool_sizes,
    .maxSets       = tn->thumb_max,
  };
  vkCreateDescriptorPool(apdt.device, &pool_info, 0, &tn->dset_pool);
  check_vk_result(err);

  // create a descriptor set layout
  VkDescriptorSetLayoutBinding binding = {
    .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount    = 1,
    .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT, //VK_SHADER_STAGE_ALL,
    .pImmutableSamplers = 0,
  };
  VkDescriptorSetLayoutCreateInfo dset_layout_info = {
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings    = &binding,
  };
  err = vkCreateDescriptorSetLayout(apdt.device, &dset_layout_info, 0, &tn->dset_layout);
  check_vk_result(err);

  VkDescriptorSetAllocateInfo dset_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = tn->dset_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &tn->dset_layout,
  };
  for(int i=0;i<tn->thumb_max;i++)
  {
    err = vkAllocateDescriptorSets(apdt.device, &dset_info, &tn->thumb[i].dset);
    check_vk_result(err);
  }
  apdt.UploadBuffer = VK_NULL_HANDLE;
  apdt.UploadBufferSize = 0;

  uint32_t id = 0;
  if(dt_thumbnails_load_one(tn, "data/busybee.bc1", &id) != VK_SUCCESS)
  {
    dt_log(s_log_err|s_log_db, "could not load required thumbnail symbols!");
    return VK_INCOMPLETE;
  }
  return VK_SUCCESS;
}

void dt_thumbnails_cleanup(dt_thumbnails_t *tn)
{
  for(int i=0;i<tn->thumb_max;i++)
  {
    if(tn->thumb[i].image)      vkDestroyImage    (apdt.device, tn->thumb[i].image,      0);
    if(tn->thumb[i].image_view) vkDestroyImageView(apdt.device, tn->thumb[i].image_view, 0);
  }
  free(tn->thumb);
  tn->thumb = 0;
  vkDestroySampler(apdt.device, apdt.tex_sampler, 0);
  vkDestroySampler(apdt.device, apdt.tex_sampler_nearest, 0);
  if(tn->dset_layout) vkDestroyDescriptorSetLayout(apdt.device, tn->dset_layout, 0);
  if(tn->dset_pool)   vkDestroyDescriptorPool     (apdt.device, tn->dset_pool,   0);
  if(tn->vkmem)       vkFreeMemory                (apdt.device, tn->vkmem,       0);
  dt_vkalloc_cleanup(&tn->alloc);
  if(apdt.UploadBuffer)
  {
    vkFreeMemory(apdt.device, apdt.UploadBufferMemory, 0);
    vkDestroyBuffer(apdt.device, apdt.UploadBuffer, 0);
  }
  free(tn->img_ths);
}

void dt_thumbnails_load_list(dt_thumbnails_t *tn, ap_col_t *col, uint32_t beg, uint32_t end)
{
  // for all images in given collection
  for(int k=beg;k<end;k++)
  {
    if(k >= col->image_cnt) break; // safety first. this probably means this job is stale! big danger!
    ap_image_t *img = &col->images[k];

    if(img->thumbnail == 0)
    { // not loaded
      char filename[PATH_MAX+100] = {0};
      snprintf(filename, sizeof(filename), "%s/%x.bc1", tn->cachedir, img->hash);
      img->thumbnail = -1u;
      if(dt_thumbnails_load_one(tn, filename, &img->thumbnail))
        img->thumbnail = 0;
    }
    else if(img->thumbnail > 0 && img->thumbnail < tn->thumb_max)
    { // loaded, update lru
      dt_thumbnail_t *th = tn->thumb + img->thumbnail;
      if(th == tn->lru) tn->lru = tn->lru->next; // move head
      tn->lru->prev = 0;
      if(tn->mru == th) tn->mru = th->prev;      // going to remove mru, need to move
      DLIST_RM_ELEMENT(th);                      // disconnect old head
      tn->mru = DLIST_APPEND(tn->mru, th);       // append to end and move tail
    }
  }
}

static int _thumbnails_get_size(const char *filename, uint32_t *wd, uint32_t *ht)
{
  uint32_t header[4] = {0};
  gzFile f = gzopen(filename, "rb");
  if(!f || gzread(f, header, sizeof(uint32_t)*4) != sizeof(uint32_t)*4)
  {
    fprintf(stderr, "[bc1] %s: can't open file!\n", filename);
    if(f) gzclose(f);
    return 1;
  }
  // checks: magic != dt_token("bc1z") || version != 1
//  if(header[0] != dt_token("bc1z") || header[1] != 1)
//  {
//    fprintf(stderr, "[i-bc1] %s: wrong magic number or version!\n", filename);
//    gzclose(f);
//  }
  *wd = 4*(header[2]/4);
  *ht = 4*(header[3]/4);
  gzclose(f);
  return 0;
}

static int _thumbnails_read(const char *filename, void *mapped)
{
  uint32_t header[4] = {0};
  gzFile f = gzopen(filename, "rb");
  if(!f || gzread(f, header, sizeof(uint32_t)*4) != sizeof(uint32_t)*4)
  {
    fprintf(stderr, "[bc1] %s: can't open file!\n", filename);
    if(f) gzclose(f);
    return 1;
  }
  // checks: magic != dt_token("bc1z") || version != 1
//  if(header[0] != dt_token("bc1z") || header[1] != 1)
//  {
//    fprintf(stderr, "[i-bc1] %s: wrong magic number or version!\n", filename);
//    gzclose(f);
//    return 1;
//  }
  const uint32_t wd = 4*(header[2]/4);
  const uint32_t ht = 4*(header[3]/4);
  // fprintf(stderr, "[i-bc1] %s magic %"PRItkn" version %u dim %u x %u\n",
  //     filename, dt_token_str(header[0]),
  //     header[1], header[2], header[3]);
  gzread(f, mapped, sizeof(uint8_t)*8*(wd/4)*(ht/4));
  gzclose(f);
  return 0;
}

// load a previously cached thumbnail to a VkImage onto the GPU.
// returns VK_SUCCESS on success
VkResult dt_thumbnails_load_one(dt_thumbnails_t *tn, const char *filename, uint32_t *thumb_index)
{
  VkResult err;
  char imgfilename[PATH_MAX+100] = {0};
  if(strncmp(filename, "data/", 5))
  { // only hash images that aren't straight from our resource directory:
    snprintf(imgfilename, sizeof(imgfilename), "%s", filename);
  }
  else
  {
    const char* dt_dir = dt_rc_get(&apdt.rc, "vkdt_folder", "");
    snprintf(imgfilename, sizeof(imgfilename), "%s/%s", dt_dir, filename);
  }
  struct stat statbuf = {0};
  if(stat(imgfilename, &statbuf)) return VK_INCOMPLETE;

  dt_thumbnail_t *th = 0;
  if(*thumb_index == -1u)
  { // allocate thumbnail from lru list
    // threads_mutex_lock(&tn->lru_lock);
    th = tn->lru;
    tn->lru = tn->lru->next;             // move head
    if(tn->mru == th) tn->mru = th->prev;// going to remove mru, need to move
    DLIST_RM_ELEMENT(th);                // disconnect old head
    tn->mru = DLIST_APPEND(tn->mru, th); // append to end and move tail
    *thumb_index = th - tn->thumb;
    // threads_mutex_unlock(&tn->lru_lock);
  }
  else th = tn->thumb + *thumb_index;

  if(*thumb_index == 0 && th->image)
    return VK_SUCCESS;

  // cache eviction:
  // clean up memory in case there was something here:
  if(th->image)      vkDestroyImage(apdt.device, th->image, VK_NULL_HANDLE);
  if(th->image_view) vkDestroyImageView(apdt.device, th->image_view, VK_NULL_HANDLE);
  th->image      = 0;
  th->image_view = 0;
  th->imgid      = -1u;
  th->offset     = -1u;
  if(th->mem)    dt_vkfree(&tn->alloc, th->mem);
  th->mem        = 0;
  // keep dset and prev/next dlist pointers! (i.e. don't memset th)

  // now grab roi size from graph's main output node
  if(_thumbnails_get_size(imgfilename, &th->wd, &th->ht))
    return VK_INCOMPLETE;

  VkFormat format = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {
      .width  = th->wd,
      .height = th->ht,
      .depth  = 1
    },
    .mipLevels             = 1,
    .arrayLayers           = 1,
    .samples               = VK_SAMPLE_COUNT_1_BIT,
    .tiling                = VK_IMAGE_TILING_OPTIMAL,
    .usage                 =
        VK_IMAGE_ASPECT_COLOR_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT,
    .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices   = 0,
    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  err = vkCreateImage(apdt.device, &images_create_info, NULL, &th->image);
  check_vk_result(err);
  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(apdt.device, th->image, &mem_req);
  if(mem_req.memoryTypeBits != tn->memory_type_bits)
    dt_log(s_log_vk|s_log_err, "[thm] memory type bits don't match!");

  dt_vkmem_t *mem = dt_vkalloc(&tn->alloc, mem_req.size, mem_req.alignment);
  // TODO: if (!mem) we have not enough memory! need to handle this now (more cache eviction?)
  // TODO: could do batch cleanup in case we need memory:
  // walk lru list from front and kill all contents (see above)
  // but leave list as it is
  assert(mem);
  th->mem    = mem;
  th->offset = mem->offset;

  VkImageViewCreateInfo images_view_create_info = {
    .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType   = VK_IMAGE_VIEW_TYPE_2D,
    .format     = format,
    .subresourceRange = {
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel   = 0,
      .levelCount     = 1,
      .baseArrayLayer = 0,
      .layerCount     = 1
    },
  };
  VkDescriptorImageInfo img_info = {
    .sampler       = th->wd > 32 ? apdt.tex_sampler : apdt.tex_sampler_nearest,
    .imageLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet img_dset = {
    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstBinding      = 0,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .pImageInfo      = &img_info,
  };

  // bind image memory, create image view and descriptor set (used to display later on):
  vkBindImageMemory(apdt.device, th->image, tn->vkmem, th->offset);
  images_view_create_info.image = th->image;
  err = vkCreateImageView(apdt.device, &images_view_create_info, NULL, &th->image_view);
  check_vk_result(err);

  img_dset.dstSet    = th->dset;
  img_info.imageView = th->image_view;
  vkUpdateDescriptorSets(apdt.device, 1, &img_dset, 0, NULL);

  clock_t beg = clock();

  if(mem_req.size > apdt.UploadBufferSize)
  {
    if(apdt.UploadBuffer)
    {
      vkFreeMemory(apdt.device, apdt.UploadBufferMemory, 0);
      vkDestroyBuffer(apdt.device, apdt.UploadBuffer, 0);
      apdt.UploadBuffer = VK_NULL_HANDLE;
    }
  }
  if(apdt.UploadBuffer == VK_NULL_HANDLE)
  // Create the Upload Buffer:
  {
    VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = mem_req.size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    err = vkCreateBuffer(apdt.device, &buffer_info, 0, &apdt.UploadBuffer);
    apdt.UploadBufferSize = mem_req.size;
    check_vk_result(err);
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(apdt.device, apdt.UploadBuffer, &req);
    VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = req.size
    };
    alloc_info.memoryTypeIndex = qvk_get_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    err = vkAllocateMemory(apdt.device, &alloc_info, 0, &apdt.UploadBufferMemory);
    check_vk_result(err);
    err = vkBindBufferMemory(apdt.device, apdt.UploadBuffer, apdt.UploadBufferMemory, 0);
    check_vk_result(err);
  }
  int res = VK_SUCCESS;
  // Upload to Buffer:
  {
    char* mapped = 0;
    err = vkMapMemory(apdt.device, apdt.UploadBufferMemory, 0, mem_req.size, 0, (void**)(&mapped));
    check_vk_result(err);
    if(_thumbnails_read(imgfilename, mapped))
    {
      dt_log(s_log_err, "[thm] reading the thumbnail graph failed on image '%s'!", imgfilename);
      res = VK_INCOMPLETE;
    }
    VkMappedMemoryRange range[1] = {};
    range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range[0].memory = apdt.UploadBufferMemory;
    range[0].size = mem_req.size;
    err = vkFlushMappedMemoryRanges(apdt.device, 1, range);
    check_vk_result(err);
    vkUnmapMemory(apdt.device, apdt.UploadBufferMemory);
  }
  // Copy to Image:
  if(res == VK_SUCCESS)
  {
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    ap_gui_get_buffer(&command_pool, &command_buffer);
    err = vkResetCommandPool(apdt.device, command_pool, 0);
    check_vk_result(err);
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &begin_info);
    check_vk_result(err);

    VkImageMemoryBarrier copy_barrier[1] = {};
    copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier[0].image = th->image;
    copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_barrier[0].subresourceRange.levelCount = 1;
    copy_barrier[0].subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

    VkBufferImageCopy region = {
      .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .imageSubresource.layerCount = 1,
      .imageExtent.width = th->wd,
      .imageExtent.height = th->ht,
      .imageExtent.depth = 1
    };
    vkCmdCopyBufferToImage(command_buffer, apdt.UploadBuffer, th->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier use_barrier[1] = {};
    use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier[0].image = th->image;
    use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    use_barrier[0].subresourceRange.levelCount = 1;
    use_barrier[0].subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);

    VkSubmitInfo end_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer
    };
    err = vkEndCommandBuffer(command_buffer);
    check_vk_result(err);
    err = vkQueueSubmit(apdt.queue, 1, &end_info, VK_NULL_HANDLE);
    check_vk_result(err);

    err = vkDeviceWaitIdle(apdt.device);
    check_vk_result(err);
  }

  clock_t end = clock();
  dt_log(s_log_perf, "[thm] read in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);

  return res;
}

typedef struct vkdt_job_t
{
  dt_thumbnails_t *tn;
  int *abort;
  int taskid;
} vkdt_job_t;

int ap_request_vkdt_thumbnail(dt_thumbnails_t *tn, ap_image_t *img)
{
  if(tn->img_th_req + 1 == tn->img_th_done || tn->img_th_req + 1 == tn->img_th_done - tn->img_th_nb)
  {
    printf("thumb queue full\n");
    return 1;
  }
  for(int i = tn->img_th_done; i != tn->img_th_req && i <= tn->img_th_nb; i++)
  {
    if(i == tn->img_th_nb)
      i = 0;
    if(tn->img_ths[i].imgid == img->imgid) return 0;
  }
  memcpy(&tn->img_ths[tn->img_th_req], img, sizeof(ap_image_t));
  ap_image_t *img2 = &tn->img_ths[tn->img_th_req];
  tn->img_th_req++;
  if(tn->img_th_req >= tn->img_th_nb)
    tn->img_th_req = 0;
  return 0;
}

void vkdt_work(uint32_t item, void *arg)
{
  vkdt_job_t *j = (vkdt_job_t *)arg;
  dt_thumbnails_t *tn = j->tn;
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 10000;
  while(1)
  {
    if(*j->abort)
      return;
    nanosleep(&ts, NULL);
    if(tn->img_th_req != tn->img_th_done)
    {
      ap_image_t *img = &tn->img_ths[tn->img_th_done];
      char cmd[512];
      snprintf(cmd, sizeof(cmd), "%svkdt-cli -g %s/%s.cfg --width 400 --height 400 --format o-bc1 --filename %s/%x.bc1",
               dt_rc_get(&apdt.rc, "vkdt_folder", ""), img->path, img->filename, apdt.thumbnails.cachedir, img->hash);
      int res = system(cmd);
      if(res)
      {
        dt_log(s_log_db, "[thm] running vkdt failed on image '%s/%s'!", img->path, img->filename);
        // mark as dead
        char cfgfilename[512];
        char bc1filename[512];
        snprintf(cfgfilename, sizeof(cfgfilename), "%s/%x.bc1", apdt.thumbnails.cachedir, img->hash);
        snprintf(bc1filename, sizeof(bc1filename), "%sdata/bomb.bc1", dt_rc_get(&apdt.rc, "vkdt_folder", ""));
// vkdt-cli failure on some jpg and tiff ?
//        link(cfgfilename, bc1filename);
      }
      tn->img_th_done++;
      if(tn->img_th_done >= tn->img_th_nb)
        tn->img_th_done = 0;
    }
  }
}

int ap_start_vkdt_thumbnail_job(dt_thumbnails_t *tn, int *abort)
{
  static vkdt_job_t j;
  j.tn = tn;
  j.abort = abort;
  j.taskid = threads_task(1, -1, &j, vkdt_work, NULL);
  return j.taskid;
}
