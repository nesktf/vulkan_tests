cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -Wall -Wextra -Wpedantic -O0 -g2 -ggdb -Wno-psabi")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -Wall -Wextra -Wpedantic -O3 -Wno-psabi")

project(vulkan_tutorial CXX C)

# set(LIBS_DIR "lib/")
set(LIBS_INCLUDE)
set(LIBS_LINK)

## For CMake dependencies
find_package(fmt REQUIRED)
list(APPEND LIBS_INCLUDE ${FMT_INCLUDE_DIRS})
list(APPEND LIBS_LINK fmt)

find_package(glm CONFIG REQUIRED)
list(APPEND LIBS_INCLUDE ${GLM_INCLUDE_DIRS})
list(APPEND LIBS_LINK glm::glm)

find_package(assimp REQUIRED)
list(APPEND LIBS_INCLUDE ${ASSIMP_INCLUDE_DIRS})
list(APPEND LIBS_LINK assimp)

find_package(VulkanHeaders REQUIRED)
list(APPEND LIBS_INCLUDE ${VULKANHEADERS_INCLUDE_DIRS})
list(APPEND LIBS_LINK vulkan)

## For PkgConfig
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
list(APPEND LIBS_INCLUDE ${GLFW_INCLUDE_DIRS})
list(APPEND LIBS_LINK glfw)

file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")
# list(APPEND SOURCE_FILES "lib/glad/glad.c")

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC lib src ${LIBS_INCLUDE})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
target_link_libraries(${PROJECT_NAME} ${LIBS_LINK})
