cmake_minimum_required(VERSION 3.10)
project(Skyliner700 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Collect all C sources
file(GLOB_RECURSE SKYLINER_SOURCES
        "${CMAKE_SOURCE_DIR}/**/*.c"
)

add_executable(${PROJECT_NAME} ${SKYLINER_SOURCES})

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