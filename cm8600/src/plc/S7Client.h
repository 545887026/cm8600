#pragma once

#include "plc/S7Address.h"

#include <QByteArray>
#include <QString>

namespace cm8600 {

/**
 * 面向本工程的 S7 客户端薄封装。
 *
 * 只做三件事：
 *   1) 把 S7Address 翻译成 snap7 的 (区号, DB号, 偏移)。
 *      S7-200 SMART 的 V 区没有独立区号，走 DB1；M 区有独立区号 0x83。
 *   2) 位读写用 snap7 的 WLBit，直接按位寻址，避免“读整字节-改位-写回”
 *      期间 PLC 改了同字节其它位而被覆盖。
 *   3) 出错时给出中文可读信息。
 *
 * 非线程安全：每个通道在自己的线程里持有一个实例。
 */
class S7Client
{
public:
    explicit S7Client(int vAreaDbNumber = 1);
    ~S7Client();

    S7Client(const S7Client &)            = delete;
    S7Client &operator=(const S7Client &) = delete;

    /**
     * 连接。
     * @param port     目标端口，真机固定 102；脱机时模拟器可以换端口，
     *                 这样两台“PLC”能同时跑在 127.0.0.1 上
     * @param timeoutMs 同时作用于发送/接收超时
     */
    bool connectTo(const QString &ip, int rack, int slot, int port, int timeoutMs,
                   QString *err = nullptr);
    void disconnect();

    /// 本地状态；只反映 snap7 自己的连接标志，不代表刚才的读写一定成功
    bool isConnected() const;

    /// 连续读若干字节。addr 必须是字节类地址（VB/VW/VD/MB），不能是位地址
    bool readBytes(const S7Address &addr, int size, QByteArray *out, QString *err = nullptr);

    /// 连续写若干字节
    bool writeBytes(const S7Address &addr, const QByteArray &data, QString *err = nullptr);

    /// 读单个位，addr 必须是位地址（V9741.1 / M25.7）
    bool readBit(const S7Address &addr, bool *value, QString *err = nullptr);

    /// 写单个位
    bool writeBit(const S7Address &addr, bool value, QString *err = nullptr);

    QString lastError() const { return m_lastError; }

    /// 最近一次操作的 snap7 原始错误码，0 表示成功
    int lastCode() const { return m_lastCode; }

private:
    bool ensureReady(QString *err);
    bool mapArea(const S7Address &addr, int *area, int *dbNumber, QString *err);
    bool fail(int code, const QString &what, QString *err);
    bool succeed();

    void   *m_client = nullptr;   ///< snap7 的 S7Object
    int     m_vDb    = 1;         ///< V 区映射到的 DB 号，S7-200 SMART 固定 1
    bool    m_connected = false;
    QString m_lastError;
    int     m_lastCode = 0;
};

} // namespace cm8600
