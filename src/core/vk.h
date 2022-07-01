#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

static void check_vk_result(VkResult err)
{
  if (err == 0)
    return;
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0)
    abort();
}
