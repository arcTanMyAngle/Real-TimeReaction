cmake_minimum_required(VERSION 3.20)
project(RealTimeReaction VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── System packages ───────────────────────────────────────────────────────────
find_package(SDL2   REQUIRED)
find_package(GLEW   REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core videoio imgproc)
find_package(SQLite3 REQUIRED)

# ── FetchContent ─────────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
    GIT_SHALLOW    TRUE)

FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.16
    GIT_SHALLOW    TRUE)

FetchContent_Declare(sqlitecpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG        3.3.1
    GIT_SHALLOW    TRUE
    CMAKE_CACHE_ARGS
        -DSQLITECPP_RUN_CPPLINT:BOOL=OFF
        -DSQLITECPP_RUN_CPPCHECK:BOOL=OFF
        -DSQLITECPP_BUILD_TESTS:BOOL=OFF)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
    GIT_SHALLOW    TRUE)

# NOTE: FetchContent uses add_subdirectory(), so the CMAKE_CACHE_ARGS passed to
# FetchContent_Declare(sqlitecpp) above are ignored. SQLiteCpp's options must be
# set as ordinary cache variables here, before MakeAvailable, to take effect.
#   - INTERNAL_SQLITE OFF: reuse the SQLite::SQLite3 target from find_package
#     above instead of SQLiteCpp's bundled copy (which would redefine it).
#   - RUN_CPPLINT/CPPCHECK OFF: the linters fail the build on the library's own
#     CRLF line endings (as checked out by git on Windows) — not our concern.
set(SQLITECPP_INTERNAL_SQLITE OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPLINT     OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK    OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(imgui implot sqlitecpp Catch2)

# ── ImGui static library ─────────────────────────────────────────────────────
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC SDL2::SDL2 GLEW::GLEW OpenGL::GL)

# ── ImPlot static library ────────────────────────────────────────────────────
add_library(implot_lib STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot_lib PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot_lib PUBLIC imgui_lib)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(rtr
    src/main.cpp
    src/core/timer.cpp
    src/core/detection.cpp
    src/core/session.cpp
    src/data/database.cpp
    src/data/analytics.cpp
    src/ui/renderer.cpp
    src/ui/camera_texture.cpp
    src/ui/screens/menu_screen.cpp
    src/ui/screens/game_screen.cpp
    src/ui/screens/results_screen.cpp
    src/ui/screens/analytics_screen.cpp
)
target_include_directories(rtr PRIVATE src/)
target_link_libraries(rtr PRIVATE
    imgui_lib implot_lib SQLiteCpp
    ${OpenCV_LIBS} SDL2::SDL2 GLEW::GLEW OpenGL::GL
)

# Create data/ dir at build time
add_custom_command(TARGET rtr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:rtr>/data/exports)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
