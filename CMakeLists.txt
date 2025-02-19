cmake_minimum_required(VERSION 3.16)
project(obs-17live VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OBS
find_package(LibObs REQUIRED)

# Set source files
set(obs_17live_SOURCES
    src/plugin-main.cpp
    src/17live-stream-settings.cpp
    src/17live-chat-dock.cpp
    src/17live-api.cpp
)

# Set header files
set(obs_17live_HEADERS
    src/17live-stream-settings.hpp
    src/17live-chat-dock.hpp
    src/17live-api.hpp
)

# Add library target
add_library(obs-17live MODULE
    ${obs_17live_SOURCES}
    ${obs_17live_HEADERS}
)

# Include directories
target_include_directories(obs-17live PRIVATE
    ${LIBOBS_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(obs-17live PRIVATE
    libobs
)

# Install target
install(TARGETS obs-17live
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/obs-plugins
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/obs-plugins/64bit
)

# Install data files
install(FILES data/locale/en-US.ini
    DESTINATION ${CMAKE_INSTALL_PREFIX}/data/obs-plugins/obs-17live/locale
)