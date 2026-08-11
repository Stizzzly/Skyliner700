cmake_minimum_required(VERSION 3.10)
project(Skyliner700 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Keep build directories out of the executable source list.  A recursive glob of
# the whole project would also pick up CMake's compiler-identification source
# once a build directory is created inside the project.
set(SKYLINER_SOURCES
        main.c
        game/camera.c
        game/flight.c
        core/window.c
        model/plane.c
        render/d3d9/d3d9_device.c
        render/d3d9/d3d9_matrix.c
        render/d3d9/d3d9_mesh.c
        render/d3d9/d3d9_sky.c
        render/d3d9/d3d9_test_ground.c
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
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/core"
        "${CMAKE_SOURCE_DIR}/model"
        "${CMAKE_SOURCE_DIR}/render"
        "${CMAKE_SOURCE_DIR}/render/d3d9"
)

if (WIN32)
    # Unicode, but no forced XP version
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

    # Link libraries (psapi нужен для GetProcessMemoryInfo)
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d9 kernel32 user32 gdi32 psapi)
endif()
