#include "vulkan_context.hpp"

#include <fmt/format.h>

#include <array>
#include <set>
#include <string>
#include <algorithm>

namespace {

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

const auto validation_layers = std::to_array<const char*>({
  "VK_LAYER_KHRONOS_validation",
});

bool check_layer_support() {
  uint32_t layer_count{0};
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

  std::vector<VkLayerProperties> avail(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, avail.data());

  for (const char* layer : validation_layers) {
    bool found{false};
    for (const auto& props : avail) {
      if (std::strcmp(layer, props.layerName) == 0) {
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

const auto device_extensions = std::to_array<const char*>({
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
});

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

} // namespace

namespace ntf {

VkBool32 vk_context::_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
    VkDebugUtilsMessageTypeFlagsEXT msg_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {

  vk_context* ctx = reinterpret_cast<vk_context*>(user_data);

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

  return VK_FALSE; // Should the callback abort the call that triggered the message?
}

void vk_context::create_instance(bool enable_layers, const std::vector<const char*>& req_ext) {
  if (enable_layers && !check_layer_support()) {
    throw std::runtime_error{"Validation layers not available"};
  }
  _vk_enable_layers = enable_layers;

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

  uint32_t ext_count{0};
  vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
  std::vector<VkExtensionProperties> exts(ext_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data());
  fmt::print("{} vulkan extensions available\n", ext_count);
  for (const auto& ext : exts) {
    fmt::print(" - {}\n", ext.extensionName);
  }

  bool ext_avail{true};
  for (const char* extname : req_ext) {
    bool found{false};
    for (const auto& props : exts) {
      if (std::strcmp(extname, props.extensionName) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      ext_avail = false;
      break;
    }
  }

  if (!ext_avail) {
    throw std::runtime_error{"Failed to find the required vulkan extensions"};
  }

  // Pass the windowing system extensions
  create_info.enabledExtensionCount = static_cast<uint32_t>(req_ext.size());
  create_info.ppEnabledExtensionNames = req_ext.data();

  // Setup the debug messenger
  VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
  messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  messenger_info.messageSeverity = 
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
    // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messenger_info.messageType =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  messenger_info.pfnUserCallback = vk_context::_vk_debug_callback;
  messenger_info.pUserData = reinterpret_cast<void*>(this);

  // Which global validation layers to enable
  if (_vk_enable_layers) {
    create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
    create_info.ppEnabledLayerNames = validation_layers.data();

    // Enable a debug messenger for vkCreateInstance and vkDestroyInstance
    create_info.pNext = reinterpret_cast<const void*>(&messenger_info);
  } else {
    create_info.enabledLayerCount = 0;
    create_info.pNext = nullptr;
  }

  // Second param is a custom allocator callback, null for now
  if (vkCreateInstance(&create_info, nullptr, &_vk_instance) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to create vulkan instance"};
  }
  fmt::print("Vulkan instance initialized\n");

  if (!_vk_enable_layers) {
    return;
  }

  // Create the debug messenger
  if (CreateDebugUtilsMessengerEXT(_vk_instance, &messenger_info, nullptr, &_vk_messenger) 
      != VK_SUCCESS) {
    throw std::runtime_error{"Failed to setup debug menssenger"};
  }
}

auto vk_context::_find_queue_families(VkPhysicalDevice device) -> queue_family_indices {
  queue_family_indices indices{};

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

auto vk_context::_query_swapchain_support(VkPhysicalDevice device) -> swapchain_support_details {
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

void vk_context::pick_physical_device() {
  // Select the first suitable physical device found
  auto is_suitable = [this](VkPhysicalDevice device) -> bool {
    auto indices = _find_queue_families(device);

    bool ext_supported = check_extension_support(device);

    bool swapchain_adequate{false};
    if (ext_supported) {
      auto swapchain_support = _query_swapchain_support(device);
      swapchain_adequate = 
        !swapchain_support.formats.empty() &&
        !swapchain_support.present_modes.empty();
    }

    return indices.is_complete() && ext_supported && swapchain_adequate;
  };

  uint32_t device_count{0};
  vkEnumeratePhysicalDevices(_vk_instance, &device_count, nullptr);
  if (device_count == 0) {
    throw std::runtime_error{"Failed to find a GPU with Vulkan support"};
  }

  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(_vk_instance, &device_count, devices.data());
  for (const auto& dev : devices) {
    if (is_suitable(dev)) {
      _vk_physicaldevice = dev;
      break;
    }
  }
  if (_vk_physicaldevice == VK_NULL_HANDLE) {
    throw std::runtime_error{"Failed to find a suitable GPU"};
  }

  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(_vk_physicaldevice, &props);

  fmt::print("Vulkan device information:\n");
  fmt::print(" - Name: {}\n", props.deviceName);
  fmt::print(" - Device ID: {}\n", props.deviceID);
  fmt::print(" - Vendor ID: {}\n", props.vendorID);
  fmt::print(" - API version: {}\n", props.apiVersion);
  fmt::print(" - Driver version: {}\n", props.driverVersion);
}

void vk_context::create_logical_device() {
  // Create a device to interface with the physical device
  auto indices = _find_queue_families(_vk_physicaldevice);

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

  if (_vk_enable_layers) {
    create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
    create_info.ppEnabledLayerNames = validation_layers.data();
  } else {
    create_info.enabledLayerCount = 0;
  }

  if (vkCreateDevice(_vk_physicaldevice, &create_info, nullptr, &_vk_device) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to create logical device"};
  }

  // Retrieve queue handles for each queue family
  // index 0 for both
  vkGetDeviceQueue(_vk_device, indices.graphics_family.value(), 0, &_vk_graphicsqueue); 
  vkGetDeviceQueue(_vk_device, indices.present_family.value(), 0, &_vk_presentqueue);
}

void vk_context::create_swapchain(std::size_t fb_width, std::size_t fb_height) {
  auto swapchain_support = _query_swapchain_support(_vk_physicaldevice);

  // What is the format of the swapchain images?
  auto format = [&]() -> VkSurfaceFormatKHR {
    // Try to get a swap surface with B8G8R8A8 pixel format or just use the first one
    // if there is not one available
    for (const auto& format : swapchain_support.formats) {
      if (format.format == VK_FORMAT_B8G8R8A8_SRGB
          && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return format;
      }
    }
    return swapchain_support.formats[0];
  }();

  // How are the images in the swapchain presented?
  auto present_mode = [&]() -> VkPresentModeKHR {
    // VK_PRESENT_MODE_IMMEDIATE_KHR -> Images submited are transferred to the screen immediately
    // VK_PRESENT_MODE_FIFO_KHR -> The swap chain is a FIFO queue of images (double buffering)
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR -> Doesn't wait for next vblank if the queue was empty
    // VK_PRESENT_MODE_MAILBOX_KHR -> Triple buffering

    for (const auto& mode : swapchain_support.present_modes) {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return mode;
      }
    }

    return VK_PRESENT_MODE_FIFO_KHR; // Only one guaranteed to be available
  }();

  // Resolution of the swapchain images (in pixels), usually the resolution of the window
  auto extent = [&]() -> VkExtent2D {
    auto& capabilities = swapchain_support.capabilities;
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }

    VkExtent2D actual_extent{
      static_cast<uint32_t>(fb_width), static_cast<uint32_t>(fb_height)
    };

    actual_extent.height = std::clamp(actual_extent.width,
                                      capabilities.minImageExtent.width,
                                      capabilities.maxImageExtent.width);
    actual_extent.width = std::clamp(actual_extent.height,
                                     capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);
    return actual_extent;
  }();
  
  // Store the extent and the format for later
  _vk_swapchain_format = format.format;
  _vk_swapchain_extent = extent;

  // For resizing the framebuffer later
  _vk_framebuffer_width = static_cast<std::size_t>(extent.width);
  _vk_framebuffer_height = static_cast<std::size_t>(extent.height);

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

  auto indices = _find_queue_families(_vk_physicaldevice);
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

void vk_context::create_imageviews() {
  // An image view describes how to access an image and which part of it to access

  _vk_swapchain_imageviews.resize(_vk_swapchain_images.size());
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

    if (vkCreateImageView(_vk_device, &create_info, nullptr, &_vk_swapchain_imageviews[i]) 
        != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create image views"};
    }
  }
}

void vk_context::create_renderpass() {
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

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.srcAccessMask = 0;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass{};
  render_pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass.attachmentCount = 1;
  render_pass.pAttachments = &color_attachment;
  render_pass.subpassCount = 1;
  render_pass.pSubpasses = &subpass;
  render_pass.dependencyCount = 1;
  render_pass.pDependencies = &dep;

  if (vkCreateRenderPass(_vk_device, &render_pass, nullptr, &_vk_renderpass) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to create render pass"};
  }
}

void vk_context::create_graphics_pipeline(std::string_view vert_src, std::string_view frag_src) {
  // Load shader modules from bytecode
  auto create_shader_module = [this](std::string_view src) -> VkShaderModule {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = src.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(src.data());

    VkShaderModule shader;
    if (vkCreateShaderModule(_vk_device, &create_info, nullptr, &shader) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create shader module"};
    }

    return shader;
  };

  auto vert_module = create_shader_module(vert_src);
  auto frag_module = create_shader_module(frag_src);


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

  if (vkCreatePipelineLayout(_vk_device, &pipeline_layout, nullptr, 
                             &_vk_graphicspipeline_layout) != VK_SUCCESS) {
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
  pipeline.layout = _vk_graphicspipeline_layout;
  pipeline.renderPass = _vk_renderpass;
  pipeline.subpass = 0; // Index of the subpass where this pipeline will be used

  // For deriving from another pipeline
  // pipeline.basePipelineHandle = VK_NULL_HANDLE;
  // pipeline.basePipelineIndex = -1;

  // Can take multiple VkGraphicsPipelineCreateInfo objects
  // and create multiple VkPipeline objects in a single call
  if (vkCreateGraphicsPipelines(_vk_device, VK_NULL_HANDLE, 1, &pipeline, nullptr, 
                                &_vk_graphicspipeline) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to create graphics pipeline"};
  }

  vkDestroyShaderModule(_vk_device, vert_module, nullptr);
  vkDestroyShaderModule(_vk_device, frag_module, nullptr);
}

void vk_context::create_framebuffers() {
  // A framebuffer object references all VkImageView objects that
  // represent attachments created during the render pass creation
  // In this case we only reference the color attachment, but we still have
  // to create a framebuffer for all the images in the swap chain

  _vk_swapchain_framebuffers.resize(_vk_swapchain_imageviews.size());
  for (std::size_t i = 0; i < _vk_swapchain_imageviews.size(); ++i) {
    VkImageView attachments[] = {_vk_swapchain_imageviews[i]};
    
    VkFramebufferCreateInfo framebuffer{};
    framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass = _vk_renderpass;
    framebuffer.attachmentCount = 1;
    framebuffer.pAttachments = attachments;
    framebuffer.width = _vk_swapchain_extent.width;
    framebuffer.height = _vk_swapchain_extent.height;
    framebuffer.layers = 1; // Layers in the image arrays

    if (vkCreateFramebuffer(_vk_device, &framebuffer, nullptr, &_vk_swapchain_framebuffers[i])
        != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create framebuffer"};
    }
  }
}

void vk_context::create_commandpool() {
  // Commands in vulkan are recorded in a command buffer
  // instead of being executed directly using function calls
  auto indices = _find_queue_families(_vk_physicaldevice);

  VkCommandPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

  // The pool will be used for drawing commands only
  pool.queueFamilyIndex = indices.graphics_family.value();

  // All command buffers can be rerecorded individually
  pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  if (vkCreateCommandPool(_vk_device, &pool, nullptr, &_vk_commandpool) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to create command pool"};
  }
}

void vk_context::create_commandbuffers() {
  // Command buffer allocation

  _vk_commandbuffers.resize(MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = _vk_commandpool;
  alloc.commandBufferCount = static_cast<uint32_t>(_vk_commandbuffers.size());

  // If the command buffer can be submited directly but not called from other buffers
  // or cannot be submitted directly but can be called from pirmary buffers
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  if (vkAllocateCommandBuffers(_vk_device, &alloc, _vk_commandbuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to allocate command buffers"};
  }
}

void vk_context::create_sync_objects() {
  // Steps for drawing a frame:
  // 1. Wait for the previous frame to finish
  // 2. Acquire an image from the swapchain
  // 3. Record a command buffer which draws an scene onto that image
  // 4. Submit the recorded command buffer (and execute it)
  // 5. Present the swapchain image

  // Steps 2, 4 and 5 hapen asynchronously on the GPU, so some
  // synchronization primitives are needed: semaphores and fences
  // - Semaphores are for synchronizing the GPU (Blocks between queue commands)
  // - Fences are for synchronizing the CPU (Blocks until some queue command finishes)

  // Semaphores should be used for swapchain operations (because they happen on the GPU)
  // Fences are used when we're waiting for the previous frame to end before drawing again, so
  // we don't overwrite the current contents of the command buffer while the GPU is using it

  _vk_image_avail_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
  _vk_render_finish_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
  _vk_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphore{};
  semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence{};
  fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

  // Create the fence signaled, since there is no previous frame on the first frame
  fence.flags = VK_FENCE_CREATE_SIGNALED_BIT; 

  for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (vkCreateSemaphore(_vk_device, &semaphore, nullptr, &_vk_image_avail_semaphores[i]) 
                        != VK_SUCCESS ||
        vkCreateSemaphore(_vk_device, &semaphore, nullptr, &_vk_render_finish_semaphores[i]) 
                        != VK_SUCCESS ||
        vkCreateFence(_vk_device, &fence, nullptr, &_vk_in_flight_fences[i]) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create sync objects"};
    }
  }
}

void vk_context::destroy() {
  for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    vkDestroySemaphore(_vk_device, _vk_image_avail_semaphores[i], nullptr);
    vkDestroySemaphore(_vk_device, _vk_render_finish_semaphores[i], nullptr);
    vkDestroyFence(_vk_device, _vk_in_flight_fences[i], nullptr);
  }

  vkDestroyCommandPool(_vk_device, _vk_commandpool, nullptr); // Cleans up the buffer too
  
  for (auto fb : _vk_swapchain_framebuffers) {
    vkDestroyFramebuffer(_vk_device, fb, nullptr);
  }

  vkDestroyPipeline(_vk_device, _vk_graphicspipeline, nullptr);
  vkDestroyPipelineLayout(_vk_device, _vk_graphicspipeline_layout, nullptr);

  vkDestroyRenderPass(_vk_device, _vk_renderpass, nullptr);

  for (auto view : _vk_swapchain_imageviews) {
    vkDestroyImageView(_vk_device, view, nullptr);
  }

  vkDestroySwapchainKHR(_vk_device, _vk_swapchain, nullptr);

  vkDestroyDevice(_vk_device, nullptr); // Cleans up device queues too

  if (_vk_enable_layers) {
    DestroyDebugUtilsMessengerEXT(_vk_instance, _vk_messenger, nullptr);
  }

  vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);

  vkDestroyInstance(_vk_instance, nullptr);
}

void vk_context::_record_command_buffer(VkCommandBuffer buffer, uint32_t image_index) {
  // Write commands to a command buffer

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  // begin_info.flags = 0;
  // begin_info.pInheritanceInfo = nullptr;

  // If the buffer was recorded once, a new call will reset it instead
  if (vkBeginCommandBuffer(buffer, &begin_info) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to begin recording command buffer"};
  }

  VkRenderPassBeginInfo render_pass{};
  render_pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass.renderPass = _vk_renderpass;
  render_pass.framebuffer = _vk_swapchain_framebuffers[image_index];
  render_pass.renderArea.offset = {0, 0};
  render_pass.renderArea.extent = _vk_swapchain_extent;

  VkClearValue clear_color{{{.2f, .2f, .2f, 1.f}}};
  render_pass.clearValueCount = 1;
  render_pass.pClearValues = &clear_color;

  // VK_SUBPASS_CONTENTS_INLINE specifies that no secondary buffers will be executed
  vkCmdBeginRenderPass(buffer, &render_pass, VK_SUBPASS_CONTENTS_INLINE);

  // VK_PIPELINE_BIND_POINT_GRAPHICS specifies that is a graphics pipeline (not a compute one)
  vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _vk_graphicspipeline);

  // Set the dynamic states
  VkViewport viewport{};
  viewport.x = 0.f;
  viewport.y = 0.f;
  viewport.width = static_cast<float>(_vk_swapchain_extent.width);
  viewport.height = static_cast<float>(_vk_swapchain_extent.height);
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;
  vkCmdSetViewport(buffer, 0, 1, &viewport); // firstViewport, viewportCount

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = _vk_swapchain_extent;
  vkCmdSetScissor(buffer, 0, 1, &scissor); // firstScissor, scissorCount

  vkCmdDraw(buffer, 3, 1, 0, 0); // vertexCount, instanceCount, firstVertex, firstInstance
  vkCmdEndRenderPass(buffer);

  if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
    throw std::runtime_error{"Failed to record command buffer"};
  }
}

void vk_context::draw_frame() {
  // Wait for 1 fence without timeout
  vkWaitForFences(_vk_device, 1, &_vk_in_flight_fences[_vk_curr_frame], VK_TRUE, UINT64_MAX);

  // Reset 1 fence
  vkResetFences(_vk_device, 1, &_vk_in_flight_fences[_vk_curr_frame]);

  // Acquire the next image without a timeout, pass a semaphore only
  uint32_t image_index{0};
  vkAcquireNextImageKHR(_vk_device, _vk_swapchain, UINT64_MAX, 
                        _vk_image_avail_semaphores[_vk_curr_frame], VK_NULL_HANDLE, &image_index);

  // Make sure the buffer is able to be recorded, the seccond arg is some flag
  vkResetCommandBuffer(_vk_commandbuffers[_vk_curr_frame], 0);

  // Record things
  _record_command_buffer(_vk_commandbuffers[_vk_curr_frame], image_index);

  // Now to submit the queue
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  // Wait with ONLY writing colors to the image until it's available
  // This means that the implementation COULD start executing the vertex shader for example
  VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore wait_semaphores[] = {_vk_image_avail_semaphores[_vk_curr_frame]};
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = wait_semaphores;
  submit.pWaitDstStageMask = wait_stages;

  // Which semaphores to signal when the command buffer finishes execution
  VkSemaphore signal_semaphores[] = {_vk_render_finish_semaphores[_vk_curr_frame]};
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &_vk_commandbuffers[_vk_curr_frame];
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = signal_semaphores;

  if (vkQueueSubmit(_vk_graphicsqueue, 1, &submit, _vk_in_flight_fences[_vk_curr_frame])
      != VK_SUCCESS) {
    throw std::runtime_error{"Failed to submit draw command buffer"};
  }

  VkPresentInfoKHR present{};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = signal_semaphores;

  VkSwapchainKHR swapchains[] = {_vk_swapchain};
  present.swapchainCount = 1;
  present.pSwapchains = swapchains;
  present.pImageIndices = &image_index;
  // present.pResults = nullptr;

  vkQueuePresentKHR(_vk_presentqueue, &present);

  _vk_curr_frame = (_vk_curr_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void vk_context::wait_idle() {
  vkDeviceWaitIdle(_vk_device);
}

} // namespace ntf
