cmake_minimum_required(VERSION 3.10)
project(Skyliner700 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Keep build directories out of the executable source list.  A recursive glob of
# the whole project would also pick up CMake's compiler-identification source
# once a build directory is created inside the project.
set(SKYLINER_SOURCES
        src/win32/main.c
        src/win32/game/camera.c
        src/common/game/flight.c
        src/common/game/flight_input.c
        src/common/game/flight_scenario.c
        src/common/world/terrain.c
        src/win32/core/window.c
        src/common/model/plane.c
        src/win32/render/d3d9/d3d9_device.c
        src/win32/render/d3d9/d3d9_matrix.c
        src/win32/render/d3d9/d3d9_mesh.c
        src/win32/render/d3d9/d3d9_sky.c
        src/win32/render/d3d9/d3d9_terrain.c
        src/win32/render/d3d9/d3d9_hud.c
)

add_executable(${PROJECT_NAME} ${SKYLINER_SOURCES})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMENT "Copying game assets next to the executable"
)

# Public include directories
target_include_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/src/win32"
        "${CMAKE_SOURCE_DIR}/src/common"
        "${CMAKE_SOURCE_DIR}/src/win32/core"
        "${CMAKE_SOURCE_DIR}/src/win32/render"
        "${CMAKE_SOURCE_DIR}/src/win32/render/d3d9"
)

if (WIN32)
    # Unicode, but no forced XP version
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

    # PSAPI_VERSION=1 keeps GetProcessMemoryInfo in PSAPI.DLL, which exists on XP.
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d9 kernel32 user32 gdi32 psapi)
endif()

include(CTest)
if (BUILD_TESTING)
    add_executable(flight_physics_tests tests/flight_physics_tests.c src/common/game/flight.c src/common/game/flight_scenario.c src/common/world/terrain.c)
    target_include_directories(flight_physics_tests PRIVATE "${CMAKE_SOURCE_DIR}/src/common")
    target_link_libraries(flight_physics_tests PRIVATE m)
    add_test(NAME flight_physics COMMAND flight_physics_tests)
endif()
