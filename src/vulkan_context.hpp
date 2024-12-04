#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <optional>

namespace ntf {

template<typename F>
concept vk_surface_factory = std::is_invocable_r_v<bool, F, VkInstance, VkSurfaceKHR*>;

class vk_context {
private:
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

public:
  vk_context() = default;
  
public:
  // Context initialization
  void create_instance(bool enable_layers, const std::vector<const char*>& req_ext);

  template<vk_surface_factory Fun>
  void create_surface(Fun&& surface_factory) {
    if (surface_factory(_vk_instance, &_vk_surface)) {
      throw std::runtime_error{"Failed to create window surface"};
    }
  }

  void pick_physical_device();
  void create_logical_device();
  void create_swapchain(std::size_t fb_width, std::size_t fb_height);
  void create_imageviews();

  // Context render configuration
  void create_renderpass();
  void create_graphics_pipeline(std::string_view vert_src, std::string_view frag_src);
  void create_framebuffers();
  void create_commandpool();
  void create_commandbuffer();
  void create_sync_objects();

  // Context rendering
  void draw_frame();
  void wait_idle();

  // Context dynamic settings
  void resize_framebuffer(std::size_t width, std::size_t height);

  // Destruction
  void destroy();

private:
  queue_family_indices _find_queue_families(VkPhysicalDevice device);
  swapchain_support_details _query_swapchain_support(VkPhysicalDevice device);

  static VKAPI_ATTR VkBool32 VKAPI_CALL _vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
    VkDebugUtilsMessageTypeFlagsEXT msg_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data
  );

  void _record_command_buffer(VkCommandBuffer buffer, uint32_t image_index);

private:
  bool _vk_enable_layers;
  VkInstance _vk_instance;
  VkDebugUtilsMessengerEXT _vk_messenger;

  VkSurfaceKHR _vk_surface;
  VkPhysicalDevice _vk_physicaldevice;
  VkDevice _vk_device;
  VkQueue _vk_graphicsqueue, _vk_presentqueue;

  VkFormat _vk_swapchain_format;
  VkExtent2D _vk_swapchain_extent;
  VkSwapchainKHR _vk_swapchain;
  std::vector<VkImage> _vk_swapchain_images;
  std::vector<VkImageView> _vk_swapchain_imageviews;
  std::vector<VkFramebuffer> _vk_swapchain_framebuffers;
  bool _vk_framebuffer_resized{false};
  std::size_t _vk_framebuffer_width{0}, _vk_framebuffer_height{0}; // For window resizing

  VkRenderPass _vk_renderpass;
  VkPipelineLayout _vk_graphicspipeline_layout;
  VkPipeline _vk_graphicspipeline;

  VkCommandPool _vk_commandpool;
  VkCommandBuffer _vk_commandbuffer;
  VkSemaphore _vk_image_avail_semaphore, _vk_render_finish_semaphore;
  VkFence _vk_in_flight_fence;
};

} // namespace ntf
