#pragma once

#include <QString>

class QLibrary;

namespace cm8600 {

/**
 * snap7.dll 的运行时加载器。
 *
 * 故意不在编译期链接 snap7：
 *   - MSVC 下不必为 MinGW 编出来的 dll 生成导入库；
 *   - 编译不依赖第三方二进制，换机器只要把 dll 丢进程序目录；
 *   - 真到了 snap7 搞不定的场合（比如 M 区读不了），可以把 S7Client 的底层
 *     换成自研实现，上层一行不用改。
 *
 * 只导出本工程用得到的那几个 C 接口。
 */
class Snap7Lib
{
public:
    // snap7 的句柄类型
    using S7Object = void *;

    // ---- 区号（客户端 API 用） ----
    enum Area : int {
        AreaPE = 0x81,   ///< 过程输入 I
        AreaPA = 0x82,   ///< 过程输出 Q
        AreaMK = 0x83,   ///< 位存储器 M
        AreaDB = 0x84,   ///< 数据块 DB（S7-200 SMART 的 V 区即 DB1）
        AreaCT = 0x1C,
        AreaTM = 0x1D,
    };

    // ---- 数据宽度 ----
    enum WordLen : int {
        WLBit   = 0x01,
        WLByte  = 0x02,
        WLChar  = 0x03,
        WLWord  = 0x04,
        WLInt   = 0x05,
        WLDWord = 0x06,
        WLDInt  = 0x07,
        WLReal  = 0x08,
    };

    // ---- Cli_SetParam / Srv_SetParam 参数号 ----
    enum Param : int {
        ParamLocalPort   = 1,    ///< u16，服务端监听端口
        ParamRemotePort  = 2,    ///< u16，客户端连接的目标端口，默认 102
        ParamPingTimeout = 3,
        ParamSendTimeout = 4,
        ParamRecvTimeout = 5,
        ParamPduRequest  = 10,
    };

    // ---- 服务端区号（模拟器用，与客户端区号不是一套） ----
    enum SrvArea : int {
        SrvAreaPE = 0,
        SrvAreaPA = 1,
        SrvAreaMK = 2,
        SrvAreaCT = 3,
        SrvAreaTM = 4,
        SrvAreaDB = 5,
    };

    static Snap7Lib &instance();

    /**
     * 加载 snap7.dll。
     * @param preferredPath 配置里指定的路径，留空则依次尝试
     *                      程序目录/snap7.dll、程序目录/lib/snap7.dll、系统搜索路径。
     * @return 成功与否；失败时 err 写明原因（含已尝试过的路径）
     */
    bool load(const QString &preferredPath, QString *err);

    bool    isLoaded() const { return m_loaded; }
    QString libraryPath() const { return m_path; }

    /// 把 snap7 错误码转成可读文本；未加载时返回错误码本身
    QString errorText(int code) const;

    // ---- 客户端 ----
    S7Object (*Cli_Create)()                                                        = nullptr;
    void     (*Cli_Destroy)(S7Object *client)                                       = nullptr;
    int      (*Cli_ConnectTo)(S7Object client, const char *addr, int rack, int slot) = nullptr;
    int      (*Cli_Disconnect)(S7Object client)                                     = nullptr;
    int      (*Cli_GetConnected)(S7Object client, int *connected)                   = nullptr;
    int      (*Cli_SetParam)(S7Object client, int paramNumber, void *value)         = nullptr;
    int      (*Cli_ReadArea)(S7Object client, int area, int dbNumber, int start,
                             int amount, int wordLen, void *usrData)                = nullptr;
    int      (*Cli_WriteArea)(S7Object client, int area, int dbNumber, int start,
                              int amount, int wordLen, void *usrData)               = nullptr;
    int      (*Cli_ErrorText)(int error, char *text, int textLen)                   = nullptr;

    // ---- 服务端（模拟器） ----
    S7Object (*Srv_Create)()                                                        = nullptr;
    void     (*Srv_Destroy)(S7Object *server)                                       = nullptr;
    int      (*Srv_SetParam)(S7Object server, int paramNumber, void *value)         = nullptr;
    int      (*Srv_RegisterArea)(S7Object server, int areaCode, quint16 index,
                                 void *usrData, int size)                           = nullptr;
    int      (*Srv_Start)(S7Object server)                                          = nullptr;
    int      (*Srv_StartTo)(S7Object server, const char *addr)                      = nullptr;
    int      (*Srv_Stop)(S7Object server)                                           = nullptr;
    int      (*Srv_LockArea)(S7Object server, int areaCode, quint16 index)          = nullptr;
    int      (*Srv_UnlockArea)(S7Object server, int areaCode, quint16 index)        = nullptr;
    int      (*Srv_ErrorText)(int error, char *text, int textLen)                   = nullptr;

private:
    Snap7Lib() = default;
    ~Snap7Lib();
    Snap7Lib(const Snap7Lib &)            = delete;
    Snap7Lib &operator=(const Snap7Lib &) = delete;

    bool resolveAll(QString *err);

    QLibrary *m_lib    = nullptr;
    bool      m_loaded = false;
    QString   m_path;
};

} // namespace cm8600
