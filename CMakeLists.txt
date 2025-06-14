cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

# 设置Windows平台下的运行时库为静态链接(/MT)，以匹配CEF库
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

# 添加CEF支持
# CEF_ROOT_DIR 应该已经在 defaults.cmake -> buildspec.cmake 中被设置
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
if(CEF_ROOT_DIR)
  set(CEF_ROOT "${CEF_ROOT_DIR}")
  message(STATUS "Using CEF_ROOT: ${CEF_ROOT}")
else()
  message(FATAL_ERROR "CEF_ROOT_DIR not set. Dependencies may not have been processed correctly.")
endif()
find_package(CEF REQUIRED)

# 添加 json11 依赖
add_subdirectory(deps/json11)
# 添加 wrappers 依赖
add_subdirectory(deps/wrappers)

# 设置头文件包含路径
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/17live
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/json11
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/wrappers
    ${CEF_ROOT_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/src # 用于找到生成的plugin-support.c
)

# 链接 json11, wrappers 库
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE 
  json11 
  wrappers
  CEF::Library
  CEF::Wrapper
  Threads::Threads # For cpp-httplib
)

# Add cpp-httplib include directory (though it might already be covered by 'deps')
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/cpp-httplib
)

# If OpenSSL support is needed for httplib, you'll need to find and link OpenSSL
# option(WITH_OPENSSL "Enable OpenSSL for cpp-httplib" ON)
# if(WITH_OPENSSL)
#   find_package(OpenSSL REQUIRED)
#   target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
#   target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
# endif()

# 配置plugin-support.c文件
configure_file(src/plugin-support.c.in src/plugin-support.c @ONLY)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.cpp
                                             src/plugin-support.c
                                             src/17live/SeventeenLiveCoreManager.cpp
                                             src/17live/SeventeenLiveConfigManager.cpp
                                             src/17live/api/SeventeenLiveModels.cpp
                                             src/17live/utility/RemoteTextThread.cpp
                                             src/17live/utility/Common.cpp
                                             src/17live/utility/Meta.cpp
                                             src/17live/api/SeventeenLiveApiWrappers.cpp
                                             src/17live/cef-view.cpp
                                             src/17live/SeventeenLiveMenuManager.cpp
                                             src/17live/SeventeenLiveLoginDialog.cpp
                                             src/17live/SeventeenLiveStreamingDock.cpp
                                             src/17live/SeventeenLiveStreamListDock.cpp
                                             src/17live/SeventeenLiveHttpServer.cpp
                                             resources.qrc)

if (MSVC)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /wd4996)
else()
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wno-deprecated-builtins)
endif()


set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Windows平台特定的CEF配置
if(OS_WINDOWS)
  # 添加Windows平台特定的编译定义
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
  )
endif()
