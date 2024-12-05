#pragma once

#include <functional>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <optional>
#include <array>

#include <glm/glm.hpp>

namespace ntf {

struct vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static VkVertexInputBindingDescription bind_description();
  static std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions();
};


template<typename F>
concept vk_surface_factory = std::is_invocable_r_v<bool, F, VkInstance, VkSurfaceKHR*>;

class vk_context {
private:
  // Allow to render up to N frames without waiting for the next frame
  static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

  struct queue_family_indices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;
    std::optional<uint32_t> transfer_family;

    bool is_complete() const { 
      return graphics_family.has_value() && present_family.has_value()
      && transfer_family.has_value();
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
    if (surface_factory(_instance, &_surface)) {
      throw std::runtime_error{"Failed to create window surface"};
    }
  }

  void pick_physical_device();
  void create_logical_device();
  void create_swapchain(std::function<void(std::size_t&, std::size_t&)> size_callback = {});
  void create_imageviews();

  // Context render configuration
  void create_renderpass();
  void create_graphics_pipeline(std::string_view vert_src, std::string_view frag_src);
  void create_framebuffers();
  void create_commandpool();
  void create_buffers();
  void create_commandbuffers();
  void create_sync_objects();

  // Context rendering
  void draw_frame();
  void wait_idle();

  // Context dynamic settings
  void flag_dirty_framebuffer() { _framebuffer_resized = true; }

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

  void _cleanup_swapchain();
  void _recreate_swapchain();
  void _create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                      VkBuffer& buffer, VkDeviceMemory& buffer_mem);
  void _copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize sz);

private:
  bool _enable_layers;
  VkInstance _instance;
  VkDebugUtilsMessengerEXT _messenger;

  VkSurfaceKHR _surface;
  VkPhysicalDevice _physical_device;
  VkDevice _device;
  VkQueue _graphics_queue, _present_queue, _transfer_queue;

  VkFormat _swapchain_format;
  VkExtent2D _swapchain_extent;
  VkSwapchainKHR _swapchain;
  std::vector<VkImage> _swapchain_images;
  std::vector<VkImageView> _swapchain_image_views;
  std::vector<VkFramebuffer> _swapchain_framebuffers;
  std::function<void(std::size_t&, std::size_t&)> _framebuffer_size_callback;
  bool _framebuffer_resized{false};

  VkRenderPass _render_pass;
  VkPipelineLayout _graphics_pipeline_layout;
  VkPipeline _graphics_pipeline;

  VkCommandPool _graphics_command_pool, _transfer_command_pool;
  VkCommandBuffer _transfer_command_buffer;
  std::vector<VkCommandBuffer> _graphics_command_buffers;
  std::vector<VkSemaphore> _image_avail_semaphores, _render_finish_semaphores;
  std::vector<VkFence> _in_flight_fences;
  uint32_t _curr_frame{0};

  VkBuffer _vertex_buffer, _index_buffer;
  VkDeviceMemory _vertex_buffer_mem, _index_buffer_mem;
};

} // namespace ntf
