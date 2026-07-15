cmake_minimum_required(VERSION 3.19)

project(MusicPlayer
    VERSION 1.0.0
    DESCRIPTION "A Qt-based music player with subtitle support"
    LANGUAGES CXX
)

# ── C++ Standard ──
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Qt Auto-Processing ──
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Sources ──
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/player.cpp
    src/playlist.cpp
    src/playtrack.cpp
    src/song.cpp
    src/lyricsdecoder.cpp
    src/library.cpp
)

set(HEADERS
    src/mainwindow.h
    src/player.h
    src/playlist.h
    src/playtrack.h
    src/song.h
    src/lyricsdecoder.h
    src/library.h
)

# ── Executable ──
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

# ── Include Paths ──
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── Find Qt ──
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets REQUIRED)

# ── Link Qt Libraries ──
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets
)

# ── Compiler Warnings ──
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
