cmake_minimum_required(VERSION 3.24)
project(drum-machine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(DRUM_MACHINE_BUILD_APP "Build the SDL3 desktop application" ON)
include(CTest)

add_library(drum_engine src/engine.cpp)
target_include_directories(drum_engine PUBLIC src)
target_compile_options(drum_engine PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE drum_engine)
    add_test(NAME engine COMMAND engine_tests)
endif()

if(DRUM_MACHINE_BUILD_APP)
    include(FetchContent)
    find_package(SDL3 3.2 QUIET CONFIG)
    if(NOT SDL3_FOUND)
        # Keep the installed executable independent of a build-tree SDL .so.
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.deps/SDL/CMakeLists.txt")
            set(FETCHCONTENT_SOURCE_DIR_SDL "${CMAKE_CURRENT_SOURCE_DIR}/.deps/SDL")
        endif()
        FetchContent_Declare(SDL GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.28 GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(SDL)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.deps/imgui/imgui.cpp")
        set(FETCHCONTENT_SOURCE_DIR_IMGUI "${CMAKE_CURRENT_SOURCE_DIR}/.deps/imgui")
    endif()
    FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(imgui)
    add_library(drum_ui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
    target_include_directories(drum_ui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(drum_ui PUBLIC SDL3::SDL3)
    add_executable(drum-machine src/main.cpp)
    target_compile_options(drum-machine PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(drum-machine PRIVATE drum_engine drum_ui)
    add_custom_command(TARGET drum-machine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:drum-machine>/assets")
    if(BUILD_TESTING)
        add_test(NAME desktop_smoke COMMAND drum-machine --smoke-test)
        add_test(NAME sample_render COMMAND drum-machine --render-demo "${CMAKE_CURRENT_BINARY_DIR}/demo.wav")
    endif()
    install(TARGETS drum-machine RUNTIME DESTINATION bin)
    install(DIRECTORY assets/ DESTINATION share/drum-machine/assets)
    install(FILES LICENSE THIRD_PARTY.md DESTINATION share/drum-machine)
    install(FILES ${imgui_SOURCE_DIR}/LICENSE.txt
        DESTINATION share/drum-machine/licenses RENAME imgui-LICENSE.txt)
    if(DEFINED sdl_SOURCE_DIR)
        install(FILES ${sdl_SOURCE_DIR}/LICENSE.txt
            DESTINATION share/drum-machine/licenses RENAME SDL-LICENSE.txt)
    endif()
endif()
