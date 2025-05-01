cmake_minimum_required(VERSION 3.28...3.30)

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

# 添加 json11 依赖
add_subdirectory(deps/json11)
# 添加 wrappers 依赖
add_subdirectory(deps/wrappers)

# 设置 json11 的头文件包含路径
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/17live
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/json11
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/wrappers
)

# 链接 json11, wrappers 库
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE json11 wrappers)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.cpp
                                             src/17live/SeventeenLiveCoreManager.cpp
                                             src/17live/SeventeenLiveConfigManager.cpp
                                             src/17live/api/SeventeenLiveModels.cpp
                                             src/17live/api/SeventeenLiveApiWrappers.cpp
                                             src/17live/SeventeenLiveMenuManager.cpp
                                             src/17live/SeventeenLiveLoginDialog.cpp
                                             src/17live/SeventeenLiveStreamingDock.cpp
                                             src/17live/utility/RemoteTextThread.cpp
                                             resources.qrc)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
