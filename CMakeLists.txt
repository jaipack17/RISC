cmake_minimum_required(VERSION 3.20)
project(risc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(risc_isa INTERFACE)
target_include_directories(risc_isa INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(risc_visualizer STATIC src/visualizer/visualizer.cpp)
target_link_libraries(risc_visualizer PUBLIC risc_isa)

# Compile-check the header-only six-stage CPU independently of the reference
# executable. Pipeline selection can be wired into a runtime target later.
add_library(risc_six_stage OBJECT src/six_stage_cpu.cpp)
target_link_libraries(risc_six_stage PRIVATE risc_isa)

add_executable(risc_run src/main.cpp)
target_link_libraries(risc_run PRIVATE risc_visualizer)
