cmake_minimum_required(VERSION 3.20)
project(MainProject VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendor/SDL_image EXCLUDE_FROM_ALL)

# Define ImGui static source group
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)
set(IMGUI_SRCS
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

# Create a static library target named 'my_imgui' entirely inside this scope
add_library(my_imgui STATIC ${IMGUI_SRCS})
set_target_properties(my_imgui PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Configure includes for the 'my_imgui' target safely
target_include_directories(my_imgui PUBLIC 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_include_directories(my_imgui PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/SDL/include
)

# ====================================================================
# 3. Setup your Main Executable (Inherits the IDE's select variant)
# ====================================================================
add_executable(main src/main.cpp)

# ====================================================================
# 4. Definitions, Includes, and Linking
# ====================================================================
target_compile_definitions(main PRIVATE SDL_MAIN_USE_CALLBACKS=1)

# Main project only needs to know about SDL headers (ImGui headers come automatically via link)
target_include_directories(main PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/SDL/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/SDL_image/include
)

# Link everything using clean target names!
target_link_libraries(main PRIVATE 
    SDL3::SDL3 
    SDL3_image::SDL3_image
    my_imgui # Connects to our safely isolated ImGui library
)
