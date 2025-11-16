cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

# Set the runtime library on Windows to static linking (/MT) to match the CEF library
if(OS_WINDOWS)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:/MT>
    $<$<CONFIG:RelWithDebInfo>:/MT>
    $<$<CONFIG:Debug>:/MTd>
  )
endif()

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(CURL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  # Add our custom cmake modules to the front of the module path
  # This ensures our custom FindWrapOpenGL.cmake is used instead of Qt6's version
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

# Add CEF support
# CEF_ROOT_DIR should already be set in defaults.cmake -> buildspec.cmake
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

if(CEF_ROOT_DIR)
  set(CEF_ROOT "${CEF_ROOT_DIR}")
  message(STATUS "Using CEF_ROOT: ${CEF_ROOT}")
else()
  message(FATAL_ERROR "CEF_ROOT_DIR not set. Dependencies may not have been processed correctly.")
endif()

find_package(CEF REQUIRED)

find_package(MbedTLS REQUIRED)

# Add nlohmann json include path from obs-deps
find_path(NLOHMANN_JSON_INCLUDE_DIR
  NAMES nlohmann/json.hpp
  PATHS ${CMAKE_PREFIX_PATH}
  PATH_SUFFIXES include
  NO_DEFAULT_PATH
)

if(NLOHMANN_JSON_INCLUDE_DIR)
  message(STATUS "Found nlohmann json: ${NLOHMANN_JSON_INCLUDE_DIR}")
else()
  message(FATAL_ERROR "nlohmann json not found in obs-deps")
endif()

# Set header file include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/17live
  ${CMAKE_CURRENT_SOURCE_DIR}/deps
  ${NLOHMANN_JSON_INCLUDE_DIR}
  ${CEF_ROOT_DIR} # This might need adjustment based on actual CEF include path
  ${CMAKE_CURRENT_BINARY_DIR}/src # For finding the generated plugin-support.c
)

# Add websocketpp include directory from obs-deps
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/cpp-httplib
  ${CMAKE_PREFIX_PATH}/include  # For websocketpp from obs-deps
)

# Link CEF libraries (nlohmann json is header-only)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  CEF::Library
  CEF::Wrapper
  Threads::Threads # For cpp-httplib and websocketpp
  MbedTLS::mbedcrypto
  MbedTLS::mbedtls
  MbedTLS::mbedx509
)

# Suppress newline-at-EOF warning on headers included by this target
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-newline-eof -Wno-error=newline-eof>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-eof-newline -Wno-error=eof-newline>
)

# Define websocketpp to use C++11 STL features
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  _WEBSOCKETPP_CPP11_STL_
)

# Configure plugin-support.c file
configure_file(src/plugin-support.c.in src/plugin-support.c @ONLY)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  resources.qrc
  src/17live/api/OneSevenLiveApiWrappers.cpp
  src/17live/api/OneSevenLiveModels.cpp
  src/17live/api/OneSevenLiveUtility.cpp
  src/17live/CefDummy.cpp
  src/17live/multi-rtmp/OneSevenLiveMultiRtmpConfigManager.cpp
  src/17live/multi-rtmp/OneSevenLiveMultiRtmpManager.cpp
  src/17live/multi-rtmp/OneSevenLiveMultiRtmpModels.cpp
  src/17live/multi-rtmp/OneSevenLiveMultiRtmpStreamController.cpp
  src/17live/multi-rtmp/ui/OneSevenLiveMultiRtmpConfigDialog.cpp
  src/17live/multi-rtmp/ui/OneSevenLiveMultiRtmpDock.cpp
  src/17live/multi-rtmp/ui/OneSevenLiveMultiRtmpListWidget.cpp
  src/17live/multi-rtmp/ui/OneSevenLiveMultiRtmpStreamItem.cpp
  src/17live/OneSevenLiveConfigManager.cpp
  src/17live/OneSevenLiveCoreManager.cpp
  src/17live/OneSevenLiveCustomEventDialog.cpp
  src/17live/OneSevenLiveHttpServer.cpp
  src/17live/OneSevenLiveLoginDialog.cpp
  src/17live/OneSevenLiveMenuManager.cpp
  src/17live/OneSevenLiveRockViewerItem.cpp
  src/17live/OneSevenLiveRockZoneDock.cpp
  src/17live/OneSevenLiveStreamListDock.cpp
  src/17live/OneSevenLiveStreamListItem.cpp
  src/17live/OneSevenLiveUpdateManager.cpp
  src/17live/OneSevenLiveUserDialog.cpp
  src/17live/OneSevenLiveWebsocketServer.cpp
  src/17live/QCefView.cpp
  src/17live/SimpleCefClient.cpp
  src/17live/streaming/OneSevenLiveLoadRoomInfoWorker.cpp
  src/17live/streaming/OneSevenLiveStreamManager.cpp
  src/17live/streaming/OneSevenLiveStreamingDock.cpp
  src/17live/twitch/OneSevenLiveTwitchAuth.cpp
  src/17live/twitch/OneSevenLiveTwitchChatClient.cpp
  src/17live/twitch/OneSevenLiveTwitchClient.cpp
  src/17live/ui/OneSevenLiveAuthDialog.cpp
  src/17live/ui/OneSevenLiveLineEditWithEye.cpp
  src/17live/ui/OneSevenLivePreviewDock.cpp
  src/17live/ui/OneSevenLivePreviewScreen.cpp
  src/17live/ui/OneSevenLivePreviewWidget.cpp
  src/17live/ui/OneSevenLivePropertiesWidget.cpp
  src/17live/ui/OneSevenLivePropertyWidget.cpp
  src/17live/utility/Common.cpp
  src/17live/utility/CustomCalendarWidget.cpp
  src/17live/utility/DownloadWorker.cpp
  src/17live/utility/Meta.cpp
  src/17live/utility/NetworkDiagnostics.cpp
  src/17live/utility/OneSevenLivePreviewConfigLoader.cpp
  src/17live/utility/RemoteTextThread.cpp
  src/17live/youtube/OneSevenLiveYouTubeAuth.cpp
  src/17live/youtube/OneSevenLiveYouTubeChatClient.cpp
  src/17live/youtube/OneSevenLiveYouTubeClient.cpp
  src/diag/DiagnosticsCollectorFactory.cpp
  src/diag/DiagnosticsCollectorBase.cpp
  src/diag/PrivacyFilter.cpp
  src/diag/ui/DiagnosticsDialog.cpp
  src/plugin-main.cpp
  src/plugin-support.c
)

if(OS_MACOS)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    src/diag/DiagnosticsCollectorMacOS.cpp
  )
elseif(OS_WINDOWS)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    src/diag/DiagnosticsCollectorWindows.cpp
  )
endif()

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /wd4996)
else()
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wno-deprecated-builtins)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Windows-specific CEF configuration
if(OS_WINDOWS)
  # Add Windows specific defines
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
  )
endif()
