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

constexpr std::size_t WIDTH = 800;
constexpr std::size_t HEIGHT = 600;

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
    vkDestroyInstance(_vk_instance, nullptr);

    glfwDestroyWindow(_win);
    glfwTerminate();
  }

private:
  void create_instance() {
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

    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());
    fmt::print("{} vulkan extensions supported:\n", extension_count);
    for (const auto& ext : extensions) {
      fmt::print(" - {}\n", ext.extensionName);
    }

    // Pass the windowing system extensions
    uint32_t glfw_extension_count{0};
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    create_info.enabledLayerCount = glfw_extension_count;
    create_info.ppEnabledExtensionNames = glfw_extensions;

    fmt::print("{} glfw extensions supported:\n", glfw_extension_count);
    for (uint32_t i = 0; i < glfw_extension_count; ++i) {
      fmt::print(" - {}\n", glfw_extensions[i]);
    }

    create_info.enabledLayerCount = 0; // Global validation layers to enable

    // Second param is a custom allocator callback, null for now
    VkResult res = vkCreateInstance(&create_info, nullptr, &_vk_instance);
    if (res != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create vulkan instance"};
    }
    fmt::print("Vulkan instance initialized\n");
  }

private:
  GLFWwindow* _win;
  VkInstance _vk_instance;
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
