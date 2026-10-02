cmake_minimum_required(VERSION 3.16)

project(CM8600Host
    VERSION 0.1.0
    DESCRIPTION "CM8600 冷媒加注机上位机中间件"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Qt：不绑定具体版本，Qt6 / Qt5.15 都能编。
# 用 add_executable 而不是 qt_add_executable，后者是 Qt6 专有的。
# ---------------------------------------------------------------------------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Network Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network Sql)

if(QT_VERSION_MAJOR EQUAL 5 AND Qt5_VERSION VERSION_LESS 5.15)
    message(FATAL_ERROR "Qt5 最低需要 5.15（用到了 QNetworkRequest::setTransferTimeout）")
endif()

message(STATUS "CM8600: 使用 Qt${QT_VERSION_MAJOR} (${Qt${QT_VERSION_MAJOR}_VERSION})")

# ---------------------------------------------------------------------------
# snap7：源码随工程一起编译，产出独立的 snap7 动态库
# ---------------------------------------------------------------------------
add_subdirectory(third_party/snap7)

# ---------------------------------------------------------------------------
# cm8600_core —— 不含界面的核心库，GUI 与自测程序共用
# ---------------------------------------------------------------------------
add_library(cm8600_core STATIC
    src/app/AppConfig.h            src/app/AppConfig.cpp
    src/app/Logger.h               src/app/Logger.cpp
    src/plc/S7Address.h            src/plc/S7Address.cpp
    src/plc/S7Codec.h              src/plc/S7Codec.cpp
    src/plc/Snap7Lib.h             src/plc/Snap7Lib.cpp
    src/plc/S7Client.h             src/plc/S7Client.cpp
    src/core/Records.h             src/core/Records.cpp
    src/core/ChannelWorker.h       src/core/ChannelWorker.cpp
    src/storage/Database.h         src/storage/Database.cpp
    src/datasource/IParamSource.h
    src/datasource/MockParamSource.h  src/datasource/MockParamSource.cpp
    src/upload/UploadService.h     src/upload/UploadService.cpp
)

target_include_directories(cm8600_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(cm8600_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
)

# ---------------------------------------------------------------------------
# CM8600Host —— 主程序（GUI）
# ---------------------------------------------------------------------------
add_executable(CM8600Host WIN32
    src/main.cpp
    src/ui/MainWindow.h            src/ui/MainWindow.cpp
    src/ui/ChannelPanel.h          src/ui/ChannelPanel.cpp
)
target_link_libraries(CM8600Host PRIVATE cm8600_core Qt${QT_VERSION_MAJOR}::Widgets)

# ---------------------------------------------------------------------------
# cm8600_sim —— 假 PLC 模拟器，用 snap7 的服务端 API 起 S7 服务
# 让上位机不接设备也能跑通扫码→下发→加注完成→入库→上传整条链路
# ---------------------------------------------------------------------------
add_executable(cm8600_sim WIN32
    sim/sim_main.cpp
    sim/SimPlc.h                   sim/SimPlc.cpp
    sim/SimPanel.h                 sim/SimPanel.cpp
)
target_include_directories(cm8600_sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cm8600_sim PRIVATE cm8600_core Qt${QT_VERSION_MAJOR}::Widgets)

# ---------------------------------------------------------------------------
# cm8600_selftest —— 控制台自测，不接 PLC 也能验证编解码
# ---------------------------------------------------------------------------
add_executable(cm8600_selftest
    tests/selftest_main.cpp
)
target_link_libraries(cm8600_selftest PRIVATE cm8600_core)

# ---------------------------------------------------------------------------
# 编译选项（只作用于我们自己的代码，第三方源码另有一套）
# ---------------------------------------------------------------------------
foreach(tgt cm8600_core CM8600Host cm8600_sim cm8600_selftest)
    if(MSVC)
        # 源码是 UTF-8 且含中文字面量，必须显式告诉 MSVC，否则乱码
        target_compile_options(${tgt} PRIVATE /utf-8 /W4 /permissive-)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
endforeach()

# snap7 是运行时用 QLibrary 加载的，不参与链接，但必须先于主程序产出
if(TARGET snap7)
    add_dependencies(CM8600Host snap7)
    add_dependencies(cm8600_sim snap7)
endif()

# ---------------------------------------------------------------------------
# 把 config/ 拷到输出目录，方便直接运行
# ---------------------------------------------------------------------------
add_custom_command(TARGET CM8600Host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/config
            $<TARGET_FILE_DIR:CM8600Host>/config
    COMMENT "复制 config/ 到输出目录")
