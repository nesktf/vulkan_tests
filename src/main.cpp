#define GLFW_INCLUDE_VULKAN
// #include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <fmt/format.h>

#include <cassert>

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
    uint32_t extension_count{0};
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    fmt::print("{} vulkan extensions supported\n", extension_count);
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
    glfwDestroyWindow(_win);
    glfwTerminate();
  }

private:
  GLFWwindow* _win;
};

int main() {
  application app;
  try {
    app.run();
  } catch (const std::exception& e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
