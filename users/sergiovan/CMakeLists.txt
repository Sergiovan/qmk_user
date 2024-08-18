cmake_minimum_required(VERSION 3.30)
project(kb_library_tests C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-fdiagnostics-color=always")

find_package(GTest REQUIRED)

enable_testing()

set(FILES_UNDER_TEST
  circular_buffer/circular_buffer.c
)

set(FILES_GTEST
  circular_buffer/circular_buffer_tests.cpp
)

add_executable(circular_buffer_test
  ${FILES_UNDER_TEST}
  ${FILES_GTEST}
)

target_compile_options(
  circular_buffer_test PRIVATE
)

target_link_libraries(
  circular_buffer_test
  GTest::gtest_main
)

if(${CMAKE_COVERAGE})
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "Cannot make code coverage in non-debug mode")
  endif()

  set_property(SOURCE ${FILES_UNDER_TEST} PROPERTY COMPILE_FLAGS
    "-fprofile-instr-generate -fcoverage-mapping"
  )

  target_link_options(circular_buffer_test PRIVATE
    -fprofile-instr-generate -fcoverage-mapping
  )

  add_custom_target(run_coverage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND ./circular_buffer_test &>/dev/null
    COMMAND llvm-profdata merge -sparse default.profraw -o default.profdata
    COMMAND llvm-cov show ./circular_buffer_test -instr-profile=default.profdata -format=html > output.html
    COMMAND firefox output.html
  )

  add_dependencies(run_coverage circular_buffer_test)
endif()

include(GoogleTest)
gtest_discover_tests(circular_buffer_test)