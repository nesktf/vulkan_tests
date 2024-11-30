#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include <fmt/format.h>

#include <cassert>
#include <iostream>

int main() {
  glfwInit();
  glfwSwapInterval(0);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* win = glfwCreateWindow(800, 600, "test", nullptr, nullptr);
  if (!win) {
    std::cout << "Failed to create window\n";
    return 1;
  }

  glfwMakeContextCurrent(win);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    glfwDestroyWindow(win);
    std::cout << "Failed to init glad\n";
    return 1;
  }


  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();

    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(win, 1);
    }

    glClearColor(.3f, .3f, .3f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glfwSwapBuffers(win);
  }

  return 0;
}
