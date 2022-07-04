#include "themes.hh"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
// Dear ImGui: standalone example application for Glfw + Vulkan
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// Important note to the reader who wish to integrate imgui_impl_vulkan.cpp/.h in their own engine/app.
// - Common ImGui_ImplVulkan_XXX functions and structures are used to interface with imgui_impl_vulkan.cpp/.h.
//   You will use those if you want to use this rendering backend in your engine/app.
// - Helper ImGui_ImplVulkanH_XXX functions and structures are only used by this example (main.cpp) and by
//   the backend itself (imgui_impl_vulkan.cpp), but should PROBABLY NOT be used by your own engine/app code.
// Read comments in imgui_impl_vulkan.h.

#include <stdio.h>          // printf, fprintf
#include <stdlib.h>         // abort
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "widget_filebrowser.hh"
#include "widget_navigation.hh"
#include "widget_thumbnail.hh"
extern "C" {
#include "gui.h"
#include "../db/database.h"
#include "../core/threads.h"
#include "../core/core.h"
}
#include "../core/vk.h"

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

//#define IMGUI_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

static VkAllocationCallbacks*   g_Allocator = NULL;
static VkInstance               g_Instance = VK_NULL_HANDLE;
static uint32_t                 g_QueueFamily = (uint32_t)-1;
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static int                      g_MinImageCount = 2;
static int                      g_SwapChainRebuild = 1;

#ifdef IMGUI_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
  (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
  fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
  return VK_FALSE;
}
#endif // IMGUI_VULKAN_DEBUG_REPORT

