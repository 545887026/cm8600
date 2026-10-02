#include "sim/SimPlc.h"

#include "plc/S7Codec.h"
#include "plc/Snap7Lib.h"

namespace cm8600 {

namespace {

// S7-200 SMART 的 V 区能到几千字节，这里开得比用到的最大地址（VB9779）宽裕些
constexpr int kDbSize = 10240;
// M 区只有 M0.0~M31.7，开 64 字节足够，多给一点不吃亏
constexpr int kMkSize = 64;

constexpr quint16 kDbIndex = 1;   ///< V 区映射的 DB 号
constexpr quint16 kMkIndex = 0;

int srvAreaOf(S7Address::Area area)
{
    return area == S7Address::Area::M ? Snap7Lib::SrvAreaMK : Snap7Lib::SrvAreaDB;
}

quint16 srvIndexOf(S7Address::Area area)
{
    return area == S7Address::Area::M ? kMkIndex : kDbIndex;
}

} // namespace

SimPlc::SimPlc(const AppConfig &cfg)
    : m_cfg(cfg)
    , m_db(kDbSize, '\0')
    , m_mk(kMkSize, '\0')
{
}

SimPlc::~SimPlc()
{
    stop();

    auto &lib = Snap7Lib::instance();
    if (m_server && lib.isLoaded() && lib.Srv_Destroy) {
        lib.Srv_Destroy(&m_server);
        m_server = nullptr;
    }
}

bool SimPlc::start(const QString &ip, int port, QString *err)
{
    auto &lib = Snap7Lib::instance();

    if (!lib.isLoaded() || !lib.Srv_Create || !lib.Srv_RegisterArea || !lib.Srv_StartTo) {
        if (err)
            *err = QStringLiteral("snap7 未加载或缺少服务端接口，模拟器无法启动");
        return false;
    }

    if (m_running)
        return true;

    if (!m_server) {
        m_server = lib.Srv_Create();
        if (!m_server) {
            if (err)
                *err = QStringLiteral("创建 snap7 服务端失败");
            return false;
        }
    }

    // 非 102 端口用于让两台“PLC”同时跑在 127.0.0.1 上
    if (port > 0 && port != 102) {
        if (!lib.Srv_SetParam) {
            if (err)
                *err = QStringLiteral("当前 snap7 不支持 Srv_SetParam，无法改监听端口");
            return false;
        }
        quint16 p = static_cast<quint16>(port);
        const int code = lib.Srv_SetParam(m_server, Snap7Lib::ParamLocalPort, &p);
        if (code != 0) {
            if (err)
                *err = QStringLiteral("设置监听端口 %1 失败：%2").arg(port).arg(lib.errorText(code));
            return false;
        }
    }

    int code = lib.Srv_RegisterArea(m_server, Snap7Lib::SrvAreaDB, kDbIndex,
                                    m_db.data(), m_db.size());
    if (code != 0) {
        if (err)
            *err = QStringLiteral("注册 DB%1（V 区）失败：%2").arg(kDbIndex).arg(lib.errorText(code));
        return false;
    }

    code = lib.Srv_RegisterArea(m_server, Snap7Lib::SrvAreaMK, kMkIndex,
                                m_mk.data(), m_mk.size());
    if (code != 0) {
        if (err)
            *err = QStringLiteral("注册 M 区失败：%1").arg(lib.errorText(code));
        return false;
    }

    const QByteArray addr = ip.toLatin1();
    code = lib.Srv_StartTo(m_server, addr.constData());
    if (code != 0) {
        if (err) {
            *err = QStringLiteral("在 %1:%2 启动失败：%3。端口被占用时换一个端口试试")
                       .arg(ip).arg(port).arg(lib.errorText(code));
        }
        return false;
    }

    m_running  = true;
    m_endpoint = QStringLiteral("%1:%2").arg(ip).arg(port);
    return true;
}

void SimPlc::stop()
{
    auto &lib = Snap7Lib::instance();
    if (m_running && m_server && lib.Srv_Stop)
        lib.Srv_Stop(m_server);
    m_running = false;
}

// ------------------------------------------------------------------ 缓冲 ---

QByteArray *SimPlc::bufferFor(const S7Address &addr, int size, int *offset)
{
    return const_cast<QByteArray *>(
        static_cast<const SimPlc *>(this)->bufferFor(addr, size, offset));
}

const QByteArray *SimPlc::bufferFor(const S7Address &addr, int size, int *offset) const
{
    if (!addr.isValid() || size <= 0)
        return nullptr;

    const QByteArray *buf = (addr.area == S7Address::Area::M) ? &m_mk : &m_db;

    if (addr.byteOffset < 0 || addr.byteOffset + size > buf->size())
        return nullptr;

    if (offset)
        *offset = addr.byteOffset;
    return buf;
}

void SimPlc::lockFor(const S7Address &addr) const
{
    auto &lib = Snap7Lib::instance();
    if (m_running && m_server && lib.Srv_LockArea)
        lib.Srv_LockArea(m_server, srvAreaOf(addr.area), srvIndexOf(addr.area));
}

void SimPlc::unlockFor(const S7Address &addr) const
{
    auto &lib = Snap7Lib::instance();
    if (m_running && m_server && lib.Srv_UnlockArea)
        lib.Srv_UnlockArea(m_server, srvAreaOf(addr.area), srvIndexOf(addr.area));
}

// ------------------------------------------------------------------ 访问 ---

void SimPlc::writeBlock(const S7Address &addr, const QByteArray &data)
{
    int off = 0;
    QByteArray *buf = bufferFor(addr, data.size(), &off);
    if (!buf)
        return;

    lockFor(addr);
    buf->replace(off, data.size(), data);
    unlockFor(addr);
}

QByteArray SimPlc::readBlock(const S7Address &addr, int size) const
{
    int off = 0;
    const QByteArray *buf = bufferFor(addr, size, &off);
    if (!buf)
        return {};

    lockFor(addr);
    const QByteArray out = buf->mid(off, size);
    unlockFor(addr);
    return out;
}

void SimPlc::writeAscii(const S7Address &addr, int len, const QString &text)
{
    // 和真机一致：定长、左侧补空格
    writeBlock(addr, S7Codec::makeAscii(text, len));
}

QString SimPlc::readAscii(const S7Address &addr, int len) const
{
    const QByteArray raw = readBlock(addr, len);
    return S7Codec::readAscii(raw, 0, raw.size());
}

void SimPlc::setBit(const S7Address &addr, bool value)
{
    if (!addr.isBit())
        return;

    int off = 0;
    QByteArray *buf = bufferFor(addr, 1, &off);
    if (!buf)
        return;

    lockFor(addr);
    S7Codec::setBit(*buf, off, addr.bitOffset, value);
    unlockFor(addr);
}

bool SimPlc::getBit(const S7Address &addr) const
{
    if (!addr.isBit())
        return false;

    int off = 0;
    const QByteArray *buf = bufferFor(addr, 1, &off);
    if (!buf)
        return false;

    lockFor(addr);
    const bool v = S7Codec::getBit(*buf, off, addr.bitOffset);
    unlockFor(addr);
    return v;
}

float SimPlc::readReal(const S7Address &addr) const
{
    const QByteArray raw = readBlock(addr, 4);
    return raw.size() == 4 ? S7Codec::getReal(raw, 0) : 0.0f;
}

qint16 SimPlc::readInt16(const S7Address &addr) const
{
    const QByteArray raw = readBlock(addr, 2);
    return raw.size() == 2 ? S7Codec::getInt16(raw, 0) : 0;
}

} // namespace cm8600
