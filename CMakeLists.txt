cmake_minimum_required(VERSION 3.19)

project(MusicPlayer
    VERSION 1.0.0
    DESCRIPTION "A Qt-based music player with subtitle support"
    LANGUAGES CXX
)

# ── C++ Standard ──
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 输出目录：exe / DLL 统一放到项目根目录的 bin/ 文件夹 ──
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

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
    src/playlistmanager.cpp
    src/playtrack.cpp
    src/song.cpp
    src/lyricsdecoder.cpp
    src/library.cpp
    src/style.cpp
    src/formatconverter.cpp
    src/conversiondialog.cpp
    src/visualizer.cpp
    src/fleeteffects.cpp
)

set(HEADERS
    src/mainwindow.h
    src/visualizer.h
    src/fleeteffects.h
    src/player.h
    src/playlist.h
    src/playlistmanager.h
    src/playtrack.h
    src/song.h
    src/lyricsdecoder.h
    src/library.h
    src/style.h
    src/formatconverter.h
    src/conversiondialog.h
)

# ── Executable ──
add_executable(${PROJECT_NAME} WIN32
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

# ── FFmpeg Path (external process) ──
# ffmpeg.exe/ffprobe.exe 已位于 bin/ 目录，exe 也在 bin/ 中，直接使用当前路径
set(FFMPEG_BIN_DIR "${CMAKE_SOURCE_DIR}/bin")
target_compile_definitions(${PROJECT_NAME} PRIVATE
    FFMPEG_BIN_DIR="${FFMPEG_BIN_DIR}"
)

# ── 复制资源文件（图标）到输出目录 ──
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/icons"
        "${CMAKE_SOURCE_DIR}/bin/assets/icons"
    COMMENT "▶ 正在复制图标资源到 bin/assets/icons/ …"
)

# ── 部署 Qt DLL（构建后自动拷贝到 exe 所在目录 bin/） ──
find_package(Qt6 REQUIRED COMPONENTS Core)
get_target_property(QT_DEPLOY_TOOL Qt6::windeployqt LOCATION)
if(NOT QT_DEPLOY_TOOL)
    # 回退到路径查找
    get_filename_component(QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
    find_program(QT_DEPLOY_TOOL windeployqt HINTS "${QT_BIN_DIR}" NO_DEFAULT_PATH)
endif()
if(QT_DEPLOY_TOOL)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${QT_DEPLOY_TOOL}" "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "▶ 正在部署 Qt DLL 到 bin/ …"
    )
    message(STATUS "windeployqt: ${QT_DEPLOY_TOOL}")
else()
    message(WARNING "未找到 windeployqt, 需手动部署 Qt DLL")
endif()

# ── Compiler Warnings ──
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
