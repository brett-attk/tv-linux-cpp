include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

add_executable(tv_core_tests test_core.cpp)
target_link_libraries(tv_core_tests PRIVATE tv_core Catch2::Catch2WithMain)
target_include_directories(tv_core_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

include(CTest)
include(Catch)
catch_discover_tests(tv_core_tests)
