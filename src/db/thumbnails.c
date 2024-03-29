#include "core/core.h"
#include "core/log.h"
#include "core/dlist.h"
#include "core/vk.h"
#include "core/token.h"
#include "db/thumbnails.h"
#include "gui/gui.h"
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
dt_thumbnails_init(dt_thumbnails_t *tn, const int wd, const int ht, const int cnt, const size_t heap_size,
                   VkFormat img_format)
{
  memset(tn, 0, sizeof(*tn));


  tn->thumb_wd = wd,
  tn->thumb_ht = ht,
  tn->thumb_max = cnt;

  tn->thumb = malloc(sizeof(dt_thumbnail_t)*tn->thumb_max);
  memset(tn->thumb, 0, sizeof(dt_thumbnail_t)*tn->thumb_max);
  // need at least one extra slot to catch free block (if contiguous, else more)
  dt_vkalloc_init(&tn->alloc, tn->thumb_max + 10, heap_size);

  // init lru list
  tn->lru = tn->thumb;
  tn->mru = tn->thumb + tn->thumb_max-1;
  tn->thumb[0].next = tn->thumb+1;
  tn->thumb[tn->thumb_max-1].prev = tn->thumb+tn->thumb_max-2;
  tn->thumb[0].owner = tn->thumb[tn->thumb_max-1].owner = -1u;
  for(int k=1;k<tn->thumb_max-1;k++)
  {
    tn->thumb[k].next = tn->thumb+k+1;
    tn->thumb[k].prev = tn->thumb+k-1;
    tn->thumb[k].owner = -1u;
  }
  dt_log(s_log_db, "allocating %3.1f MB for thumbnails", heap_size/(1024.0*1024.0));

  // alloc dummy image to get memory type bits and something to display
  tn->format = img_format;
  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = tn->format,
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
  err = vkCreateImage(d.vk.device, &images_create_info, NULL, &img);
  check_vk_result(err);
  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(d.vk.device, img, &mem_req);
  tn->memory_type_bits = mem_req.memoryTypeBits;
  vkDestroyImage(d.vk.device, img, VK_NULL_HANDLE);
  VkMemoryAllocateInfo mem_alloc_info = {
    .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize  = heap_size,
    .memoryTypeIndex = qvk_get_memory_type(tn->memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
  };
  err = vkAllocateMemory(d.vk.device, &mem_alloc_info, 0, &tn->vkmem);
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
  vkCreateDescriptorPool(d.vk.device, &pool_info, 0, &tn->dset_pool);
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
  err = vkCreateDescriptorSetLayout(d.vk.device, &dset_layout_info, 0, &tn->dset_layout);
  check_vk_result(err);

  VkDescriptorSetAllocateInfo dset_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = tn->dset_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &tn->dset_layout,
  };
  for(int i=0;i<tn->thumb_max;i++)
  {
    err = vkAllocateDescriptorSets(d.vk.device, &dset_info, &tn->thumb[i].dset);
    check_vk_result(err);
  }
  d.vk.image_buffer = VK_NULL_HANDLE;
  d.vk.image_buffer_size = 0;

  return VK_SUCCESS;
}

void dt_thumbnails_cleanup(dt_thumbnails_t *tn)
{
  for(int i=0;i<tn->thumb_max;i++)
  {
    if(tn->thumb[i].image)      vkDestroyImage    (d.vk.device, tn->thumb[i].image,      0);
    if(tn->thumb[i].image_view) vkDestroyImageView(d.vk.device, tn->thumb[i].image_view, 0);
  }
  free(tn->thumb);
  tn->thumb = 0;
  vkDestroySampler(d.vk.device, d.vk.tex_sampler, 0);
  vkDestroySampler(d.vk.device, d.vk.tex_sampler_nearest, 0);
  if(tn->dset_layout) vkDestroyDescriptorSetLayout(d.vk.device, tn->dset_layout, 0);
  if(tn->dset_pool)   vkDestroyDescriptorPool     (d.vk.device, tn->dset_pool,   0);
  if(tn->vkmem)       vkFreeMemory                (d.vk.device, tn->vkmem,       0);
  dt_vkalloc_cleanup(&tn->alloc);
  if(d.vk.image_buffer)
  {
    vkFreeMemory(d.vk.device, d.vk.image_buffer_memory, 0);
    vkDestroyBuffer(d.vk.device, d.vk.image_buffer, 0);
  }
  ap_fifo_clean(&tn->cache_req);
}

void dt_thumbnails_move_to_mru(dt_thumbnails_t *tn, const uint32_t index)
{
  dt_thumbnail_t *th = tn->thumb + index;
  if(th == tn->lru) tn->lru = tn->lru->next; // move head
  tn->lru->prev = 0;
  if(tn->mru == th) tn->mru = th->prev;      // going to remove mru, need to move
  DLIST_RM_ELEMENT(th);                      // disconnect old head
  tn->mru = DLIST_APPEND(tn->mru, th);       // append to end and move tail
}

