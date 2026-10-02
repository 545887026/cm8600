# ---------------------------------------------------------------------------
# snap7 —— 西门子 S7 通讯库，源码随本工程一起编译
#
# 为什么放源码而不是预编译 DLL：
#   官方只提供 MinGW 编出来的 win64 dll，换 Qt 版本 / 换 MSVC / 换 32 位就得重新
#   找对应的二进制。源码进来以后跟着主工程的工具链一起重编，一劳永逸。
#   snap7 本身不依赖 Qt，所以 Qt5 / Qt6 都不影响它。
#
# 授权：snap7 是 LGPLv3。这里编成【独立的动态库】，主程序用 QLibrary 运行时加载，
#       属于动态链接，不会把 LGPL 传染给主程序。不要改成 STATIC。
#
# 源码怎么放：
#   把 snap7-full-x.y.z 里的 src 目录整个拷到本目录下，形成
#       third_party/snap7/src/core/*.cpp|*.h
#       third_party/snap7/src/sys/*.cpp|*.h
#       third_party/snap7/src/lib/*.cpp|*.h
#   没有源码时本文件会跳过（只打印提示），主程序照样能编译，
#   只是运行时需要自己准备一个 snap7.dll 放到 exe 旁边。
# ---------------------------------------------------------------------------

# 源码位置。可以用 -DSNAP7_SRC_DIR=... 直接指定；否则按下面几种常见落地方式去找。
# 官方那个 7z 解压出来会套两层同名目录（snap7-full-x.y.z/snap7-full-x.y.z/src），
# 所以候选里带了 */src 这一档，解压完原地放着就行，不用搬。
set(SNAP7_SRC_DIR "" CACHE PATH "snap7 源码 src 目录（含 core/sys/lib）")

if(NOT SNAP7_SRC_DIR)
    file(GLOB _snap7_candidates LIST_DIRECTORIES true
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../snap7-full-*/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../snap7-full-*/*/src
    )
    foreach(_cand IN LISTS _snap7_candidates)
        if(EXISTS ${_cand}/core/s7_client.cpp AND EXISTS ${_cand}/lib/snap7_libmain.cpp)
            set(SNAP7_SRC_DIR ${_cand})
            break()
        endif()
    endforeach()
endif()

if(SNAP7_SRC_DIR)
    file(GLOB SNAP7_SOURCES CONFIGURE_DEPENDS
        ${SNAP7_SRC_DIR}/core/*.cpp
        ${SNAP7_SRC_DIR}/sys/*.cpp
        ${SNAP7_SRC_DIR}/lib/*.cpp
    )
endif()

if(NOT SNAP7_SOURCES)
    message(STATUS
        "CM8600: 未找到 snap7 源码，跳过 snap7 目标。主程序仍可编译运行，\n"
        "        但 PLC 通讯需要自备 snap7.dll 放在 exe 同目录。\n"
        "        放源码的办法见 third_party/snap7/README.md。")
    return()
endif()

message(STATUS "CM8600: snap7 源码目录 = ${SNAP7_SRC_DIR}")

list(LENGTH SNAP7_SOURCES SNAP7_SOURCE_COUNT)
message(STATUS "CM8600: 编译 snap7 源码，共 ${SNAP7_SOURCE_COUNT} 个文件")

add_library(snap7 SHARED ${SNAP7_SOURCES})

target_include_directories(snap7 PRIVATE
    ${SNAP7_SRC_DIR}/core
    ${SNAP7_SRC_DIR}/sys
    ${SNAP7_SRC_DIR}/lib
)

# 产出就叫 snap7.dll / libsnap7.so，跟官方一致，Snap7Lib 按这个名字找
set_target_properties(snap7 PROPERTIES
    OUTPUT_NAME snap7
    PREFIX ""
    # MSVC 默认不导出任何符号；snap7 源码是按 MinGW“全导出”的习惯写的，
    # 这里让 CMake 自动生成导出表，效果等同。注意：x86 下 __stdcall 会带
    # @N 修饰导致 QLibrary 解析不到，本工程按 x64 编译，没这个问题。
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

if(WIN32)
    target_compile_definitions(snap7 PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
    target_link_libraries(snap7 PRIVATE ws2_32 winmm)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(snap7 PRIVATE Threads::Threads)
endif()

# 第三方代码不跟着主工程的 /W4 /permissive- 走，否则满屏警告淹掉我们自己的
if(MSVC)
    target_compile_options(snap7 PRIVATE /w)
else()
    target_compile_options(snap7 PRIVATE -w)
endif()
