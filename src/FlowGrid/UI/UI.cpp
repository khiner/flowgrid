#include "UI.h"

#include "imgui.h"
#include "implot.h"

#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "Core/ImGuiSettings.h"
#include "Project/Style/Style.h"

#include "Project/FileDialog/FileDialog.h" // xxx only used for loading fonts

#ifdef TRACING_ENABLED
#include <Tracy.hpp>
#endif

// Use this to remove the default 120 fps limit.
// #define IMGUI_UNLIMITED_FRAME_RATE

// Use this to skip all project rendering and only render the metrics window,
// which shows (among other things) the frame time and rate.
// This is useful to get a sense of the best case rendering performance without project rendering overhead.
// #define ONLY_RENDER_METRICS_WINDOW

static SDL_Window *Window = nullptr;
static ImGui_ImplVulkanH_Window *VulkanWindow = nullptr;

// Vulkan data
static VkAllocationCallbacks *g_Allocator = nullptr;
static VkInstance g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice g_Device = VK_NULL_HANDLE;
static uint32_t g_QueueFamily = (uint32_t)-1;
static VkQueue g_Queue = VK_NULL_HANDLE;
static VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static int g_MinImageCount = 2;
static bool g_SwapChainRebuild = false;

static void CheckVk(VkResult err) {
    if (err != 0) throw std::runtime_error(std::format("Vulkan error: {}", int(err)));
}

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties> &properties, const char *extension) {
    for (const VkExtensionProperties &p : properties) {
        if (strcmp(p.extensionName, extension) == 0) return true;
    }
    return false;
}

static VkPhysicalDevice SetupVulkan_SelectPhysicalDevice() {
    uint32_t gpu_count;
    CheckVk(vkEnumeratePhysicalDevices(g_Instance, &gpu_count, nullptr));
    IM_ASSERT(gpu_count > 0);

    ImVector<VkPhysicalDevice> gpus;
    gpus.resize(gpu_count);
    CheckVk(vkEnumeratePhysicalDevices(g_Instance, &gpu_count, gpus.Data));

    // If any GPUs got reported, find a discrete GPU if present, or use the first one available.
    // This covers most common cases (multi-gpu/integrated+dedicated graphics).
    for (VkPhysicalDevice &device : gpus) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) return device;
    }

    // Use first GPU (integrated) if a discrete one is not available.
    if (gpu_count > 0) return gpus[0];

    return VK_NULL_HANDLE;
}

static void SetupVulkan(ImVector<const char *> instance_extensions) {
    // Create Vulkan Instance
    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        // Enumerate available extensions
        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data));

        // Enable required extensions
        if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
            instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#endif

        // Create Vulkan Instance
        create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
        create_info.ppEnabledExtensionNames = instance_extensions.Data;
        CheckVk(vkCreateInstance(&create_info, g_Allocator, &g_Instance));
    }

    // Select Physical Device (GPU)
    g_PhysicalDevice = SetupVulkan_SelectPhysicalDevice();

    // Select graphics queue family
    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, nullptr);
        VkQueueFamilyProperties *queues = (VkQueueFamilyProperties *)malloc(sizeof(VkQueueFamilyProperties) * count);
        vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, queues);
        for (uint32_t i = 0; i < count; i++) {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                g_QueueFamily = i;
                break;
            }
        }
        free(queues);
        IM_ASSERT(g_QueueFamily != (uint32_t)-1);
    }

    // Create logical device (with 1 queue)
    {
        ImVector<const char *> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");

        // Enumerate physical device extension
        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, properties.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        }
#endif

        const float queue_priority[] = {1.0f};
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = g_QueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
        create_info.ppEnabledExtensionNames = device_extensions.Data;
        CheckVk(vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device));
        vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
    }

    // Create descriptor pool.
    // The app only requires a single combined image sampler descriptor for the font image.
    {
        const VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        CheckVk(vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool));
    }
}

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window *VulkanWindow, VkSurfaceKHR surface, int width, int height) {
    VulkanWindow->Surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, VulkanWindow->Surface, &res);
    if (res != VK_TRUE) throw std::runtime_error("Error no WSI support on physical device 0");

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    VulkanWindow->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, VulkanWindow->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
    VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR};
#else
    VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
#endif
    VulkanWindow->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, VulkanWindow->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
    // std::cout << "[vulkan] Selected PresentMode = " << VulkanWindow->PresentMode << '\n';

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, VulkanWindow, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
}

