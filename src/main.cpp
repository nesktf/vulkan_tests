#define GLFW_INCLUDE_VULKAN
// #include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <fmt/format.h>

#include <cassert>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <limits>
#include <string>
#include <sstream>
#include <fstream>

constexpr std::size_t WIDTH = 800;
constexpr std::size_t HEIGHT = 600;

const std::vector<const char*> validation_layers = {
  "VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> device_extensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

// Load vkCreateDebugUtilsMessengerEXT, since is an extension and is not loaded by default
VkResult CreateDebugUtilsMessengerEXT(
  VkInstance instance,
  const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
  const VkAllocationCallbacks* pAllocator,
  VkDebugUtilsMessengerEXT* pDebugMessenger) {
  auto func = 
    (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,
                                                              "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

// Do the same with vkDestroyDebugUtilsMessengerEXT
void DestroyDebugUtilsMessengerEXT(
  VkInstance instance,
  VkDebugUtilsMessengerEXT debugMessenger,
  const VkAllocationCallbacks* pAllocator) {
  auto func = 
    (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,
                                                               "vkDestroyDebugUtilsMessengerEXT");
  if (func) {
    func(instance, debugMessenger, pAllocator);
  }
}

struct queue_family_indices {
  std::optional<uint32_t> graphics_family;
  std::optional<uint32_t> present_family;

  bool is_complete() const { 
    return graphics_family.has_value() && present_family.has_value();
  }
};

struct swapchain_support_details {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

static std::optional<std::string> file_contents(std::string_view path) {
  std::string out {};
  std::fstream fs{path.data()};

  if (!fs.is_open()) {
    return std::nullopt;
  }

  std::ostringstream ss;
  ss << fs.rdbuf();
  out = ss.str();

  fs.close();
  return out;
}

#ifdef NDEBUG
const bool enable_validation_layers = false;
#else
const bool enable_validation_layers = true;
#endif

class application {
public:
  void run() {
    init_window();
    init_vulkan();
    main_loop();
    cleanup();
  }

private:
  void init_window() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    _win = glfwCreateWindow(WIDTH, HEIGHT, "test", nullptr, nullptr);

    if (!_win) {
      throw std::runtime_error{"Failed to create GLFW window"};
    }
  }

  void init_vulkan() {
    create_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swap_chain();
    create_image_views();
    create_render_pass();
    create_graphics_pipeline();
  }

  void main_loop() {
    while (!glfwWindowShouldClose(_win)) {
      glfwPollEvents();

      if (glfwGetKey(_win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(_win, 1);
      }

    }
  }

  void cleanup() {
    vkDestroyPipeline(_vk_device, _vk_pipeline, nullptr);
    vkDestroyPipelineLayout(_vk_device, _vk_pipeline_layout, nullptr);
    vkDestroyRenderPass(_vk_device, _vk_render_pass, nullptr);
    for (auto view : _vk_swapchain_image_views) {
      vkDestroyImageView(_vk_device, view, nullptr);
    }
    vkDestroySwapchainKHR(_vk_device, _vk_swapchain, nullptr);
    vkDestroyDevice(_vk_device, nullptr); // Cleans up device queues too
    if (enable_validation_layers) {
      DestroyDebugUtilsMessengerEXT(_vk_instance, _debug_messenger, nullptr);
    }
    vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
    vkDestroyInstance(_vk_instance, nullptr);

    glfwDestroyWindow(_win);
    glfwTerminate();
  }

private:
  bool check_validation_layer_support() {
    // Check if the validation layers specified above are available
    uint32_t layer_count{};
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> avail(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, avail.data());

    for (const char* layer : validation_layers) {
      bool found{false};
      for (const auto& props : avail) {
        if (strcmp(layer, props.layerName) == 0) {
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
    }

    return true;
  }

  std::vector<const char*> get_extensions() {
    uint32_t glfw_extension_count{0};
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    fmt::print("{} required glfw extensions:\n", glfw_extension_count);
    for (uint32_t i = 0; i < glfw_extension_count; ++i) {
      fmt::print(" - {}\n", glfw_extensions[i]);
    }

    std::vector<const char*> ext(glfw_extensions, glfw_extensions+glfw_extension_count);
    if (enable_validation_layers) {
      // ext.push_back("VK_EXT_debug_utils");
      ext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // for setting up a debug messenger
    }

    return ext;
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
    VkDebugUtilsMessageTypeFlagsEXT msg_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {

    const char* severity;
    switch (msg_severity) {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        severity = "VERBOSE";
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        severity = "INFO";
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        severity = "WARNING";
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        severity = "ERROR";
        break;
      default:
        severity = "UNKNOWN";
        break;
    }

    const char* kind;
    switch (msg_type) {
      case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        kind = "GENERAL";
        break;
      case  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        kind = "VALIDATION";
        break;
      case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        kind = "PERFORMANCE";
        break;
      default:
        kind = "UNKNOWN";
        break;
    }

    fmt::print(stderr, "[{}][{}] Validation layer: {}\n", severity, kind, callback_data->pMessage);

    return VK_FALSE; // Should abort the call that triggered the message?
  }

  VkDebugUtilsMessengerCreateInfoEXT create_messenger_info() {
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = 
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;
    create_info.pUserData = nullptr;
    // create_info.pUserData = reinterpret_cast<void*>(this);
    return create_info;
  }

  void create_instance() {
    if (enable_validation_layers && !check_validation_layer_support()) {
      throw std::runtime_error{"Validation layers not available :c"};
    }

    // Application info passed to vulkan
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // struct type
    // app_info.pNext = nullptr; // No extensions, not needed if default constructed
    app_info.pApplicationName = "Vulkan tutorial";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0 ,0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    // Check for driver extensions
    uint32_t extension_count{0};
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> avail_extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, avail_extensions.data());
    fmt::print("{} vulkan extensions supported:\n", extension_count);
    for (const auto& ext : avail_extensions) {
      fmt::print(" - {}\n", ext.extensionName);
    }

    // Pass the windowing system extensions
    auto extensions = get_extensions();
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();


    // Which global validation layers to enable
    auto debug_messenger_info = create_messenger_info();
    if (enable_validation_layers) {
      create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
      create_info.ppEnabledLayerNames = validation_layers.data();

      // Enable a debug messenger for vkCreateInstance and vkDestroyInstance
      create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debug_messenger_info;
    } else {
      create_info.enabledLayerCount = 0; 
      create_info.pNext = nullptr;
    }

    // Second param is a custom allocator callback, null for now
    if (vkCreateInstance(&create_info, nullptr, &_vk_instance) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create vulkan instance"};
    }
    fmt::print("Vulkan instance initialized\n");
  }

  void setup_debug_messenger() {
    // Set the messenger callback
    if (!enable_validation_layers) {
      return;
    }

    auto create_info = create_messenger_info();
    if (CreateDebugUtilsMessengerEXT(_vk_instance, &create_info, nullptr, &_debug_messenger) 
        != VK_SUCCESS) {
      throw std::runtime_error{"Failed to setup debug messenger"};
    }
  }

  void create_surface() {
    // Create window surface (has to be done before device selection)
    // It needs the VK_KHR_surface extension, but it is already included
    // in the glfw extension list
    if (glfwCreateWindowSurface(_vk_instance, _win, nullptr, &_vk_surface) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create window surface"};
    }
    // If not using glfw, has to be done passing an XCB connection and
    // window details from X11 in vkCreateXcbSurfaceKHR
    // (who cares about windows right?)
  }

  queue_family_indices find_queue_families(VkPhysicalDevice device) {
    queue_family_indices indices;

    uint32_t queue_family_count{0};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    uint i = 0;
    for (const auto& family : queue_families) {
      // Require a device with a queue family that supports graphics commands
      if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphics_family = i;
      }

      // Require a device with a queue family that supports presentation
      // (might not be the same queue as the graphics one, so we store another index)
      VkBool32 present_support{false};
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _vk_surface, &present_support);
      if (present_support) {
        indices.present_family = i;
      }

      if (indices.is_complete()) {
        break;
      }

      ++i;
    }

    return indices;
  } 

  bool check_extension_support(VkPhysicalDevice device) {
    // Check if the physical device has all the extensions in device_extensions
    uint32_t ext_count{0};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, nullptr);

    std::vector<VkExtensionProperties> avail_ext(ext_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, avail_ext.data());

    std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());
    for (const auto& ext : avail_ext) {
      required_extensions.erase(ext.extensionName);
    }

    return required_extensions.empty();
  }

  swapchain_support_details query_swapchain_support(VkPhysicalDevice device) {
    // Get supported formats and present modes from a device
    swapchain_support_details details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _vk_surface, &details.capabilities);

    uint32_t format_count{0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, _vk_surface, &format_count, nullptr);
    if (format_count) {
      details.formats.resize(format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, _vk_surface, &format_count,
                                           details.formats.data());
    }

    uint32_t present_mode_count{0};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, _vk_surface, &present_mode_count, nullptr);
    if (present_mode_count) {
      details.present_modes.resize(present_mode_count);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, _vk_surface, &present_mode_count,
                                                details.present_modes.data());
    }

    return details;
  }

  bool is_device_suitable(VkPhysicalDevice device) {
    // VkPhysicalDeviceProperties props; // Basic device properties
    // VkPhysicalDeviceFeatures features; // Optional features
    //
    // vkGetPhysicalDeviceProperties(device, &props);
    // vkGetPhysicalDeviceFeatures(device, &features);

    // example:
    // return props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && features.geometryShader;

    // From the tutorial:
    // Instead of checking if a device is suitable and going with the first one, you could
    // also give each device a score and pick the highest, or fallback to an integrated
    // GPU if that's the only available one


    // For now, just limit ourselves to require some queue families
    auto indices = find_queue_families(device);

    bool ext_supported = check_extension_support(device);

    bool swapchain_adequate{false};
    if (ext_supported) {
      auto swapchain_support = query_swapchain_support(device);
      swapchain_adequate = 
        !swapchain_support.formats.empty() &&
        !swapchain_support.present_modes.empty();
    }

    return indices.is_complete() && ext_supported && swapchain_adequate;
  }

  void pick_physical_device() {
    // Select the first suitable physical device found
    uint32_t device_count{0};
    vkEnumeratePhysicalDevices(_vk_instance, &device_count, nullptr);
    if (device_count == 0) {
      throw std::runtime_error{"Failed to find a GPU with Vulkan support"};
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(_vk_instance, &device_count, devices.data());
    for (const auto& dev : devices) {
      if (is_device_suitable(dev)) {
        _vk_physical_device = dev;
        break;
      }
    }
    if (_vk_physical_device == VK_NULL_HANDLE) {
      throw std::runtime_error{"Failed to find a suitable GPU"};
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(_vk_physical_device, &props);

    fmt::print("Vulkan device information:\n");
    fmt::print(" - Name: {}\n", props.deviceName);
    fmt::print(" - Device ID: {}\n", props.deviceID);
    fmt::print(" - Vendor ID: {}\n", props.vendorID);
    fmt::print(" - API version: {}\n", props.apiVersion);
    fmt::print(" - Driver version: {}\n", props.driverVersion);
  }

  void create_logical_device() {
    // Create a device to interface with the physical device
    auto indices = find_queue_families(_vk_physical_device);

    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    std::set<uint32_t> unique_queue_families = { 
      indices.graphics_family.value(), indices.present_family.value()
    }; // To avoid adding the same queue family more than once


    float queue_priority {1.f}; // Priority for the scheduling of command buffer execution

    // How many queues we want for each queue family?
    for (uint32_t family : unique_queue_families) {
      VkDeviceQueueCreateInfo queue_info{};
      queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_info.queueFamilyIndex = family;
      queue_info.queueCount = 1;
      queue_info.pQueuePriorities = &queue_priority;
      queue_infos.emplace_back(queue_info);
    }

    // Which physical device features are we going to use?
    VkPhysicalDeviceFeatures features{}; // all VK_FALSE for now

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    create_info.pEnabledFeatures = &features;

    // Specify extensions and validation layers (device specific this time)
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();

    if (enable_validation_layers) {
      create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
      create_info.ppEnabledLayerNames = validation_layers.data();
    } else {
      create_info.enabledLayerCount = 0;
    }

    if (vkCreateDevice(_vk_physical_device, &create_info, nullptr, &_vk_device) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create logical device"};
    }

    // Retrieve queue handles for each queue family
    // index 0 for both
    vkGetDeviceQueue(_vk_device, indices.graphics_family.value(), 0, &_vk_graphics_queue); 
    vkGetDeviceQueue(_vk_device, indices.present_family.value(), 0, &_vk_present_queue);
  }

  VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) {
    // Try to get a swap surface with B8G8R8A8 pixel format or just use the first one
    // if there is not one available
    for (const auto& format : formats) {
      if (format.format == VK_FORMAT_B8G8R8A8_SRGB
          && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return format;
      }
    }
    return formats[0];
  }

  VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& modes) {
    // VK_PRESENT_MODE_IMMEDIATE_KHR -> Images submited are transferred to the screen immediately
    // VK_PRESENT_MODE_FIFO_KHR -> The swap chain is a FIFO queue of images (double buffering)
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR -> Doesn't wait for next vblank if the queue was empty
    // VK_PRESENT_MODE_MAILBOX_KHR -> Triple buffering

    for (const auto& mode : modes) {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return mode;
      }
    }

    return VK_PRESENT_MODE_FIFO_KHR; // Only one guaranteed to be available
  }

  VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) {
    // Resolution of the swap chain images (in pixels)
    // Usually the resolution of the window
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }
    int w{0}, h{0};
    glfwGetFramebufferSize(_win, &w, &h); // not the same as WIDTH, HEIGHT

    VkExtent2D actual_extent{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    actual_extent.height = std::clamp(actual_extent.width,
                                      capabilities.minImageExtent.width,
                                      capabilities.maxImageExtent.width);
    actual_extent.width = std::clamp(actual_extent.height,
                                     capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);
    return actual_extent;
  }

  void create_swap_chain() {
    auto swapchain_support = query_swapchain_support(_vk_physical_device);
    
    auto format = choose_swap_surface_format(swapchain_support.formats);
    _vk_swapchain_format = format.format;
    auto present_mode = choose_swap_present_mode(swapchain_support.present_modes);
    _vk_swapchain_extent = choose_swap_extent(swapchain_support.capabilities);

    // Images that will be stored in the swap chain
    // Add one to avoid waiting for the driverto complete operatons
    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;

    // Clamp
    if (swapchain_support.capabilities.maxImageCount > 0
        && image_count > swapchain_support.capabilities.maxImageCount) {
      image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = _vk_surface;
    create_info.imageFormat = _vk_swapchain_format;
    create_info.minImageCount = image_count;
    create_info.imageColorSpace = format.colorSpace;
    create_info.imageExtent = _vk_swapchain_extent;
    create_info.imageArrayLayers = 1; // Layers for each image, 1 if not using stereoscopic 3D
    
    // Sets the operations for the swapchain, in this case we only render directly
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    // create_info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT; // For postprocessing

    auto indices = find_queue_families(_vk_physical_device);
    uint32_t queue_indices[] = {
      indices.graphics_family.value(), indices.present_family.value()
    };
    // imageSharingMode only refers to ownership transfer needs
    if (indices.graphics_family != indices.present_family) {
      create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // Shared across queue families
      create_info.queueFamilyIndexCount = 2;
      create_info.pQueueFamilyIndices = queue_indices;
    } else {
      create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Owned by a single queue family
      // create_info.queueFamilyIndexCount = 0;
      // create_info.pQueueFamilyIndices = nullptr;
    }

    // I hate inverted framebuffers
    create_info.preTransform = swapchain_support.capabilities.currentTransform;

    // For blending with other windows
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;

    // Used when the swap chain has to be reconstructed
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(_vk_device, &create_info, nullptr, &_vk_swapchain) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create swap chain"};
    }

    vkGetSwapchainImagesKHR(_vk_device, _vk_swapchain, &image_count, nullptr);
    _vk_swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(_vk_device, _vk_swapchain, &image_count, _vk_swapchain_images.data());
  }

  void create_image_views() {
    // An image view describes how to access an image and which part of it to access

    _vk_swapchain_image_views.resize(_vk_swapchain_images.size());
    for (std::size_t i = 0; i < _vk_swapchain_images.size(); ++i) {
      VkImageViewCreateInfo create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      create_info.image = _vk_swapchain_images[i];

      create_info.viewType = VK_IMAGE_VIEW_TYPE_2D; // Treat the image as a 2D texture
      create_info.format = _vk_swapchain_format;

      // Map the color channels to something, VK_COMPONENT_SWIZZLE_IDENTITY by default
      create_info.components = VkComponentMapping {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
      };

      // Only one layer per image
      create_info.subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      };

      if (vkCreateImageView(_vk_device, &create_info, nullptr, &_vk_swapchain_image_views[i]) 
          != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create image views"};
      }
    }
  }

  VkShaderModule create_shader_module(std::string_view src) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = src.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(src.data());

    VkShaderModule shader;
    if (vkCreateShaderModule(_vk_device, &create_info, nullptr, &shader) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create shader module"};
    }

    return shader;
  }

  void create_render_pass() {
    // Specify all the framebuffer attachments that will be used while rendering

    // A single color buffer attachment, represented by one of the images from the swap chain
    VkAttachmentDescription color_attachment{};
    color_attachment.format = _vk_swapchain_format; // Same as the swapchain image format
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

    // What will happen with the data in the attachment before and after rendering
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear values at the start
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store rendered contents in memory

    // No stencil buffer for now
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Layout for the framebuffer image (?) before and after the render pass
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


    // Things for subpasses, each one references one or more of the previous attachments
    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Only one subpass for the fragment shader out_color
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // It's a graphics subpass
    subpass.colorAttachmentCount = 1;

    // This maps to the location index in the shader:
    // layout(location = 0) out vec4 out_color;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass{};
    render_pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass.attachmentCount = 1;
    render_pass.pAttachments = &color_attachment;
    render_pass.subpassCount = 1;
    render_pass.pSubpasses = &subpass;

    if (vkCreateRenderPass(_vk_device, &render_pass, nullptr, &_vk_render_pass) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create render pass"};
    }
  }

  void create_graphics_pipeline() {
    // Load shader modules from bytecode
    auto vert_src = file_contents("res/shader.vs.spv");
    auto frag_src = file_contents("res/shader.fs.spv");

    auto vert_module = create_shader_module(vert_src.value());
    auto frag_module = create_shader_module(frag_src.value());

    VkPipelineShaderStageCreateInfo vert_stage_info{};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_module;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_info{};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag_module;
    frag_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage_info, frag_stage_info};

    // Things that can be changed without reconstructing the pipeline
    // The data for these has to be specified at drawing time
    std::vector<VkDynamicState> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // Specify the format of the vertex data passed to the vertex shader
    // No vertex data for now
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 0;
    vertex_input.pVertexBindingDescriptions = nullptr;
    vertex_input.vertexAttributeDescriptionCount = 0;
    vertex_input.pVertexAttributeDescriptions = nullptr;

    // Describe the primitive that will be used for drawing
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE; // Breakup _STRIP primitives for reuse

    // // Equivalent to glViewport
    // VkViewport viewport{};
    // viewport.x = 0.f;
    // viewport.y = 0.f;
    // viewport.width = static_cast<float>(_vk_swapchain_extent.width);
    // viewport.height = static_cast<float>(_vk_swapchain_extent.height);
    //
    // // Range of depth values for the framebuffer
    // viewport.minDepth = 0.f;
    // viewport.maxDepth = 1.f;
    //
    // // Scissor rectangles define which regions of pixels will be discarded in the framebuffer
    // VkRect2D scissor{};
    // scissor.offset = {0, 0};
    // scissor.extent = _vk_swapchain_extent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    // viewport_state.pViewports = &viewport;
    // viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    // Clamp fragments beyond znear and zfar instead of discarding them
    rasterizer.depthClampEnable = VK_FALSE;

    rasterizer.rasterizerDiscardEnable = VK_FALSE; // VK_TRUE disables any output to the fb
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // How fragments are generated for geometry
    rasterizer.lineWidth = 1.f; // Thickness of lines (number of fragments)
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // Type of culling to be used
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; // Vertex order for faces

    // Which cosntants to use for altering depth values
    rasterizer.depthBiasEnable = VK_FALSE;
    // rasterizer.depthBiasConstantFactor = 0.f;
    // rasterizer.depthBiasClamp = 0.f;
    // rasterizer.depthBiasSlopeFactor = 0.f;

    // Anti-aliasing with multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    // multisampling.minSampleShading = 1.f;
    // multisampling.pSampleMask = nullptr;
    // multisampling.alphaToCoverageEnable = VK_FALSE;
    // multisampling.alphaToOneEnable = VK_FALSE;

    // No depth/stencil buffer for now
    // VkPipelineDepthStencilStateCreateInfo depth_stencil{};

    // Color blending configuration per attached framebuffer
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT |
      VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    // color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    // color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    // color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    // color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    // color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    // color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    
    // For alpha blending:
    // color_blend_attachment.blendEnable = VK_TRUE;
    // color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    // color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    // color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    // color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    // color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    // color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    // Global color blending settings
    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    // color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    // color_blending.blendConstants[0] = 0.f;
    // color_blending.blendConstants[1] = 0.f;
    // color_blending.blendConstants[2] = 0.f;
    // color_blending.blendConstants[3] = 0.f;

    // Layout for shader uniforms
    VkPipelineLayoutCreateInfo pipeline_layout{};
    pipeline_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    // pipeline_layout.setLayoutCount = 0;
    // pipeline_layout.pSetLayouts = nullptr;
    // pipeline_layout.pushConstantRangeCount = 0;
    // pipeline_layout.pPushConstantRanges= nullptr;

    if (vkCreatePipelineLayout(_vk_device, &pipeline_layout, nullptr, &_vk_pipeline_layout)
        != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create pipeline layout"};
    }

    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2;
    pipeline.pStages = shader_stages;
    pipeline.pVertexInputState = &vertex_input;
    pipeline.pInputAssemblyState = &input_assembly;
    pipeline.pViewportState = &viewport_state;
    pipeline.pRasterizationState = &rasterizer;
    pipeline.pMultisampleState = &multisampling;
    // pipeline.pDepthStencilState = nullptr;
    pipeline.pColorBlendState = &color_blending;
    pipeline.pDynamicState = &dynamic_state;
    pipeline.layout = _vk_pipeline_layout;
    pipeline.renderPass = _vk_render_pass;
    pipeline.subpass = 0; // Index of the subpass where this pipeline will be used

    // For deriving from another pipeline
    // pipeline.basePipelineHandle = VK_NULL_HANDLE;
    // pipeline.basePipelineIndex = -1;

    // Can take multiple VkGraphicsPipelineCreateInfo objects
    // and create multiple VkPipeline objects in a single call
    if (vkCreateGraphicsPipelines(_vk_device, VK_NULL_HANDLE, 1, &pipeline, nullptr, 
                                  &_vk_pipeline) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create graphics pipeline"};
    }

    vkDestroyShaderModule(_vk_device, vert_module, nullptr);
    vkDestroyShaderModule(_vk_device, frag_module, nullptr);
  }


private:
  GLFWwindow* _win;

  VkInstance _vk_instance;
  VkDebugUtilsMessengerEXT _debug_messenger;

  VkSurfaceKHR _vk_surface;
  VkPhysicalDevice _vk_physical_device{VK_NULL_HANDLE};
  VkDevice _vk_device;
  VkQueue _vk_graphics_queue, _vk_present_queue;

  VkFormat _vk_swapchain_format;
  VkExtent2D _vk_swapchain_extent;
  VkSwapchainKHR _vk_swapchain;
  std::vector<VkImage> _vk_swapchain_images;
  std::vector<VkImageView> _vk_swapchain_image_views;

  VkRenderPass _vk_render_pass;
  VkPipelineLayout _vk_pipeline_layout;
  VkPipeline _vk_pipeline;
};

int main() {
  application app;
  try {
    app.run();
  } catch (const std::exception& e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
