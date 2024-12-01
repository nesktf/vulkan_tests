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

const std::vector<const char*> validation_layers = {
  "VK_LAYER_KHRONOS_validation",
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
    if (enable_validation_layers) {
      DestroyDebugUtilsMessengerEXT(_vk_instance, _debug_messenger, nullptr);
    }
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

    fmt::print("{} glfw extensions supported:\n", glfw_extension_count);
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

  void setup_debug_messenger() {
    // Set the messenger callback
    if (!enable_validation_layers) {
      return;
    }

    auto create_info = create_messenger_info();
    if (CreateDebugUtilsMessengerEXT(_vk_instance, &create_info, nullptr, &_debug_messenger) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to setup debug messenger"};
    }
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
    VkResult res = vkCreateInstance(&create_info, nullptr, &_vk_instance);
    if (res != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create vulkan instance"};
    }
    fmt::print("Vulkan instance initialized\n");
  }

private:
  GLFWwindow* _win;
  VkInstance _vk_instance;
  VkDebugUtilsMessengerEXT _debug_messenger;
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
