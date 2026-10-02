# snap7（第三方）

西门子 S7 通讯库。本工程**不放预编译 DLL，只编源码**。

## 为什么

官方只发 MinGW 编出来的 win64 DLL。换 Qt 版本、换 MSVC 版本、换 32/64 位，
都得重新去找对应的二进制。源码放进工程后，跟着主工程当前的工具链一起重编，
换 kit 不用管它。snap7 自身不依赖 Qt，所以 Qt5 / Qt6 对它没有影响。

## 授权

snap7 是 **LGPLv3**。

本工程把它编成**独立的动态库**（`add_library(snap7 SHARED ...)`），
主程序通过 `QLibrary` 在运行时加载（见 `src/plc/Snap7Lib.cpp`），属于动态链接，
不会把 LGPL 传染到主程序。

**不要改成 `STATIC`**——静态链接会触发 LGPL 中"必须提供可重新链接的目标文件"的要求。

## 怎么放源码

1. 下载 `snap7-full-<版本>.7z`
   （官网 https://sourceforge.net/projects/snap7/files/ ，
    或 GitHub 官方仓库 https://github.com/davenardella/snap7 ）
   Windows 11 的资源管理器能直接解压 .7z；命令行可以用系统自带的
   `C:\Windows\System32\tar.exe -xf xxx.7z`（它是 bsdtar，能读 7z）。
2. **解压到 `third_party/` 下原地放着即可，不用搬文件**。
   `CMakeLists.txt` 会按这几个位置依次找（判据是 `core/s7_client.cpp` 与
   `lib/snap7_libmain.cpp` 同时存在）：

   - `third_party/snap7/src`
   - `third_party/snap7-full-*/src`
   - `third_party/snap7-full-*/*/src` ← 官方 7z 会套两层同名目录，落在这档

   实在不想让它猜，也可以显式指定：
   `cmake -DSNAP7_SRC_DIR=D:/.../snap7-full-1.4.2/snap7-full-1.4.2/src ...`

   如果选择手工归位，把 `src` 拷到本目录下形成：

```
third_party/snap7/
├─ CMakeLists.txt
├─ README.md
└─ src/
   ├─ core/   s7_client.cpp  s7_isotcp.cpp  s7_micro_client.cpp
   │          s7_partner.cpp s7_peer.cpp    s7_server.cpp
   │          s7_text.cpp    s7_types.cpp   （及对应的 .h）
   ├─ sys/    snap_msgsock.cpp  snap_sysutils.cpp
   │          snap_tcpsrvr.cpp  snap_threads.cpp  （及对应的 .h）
   └─ lib/    snap7_libmain.cpp  snap7.h
```

3. 在 Qt Creator 里重新执行一次 CMake。会多出 `snap7` 目标，
   产物 `snap7.dll` 直接落在 `bin/`，与 `CM8600Host.exe` 同目录。

源码不在时 `CMakeLists.txt` 会跳过该目标并打印提示，主程序照样能编译，
只是运行时需要自备一个 `snap7.dll`。

## 几个已知点

- **MSVC 默认不导出符号**，而 snap7 源码是按 MinGW「全导出」的习惯写的。
  CMake 里用 `WINDOWS_EXPORT_ALL_SYMBOLS ON` 自动生成导出表解决。
- **只支持 x64**。x86 下 `__stdcall` 会给导出名加 `@N` 后缀，
  `QLibrary::resolve("Cli_Create")` 就找不到了。本工程按 x64 编译。
- 第三方代码用 `/w`（MSVC）或 `-w`（GCC/Clang）关掉警告，
  免得淹没主工程 `/W4` 的输出。

## 如果要升级 snap7 版本

直接替换 `src/` 目录即可，`CMakeLists.txt` 用 glob 收集源文件，不用改。
若对源码打过补丁，请把补丁记录在本文件下方，别让它悄无声息地丢掉。

### 已打的补丁

（暂无）
