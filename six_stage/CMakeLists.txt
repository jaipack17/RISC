cmake_minimum_required(VERSION 3.20)
project(risc201_six_stage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(risc_six_stage
  main.cpp
  visualizer.cpp
  "${CMAKE_CURRENT_LIST_DIR}/../src/assembler/assembler.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../src/microassembler/microassembler.cpp"
)
target_include_directories(risc_six_stage PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../include")
target_compile_definitions(risc_six_stage PRIVATE
  RISC201_DEFAULT_MICROCODE_PATH="${CMAKE_CURRENT_LIST_DIR}/../microcode/vertical.masm"
)

if(MSVC)
  target_compile_options(risc_six_stage PRIVATE /W4)
else()
  target_compile_options(risc_six_stage PRIVATE -Wall -Wextra -Wpedantic)
endif()
