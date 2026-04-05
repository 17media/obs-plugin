cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)
option(USE_STATIC_MSVC_RUNTIME "Use static MSVC runtime library (/MT) instead of dynamic (/MD)" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

# Set the runtime library on Windows based on the option
# if(OS_WINDOWS)
# # obs-studio official strategy: use static runtime (/MT) for CEF compatibility
# # Provide stdio function compatibility for MbedTLS through legacy_stdio_definitions.lib
# if(USE_STATIC_MSVC_RUNTIME)
# message(STATUS "Using static MSVC runtime for Windows build")
# set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# else()
# message(STATUS "Using dynamic MSVC runtime for Windows build")
# set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
# endif()

# # Ensure all targets use consistent runtime library settings
# set(CMAKE_MSVC_RUNTIME_LIBRARY_DEFAULT ${CMAKE_MSVC_RUNTIME_LIBRARY})
# message(STATUS "CMAKE_MSVC_RUNTIME_LIBRARY set to: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
# endif()
find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(CURL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl)

# zlib for gzip inflate used in Ably payload decoding
find_package(ZLIB REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ZLIB::ZLIB)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(OS_WINDOWS)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    ws2_32
    bcrypt
    winhttp
  )
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

# obs-studio official strategy: prefer pre-built static libraries to avoid runtime conflicts
if(NOT OS_WINDOWS)
  find_package(MbedTLS REQUIRED)
endif()

# Add LibDataChannel support from obs-deps
find_package(LibDataChannel REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE LibDataChannel::LibDataChannel)

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
  ${CEF_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/src # For finding the generated plugin-support.c
)

# Add websocketpp include directory from obs-deps
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/cpp-httplib
  ${CMAKE_PREFIX_PATH}/include # For websocketpp from obs-deps
)

# Link CEF libraries (nlohmann json is header-only)
find_package(Threads REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  Threads::Threads # For cpp-httplib and websocketpp
)

# Platform-specific MbedTLS linking
if(NOT OS_WINDOWS)
  # # obs-studio official strategy: use static runtime (/MT) for CEF compatibility
  # # Provide stdio function compatibility for MbedTLS through legacy_stdio_definitions.lib
  # target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  # ws2_32
  # bcrypt
  # winhttp
  # )

  # # Windows-specific definitions for stdio compatibility
  # target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  # _CRT_DECLARE_NONSTDC_NAMES=1 # For stdio functions like setbuf
  # _WIN32_WINNT=0x0A00 # Windows 10 and later
  # )

  # # obs-studio strategy: use static runtime, provide stdio compatibility through legacy libraries
  # # Provide stdio function compatibility for MbedTLS (setbuf, ferror, remove, etc.)

  # # CEF wrapper library compatibility handling - use static runtime
  # set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
  # MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  # LINK_FLAGS "/WX:NO"
  # )
  # else()
  # macOS/Linux: Standard MbedTLS linking
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    MbedTLS::mbedcrypto
    MbedTLS::mbedtls
    MbedTLS::mbedx509
  )
endif()

# Suppress newline-at-EOF warning on headers included by this target
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-newline-eof -Wno-error=newline-eof>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-eof-newline -Wno-error=eof-newline>
)

# Define websocketpp to use C++11 STL features
# target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
# _WEBSOCKETPP_CPP11_STL_
# )

# Configure plugin-support.c file
foreach(_v IN ITEMS ONESEVENLIVE_API_URL YOUTUBE_API_CLIENT_ID YOUTUBE_API_CLIENT_SECRET TWITCH_API_CLIENT_ID)
  if(NOT DEFINED ${_v} AND DEFINED ENV{${_v}})
    set(${_v} "$ENV{${_v}}" CACHE STRING "")
  endif()
endforeach()

