cmake_minimum_required(VERSION 3.20)
project(Threadlight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
option(THREADLIGHT_BUILD_GAME "Build the raylib game" ON)

if(THREADLIGHT_BUILD_GAME)
  find_package(raylib 5.5 CONFIG REQUIRED)
  add_executable(Threadlight src/main.cpp)
  target_link_libraries(Threadlight PRIVATE raylib)
  if(MSVC)
    target_compile_options(Threadlight PRIVATE /utf-8)
  endif()
endif()

if(BUILD_TESTING)
  add_executable(ThreadlightLogicTests tests/game_logic_test.cpp)
  target_include_directories(ThreadlightLogicTests PRIVATE src)
  add_test(NAME ThreadlightLogicTests COMMAND ThreadlightLogicTests)
endif()
