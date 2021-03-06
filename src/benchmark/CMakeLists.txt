include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(
    hyriseOperatorBenchmark

    benchmark_basic_fixture.cpp
    benchmark_basic_fixture.hpp
    benchmark_main.cpp
    operators/aggregate_benchmark.cpp
    operators/difference_benchmark.cpp
    operators/join_benchmark.cpp
    operators/projection_benchmark.cpp
    operators/union_positions_benchmark.cpp
    operators/sort_benchmark.cpp
    operators/sql_benchmark.cpp
    operators/table_scan_benchmark.cpp
    operators/union_all_benchmark.cpp
    statistics/generate_table_statistics_benchmark.cpp
    tpch_db_generator_benchmark.cpp
)

target_link_libraries(
    hyriseOperatorBenchmark
    hyrise
    hyriseBenchmarkLib
    benchmark
)

# General purpose benchmark runner
add_executable(hyriseBenchmark benchmark_runner_main.cpp)
target_link_libraries(
    hyriseBenchmark

    hyrise
    hyriseBenchmarkLib
)

# Configure hyriseBenchmarkTPCH
add_executable(hyriseBenchmarkTPCH tpch_benchmark.cpp)
target_link_libraries(
    hyriseBenchmarkTPCH

    hyrise
    hyriseBenchmarkLib
)
