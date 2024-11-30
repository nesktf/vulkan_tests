#define GLFW_INCLUDE_VULKAN
// #include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <fmt/format.h>

#include <cassert>

int main() {
  glfwInit();
  glfwSwapInterval(0);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  GLFWwindow* win = glfwCreateWindow(800, 600, "test", nullptr, nullptr);
  if (!win) {
    fmt::print("Failed to create GLFW window\n");
    return 1;
  }
  // glfwMakeContextCurrent(win);

  uint32_t extension_count{0};
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

  fmt::print("{} vulkan extensions supported\n", extension_count);

  glm::mat4 mat{};
  glm::vec4 vec{};
  auto test = mat*vec;

  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();

    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(win, 1);
    }
  }

  glfwDestroyWindow(win);
  glfwTerminate();

  return 0;
}
