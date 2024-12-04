#include "vulkan_context.hpp"

#include <fmt/format.h>

#include <cassert>
#include <optional>
#include <string>
#include <sstream>
#include <fstream>

constexpr std::size_t WIDTH = 800;
constexpr std::size_t HEIGHT = 600;

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


int main() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* win = glfwCreateWindow(WIDTH, HEIGHT, "test", nullptr, nullptr);
  if (!win) {
    fmt::print("Failed to create GLFW window\n");
    return EXIT_FAILURE;
  }

  uint32_t glfw_extension_count{0};
  const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

  fmt::print("{} required vulkan extensions for GLFW:\n", glfw_extension_count);
  for (uint32_t i = 0; i < glfw_extension_count; ++i) {
    fmt::print(" - {}\n", glfw_extensions[i]);
  }

  std::vector<const char*> extensions(glfw_extensions, glfw_extensions+glfw_extension_count);
  if (enable_validation_layers) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // for setting up a debug messenger
  }

  ntf::vk_context context;
  try {
    context.create_instance(enable_validation_layers, extensions);

    context.create_surface([win](VkInstance instance, VkSurfaceKHR* surface) -> bool {
      // Create window surface (has to be done before device selection)
      // It needs the VK_KHR_surface extension, but it is already included
      // in the glfw extension list
      return glfwCreateWindowSurface(instance, win, nullptr, surface);
      // If not using glfw, it has to be done passing an XCB connection and
      // window details from X11 in vkCreateXcbSurfaceKHR
      // (who cares about windows right?)
    });

    context.pick_physical_device();

    context.create_logical_device();

    int w{0}, h{0}; // Not the same as WIDTH, HEIGHT in high DPI screens
    glfwGetFramebufferSize(win, &w, &h);
    context.create_swapchain(static_cast<std::size_t>(w), static_cast<std::size_t>(h));

    context.create_imageviews();

    context.create_renderpass();

    auto vert_src = file_contents("res/shader.vs.spv");
    auto frag_src = file_contents("res/shader.fs.spv");

    context.create_graphics_pipeline(vert_src.value(), frag_src.value());

    context.create_framebuffers();

    context.create_commandpool();
    context.create_commandbuffers();

    context.create_sync_objects();

    while (!glfwWindowShouldClose(win)) {
      glfwPollEvents();

      if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(win, 1);
      }

      context.draw_frame();
    }
    context.wait_idle();

    context.destroy();
  } catch (const std::exception& ex) {
    fmt::print(stderr, "{}\n", ex.what());

    glfwDestroyWindow(win);
    glfwTerminate();

    return EXIT_FAILURE;
  }

  glfwDestroyWindow(win);
  glfwTerminate();

  return EXIT_SUCCESS;
}
