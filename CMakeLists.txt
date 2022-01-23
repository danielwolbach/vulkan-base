cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
project(vk-base VERSION 0.1.0 LANGUAGES C)

add_custom_target(vertex-shader COMMAND glslc -fshader-stage=vert -o vertex.spv "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/vertex.glsl")
add_custom_target(fragment-shader COMMAND glslc -fshader-stage=frag -o fragment.spv "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/fragment.glsl")

add_subdirectory(external/glfw)
find_package(Vulkan)

file(GLOB_RECURSE FILE_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/source/*.c ${CMAKE_CURRENT_SOURCE_DIR}/source/*.h)

add_executable(${PROJECT_NAME} "${FILE_SOURCES}")
add_dependencies(vk-base vertex-shader fragment-shader)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw Vulkan::Vulkan)
