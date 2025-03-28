project(MemCpyBench LANGUAGES CXX)
set(BENCHMARK_ENABLE_TESTING OFF)
add_subdirectory(benchmark)

add_executable(MemCpyBench bench.cpp)
target_link_libraries(MemCpyBench benchmark::benchmark)
target_compile_options(MemCpyBench PRIVATE -fno-builtin -mcpu=native)

