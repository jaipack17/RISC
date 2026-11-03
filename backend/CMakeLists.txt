find_package(Threads REQUIRED)

add_executable(risc_web server.cpp session.cpp)
target_include_directories(risc_web SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/external)
target_link_libraries(risc_web PRIVATE risc_assembler risc_microassembler risc_visualizer Threads::Threads)
set_target_properties(risc_web PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
