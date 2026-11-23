cmake_minimum_required(VERSION 3.21)
project(Dungeonbound VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Core DSA + game logic. No SFML dependency, so the tests build anywhere.
# ---------------------------------------------------------------------------
add_library(dungeon_logic STATIC
    src/core/Graph.cpp
    src/game/Item.cpp
    src/game/Entity.cpp
    src/game/Inventory.cpp
    src/game/Dungeon.cpp
    src/game/DungeonGenerator.cpp
    src/game/Pathfinder.cpp
    src/game/BattleSystem.cpp
    src/game/GameSession.cpp
)
target_include_directories(dungeon_logic PUBLIC src)

add_executable(dungeon_tests tests/test_main.cpp)
target_link_libraries(dungeon_tests PRIVATE dungeon_logic)

enable_testing()
add_test(NAME dungeon_tests COMMAND dungeon_tests)

# ---------------------------------------------------------------------------
# SFML game (presentation layer)
# ---------------------------------------------------------------------------
option(DUNGEONBOUND_BUILD_GAME "Build the SFML game executable" ON)

if(DUNGEONBOUND_BUILD_GAME)
    set(_sfml_components Graphics Window System)
    if(WIN32)
        list(APPEND _sfml_components Main)
    endif()
    find_package(SFML 3 REQUIRED COMPONENTS ${_sfml_components})

    add_executable(dungeonbound
        src/main.cpp
        src/ui/Assets.cpp
        src/ui/Draw.cpp
        src/ui/InputHandler.cpp
        src/ui/Renderer.cpp
        src/ui/HUD.cpp
        src/ui/InventoryPanel.cpp
        src/ui/BattleScreen.cpp
        src/ui/Menus.cpp
        src/ui/Game.cpp
    )
    target_link_libraries(dungeonbound PRIVATE dungeon_logic SFML::Graphics SFML::Window SFML::System)

    if(WIN32)
        # Release builds are GUI applications: no console window behind the game.
        target_link_libraries(dungeonbound PRIVATE SFML::Main)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            set_target_properties(dungeonbound PROPERTIES WIN32_EXECUTABLE TRUE)
        endif()
    endif()

    # Fonts live next to the executable so the game runs from the build folder.
    add_custom_command(TARGET dungeonbound POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:dungeonbound>/assets)

    # `cmake --install build --prefix dist` produces a self-contained folder:
    # the executable, its assets and every DLL it needs.
    get_filename_component(_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    install(TARGETS dungeonbound
        RUNTIME_DEPENDENCIES
            PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
            POST_EXCLUDE_REGEXES ".*[Ss]ystem32.*" ".*[Ww]in[Ss]x[Ss].*"
            DIRECTORIES "${_compiler_bin}"
        RUNTIME DESTINATION .)
    install(DIRECTORY assets DESTINATION .)
endif()