dt_thumbnail_t *dt_thumbnails_allocate(dt_thumbnails_t *tn, uint32_t *index)
{
  dt_thumbnail_t *th = NULL;
  if(*index == -1u)
  { // allocate thumbnail from lru list
    th = tn->lru;
    tn->lru = tn->lru->next;             // move head
    if(tn->mru == th) tn->mru = th->prev;// going to remove mru, need to move
    DLIST_RM_ELEMENT(th);                // disconnect old head
    tn->mru = DLIST_APPEND(tn->mru, th); // append to end and move tail
    *index = th - tn->thumb;
    if(tn->reset_owner != NULL && th->owner != -1u)
      tn->reset_owner(th->owner);
  }
  else th = tn->thumb + *index;
  return th;
}

// load a previously cached thumbnail to a VkImage onto the GPU.
// returns VK_SUCCESS on success
VkResult dt_thumbnails_load_one(dt_thumbnails_t *tn, dt_thumbnail_t *th, const char *imgfilename, VkResult _thumbnails_read())
{
  VkResult err;
  // cache eviction:
  // clean up memory in case there was something here:
  if(th->image)      vkDestroyImage(d.vk.device, th->image, VK_NULL_HANDLE);
  if(th->image_view) vkDestroyImageView(d.vk.device, th->image_view, VK_NULL_HANDLE);
  th->image      = 0;
  th->image_view = 0;
  th->offset     = -1u;
  th->owner      = -1u;
  if(th->mem)    dt_vkfree(&tn->alloc, th->mem);
  th->mem        = 0;
  // keep dset and prev/next dlist pointers! (i.e. don't memset th)

  VkImageCreateInfo images_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = tn->format,
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

  err = vkCreateImage(d.vk.device, &images_create_info, NULL, &th->image);
  check_vk_result(err);
  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(d.vk.device, th->image, &mem_req);
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
    .format     = tn->format,
    .subresourceRange = {
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel   = 0,
      .levelCount     = 1,
      .baseArrayLayer = 0,
      .layerCount     = 1
    },
  };
  VkDescriptorImageInfo img_info = {
    .sampler       = th->wd > 32 ? d.vk.tex_sampler : d.vk.tex_sampler_nearest,
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
  vkBindImageMemory(d.vk.device, th->image, tn->vkmem, th->offset);
  images_view_create_info.image = th->image;
  err = vkCreateImageView(d.vk.device, &images_view_create_info, NULL, &th->image_view);
  check_vk_result(err);

  img_dset.dstSet = th->dset;
  img_info.imageView = th->image_view;
  vkUpdateDescriptorSets(d.vk.device, 1, &img_dset, 0, NULL);

  clock_t beg = clock();

  if(mem_req.size > d.vk.image_buffer_size)
  {
    if(d.vk.image_buffer)
    {
      vkFreeMemory(d.vk.device, d.vk.image_buffer_memory, 0);
      vkDestroyBuffer(d.vk.device, d.vk.image_buffer, 0);
      d.vk.image_buffer = VK_NULL_HANDLE;
    }
  }
  if(d.vk.image_buffer == VK_NULL_HANDLE)
  // Create the Upload Buffer:
  {
    VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = mem_req.size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    err = vkCreateBuffer(d.vk.device, &buffer_info, 0, &d.vk.image_buffer);
    d.vk.image_buffer_size = mem_req.size;
    check_vk_result(err);
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(d.vk.device, d.vk.image_buffer, &req);
    VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = req.size
    };
    alloc_info.memoryTypeIndex = qvk_get_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    err = vkAllocateMemory(d.vk.device, &alloc_info, 0, &d.vk.image_buffer_memory);
    check_vk_result(err);
    err = vkBindBufferMemory(d.vk.device, d.vk.image_buffer, d.vk.image_buffer_memory, 0);
    check_vk_result(err);
  }
  int res = VK_SUCCESS;
  // Upload to Buffer:
  {
    char* mapped = 0;
    err = vkMapMemory(d.vk.device, d.vk.image_buffer_memory, 0, mem_req.size, 0, (void**)(&mapped));
    check_vk_result(err);
    if(_thumbnails_read(imgfilename, mapped))
    {
      dt_log(s_log_err, "[thm] reading the thumbnail '%s'!", imgfilename);
      res = VK_INCOMPLETE;
    }
    VkMappedMemoryRange range[1] = {};
    range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range[0].memory = d.vk.image_buffer_memory;
    range[0].size = mem_req.size;
    err = vkFlushMappedMemoryRanges(d.vk.device, 1, range);
    check_vk_result(err);
    vkUnmapMemory(d.vk.device, d.vk.image_buffer_memory);
  }
  // Copy to Image:
  if(res == VK_SUCCESS)
  {
    VkCommandBuffer command_buffer;
    VkFence fence;
    ap_gui_get_buffer(NULL, &command_buffer, &fence);

    err = vkWaitForFences(d.vk.device, 1, &fence, VK_TRUE, 1ul<<40);
    check_vk_result(err);
    err = vkResetFences(d.vk.device, 1, &fence);
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
    vkCmdCopyBufferToImage(command_buffer, d.vk.image_buffer, th->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

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
    err = vkQueueSubmit(d.vk.queue, 1, &end_info, fence);
    check_vk_result(err);
  }

  clock_t end = clock();
  dt_log(s_log_perf, "[thm] read %s in %3.0fms", imgfilename, 1000.0*(end-beg)/CLOCKS_PER_SEC);

  return res;
}

