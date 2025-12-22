cmake_minimum_required(VERSION 3.5)
project(ComputerGraphics-Project)

find_package(OpenGL REQUIRED)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_subdirectory(external)

include_directories(
	external
	cloudWorld
	external/glfw-3.1.2/include   # GLFW/glfw3.h
	external/glm-0.9.7.1          # glm/glm.hpp
	external/glad-opengl-3.3/include
	external/tinygltf-2.9.3
)

add_executable(cloudWorld
	cloudWorld/cloudWorld.cpp
	cloudWorld/render/shader.cpp
)
target_link_libraries(cloudWorld
	${OPENGL_LIBRARY}
	glfw
	glad
)
