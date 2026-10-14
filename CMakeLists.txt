cmake_minimum_required(VERSION 3.20)
project(RogueLegacyCPP VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# 1. Fetch Raylib 5.0
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# 2. Fetch nlohmann/json v3.11.3
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(SOURCES
    src/main.cpp
    src/core/SaveManager.cpp
    src/data/Registry.cpp
    src/audio/SoundSynth.cpp
    src/gfx/ShaderManager.cpp
    src/gfx/ParticleSystem.cpp
    src/gfx/ProceduralRenderer.cpp
    src/gfx/UiText.cpp
    src/entities/Player.cpp
    src/entities/Projectile.cpp
    src/entities/Enemy.cpp
    src/entities/Boss.cpp
    src/entities/Interactable.cpp
    src/world/Room.cpp
    src/world/CastleGenerator.cpp
    src/states/TitleState.cpp
    src/states/LineageState.cpp
    src/states/ManorState.cpp
    src/states/OutskirtsState.cpp
    src/states/CastlePlayState.cpp
    src/states/DeathSummaryState.cpp
)

add_executable(rogue_legacy_cpp ${SOURCES})
target_include_directories(rogue_legacy_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rogue_legacy_cpp PRIVATE
    raylib
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(rogue_legacy_cpp PRIVATE /W3 /permissive-)
else()
    target_compile_options(rogue_legacy_cpp PRIVATE -Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
endif()