static VkResult _dev_changed(ap_image_t *img)
{
  char filename[PATH_MAX+100] = {0};
  time_t tcfg = 0, tbc1 = 0;
  struct stat statbuf = {0};

  snprintf(filename, sizeof(filename), "%s/%s.cfg", img->path, img->filename);
  if(!stat(filename, &statbuf))
    tcfg = statbuf.st_mtim.tv_sec;
  else
    return VK_SUCCESS;  // no dev => no change

  snprintf(filename, sizeof(filename), "%s/%x.bc1", d.thumbs.cachedir, img->hash);
  if(!stat(filename, &statbuf))
    tbc1 = statbuf.st_mtim.tv_sec;

  if(tbc1 >= tcfg) return VK_SUCCESS; // already up to date
  return VK_INCOMPLETE;
}

void dt_thumbnails_vkdt_load_list(dt_thumbnails_t *tn, uint32_t beg, uint32_t end)
{
  // for all images in given collection
  for(int k = beg; k < end; k++)
  {
    if(k >= d.img.collection_cnt) break;
    ap_image_t *img = &d.img.images[d.img.collection[k]];

    if(img->thumb_status == s_thumb_loaded)// && img->thumbnail > 1 && img->thumbnail < tn->thumb_max)
    {
      // check if development has changed
      if(_dev_changed(img) == VK_INCOMPLETE)
      {// force update
        img->thumbnail = -1u;
        img->thumb_status = s_thumb_unavailable;
      }
      else // loaded, update lru/mru
        dt_thumbnails_move_to_mru(tn, img->thumbnail);
    }
    else if(img->thumb_status != s_thumb_dead && img->thumb_status != s_thumb_downloading)
    { // not loaded
      char filename[PATH_MAX+100] = {0};
      snprintf(filename, sizeof(filename), "%s/%x.bc1", tn->cachedir, img->hash);
      img->thumbnail = -1u;
      if(dt_thumbnails_vkdt_load_one(tn, filename, &img->thumbnail))
        img->thumbnail = 0;
      else
        img->thumb_status = s_thumb_loaded;
    }
    if(img->thumb_status == s_thumb_unavailable)
    {
      img->thumb_status = s_thumb_downloading;
      if(ap_fifo_push(&tn->cache_req, &d.img.collection[k]))
        img->thumb_status = s_thumb_unavailable;
    }
  }
}

static int _thumbnails_vkdt_get_size(const char *filename, uint32_t *wd, uint32_t *ht)
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
  if(header[0] != dt_token("bc1z") || header[1] != 1)
  {
    fprintf(stderr, "[i-bc1] %s: wrong magic number or version!\n", filename);
    gzclose(f);
  }
  *wd = 4*(header[2]/4);
  *ht = 4*(header[3]/4);
  gzclose(f);
  return 0;
}

static int _thumbnails_vkdt_read(const char *filename, void *mapped)
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
  if(header[0] != dt_token("bc1z") || header[1] != 1)
  {
    fprintf(stderr, "[i-bc1] %s: wrong magic number or version!\n", filename);
    gzclose(f);
    return 1;
  }
  const uint32_t wd = 4*(header[2]/4);
  const uint32_t ht = 4*(header[3]/4);
  // fprintf(stderr, "[i-bc1] %s magic %"PRItkn" version %u dim %u x %u\n",
  //     filename, dt_token_str(header[0]),
  //     header[1], header[2], header[3]);
  gzread(f, mapped, sizeof(uint8_t)*8*(wd/4)*(ht/4));
  gzclose(f);
  return 0;
}