configure_file(src/plugin-support.c.in src/plugin-support.c @ONLY)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  resources.qrc
  src/17live/api/OneSevenLiveAblyChatClient.cpp
  src/17live/api/OneSevenLiveApiWrappers.cpp
  src/17live/api/OneSevenLiveModels.cpp
  src/17live/api/OneSevenLiveUtility.cpp
  src/17live/multi-rtmp/OneSevenLiveMultiRtmpConfigManager.cpp
  src/17live/multi-rtmp/OneSevenLiveMultiRtmpManager.cpp
  src/17live/multi-rtmp/OneSevenLiveMultiRtmpModels.cpp
  src/17live/multi-rtmp/OneSevenLiveMultiRtmpStreamController.cpp
  src/17live/multi-rtmp/ui/OneSevenLiveMultiRtmpConfigDialog.cpp
  src/17live/multi-rtmp/ui/OneSevenLiveMultiRtmpDock.cpp
  src/17live/multi-rtmp/ui/OneSevenLiveMultiRtmpListWidget.cpp
  src/17live/multi-rtmp/ui/OneSevenLiveMultiRtmpStreamItem.cpp
  src/17live/chat/OneSevenLiveChatWidget.cpp
  src/17live/OneSevenLiveConfigManager.cpp
  src/17live/core/DockOrchestrator.cpp
  src/17live/core/CoreRuntime.cpp
  src/17live/OneSevenLiveCoreManager.cpp
  src/17live/OneSevenLiveCustomEventDialog.cpp
  src/17live/OneSevenLiveHttpServer.cpp
  src/17live/OneSevenLiveLoginDialog.cpp
  src/17live/OneSevenLiveMenuManager.cpp
  src/17live/OneSevenLiveUpdateManager.cpp
  src/17live/rockzone/OneSevenLiveRockViewerItem.cpp
  src/17live/rockzone/OneSevenLiveRockZoneDock.cpp
  src/17live/rockzone/OneSevenLiveUserDialog.cpp
  src/17live/streaming/OneSevenLiveLoadRoomInfoWorker.cpp
  src/17live/streaming/OneSevenLiveStreamManager.cpp
  src/17live/streaming/OneSevenLiveStreamingDock.cpp
  src/17live/streamlist/OneSevenLiveStreamListDock.cpp
  src/17live/streamlist/OneSevenLiveStreamListItem.cpp
  src/17live/twitch/OneSevenLiveTwitchAuth.cpp
  src/17live/twitch/OneSevenLiveTwitchChatClient.cpp
  src/17live/twitch/OneSevenLiveTwitchClient.cpp
  src/17live/preview/OneSevenLivePreviewConfigLoader.cpp
  src/17live/preview/OneSevenLivePreviewDock.cpp
  src/17live/preview/OneSevenLivePreviewWidget.cpp
  src/17live/ui/cef/CefWidgetHost.cpp
  src/17live/ui/OneSevenLiveAuthDialog.cpp
  src/17live/ui/OneSevenLiveBrowserDock.cpp
  src/17live/ui/OneSevenLiveLineEditWithEye.cpp
  src/17live/ui/OneSevenLivePropertiesWidget.cpp
  src/17live/ui/OneSevenLivePropertyWidget.cpp
  src/17live/utility/Common.cpp
  src/17live/utility/CustomCalendarWidget.cpp
  src/17live/utility/DownloadWorker.cpp
  src/17live/utility/Meta.cpp
  src/17live/utility/NetworkDiagnostics.cpp
  src/17live/utility/RemoteTextThread.cpp
  src/17live/chat/OneSevenLiveChatMessageHandler.cpp
  src/17live/websocket/OneSevenLiveWebsocketClient.cpp
  src/17live/websocket/OneSevenLiveWebsocketServer.cpp
  src/17live/websocket/WebsocketUtils.cpp
  src/17live/youtube/OneSevenLiveYouTubeAuth.cpp
  src/17live/youtube/OneSevenLiveYouTubeChatClient.cpp
  src/17live/youtube/OneSevenLiveYouTubeClient.cpp
  src/17live/youtube/ui/OneSevenLiveYouTubeBroadcastDialog.cpp
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
# if(OS_WINDOWS)
# # Add Windows specific defines
# target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
# NOMINMAX
# WIN32_LEAN_AND_MEAN
# _CRT_SECURE_NO_WARNINGS
# _CRT_DECLARE_NONSTDC_NAMES=1 # Ensure stdio functions like setbuf are properly declared
# )

# # Disable treat warnings as errors to allow LNK4098 warnings
# target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
# /WX:NO # Don't treat linker warnings as errors
# /ignore:4098 # Ignore default library conflicts (mixed runtime libraries)
# /ignore:4286 # Ignore symbol already defined warnings
# )

# # obs-studio strategy: use static runtime to ensure CEF compatibility
# # Provide stdio function compatibility for MbedTLS through legacy_stdio_definitions.lib

# # Provide stdio function compatibility layer (resolve MbedTLS setbuf and other function requirements)

# # Comprehensive runtime library conflict resolution
# message(STATUS "Windows static runtime configuration with SChannel via WinHTTP")
# endif()
