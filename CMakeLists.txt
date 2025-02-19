cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)
endif()

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

target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
    src/plugin-main.cpp
    src/17live-stream-settings.cpp
    src/17live-chat-dock.cpp
    src/17live-api.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Add library target
# add_library(obs-17live MODULE
#     ${obs_17live_SOURCES}
#     ${obs_17live_HEADERS}
# )

# Include directories
# target_include_directories(obs-17live PRIVATE
#     ${LIBOBS_INCLUDE_DIR}
#     ${CMAKE_SOURCE_DIR}/src
# )

# Link libraries
# target_link_libraries(obs-17live PRIVATE
#     libobs
# )

# # Install target
# install(TARGETS obs-17live
#     LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/obs-plugins
#     RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/obs-plugins/64bit
# )

# # Install data files
# install(FILES data/locale/en-US.ini
#     DESTINATION ${CMAKE_INSTALL_PREFIX}/data/obs-plugins/obs-17live/locale
# )