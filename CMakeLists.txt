cmake_minimum_required(VERSION 3.24)
project(ExoTVModern VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets Multimedia)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

qt_add_executable(ExoTVModern WIN32
    src/main.cpp
    src/MainWindow.h src/MainWindow.cpp
    src/VideoEngine.h src/VideoEngine.cpp
    src/AudioOutput.h src/AudioOutput.cpp
    src/VideoWidget.h src/VideoWidget.cpp
)

target_link_libraries(ExoTVModern PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    PkgConfig::FFMPEG
)

if(MSVC)
    target_compile_options(ExoTVModern PRIVATE /W4 /permissive-)
endif()

install(TARGETS ExoTVModern RUNTIME DESTINATION .)
