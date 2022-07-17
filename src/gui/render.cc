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

#include "gui/themes.hh"
#include "gui/widget_filebrowser.hh"
#include "gui/widget_navigation.hh"
#include "gui/widget_thumbnail.hh"
extern "C" {
#include "gui/gui.h"
#include "db/database.h"
#include "core/threads.h"
#include "core/core.h"
}
#include "core/vk.h"

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

    d.vk.physical_device = gpus[use_gpu];
    free(gpus);
    vkGetPhysicalDeviceMemoryProperties(d.vk.physical_device, &d.vk.mem_properties);
  }

  // Select graphics queue family
  {
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(d.vk.physical_device, &count, NULL);
    VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(d.vk.physical_device, &count, queues);
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
    err = vkCreateDevice(d.vk.physical_device, &create_info, g_Allocator, &d.vk.device);
    check_vk_result(err);
    vkGetDeviceQueue(d.vk.device, g_QueueFamily, 0, &d.vk.queue);
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
    err = vkCreateDescriptorPool(d.vk.device, &pool_info, g_Allocator, &g_DescriptorPool);
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
    err = vkCreateSampler(d.vk.device, &sampler_info, NULL, &d.vk.tex_sampler);
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
    vkCreateSampler(d.vk.device, &sampler_nearest_info, NULL, &d.vk.tex_sampler_nearest);
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
  vkGetPhysicalDeviceSurfaceSupportKHR(d.vk.physical_device, g_QueueFamily, wd->Surface, &res);
  if (res != VK_TRUE)
  {
    fprintf(stderr, "Error no WSI support on physical device 0\n");
    exit(-1);
  }

  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
  const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(d.vk.physical_device, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

  // Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
  VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
  VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
  wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(d.vk.physical_device, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
  //printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

  // Create SwapChain, RenderPass, Framebuffer, etc.
  IM_ASSERT(g_MinImageCount >= 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, d.vk.physical_device, d.vk.device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
}

static void CleanupVulkan()
{
  vkDestroyDescriptorPool(d.vk.device, g_DescriptorPool, g_Allocator);

#ifdef IMGUI_VULKAN_DEBUG_REPORT
  // Remove the debug report callback
  auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkDestroyDebugReportCallbackEXT");
  vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
#endif // IMGUI_VULKAN_DEBUG_REPORT

  vkDestroyDevice(d.vk.device, g_Allocator);
  vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow()
{
  ImGui_ImplVulkanH_DestroyWindow(g_Instance, d.vk.device, &g_MainWindowData, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
  VkResult err;

  VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
  VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  err = vkAcquireNextImageKHR(d.vk.device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
  {
    d.swapchainrebuild = 1;
    return;
  }
  check_vk_result(err);

  ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
  {
    err = vkWaitForFences(d.vk.device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
    check_vk_result(err);

    err = vkResetFences(d.vk.device, 1, &fd->Fence);
    check_vk_result(err);
  }
  {
    err = vkResetCommandPool(d.vk.device, fd->CommandPool, 0);
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
    err = vkQueueSubmit(d.vk.queue, 1, &info, fd->Fence);
    check_vk_result(err);
  }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd)
{
  if (d.swapchainrebuild)
    return;
  VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  VkPresentInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &render_complete_semaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &wd->Swapchain;
  info.pImageIndices = &wd->FrameIndex;
  VkResult err = vkQueuePresentKHR(d.vk.queue, &info);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
  {
    d.swapchainrebuild = 1;
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
char impdt_text[256];
int impdt_taskid;
int impdt_abort;
dt_filebrowser_widget_t vkdtbrowser;
dt_filebrowser_widget_t darktablebrowser;
ap_navigation_widget_t folderbrowser;
ap_navigation_widget_t tagbrowser;

extern "C" int ap_gui_init_imgui()
{
  // Setup GLFW window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  d.window = glfwCreateWindow(1280, 720, "My app", NULL, NULL);

  // Setup Vulkan
  if (!glfwVulkanSupported())
  {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }
  d.vk.device = VK_NULL_HANDLE;
  uint32_t extensions_count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
  SetupVulkan(extensions, extensions_count);

  // Create Window Surface
  VkSurfaceKHR surface;
  VkResult err = glfwCreateWindowSurface(g_Instance, d.window, g_Allocator, &surface);
  check_vk_result(err);

  // Create Framebuffers
  int w, h;
  glfwGetFramebufferSize(d.window, &w, &h);
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
  d.theme = dt_rc_get_int(&d.rc, "gui/theme", 1);
  themes_set[d.theme]();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForVulkan(d.window, true);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = g_Instance;
  init_info.PhysicalDevice = d.vk.physical_device;
  init_info.Device = d.vk.device;
  init_info.QueueFamily = g_QueueFamily;
  init_info.Queue = d.vk.queue;
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

    const char* dt_dir = dt_rc_get(&d.rc, "vkdt_folder", "");
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

    err = vkResetCommandPool(d.vk.device, command_pool, 0);
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
    err = vkQueueSubmit(d.vk.queue, 1, &end_info, VK_NULL_HANDLE);
    check_vk_result(err);

    err = vkDeviceWaitIdle(d.vk.device);
    check_vk_result(err);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }
  wd->ClearValue = (VkClearValue){{.float32={0.18f, 0.18f, 0.18f, 1.0f}}};

  // Our state
  show_demo_window = true;
  impdt_text[0] = 0;
  impdt_taskid = -1;
  impdt_abort = 0;
  d.swapchainrebuild = 1;
  d.ipl = dt_rc_get_int(&d.rc, "gui/images_per_line", 6);
  dt_filebrowser_init(&vkdtbrowser);
  dt_filebrowser_init(&darktablebrowser);
  ap_navigation_init(&folderbrowser, 0);
  ap_navigation_init(&tagbrowser, 1);
  return 0;
}

// call from main loop:
extern "C" void ap_gui_render_frame_imgui()
{
  ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;

  // Resize swap chain?
  if (d.swapchainrebuild)
  {
    glfwGetFramebufferSize(d.window, &d.win_width, &d.win_height);
    ap_gui_window_resize();
    if (d.win_width > 0 && d.win_height > 0)
    {
      ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
      ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, d.vk.physical_device, d.vk.device, &g_MainWindowData, g_QueueFamily, g_Allocator, d.win_width, d.win_height, g_MinImageCount);
      g_MainWindowData.FrameIndex = 0;
      int new_width;
      int new_height;
      glfwGetFramebufferSize(d.window, &new_width, &new_height);
      if(new_width == d.win_width && new_height == d.win_height) // F11 needs twice
        d.swapchainrebuild = 0;
      ImGuiStyle & style = ImGui::GetStyle();
      style.WindowPadding = ImVec2(d.win_width*0.001, d.win_width*0.001);
      style.FramePadding  = ImVec2(d.win_width*0.002, d.win_width*0.002);
      style.ItemSpacing   = ImVec2(d.win_width*0.002, d.win_width*0.002);
    }
  }

  // Start the Dear ImGui frame
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // right panel
  {
    ImGui::SetNextWindowPos(ImVec2(d.win_width - d.panel_wd, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(d.panel_wd, d.panel_ht), ImGuiCond_Always);
    ImGui::Begin("RightPanel", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    if(ImGui::CollapsingHeader("Images"))
    {
      ImGui::Indent();
      int32_t filter_prop = static_cast<int32_t>(d.img.collection_filter);
      int32_t sort_prop   = static_cast<int32_t>(d.img.collection_sort);

      if(ImGui::Combo("sort", &sort_prop, dt_db_property_text))
      {
        d.img.collection_sort = static_cast<dt_db_property_t>(sort_prop);
        dt_gui_update_collection();
      }
      if(ImGui::Combo("filter", &filter_prop, dt_db_property_text))
      {
        d.img.collection_filter = static_cast<dt_db_property_t>(filter_prop);
        dt_gui_update_collection();
      }
      if(filter_prop == s_prop_rating || filter_prop == s_prop_labels)
      {
        int filter_val = static_cast<int>(d.img.collection_filter_val);
        if(ImGui::InputInt("filter value", &filter_val, 1, 100, 0))
        {
          d.img.collection_filter_val = static_cast<uint64_t>(filter_val);
          dt_gui_update_collection();
        }
      }
      else if(filter_prop == s_prop_filename || filter_prop == s_prop_createdate)
      {
        if(ImGui::InputText("filter text", d.img.collection_filter_text, IM_ARRAYSIZE(d.img.collection_filter_text)))
        {
          dt_gui_update_collection();
        }
      }
      ImGui::Unindent();

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
      const char* combo_preview_value = themes_name[d.theme];  // Pass in the preview value visible before opening the combo (it could be anything)
      if (ImGui::BeginCombo("theme", combo_preview_value, 0))
      {
        for (int n = 0; themes_name[n]; n++)
        {
          const bool is_selected = (d.theme == n);
          if (ImGui::Selectable(themes_name[n], is_selected))
          {
            d.theme = n;
            themes_set[n]();
            dt_rc_set_int(&d.rc, "gui/theme", n);
          }
          // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
          if (is_selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      if(ImGui::Button("Vkdt folder"))
      {
        const char* vk_dir = dt_rc_get(&d.rc, "vkdt_folder", "");
        if(vk_dir[0])
          snprintf(vkdtbrowser.cwd, sizeof(vkdtbrowser.cwd), "%s", vk_dir);
        dt_filebrowser_open(&vkdtbrowser);
      }
      if(dt_filebrowser_display(&vkdtbrowser, 'd'))
      { // "ok" pressed
        dt_rc_set(&d.rc, "vkdt_folder", vkdtbrowser.cwd);
        dt_filebrowser_cleanup(&vkdtbrowser); // reset all but cwd
      }
      ImGui::SameLine();
      ImGui::Text("%s", dt_rc_get(&d.rc, "vkdt_folder", ""));

      int ipl = d.ipl;
      if(ImGui::InputInt("number of images per line", &ipl, 1, 20, 0))
      {
        d.ipl = ipl;
        dt_rc_set_int(&d.rc, "gui/images_per_line", d.ipl);
      }

      if(ImGui::CollapsingHeader("Import darktable"))
      {
        if(ImGui::Button("Darktable folder"))
        {
          const char* dt_dir = dt_rc_get(&d.rc, "darktable_folder", "");
          if(dt_dir[0])
            snprintf(darktablebrowser.cwd, sizeof(darktablebrowser.cwd), "%s", dt_dir);
          dt_filebrowser_open(&darktablebrowser);
        }
        if(dt_filebrowser_display(&darktablebrowser, 'd'))
        { // "ok" pressed
          dt_rc_set(&d.rc, "darktable_folder", darktablebrowser.cwd);
          dt_filebrowser_cleanup(&darktablebrowser); // reset all but cwd
        }
        ImGui::SameLine();
        ImGui::Text("%s", dt_rc_get(&d.rc, "darktable_folder", ""));

        if(ImGui::Button("Import"))
        {
          if(!threads_task_running(impdt_taskid))
          {
            const char* dt_dir = dt_rc_get(&d.rc, "darktable_folder", "");
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
    }

//      ImGui::ShowMetricsWindow();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImVec2 window_size = ImGui::GetWindowSize();
    if(window_size.x != d.panel_wd)
    {
      d.panel_wd = window_size.x;
      dt_rc_set_int(&d.rc, "gui/right_panel_width", d.panel_wd);
      d.swapchainrebuild = 1;
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
    ImGui::SetNextWindowPos (ImVec2(d.center_x,  d.center_y),  ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(d.center_wd, d.center_ht), ImGuiCond_Always);
    ImGui::Begin("LighttableCenter", 0, window_flags);

    const int border = 0.004 * d.win_width;
    const int width = d.center_wd / d.ipl - border*2 - style.ItemSpacing.x*2;
    const int height = width;
    const int cnt = d.img.collection_cnt;
    const int lines = (cnt+d.ipl-1)/d.ipl;

    if(d.scrollpos == s_scroll_top)
      ImGui::SetScrollY(0.0f);
    else if (d.scrollpos == s_scroll_current)
    {
      if(d.img.current_colid != -1u)
        ImGui::SetScrollY((float)d.img.current_colid/(float)d.img.collection_cnt * ImGui::GetScrollMaxY());
    }
    else if (d.scrollpos == s_scroll_bottom)
      ImGui::SetScrollY(ImGui::GetScrollMaxY());
    d.scrollpos = 0;

    ImGuiListClipper clipper;
    clipper.Begin(lines);
    int first = 1;
    while(clipper.Step())
    {
      // for whatever reason (gauge sizes?) imgui will always pass [0,1) as a first range.
      // we don't want these to trigger a deferred load.
      if(first || clipper.ItemsHeight > 0.0f)
      {
        first = 0;
        dt_thumbnails_load_list(clipper.DisplayStart * d.ipl, clipper.DisplayEnd * d.ipl);
        for(int line=clipper.DisplayStart;line<clipper.DisplayEnd;line++)
        {
          uint32_t i = line * d.ipl;
          for(uint32_t k = 0; k < d.ipl; k++)
          {
            const uint32_t j = i + k;
            ap_image_t *image = &d.img.images[d.img.collection[j]];
            uint32_t tid = image->thumbnail;
            if(tid == 0)
              ap_request_vkdt_thumbnail(image);
            char img[256];
            snprintf(img, sizeof(img), "%s", image->filename);

            if(d.img.collection[j] == d.img.current_imgid)
            {
              ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0, 1.0, 1.0, 1.0));
              ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0.8, 0.8, 1.0));
            }
            else if(d.img.images[d.img.collection[j]].selected)
            {
              ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6, 0.6, 0.6, 1.0));
              ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0.8, 0.8, 1.0));
            }
            float scale = MIN(
                width / (float)d.thumbs.thumb[tid].wd,
                height / (float)d.thumbs.thumb[tid].ht);
            float w = d.thumbs.thumb[tid].wd * scale;
            float h = d.thumbs.thumb[tid].ht * scale;

            uint32_t ret = ImGui::ThumbnailImage(
                d.img.collection[j],
                d.thumbs.thumb[tid].dset,
                ImVec2(w, h),
                ImVec2(0,0), ImVec2(1,1),
                border,
                ImVec4(0.5f,0.5f,0.5f,1.0f),
                ImVec4(1.0f,1.0f,1.0f,1.0f),
                image->rating,  // rating
                image->labels,  // labels
                img,
                0); // nav_focus

            if(d.img.collection[j] == d.img.current_imgid ||
              d.img.images[d.img.collection[j]].selected)
              ImGui::PopStyleColor(2);

            if(ret)
            {
//              vkdt.wstate.busy += 2;
              if(ImGui::GetIO().KeyCtrl)
              {
                if(d.img.images[d.img.collection[j]].selected)
                  dt_gui_selection_remove(j);
                else
                  dt_gui_selection_add(j);
              }
              else if(ImGui::GetIO().KeyShift)
              { // shift selects ranges
                if(d.img.current_colid != -1u)
                {
                  int a = MIN(d.img.current_colid, j);
                  int b = MAX(d.img.current_colid, j);
                  dt_gui_selection_clear();
                  for(uint32_t i=a;i<=b;i++)
                    dt_gui_selection_add(i);
                }
              }
              else
              { // no modifier, select exactly this image:
                if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                  ap_gui_image_edit(d.img.collection[j]);
                }
                else
                {
                  dt_gui_selection_clear();
                  dt_gui_selection_add(j);
                }
              }
            }

            if(j + 1 >= cnt) break;
            if(k < d.ipl - 1) ImGui::SameLine();
          }
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
    FrameRender(wd, draw_data);
    FramePresent(wd);
  }

}

extern "C" void ap_gui_cleanup_imgui()
{
  // Cleanup
  impdt_abort = 1;
  d.thumbs.img_th_abort = 1;
  VkResult err = vkDeviceWaitIdle(d.vk.device);
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

extern "C" void ap_gui_get_buffer(VkCommandPool *command_pool, VkCommandBuffer *command_buffer, VkFence *fence)
{
  ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
  if(command_pool)
    *command_pool = wd->Frames[wd->FrameIndex].CommandPool;
  if(command_buffer)
    *command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;
  if(fence)
    *fence = wd->Frames[wd->FrameIndex].Fence;
}

extern "C" void dt_gui_imgui_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}