static void SetupVulkan(const char** extensions, uint32_t extensions_count)
{
  VkResult err;

  // Create Vulkan Instance
  {
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledExtensionCount = extensions_count;
    create_info.ppEnabledExtensionNames = extensions;
#ifdef IMGUI_VULKAN_DEBUG_REPORT
    // Enabling validation layers
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = layers;

    // Enable debug report extension (we need additional storage, so we duplicate the user array to add our new extension to it)
    const char** extensions_ext = (const char**)malloc(sizeof(const char*) * (extensions_count + 1));
    memcpy(extensions_ext, extensions, extensions_count * sizeof(const char*));
    extensions_ext[extensions_count] = "VK_EXT_debug_report";
    create_info.enabledExtensionCount = extensions_count + 1;
    create_info.ppEnabledExtensionNames = extensions_ext;

    // Create Vulkan Instance
    err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
    check_vk_result(err);
    free(extensions_ext);

    // Get the function pointer (required for any extensions)
    auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkCreateDebugReportCallbackEXT");
    IM_ASSERT(vkCreateDebugReportCallbackEXT != NULL);

    // Setup the debug report callback
    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
    debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debug_report_ci.pfnCallback = debug_report;
    debug_report_ci.pUserData = NULL;
    err = vkCreateDebugReportCallbackEXT(g_Instance, &debug_report_ci, g_Allocator, &g_DebugReport);
    check_vk_result(err);
#else
    // Create Vulkan Instance without any debug feature
    err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
    check_vk_result(err);
    IM_UNUSED(g_DebugReport);
#endif
  }

  // Select GPU
  {
    uint32_t gpu_count;
    err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, NULL);
    check_vk_result(err);
    IM_ASSERT(gpu_count > 0);

    VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpu_count);
    err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, gpus);
    check_vk_result(err);

    // If a number >1 of GPUs got reported, find discrete GPU if present, or use first one available. This covers
    // most common cases (multi-gpu/integrated+dedicated graphics). Handling more complicated setups (multiple
    // dedicated GPUs) is out of scope of this sample.
    int use_gpu = 0;
    for (int i = 0; i < (int)gpu_count; i++)
    {
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(gpus[i], &properties);
      if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      {
        use_gpu = i;
        break;
      }
    }

    apdt.physical_device = gpus[use_gpu];
    free(gpus);
    vkGetPhysicalDeviceMemoryProperties(apdt.physical_device, &apdt.mem_properties);
  }

  // Select graphics queue family
  {
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(apdt.physical_device, &count, NULL);
    VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(apdt.physical_device, &count, queues);
    for (uint32_t i = 0; i < count; i++)
      if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
        g_QueueFamily = i;
        break;
      }
    free(queues);
    IM_ASSERT(g_QueueFamily != (uint32_t)-1);
  }

  // Create Logical Device (with 1 queue)
  {
    int device_extension_count = 1;
    const char* device_extensions[] = { "VK_KHR_swapchain" };
    const float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = g_QueueFamily;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
    create_info.pQueueCreateInfos = queue_info;
    create_info.enabledExtensionCount = device_extension_count;
    create_info.ppEnabledExtensionNames = device_extensions;
    err = vkCreateDevice(apdt.physical_device, &create_info, g_Allocator, &apdt.device);
    check_vk_result(err);
    vkGetDeviceQueue(apdt.device, g_QueueFamily, 0, &apdt.queue);
  }

  // Create Descriptor Pool
  {
    VkDescriptorPoolSize pool_sizes[] =
    {
      { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
      { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    err = vkCreateDescriptorPool(apdt.device, &pool_info, g_Allocator, &g_DescriptorPool);
    check_vk_result(err);
  }
  {
    // create texture samplers
    VkSamplerCreateInfo sampler_info = {};
      sampler_info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      sampler_info.magFilter               = VK_FILTER_LINEAR;
      sampler_info.minFilter               = VK_FILTER_LINEAR;
      sampler_info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      sampler_info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      sampler_info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      sampler_info.anisotropyEnable        = VK_FALSE;
      sampler_info.maxAnisotropy           = 16;
      sampler_info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      sampler_info.unnormalizedCoordinates = VK_FALSE;
      sampler_info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      sampler_info.minLod                  = 0.0f;
      sampler_info.maxLod                  = 128.0f;
    err = vkCreateSampler(apdt.device, &sampler_info, NULL, &apdt.tex_sampler);
    check_vk_result(err);
    VkSamplerCreateInfo sampler_nearest_info = {};
      sampler_nearest_info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      sampler_nearest_info.magFilter               = VK_FILTER_NEAREST;
      sampler_nearest_info.minFilter               = VK_FILTER_NEAREST;
      sampler_nearest_info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      sampler_nearest_info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      sampler_nearest_info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      sampler_nearest_info.anisotropyEnable        = VK_FALSE;
      sampler_nearest_info.maxAnisotropy           = 16;
      sampler_nearest_info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      sampler_nearest_info.unnormalizedCoordinates = VK_FALSE;
      sampler_nearest_info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    vkCreateSampler(apdt.device, &sampler_nearest_info, NULL, &apdt.tex_sampler_nearest);
    check_vk_result(err);
  }
}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
  wd->Surface = surface;

  // Check for WSI support
  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR(apdt.physical_device, g_QueueFamily, wd->Surface, &res);
  if (res != VK_TRUE)
  {
    fprintf(stderr, "Error no WSI support on physical device 0\n");
    exit(-1);
  }

  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
  const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(apdt.physical_device, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

  // Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
  VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
  VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
  wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(apdt.physical_device, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
  //printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

  // Create SwapChain, RenderPass, Framebuffer, etc.
  IM_ASSERT(g_MinImageCount >= 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, apdt.physical_device, apdt.device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
}

static void CleanupVulkan()
{
  vkDestroyDescriptorPool(apdt.device, g_DescriptorPool, g_Allocator);

#ifdef IMGUI_VULKAN_DEBUG_REPORT
  // Remove the debug report callback
  auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkDestroyDebugReportCallbackEXT");
  vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
#endif // IMGUI_VULKAN_DEBUG_REPORT

  vkDestroyDevice(apdt.device, g_Allocator);
  vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow()
{
  ImGui_ImplVulkanH_DestroyWindow(g_Instance, apdt.device, &g_MainWindowData, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
  VkResult err;

  VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
  VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  err = vkAcquireNextImageKHR(apdt.device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
  {
    g_SwapChainRebuild = true;
    return;
  }
  check_vk_result(err);

  ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
  {
    err = vkWaitForFences(apdt.device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
    check_vk_result(err);

    err = vkResetFences(apdt.device, 1, &fd->Fence);
    check_vk_result(err);
  }
  {
    err = vkResetCommandPool(apdt.device, fd->CommandPool, 0);
    check_vk_result(err);
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
    check_vk_result(err);
  }
  {
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = wd->RenderPass;
    info.framebuffer = fd->Framebuffer;
    info.renderArea.extent.width = wd->Width;
    info.renderArea.extent.height = wd->Height;
    info.clearValueCount = 1;
    info.pClearValues = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
  }

  // Record dear imgui primitives into command buffer
  ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

  // Submit command buffer
  vkCmdEndRenderPass(fd->CommandBuffer);
  {
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &image_acquired_semaphore;
    info.pWaitDstStageMask = &wait_stage;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &fd->CommandBuffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &render_complete_semaphore;

    err = vkEndCommandBuffer(fd->CommandBuffer);
    check_vk_result(err);
    err = vkQueueSubmit(apdt.queue, 1, &info, fd->Fence);
    check_vk_result(err);
  }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd)
{
  if (g_SwapChainRebuild)
    return;
  VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  VkPresentInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &render_complete_semaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &wd->Swapchain;
  info.pImageIndices = &wd->FrameIndex;
  VkResult err = vkQueuePresentKHR(apdt.queue, &info);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
  {
    g_SwapChainRebuild = true;
    return;
  }
  check_vk_result(err);
  wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->ImageCount; // Now we can use the next set of semaphores
}

static void glfw_error_callback(int error, const char* description)
{
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

// Our state
bool show_demo_window;
bool show_another_window;
ImVec4 clear_color;
char impdt_text[256];
int impdt_taskid;
int impdt_abort;
dt_filebrowser_widget_t filebrowser;
ap_navigation_widget_t folderbrowser;
ap_navigation_widget_t tagbrowser;

extern "C" int ap_gui_init_imgui()
{
  // Setup GLFW window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  apdt.window = glfwCreateWindow(1280, 720, "My app", NULL, NULL);

  // Setup Vulkan
  if (!glfwVulkanSupported())
  {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }
  apdt.device = VK_NULL_HANDLE;
  uint32_t extensions_count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
  SetupVulkan(extensions, extensions_count);

  // Create Window Surface
  VkSurfaceKHR surface;
  VkResult err = glfwCreateWindowSurface(g_Instance, apdt.window, g_Allocator, &surface);
  check_vk_result(err);

  // Create Framebuffers
  int w, h;
  glfwGetFramebufferSize(apdt.window, &w, &h);
  ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
  SetupVulkanWindow(wd, surface, w, h);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  // ImGui::StyleColorsDark();
  // ImGui::StyleColorsClassic();
  apdt.theme = dt_rc_get_int(&apdt.rc, "gui/theme", 1);
  themes_set[apdt.theme]();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForVulkan(apdt.window, true);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = g_Instance;
  init_info.PhysicalDevice = apdt.physical_device;
  init_info.Device = apdt.device;
  init_info.QueueFamily = g_QueueFamily;
  init_info.Queue = apdt.queue;
  init_info.PipelineCache = g_PipelineCache;
  init_info.DescriptorPool = g_DescriptorPool;
  init_info.Subpass = 0;
  init_info.MinImageCount = g_MinImageCount;
  init_info.ImageCount = wd->ImageCount;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.Allocator = g_Allocator;
  init_info.CheckVkResultFn = check_vk_result;
  ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

  char tmp[PATH_MAX+100] = {0};
  {
    int monitors_cnt;
    GLFWmonitor** monitors = glfwGetMonitors(&monitors_cnt);
    if(monitors_cnt > 2)
      fprintf(stderr, "[gui] you have more than 2 monitors attached! only the first two will be colour managed!\n");
    const char *name0 = glfwGetMonitorName(monitors[0]);
    const char *name1 = glfwGetMonitorName(monitors[MIN(monitors_cnt-1, 1)]);
    int xpos0, xpos1, ypos;
    glfwGetMonitorPos(monitors[0], &xpos0, &ypos);
    glfwGetMonitorPos(monitors[MIN(monitors_cnt-1, 1)], &xpos1, &ypos);
    float gamma0[] = {1.0f/2.2f, 1.0f/2.2f, 1.0f/2.2f};
    float rec2020_to_dspy0[] = { // to linear sRGB D65
       1.66022709, -0.58754775, -0.07283832,
      -0.12455356,  1.13292608, -0.0083496,
      -0.01815511, -0.100603  ,  1.11899813 };
    float gamma1[] = {1.0f/2.2f, 1.0f/2.2f, 1.0f/2.2f};
    float rec2020_to_dspy1[] = { // to linear sRGB D65
       1.66022709, -0.58754775, -0.07283832,
      -0.12455356,  1.13292608, -0.0083496,
      -0.01815511, -0.100603  ,  1.11899813 };

    const char* dt_dir = dt_rc_get(&apdt.rc, "vkdt_folder", "");
    snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_dir, name0);
    FILE *f = fopen(tmp, "r");
    if(f)
    {
      fscanf(f, "%f %f %f\n", gamma0, gamma0+1, gamma0+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+0, rec2020_to_dspy0+1, rec2020_to_dspy0+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+3, rec2020_to_dspy0+4, rec2020_to_dspy0+5);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy0+6, rec2020_to_dspy0+7, rec2020_to_dspy0+8);
      fclose(f);
    }
    else fprintf(stderr, "[gui] no display profile file display.%s, using sRGB!\n", name0);
    snprintf(tmp, sizeof(tmp), "%s/display.%s", dt_dir, name1);
    f = fopen(tmp, "r");
    if(f)
    {
      fscanf(f, "%f %f %f\n", gamma1, gamma1+1, gamma1+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy1+0, rec2020_to_dspy1+1, rec2020_to_dspy1+2);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy1+3, rec2020_to_dspy1+4, rec2020_to_dspy1+5);
      fscanf(f, "%f %f %f\n", rec2020_to_dspy1+6, rec2020_to_dspy1+7, rec2020_to_dspy1+8);
      fclose(f);
    }
    else fprintf(stderr, "[gui] no display profile file display.%s, using sRGB!\n", name1);
    int bitdepth = 8; // the display output will be dithered according to this
    if(g_MainWindowData.SurfaceFormat.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
       g_MainWindowData.SurfaceFormat.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
      bitdepth = 10;
    ImGui_ImplVulkan_SetDisplayProfile(gamma0, rec2020_to_dspy0, gamma1, rec2020_to_dspy1, xpos1, bitdepth);
  }


  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
  // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
  // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
  // - Read 'docs/FONTS.md' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
  //io.Fonts->AddFontDefault();
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
  //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
  //IM_ASSERT(font != NULL);

  // Upload Fonts
  {
    // Use any command queue
    VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
    VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

    err = vkResetCommandPool(apdt.device, command_pool, 0);
    check_vk_result(err);
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &begin_info);
    check_vk_result(err);

    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    err = vkEndCommandBuffer(command_buffer);
    check_vk_result(err);
    err = vkQueueSubmit(apdt.queue, 1, &end_info, VK_NULL_HANDLE);
    check_vk_result(err);

    err = vkDeviceWaitIdle(apdt.device);
    check_vk_result(err);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }
  wd->ClearValue = (VkClearValue){{.float32={0.18f, 0.18f, 0.18f, 1.0f}}};

  // Our state
  show_demo_window = true;
  clear_color = ImVec4(0.5f, 0.5f, 0.50f, 1.00f);
  impdt_text[0] = 0;
  impdt_taskid = -1;
  impdt_abort = 0;
  dt_filebrowser_init(&filebrowser);
  ap_navigation_init(&folderbrowser, 0);
  ap_navigation_init(&tagbrowser, 1);
  return 0;
}

// call from main loop:
extern "C" void ap_gui_render_frame_imgui()
{
  ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;

  // Resize swap chain?
  if (g_SwapChainRebuild)
  {
    glfwGetFramebufferSize(apdt.window, &apdt.win_width, &apdt.win_height);
    ap_gui_window_resize();
    if (apdt.win_width > 0 && apdt.win_height > 0)
    {
      ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
      ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, apdt.physical_device, apdt.device, &g_MainWindowData, g_QueueFamily, g_Allocator, apdt.win_width, apdt.win_height, g_MinImageCount);
      g_MainWindowData.FrameIndex = 0;
      g_SwapChainRebuild = false;
      ImGuiStyle & style = ImGui::GetStyle();
      style.WindowPadding = ImVec2(apdt.win_width*0.001, apdt.win_width*0.001);
      style.FramePadding  = ImVec2(apdt.win_width*0.002, apdt.win_width*0.002);
      style.ItemSpacing   = ImVec2(apdt.win_width*0.002, apdt.win_width*0.002);
    }
  }

  // Start the Dear ImGui frame
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // right panel
  {
    ImGui::SetNextWindowPos(ImVec2(apdt.win_width - apdt.panel_wd, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(apdt.panel_wd, apdt.panel_ht), ImGuiCond_Always);
    ImGui::Begin("RightPanel", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    if(ImGui::CollapsingHeader("Images"))
    {
      ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
      if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
      {
        if (ImGui::BeginTabItem("Folders"))
        {
          ap_navigation_open(&folderbrowser);
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Tags"))
        {
          ap_navigation_open(&tagbrowser);
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
      ImGui::Separator();
    }

    if(ImGui::CollapsingHeader("Preferences"))
    {
      const char* combo_preview_value = themes_name[apdt.theme];  // Pass in the preview value visible before opening the combo (it could be anything)
      if (ImGui::BeginCombo("theme", combo_preview_value, 0))
      {
        for (int n = 0; themes_name[n]; n++)
        {
          const bool is_selected = (apdt.theme == n);
          if (ImGui::Selectable(themes_name[n], is_selected))
          {
            apdt.theme = n;
            themes_set[n]();
            dt_rc_set_int(&apdt.rc, "gui/theme", n);
          }
          // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
          if (is_selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      if(ImGui::Button("Vkdt folder"))
      {
        const char* dt_dir = dt_rc_get(&apdt.rc, "vkdt_folder", "");
        if(dt_dir[0])
          snprintf(filebrowser.cwd, sizeof(filebrowser.cwd), "%s", dt_dir);
        dt_filebrowser_open(&filebrowser);
      }
      if(dt_filebrowser_display(&filebrowser, 'd'))
      { // "ok" pressed
        dt_rc_set(&apdt.rc, "vkdt_folder", filebrowser.cwd);
        dt_filebrowser_cleanup(&filebrowser); // reset all but cwd
      }
      ImGui::SameLine();
      ImGui::Text("%s", dt_rc_get(&apdt.rc, "vkdt_folder", ""));
    }

    if(ImGui::CollapsingHeader("Import darktable"))
    {
      if(ImGui::Button("Darktable folder"))
      {
        const char* dt_dir = dt_rc_get(&apdt.rc, "darktable_folder", "");
        if(dt_dir[0])
          snprintf(filebrowser.cwd, sizeof(filebrowser.cwd), "%s", dt_dir);
        dt_filebrowser_open(&filebrowser);
      }
      if(dt_filebrowser_display(&filebrowser, 'd'))
      { // "ok" pressed
        dt_rc_set(&apdt.rc, "darktable_folder", filebrowser.cwd);
        dt_filebrowser_cleanup(&filebrowser); // reset all but cwd
      }
      ImGui::SameLine();
      ImGui::Text("%s", dt_rc_get(&apdt.rc, "darktable_folder", ""));

      if(ImGui::Button("Import"))
      {
        if(!threads_task_running(impdt_taskid))
        {
          const char* dt_dir = dt_rc_get(&apdt.rc, "darktable_folder", "");
          if(dt_dir[0])
          {
            impdt_abort = 0;
            impdt_text[0] = 0;
            impdt_taskid = ap_import_darktable(dt_dir, impdt_text, &impdt_abort);
          }
        }
      }
      ImGui::SameLine();
      if(ImGui::Button("abort") && threads_task_running(impdt_taskid))
        impdt_abort = 1;
      ImGui::SameLine();
      ImGui::Text("%s", impdt_text);
    }

//      ImGui::ShowMetricsWindow();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImVec2 window_size = ImGui::GetWindowSize();
    if(window_size.x != apdt.panel_wd)
    {
      apdt.panel_wd = window_size.x;
      dt_rc_set_int(&apdt.rc, "gui/right_panel_width", apdt.panel_wd);
      g_SwapChainRebuild = 1;
    }
    ImGui::End();  // end right panel
  }

//    ImGui::ShowDemoWindow(&show_demo_window);

  // Center view
///*
  {
    ImGuiStyle &style = ImGui::GetStyle();
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoBackground;
    ImGui::SetNextWindowPos (ImVec2(apdt.center_x,  apdt.center_y),  ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(apdt.center_wd, apdt.center_ht), ImGuiCond_Always);
    ImGui::Begin("LighttableCenter", 0, window_flags);

    const int ipl = 6;
    const int border = 0.004 * apdt.win_width;
    const int width = apdt.center_wd / ipl - border*2 - style.ItemSpacing.x*2;
    const int height = width;
    const int cnt = apdt.col.image_cnt;
    const int lines = (cnt+ipl-1)/ipl;

    ImGuiListClipper clipper;
    clipper.Begin(lines);
    while(clipper.Step())
    {
      // for whatever reason (gauge sizes?) imgui will always pass [0,1) as a first range.
      // we don't want these to trigger a deferred load.
      // in case [0,1) is within the visible region, however, [1,8) might be the next
      // range, for instance. this means we'll need to do some weird dance to detect it
      // TODO: ^
      // fprintf(stderr, "displaying range %u %u\n", clipper.DisplayStart, clipper.DisplayEnd);
      dt_thumbnails_load_list(
          &apdt.thumbnails,
          &apdt.col,
          MIN(clipper.DisplayStart * ipl, (int)apdt.col.image_cnt-1),
          MIN(clipper.DisplayEnd   * ipl, (int)apdt.col.image_cnt));

      for(int line=clipper.DisplayStart;line<clipper.DisplayEnd;line++)
      {
        int i = line * ipl;
        for(int k=0;k<ipl;k++)
        {
          ap_image_t *images = apdt.col.images;
          uint32_t tid = images[i+k].thumbnail;
          if(tid == -1u) tid = 0; // busybee
          char img[256];
          snprintf(img, sizeof(img), "%s", images[i+k].filename);

//          if(vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db))
//          {
//            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0, 1.0, 1.0, 1.0));
//            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0.8, 0.8, 1.0));
//          }
//          else if(vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected)
//          {
//            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6, 0.6, 0.6, 1.0));
//            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0.8, 0.8, 1.0));
//          }
          float scale = MIN(
              width/(float)apdt.thumbnails.thumb[tid].wd,
              height/(float)apdt.thumbnails.thumb[tid].ht);
          float w = apdt.thumbnails.thumb[tid].wd * scale;
          float h = apdt.thumbnails.thumb[tid].ht * scale;

          if(ImGui::ThumbnailImage(
              i+k,
              apdt.thumbnails.thumb[tid].dset,
              ImVec2(w, h),
              ImVec2(0,0), ImVec2(1,1),
              border,
              ImVec4(0.5f,0.5f,0.5f,1.0f),
              ImVec4(1.0f,1.0f,1.0f,1.0f),
              0,  // rating
              0,  // label
              img,
              0)) // nav_focus
          {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "%svkdt %s/%s",
                     dt_rc_get(&apdt.rc, "vkdt_folder", ""), images[i+k].path, images[i+k].filename);
            FILE *handle = popen(cmd, "r");

          }

//          ImGui::PopStyleColor(2);

//          if(vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db))
//            vkdt.wstate.set_nav_focus = MAX(0, vkdt.wstate.set_nav_focus-1);
//          if(vkdt.db.collection[i] == dt_db_current_imgid(&vkdt.db) ||
//            (vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected))
//            ImGui::PopStyleColor(2);

/*          if(ret)
          {
            vkdt.wstate.busy += 2;
            if(ImGui::GetIO().KeyCtrl)
            {
              if(vkdt.db.image[vkdt.db.collection[i]].labels & s_image_label_selected)
                dt_db_selection_remove(&vkdt.db, i);
              else
                dt_db_selection_add(&vkdt.db, i);
            }
            else if(ImGui::GetIO().KeyShift)
            { // shift selects ranges
              uint32_t colid = dt_db_current_colid(&vkdt.db);
              if(colid != -1u)
              {
                int a = MIN(colid, (uint32_t)i);
                int b = MAX(colid, (uint32_t)i);
                dt_db_selection_clear(&vkdt.db);
                for(int i=a;i<=b;i++)
                  dt_db_selection_add(&vkdt.db, i);
              }
            }
            else
            { // no modifier, select exactly this image:
              if(dt_db_selection_contains(&vkdt.db, i) ||
                (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)))
              {
                dt_view_switch(s_view_darkroom);
              }
              else
              {
                dt_db_selection_clear(&vkdt.db);
                dt_db_selection_add(&vkdt.db, i);
              }
            }
          }
          */

          if(i+k+1 >= cnt) break;
          if(k < ipl-1) ImGui::SameLine();
        }
      }
    }

    ImGui::End(); // end center lighttable
  }
//*/

  // Rendering
  ImGui::Render();

  ImDrawData* draw_data = ImGui::GetDrawData();
  const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
  if (!is_minimized)
  {
//    wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
//    wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
//    wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
//    wd->ClearValue.color.float32[3] = clear_color.w;
//    wd->ClearValue.color.float32[3] = 1;
    FrameRender(wd, draw_data);
    FramePresent(wd);
  }

}

extern "C" void ap_gui_cleanup_imgui()
{
  // Cleanup
  impdt_abort = 1;
  VkResult err = vkDeviceWaitIdle(apdt.device);
  check_vk_result(err);
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

extern "C" void ap_gui_cleanup_vulkan()
{
  CleanupVulkanWindow();
  CleanupVulkan();
}

extern "C" void ap_gui_get_buffer(VkCommandPool *command_pool, VkCommandBuffer *command_buffer)
{
  ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
  *command_pool = wd->Frames[wd->FrameIndex].CommandPool;
  *command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;
}