VkResult dt_thumbnails_vkdt_load_one(dt_thumbnails_t *tn, const char *filename, uint32_t *thumb_index)
{
  char imgfilename[PATH_MAX+100] = {0};
  if(strncmp(filename, "data/", 5))
  { // only hash images that aren't straight from our resource directory:
    snprintf(imgfilename, sizeof(imgfilename), "%s", filename);
  }
  else
  {
    const char* dt_dir = dt_rc_get(&d.rc, "vkdt_folder", "");
    snprintf(imgfilename, sizeof(imgfilename), "%s/%s", dt_dir, filename);
  }
  struct stat statbuf = {0};
  if(stat(imgfilename, &statbuf)) return VK_INCOMPLETE;

  dt_thumbnail_t *th = dt_thumbnails_allocate(tn, thumb_index);

  if(_thumbnails_vkdt_get_size(imgfilename, &th->wd, &th->ht))
    return VK_INCOMPLETE;

  return dt_thumbnails_load_one(tn, th, imgfilename, _thumbnails_vkdt_read);
}

VkResult dt_thumbnails_vkdt_init()
{

  VkResult res = dt_thumbnails_init(&d.thumbs, 400, 400, 1000, 1ul<<30, VK_FORMAT_BC1_RGB_SRGB_BLOCK);

  const char *home = getenv("HOME");
  snprintf(d.thumbs.cachedir, sizeof(d.thumbs.cachedir), "%s/.cache/vkdt", home);
  int err1 = mkdir(d.thumbs.cachedir, 0755);
  if(err1 && errno != EEXIST)
  {
    dt_log(s_log_err|s_log_db, "could not create thumbnail cache directory!");
    return VK_INCOMPLETE;
  }

  // init thumbnail generation queue
  ap_fifo_init(&d.thumbs.cache_req, sizeof(uint32_t), 3);
  d.thumbs.cache_req_abort = 0;
  ap_thumbnails_vkdt_start_job();

  // [0] and [1] are special: busy bee and bomb
  d.thumbs.lru = d.thumbs.thumb + 3;
  d.thumbs.thumb[3].prev = 0;
  uint32_t thumb_index = 0;
  if(dt_thumbnails_vkdt_load_one(&d.thumbs, "data/busybee.bc1", &thumb_index) != VK_SUCCESS)
  {
    dt_log(s_log_err|s_log_db, "could not load required thumbnail symbols!");
    return VK_INCOMPLETE;
  }
  // workaround: setting bomb on thumb [1] overrides the busybee ...
  thumb_index = 1;
  if(dt_thumbnails_vkdt_load_one(&d.thumbs, "data/busybee.bc1", &thumb_index) != VK_SUCCESS)
  {
    dt_log(s_log_err|s_log_db, "could not load required thumbnail symbols!");
    return VK_INCOMPLETE;
  }
  thumb_index = 2;
  if(dt_thumbnails_vkdt_load_one(&d.thumbs, "data/bomb.bc1", &thumb_index) != VK_SUCCESS)
  {
    dt_log(s_log_err|s_log_db, "could not load required thumbnail symbols!");
    return VK_INCOMPLETE;
  }
  return res;
}

typedef struct vkdt_job_t
{
  int taskid;
} vkdt_job_t;

void vkdt_work(uint32_t item, void *arg)
{
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 10000;

  while(1)
  {
    if(d.thumbs.cache_req_abort)
      return;
    nanosleep(&ts, NULL);
    uint32_t index;
    int res = ap_fifo_pop(&d.thumbs.cache_req, &index);
    if(!res)
    {
      char cmd[512];
      const char *f2 = d.img.images[index].filename + strlen(d.img.images[index].filename);
      while(f2 > d.img.images[index].filename && *f2 != '.') f2--;
      snprintf(cmd, sizeof(cmd), "%svkdt-cli -g \'%s/%s.cfg\' --width 400 --height 400 "
                                 "--format o-bc1 --filename %s/%x.bc1 "
                                 "--config param:f2srgb:main:usemat:0", // rec2020
               dt_rc_get(&d.rc, "vkdt_folder", ""), d.img.images[index].path, d.img.images[index].filename,
               d.thumbs.cachedir, d.img.images[index].hash);

      double beg = dt_time();
      int res = system(cmd);  // launch vkdt-cli
      double end = dt_time();
      if(res)
      {
        d.img.images[index].thumb_status = s_thumb_dead;
        dt_log(s_log_db, "[thm] running vkdt failed on image '%s/%s'!", d.img.images[index].path, d.img.images[index].filename);
        // mark as dead
        d.img.images[index].thumbnail = 2; // bomb
      }
      else
      {
        d.img.images[index].thumb_status = s_thumb_ondisk;
        dt_log(s_log_perf, "[thm] created in %3.0fms", 1000.0*(end-beg));
      }
    }
  }
}

int ap_thumbnails_vkdt_start_job()
{
  static vkdt_job_t j;
  j.taskid = threads_task(1, -1, &j, vkdt_work, NULL);
  return j.taskid;
}

void ap_thumbnails_vkdt_reset()
{
  ap_fifo_empty(&d.thumbs.cache_req);
}
