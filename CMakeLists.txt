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
  # Prefer the actual library path when available. Some pkg-config based
  # raylib config packages expose Unix -L/-l flags as one imported link item,
  # which MSVC cannot interpret.
  if(TARGET raylib::raylib)
    target_link_libraries(Threadlight PRIVATE raylib::raylib)
  elseif(raylib_LIBRARY)
    target_link_libraries(Threadlight PRIVATE "${raylib_LIBRARY}")
    target_include_directories(Threadlight PRIVATE ${raylib_INCLUDE_DIRS})
  else()
    target_link_libraries(Threadlight PRIVATE raylib)
  endif()
  if(WIN32)
    target_link_libraries(Threadlight PRIVATE winmm)
  endif()
  if(MSVC)
    # The pinned static raylib package is built with the static CRT (/MT).
    # Match it so the Release executable has one consistent runtime and no
    # LNK4098 LIBCMT/MSVCRT conflict.
    set_property(TARGET Threadlight PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_options(Threadlight PRIVATE /utf-8)
  endif()
endif()

if(BUILD_TESTING)
  add_executable(ThreadlightLogicTests tests/game_logic_test.cpp)
  target_include_directories(ThreadlightLogicTests PRIVATE src)
  add_test(NAME ThreadlightLogicTests COMMAND ThreadlightLogicTests)

  add_executable(ThreadlightAudioTests tests/audio_design_test.cpp)
  target_include_directories(ThreadlightAudioTests PRIVATE src)
  add_test(NAME ThreadlightAudioTests COMMAND ThreadlightAudioTests)

  add_executable(ThreadlightVisualFeedbackTests tests/visual_feedback_test.cpp)
  target_include_directories(ThreadlightVisualFeedbackTests PRIVATE src)
  add_test(NAME ThreadlightVisualFeedbackTests COMMAND ThreadlightVisualFeedbackTests)

  add_executable(ThreadlightVisualEffectsTests tests/visual_effects_test.cpp)
  target_include_directories(ThreadlightVisualEffectsTests PRIVATE src)
  add_test(NAME ThreadlightVisualEffectsTests COMMAND ThreadlightVisualEffectsTests)

  add_executable(ThreadlightSettingsTests tests/settings_test.cpp)
  target_include_directories(ThreadlightSettingsTests PRIVATE src)
  add_test(NAME ThreadlightSettingsTests COMMAND ThreadlightSettingsTests)

  add_executable(ThreadlightMenuStateTests tests/menu_state_test.cpp)
  target_include_directories(ThreadlightMenuStateTests PRIVATE src)
  add_test(NAME ThreadlightMenuStateTests COMMAND ThreadlightMenuStateTests)

  add_executable(ThreadlightTimingTests tests/timing_test.cpp)
  target_include_directories(ThreadlightTimingTests PRIVATE src)
  add_test(NAME ThreadlightTimingTests COMMAND ThreadlightTimingTests)

  add_executable(ThreadlightPresentationTests tests/presentation_test.cpp)
  target_include_directories(ThreadlightPresentationTests PRIVATE src)
  add_test(NAME ThreadlightPresentationTests COMMAND ThreadlightPresentationTests)

  add_executable(ThreadlightFoundationTests tests/foundation_test.cpp)
  target_include_directories(ThreadlightFoundationTests PRIVATE src)
  add_test(NAME ThreadlightFoundationTests COMMAND ThreadlightFoundationTests)

  add_executable(ThreadlightFoundationScenarioTests tests/foundation_scenario_test.cpp)
  target_include_directories(ThreadlightFoundationScenarioTests PRIVATE src)
  add_test(NAME ThreadlightFoundationScenarioTests COMMAND ThreadlightFoundationScenarioTests)

  add_executable(ThreadlightCombatUpgradesTests tests/combat_upgrades_test.cpp)
  target_include_directories(ThreadlightCombatUpgradesTests PRIVATE src)
  add_test(NAME ThreadlightCombatUpgradesTests COMMAND ThreadlightCombatUpgradesTests)

  add_executable(ThreadlightSkitterTests tests/skitter_test.cpp)
  target_include_directories(ThreadlightSkitterTests PRIVATE src)
  add_test(NAME ThreadlightSkitterTests COMMAND ThreadlightSkitterTests)

  add_executable(ThreadlightUpgradeLayoutTests tests/upgrade_layout_test.cpp)
  target_include_directories(ThreadlightUpgradeLayoutTests PRIVATE src)
  add_test(NAME ThreadlightUpgradeLayoutTests COMMAND ThreadlightUpgradeLayoutTests)

  add_executable(ThreadlightChapterTests tests/chapter_test.cpp)
  target_include_directories(ThreadlightChapterTests PRIVATE src)
  add_test(NAME ThreadlightChapterTests COMMAND ThreadlightChapterTests)

  add_executable(ThreadlightRootwardenTests tests/rootwarden_test.cpp)
  target_include_directories(ThreadlightRootwardenTests PRIVATE src)
  add_test(NAME ThreadlightRootwardenTests COMMAND ThreadlightRootwardenTests)

  add_executable(ThreadlightChapterHudTests tests/chapter_hud_test.cpp)
  target_include_directories(ThreadlightChapterHudTests PRIVATE src)
  add_test(NAME ThreadlightChapterHudTests COMMAND ThreadlightChapterHudTests)


  add_executable(ThreadlightRootwardenScenarioTests tests/rootwarden_scenario_test.cpp)
  target_include_directories(ThreadlightRootwardenScenarioTests PRIVATE src)
  add_test(NAME ThreadlightRootwardenScenarioTests COMMAND ThreadlightRootwardenScenarioTests)

  add_executable(ThreadlightAfterimageTests tests/afterimage_test.cpp)
  target_include_directories(ThreadlightAfterimageTests PRIVATE src)
  add_test(NAME ThreadlightAfterimageTests COMMAND ThreadlightAfterimageTests)

  add_executable(ThreadlightRangedTests tests/ranged_combat_test.cpp)
  target_include_directories(ThreadlightRangedTests PRIVATE src)
  add_test(NAME ThreadlightRangedTests COMMAND ThreadlightRangedTests)

  add_executable(ThreadlightM4IntegrationTests tests/m4_integration_test.cpp)
  target_include_directories(ThreadlightM4IntegrationTests PRIVATE src)
  add_test(NAME ThreadlightM4IntegrationTests COMMAND ThreadlightM4IntegrationTests)
  add_executable(ThreadlightM4ScenarioTests tests/m4_scenario_test.cpp)
  target_include_directories(ThreadlightM4ScenarioTests PRIVATE src)
  add_test(NAME ThreadlightM4ScenarioTests COMMAND ThreadlightM4ScenarioTests)

endif()