static void CleanupVulkan() {
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow() {
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
}

static void RenderFrameVulkan(ImDrawData *draw_data) {
    VkSemaphore image_acquired_semaphore = VulkanWindow->FrameSemaphores[VulkanWindow->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = VulkanWindow->FrameSemaphores[VulkanWindow->SemaphoreIndex].RenderCompleteSemaphore;
    const VkResult err = vkAcquireNextImageKHR(g_Device, VulkanWindow->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &VulkanWindow->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        g_SwapChainRebuild = true;
        return;
    }
    CheckVk(err);

    ImGui_ImplVulkanH_Frame *fd = &VulkanWindow->Frames[VulkanWindow->FrameIndex];
    {
        CheckVk(vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX)); // wait indefinitely instead of periodically checking
        CheckVk(vkResetFences(g_Device, 1, &fd->Fence));
    }
    {
        CheckVk(vkResetCommandPool(g_Device, fd->CommandPool, 0));
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CheckVk(vkBeginCommandBuffer(fd->CommandBuffer, &info));
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = VulkanWindow->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = VulkanWindow->Width;
        info.renderArea.extent.height = VulkanWindow->Height;
        info.clearValueCount = 1;
        info.pClearValues = &VulkanWindow->ClearValue;
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

        CheckVk(vkEndCommandBuffer(fd->CommandBuffer));
        CheckVk(vkQueueSubmit(g_Queue, 1, &info, fd->Fence));
    }
}

UIContext::UIContext(const ImGuiSettings &settings, const fg::Style &style) : Settings(settings), Style(style) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMEPAD) != 0) {
        throw std::runtime_error(std::format("SDL_Init error: {}", SDL_GetError()));
    }

    // Enable native IME.
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    const auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    Window = SDL_CreateWindowWithPosition("FlowGrid", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (Window == nullptr) throw std::runtime_error(std::format("SDL_CreateWindow error: {}", SDL_GetError()));

    ImVector<const char *> extensions;
    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(&extensions_count, nullptr);
    extensions.resize(extensions_count);
    SDL_Vulkan_GetInstanceExtensions(&extensions_count, extensions.Data);
    SetupVulkan(extensions);

    // Create Window Surface
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(Window, g_Instance, &surface) == 0) throw std::runtime_error("Failed to create Vulkan surface.\n");

    // Create Framebuffers
    int w, h;
    SDL_GetWindowSize(Window, &w, &h);
    VulkanWindow = &g_MainWindowData;
    SetupVulkanWindow(VulkanWindow, surface, w, h);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr; // Disable ImGui's .ini file saving. We handle this manually.
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.FontAllowUserScaling = true;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForVulkan(Window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = VulkanWindow->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = CheckVk;
    ImGui_ImplVulkan_Init(&init_info, VulkanWindow->RenderPass);

    // Setup fonts
    io.FontGlobalScale = Style.ImGui.FontScale / FontAtlasScale;
    Fonts.Main = io.Fonts->AddFontFromFileTTF("../res/fonts/AbletonSansMedium.otf", 16 * FontAtlasScale);
    Fonts.FixedWidth = io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/Cousine-Regular.ttf", 15 * FontAtlasScale);
    io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/ProggyClean.ttf", 14 * FontAtlasScale);
    IGFD::AddFonts();

    // Upload Fonts
    {
        // Use any command queue
        VkCommandPool command_pool = VulkanWindow->Frames[VulkanWindow->FrameIndex].CommandPool;
        VkCommandBuffer command_buffer = VulkanWindow->Frames[VulkanWindow->FrameIndex].CommandBuffer;

        CheckVk(vkResetCommandPool(g_Device, command_pool, 0));
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CheckVk(vkBeginCommandBuffer(command_buffer, &begin_info));

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        CheckVk(vkEndCommandBuffer(command_buffer));
        CheckVk(vkQueueSubmit(g_Queue, 1, &end_info, VK_NULL_HANDLE));

        CheckVk(vkDeviceWaitIdle(g_Device));
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }
}

UIContext::~UIContext() {
    CheckVk(vkDeviceWaitIdle(g_Device));
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    ImPlot::DestroyContext();

    CleanupVulkanWindow();
    CleanupVulkan();

    SDL_DestroyWindow(Window);
    SDL_Quit();
}

void PrepareFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void RenderFrame() {
    ImGui::Render();
    ImDrawData *main_draw_data = ImGui::GetDrawData();
    const bool main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    VulkanWindow->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
    VulkanWindow->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
    VulkanWindow->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
    VulkanWindow->ClearValue.color.float32[3] = clear_color.w;
    if (!main_is_minimized) {
        RenderFrameVulkan(main_draw_data);
    }

    const auto &io = ImGui::GetIO();
    // Update and Render additional Platform Windows
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    // Present Main Platform Window
    if (!main_is_minimized) {
        if (g_SwapChainRebuild) return;

        VkSemaphore render_complete_semaphore = VulkanWindow->FrameSemaphores[VulkanWindow->SemaphoreIndex].RenderCompleteSemaphore;
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &render_complete_semaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &VulkanWindow->Swapchain;
        info.pImageIndices = &VulkanWindow->FrameIndex;
        VkResult err = vkQueuePresentKHR(g_Queue, &info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
            g_SwapChainRebuild = true;
            return;
        }
        CheckVk(err);

        VulkanWindow->SemaphoreIndex = (VulkanWindow->SemaphoreIndex + 1) % VulkanWindow->ImageCount; // Now we can use the next set of semaphores
    }
}

bool UIContext::Tick(const Component &drawable) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT ||
            (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(Window))) {
            return false;
        }
    }

    // Resize swap chain?
    if (g_SwapChainRebuild) {
        int width, height;
        SDL_GetWindowSize(Window, &width, &height);
        if (width > 0 && height > 0) {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
    }

    // Check if new UI settings need to be applied.
    Settings.UpdateIfChanged(ImGui::GetCurrentContext());
    Style.ImGui.UpdateIfChanged(ImGui::GetCurrentContext());
    Style.ImPlot.UpdateIfChanged(ImPlot::GetCurrentContext());

    auto &io = ImGui::GetIO();

    static int PrevFontIndex = 0;
    static float PrevFontScale = 1.0;
    if (PrevFontIndex != Style.ImGui.FontIndex) {
        io.FontDefault = io.Fonts->Fonts[Style.ImGui.FontIndex];
        PrevFontIndex = Style.ImGui.FontIndex;
    }
    if (PrevFontScale != Style.ImGui.FontScale) {
        io.FontGlobalScale = Style.ImGui.FontScale / FontAtlasScale;
        PrevFontScale = Style.ImGui.FontScale;
    }

    PrepareFrame();
#ifdef ONLY_RENDER_METRICS_WINDOW
    ImGui::ShowMetricsWindow();
#else
    drawable.Draw(); // All project content drawing, initial dockspace setup, keyboard shortcuts.
#endif
    RenderFrame();

#ifdef TRACING_ENABLED
    FrameMark;
#endif

    return true;
}
