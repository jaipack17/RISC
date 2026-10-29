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

add_library(risc_assembler STATIC src/assembler/assembler.cpp)
target_link_libraries(risc_assembler PUBLIC risc_isa)

add_executable(risc_asm src/assembler_main.cpp)
target_link_libraries(risc_asm PRIVATE risc_assembler)

add_library(risc_microassembler STATIC src/microassembler/microassembler.cpp src/microassembler/splitter.cpp)
target_link_libraries(risc_microassembler PUBLIC risc_isa)

add_executable(risc_masm src/microassembler_main.cpp)
target_link_libraries(risc_masm PRIVATE risc_microassembler)

add_executable(risc_run src/main.cpp)
target_link_libraries(risc_run PRIVATE risc_visualizer risc_assembler)

add_executable(risc_micro_run src/micro_main.cpp)
target_link_libraries(risc_micro_run PRIVATE risc_assembler risc_microassembler)

add_executable(risc_6stage_run src/six_stage_main.cpp)
target_link_libraries(risc_6stage_run PRIVATE risc_assembler risc_microassembler)

add_library(risc_micro_visualizer STATIC src/visualizer/micro_visualizer.cpp)
target_link_libraries(risc_micro_visualizer PUBLIC risc_visualizer risc_microassembler)

add_executable(risc_micro_view src/micro_view_main.cpp)
target_link_libraries(risc_micro_view PRIVATE risc_micro_visualizer risc_assembler)