cmake_minimum_required(VERSION 3.28...3.30)

if (APPLE) 
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

find_package(Qt6 REQUIRED Core Widgets)

add_library(wrappers STATIC)

# 设置源文件
target_sources(wrappers 
    PRIVATE
        qt-wrappers.cpp
)

# 设置头文件
target_sources(wrappers
    PUBLIC
        qt-wrappers.hpp
)

# 设置包含目录
target_include_directories(wrappers
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 设置链接库
target_link_libraries(wrappers
    PUBLIC
        Qt::Core
        Qt::Widgets
        OBS::libobs
)

# 设置编译选项
target_compile_features(wrappers PUBLIC cxx_std_17)

set_target_properties(wrappers PROPERTIES
  AUTOMOC ON
  AUTORCC ON
  AUTOUIC ON
)

# 如果需要，设置导出宏
if(BUILD_SHARED_LIBS)
    target_compile_definitions(wrappers PRIVATE WRAPPERS_EXPORTS)
endif()
