#pragma once

#include "app/AppConfig.h"
#include "plc/S7Address.h"

#include <QByteArray>
#include <QString>

namespace cm8600 {

/**
 * 假 PLC：用 snap7 的服务端 API 起一个 S7 服务，把 V 区（DB1）和 M 区
 * 暴露给上位机读写。上位机那边完全感知不到对面是模拟的。
 *
 * 为什么值得做：真机联调时间有限，扫码触发、查询失败、加注完成、断网重连
 * 这些分支在办公室全部能验完，到现场只验真机时序就行。
 *
 * 线程：snap7 服务端在自己的线程里直接读写我们给的缓冲区，所以所有访问
 * 都要用 Srv_LockArea/Srv_UnlockArea 包起来，否则上位机可能读到半新半旧的
 * 140 字节结果块。
 */
class SimPlc
{
public:
    explicit SimPlc(const AppConfig &cfg);
    ~SimPlc();

    SimPlc(const SimPlc &)            = delete;
    SimPlc &operator=(const SimPlc &) = delete;

    bool start(const QString &ip, int port, QString *err);
    void stop();
    bool isRunning() const { return m_running; }

    QString endpoint() const { return m_endpoint; }

    // ---- 带区锁的访问 ----
    void       writeBlock(const S7Address &addr, const QByteArray &data);
    QByteArray readBlock (const S7Address &addr, int size) const;

    void    writeAscii(const S7Address &addr, int len, const QString &text);
    QString readAscii (const S7Address &addr, int len) const;

    void setBit(const S7Address &addr, bool value);
    bool getBit(const S7Address &addr) const;

    float  readReal (const S7Address &addr) const;
    qint16 readInt16(const S7Address &addr) const;

private:
    // 返回目标缓冲区与其中的偏移；地址非法或越界时返回 nullptr
    QByteArray       *bufferFor(const S7Address &addr, int size, int *offset);
    const QByteArray *bufferFor(const S7Address &addr, int size, int *offset) const;

    void lockFor(const S7Address &addr) const;
    void unlockFor(const S7Address &addr) const;

    const AppConfig &m_cfg;

    void      *m_server = nullptr;
    bool       m_running = false;
    QString    m_endpoint;

    QByteArray m_db;   ///< V 区，映射为 DB1
    QByteArray m_mk;   ///< M 区
};

} // namespace cm8600
