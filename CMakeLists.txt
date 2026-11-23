cmake_minimum_required(VERSION 3.24)
project(ExoTVModern VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets Multimedia)
find_package(unofficial-ffmpeg CONFIG REQUIRED)
qt_add_executable(ExoTVModern WIN32
    src/main.cpp
    src/MainWindow.h src/MainWindow.cpp
    src/VideoEngine.h src/VideoEngine.cpp
    src/AudioOutput.h src/AudioOutput.cpp
    src/VideoWidget.h src/VideoWidget.cpp
)
target_link_libraries(ExoTVModern PRIVATE
    Qt6::Widgets Qt6::Multimedia
    unofficial::ffmpeg::avcodec
    unofficial::ffmpeg::avformat
    unofficial::ffmpeg::avutil
    unofficial::ffmpeg::swscale
    unofficial::ffmpeg::swresample
)
if(MSVC)
  target_compile_options(ExoTVModern PRIVATE /W4 /permissive-)
endif()
install(TARGETS ExoTVModern RUNTIME DESTINATION .)
