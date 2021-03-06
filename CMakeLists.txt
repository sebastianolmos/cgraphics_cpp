set(CMAKE_LEGACY_CYGWIN_WIN32 OFF)
cmake_minimum_required(VERSION 3.15)
project(cgraphics C CXX)
find_package(OpenGL REQUIRED)

set(LIBS_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/libs/glad/include"
									"${CMAKE_CURRENT_SOURCE_DIR}/libs/glfw/include"
									"${CMAKE_CURRENT_SOURCE_DIR}/libs/glm"
									"${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui-1.78"
									"${CMAKE_CURRENT_SOURCE_DIR}/libs/assimp/include"
									"${CMAKE_CURRENT_SOURCE_DIR}/libs/stb")

set(CGRAPHICS_INCLUDE_DIRECTORY "${CMAKE_SOURCE_DIR}/source/")

if(MSVC)
	set(LIBS_LIBRARIES glad glfw ${OPENGL_LIBRARIES} ImGui assimp stb)
else()
	set(LIBS_LIBRARIES glad glfw ${OPENGL_LIBRARIES} ImGui assimp stb)
endif(MSVC)

configure_file(CMakeConfigFiles/root_directory.h.in "${CMAKE_SOURCE_DIR}/source/root_directory.h")
add_subdirectory(libs)
add_subdirectory(source)
add_subdirectory(examples)