cmake_minimum_required(VERSION 3.20)
project(MainProject VERSION 1.0.0 LANGUAGES C CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# 1. 서브모듈 폴더 추가
add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendor/SDL_image EXCLUDE_FROM_ALL)

# ====================================================================
# 2. Define ImGui as a standalone target without touching its folder
# ====================================================================
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

# 2. 메인 실행 파일 생성
add_executable(main src/main.cpp)

# 4. 서브모듈 라이브러리를 메인 파일에 링크
target_link_libraries(main PRIVATE 
    SDL3::SDL3
    SDL3_image::SDL3_image
    my_imgui
)

# ====================================================================
# 7. Packaging & Installation Configuration (CPack)
# ====================================================================
include(GNUInstallDirs)

# Install the main executable
install(TARGETS main
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Optional: If you have a runtime assets folder (like fonts/icons), uncomment below
# install(DIRECTORY assets DESTINATION ${CMAKE_INSTALL_DATADIR}/MainProject)

# Configure CPack variables
set(CPACK_PACKAGE_NAME "SDL3ImageViewer")
set(CPACK_PACKAGE_VENDOR "EricS")
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Sleek C++ SDL3 & ImGui Image Viewer")

# Determine package type based on Operating System
if(WIN32)
    set(CPACK_GENERATOR "ZIP") # Creates a portable .zip on Windows
else()
    set(CPACK_GENERATOR "TGZ") # Creates a portable .tar.gz on Linux/macOS
endif()

include(CPack)
