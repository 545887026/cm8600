#include "plc/S7Client.h"

#include "plc/Snap7Lib.h"

namespace cm8600 {

S7Client::S7Client(int vAreaDbNumber)
    : m_vDb(vAreaDbNumber)
{
}

S7Client::~S7Client()
{
    disconnect();

    auto &lib = Snap7Lib::instance();
    if (m_client && lib.isLoaded() && lib.Cli_Destroy) {
        lib.Cli_Destroy(&m_client);
        m_client = nullptr;
    }
}

bool S7Client::ensureReady(QString *err)
{
    auto &lib = Snap7Lib::instance();
    if (!lib.isLoaded()) {
        m_lastError = QStringLiteral("snap7.dll 未加载");
        if (err) *err = m_lastError;
        return false;
    }

    if (!m_client) {
        m_client = lib.Cli_Create();
        if (!m_client) {
            m_lastError = QStringLiteral("创建 snap7 客户端失败");
            if (err) *err = m_lastError;
            return false;
        }
    }
    return true;
}

bool S7Client::fail(int code, const QString &what, QString *err)
{
    m_lastCode  = code;
    m_lastError = QStringLiteral("%1 失败：%2").arg(what, Snap7Lib::instance().errorText(code));
    if (err)
        *err = m_lastError;
    return false;
}

bool S7Client::succeed()
{
    m_lastCode = 0;
    m_lastError.clear();
    return true;
}

bool S7Client::connectTo(const QString &ip, int rack, int slot, int port, int timeoutMs,
                         QString *err)
{
    if (!ensureReady(err))
        return false;

    auto &lib = Snap7Lib::instance();

    // 超时必须设，否则网线拔掉时 ConnectTo 会阻塞很久，界面上看就是卡死
    if (lib.Cli_SetParam && timeoutMs > 0) {
        int t = timeoutMs;
        lib.Cli_SetParam(m_client, Snap7Lib::ParamPingTimeout, &t);
        lib.Cli_SetParam(m_client, Snap7Lib::ParamSendTimeout, &t);
        lib.Cli_SetParam(m_client, Snap7Lib::ParamRecvTimeout, &t);
    }

    // 非标准端口只在脱机接模拟器时用到；真机保持 102
    if (lib.Cli_SetParam && port > 0 && port != 102) {
        quint16 p = static_cast<quint16>(port);
        lib.Cli_SetParam(m_client, Snap7Lib::ParamRemotePort, &p);
    }

    // TSAP 第一字节 = 连接类型。默认 PG(0x01)，这里显式切成 S7 Basic(0x03)，
    // 配合 rack=0/slot=1 得到 TSAP 03.01；对侧某些 PLC 只放通这个 TSAP。
    // 第二字节由 Cli_ConnectTo 内部按 rack*32+slot 填。
    if (lib.Cli_SetConnectionType)
        lib.Cli_SetConnectionType(m_client, Snap7Lib::CTS7Basic);

    const QByteArray addr = ip.toLatin1();
    const int code = lib.Cli_ConnectTo(m_client, addr.constData(), rack, slot);
    if (code != 0) {
        m_connected = false;
        return fail(code, QStringLiteral("连接 %1:%2 (rack=%3,slot=%4)")
                              .arg(ip).arg(port).arg(rack).arg(slot), err);
    }

    m_connected = true;
    return succeed();
}

void S7Client::disconnect()
{
    auto &lib = Snap7Lib::instance();
    if (m_client && lib.isLoaded() && lib.Cli_Disconnect)
        lib.Cli_Disconnect(m_client);
    m_connected = false;
}

bool S7Client::isConnected() const
{
    auto &lib = Snap7Lib::instance();
    if (!m_client || !lib.isLoaded() || !lib.Cli_GetConnected)
        return false;

    int connected = 0;
    if (lib.Cli_GetConnected(m_client, &connected) != 0)
        return false;
    return connected != 0;
}

bool S7Client::mapArea(const S7Address &addr, int *area, int *dbNumber, QString *err)
{
    if (!addr.isValid()) {
        m_lastError = QStringLiteral("无效的 PLC 地址");
        if (err) *err = m_lastError;
        return false;
    }

    switch (addr.area) {
    case S7Address::Area::V:
        *area     = Snap7Lib::AreaDB;   // V 区走 DB1
        *dbNumber = m_vDb;
        return true;
    case S7Address::Area::M:
        *area     = Snap7Lib::AreaMK;   // M 区有独立区号，DB 号填 0
        *dbNumber = 0;
        return true;
    default:
        m_lastError = QStringLiteral("不支持的区号：%1").arg(addr.toString());
        if (err) *err = m_lastError;
        return false;
    }
}

bool S7Client::readBytes(const S7Address &addr, int size, QByteArray *out, QString *err)
{
    if (!out || size <= 0) {
        m_lastError = QStringLiteral("读取长度非法");
        if (err) *err = m_lastError;
        return false;
    }
    if (addr.isBit()) {
        m_lastError = QStringLiteral("%1 是位地址，请用 readBit").arg(addr.toString());
        if (err) *err = m_lastError;
        return false;
    }
    if (!ensureReady(err))
        return false;

    int area = 0, db = 0;
    if (!mapArea(addr, &area, &db, err))
        return false;

    QByteArray buf(size, Qt::Uninitialized);
    const int code = Snap7Lib::instance().Cli_ReadArea(
        m_client, area, db, addr.byteOffset, size, Snap7Lib::WLByte, buf.data());

    if (code != 0) {
        m_connected = false;   // 读失败一律当断链，交给上层重连，避免半死不活
        return fail(code, QStringLiteral("读 %1 共 %2 字节").arg(addr.toString()).arg(size), err);
    }

    *out = buf;
    return succeed();
}

bool S7Client::writeBytes(const S7Address &addr, const QByteArray &data, QString *err)
{
    if (data.isEmpty()) {
        m_lastError = QStringLiteral("写入内容为空");
        if (err) *err = m_lastError;
        return false;
    }
    if (addr.isBit()) {
        m_lastError = QStringLiteral("%1 是位地址，请用 writeBit").arg(addr.toString());
        if (err) *err = m_lastError;
        return false;
    }
    if (!ensureReady(err))
        return false;

    int area = 0, db = 0;
    if (!mapArea(addr, &area, &db, err))
        return false;

    QByteArray payload = data;   // snap7 接口不是 const 指针
    const int code = Snap7Lib::instance().Cli_WriteArea(
        m_client, area, db, addr.byteOffset, payload.size(), Snap7Lib::WLByte, payload.data());

    if (code != 0) {
        m_connected = false;
        return fail(code, QStringLiteral("写 %1 共 %2 字节").arg(addr.toString()).arg(payload.size()), err);
    }
    return succeed();
}

bool S7Client::readBit(const S7Address &addr, bool *value, QString *err)
{
    if (!value) {
        m_lastError = QStringLiteral("readBit 输出参数为空");
        if (err) *err = m_lastError;
        return false;
    }
    if (!addr.isBit()) {
        m_lastError = QStringLiteral("%1 不是位地址").arg(addr.toString());
        if (err) *err = m_lastError;
        return false;
    }
    if (!ensureReady(err))
        return false;

    int area = 0, db = 0;
    if (!mapArea(addr, &area, &db, err))
        return false;

    quint8 raw = 0;
    const int code = Snap7Lib::instance().Cli_ReadArea(
        m_client, area, db, addr.bitIndex(), 1, Snap7Lib::WLBit, &raw);

    if (code != 0) {
        m_connected = false;
        return fail(code, QStringLiteral("读位 %1").arg(addr.toString()), err);
    }

    *value = (raw != 0);
    return succeed();
}

bool S7Client::writeBit(const S7Address &addr, bool value, QString *err)
{
    if (!addr.isBit()) {
        m_lastError = QStringLiteral("%1 不是位地址").arg(addr.toString());
        if (err) *err = m_lastError;
        return false;
    }
    if (!ensureReady(err))
        return false;

    int area = 0, db = 0;
    if (!mapArea(addr, &area, &db, err))
        return false;

    quint8 raw = value ? 1 : 0;
    const int code = Snap7Lib::instance().Cli_WriteArea(
        m_client, area, db, addr.bitIndex(), 1, Snap7Lib::WLBit, &raw);

    if (code != 0) {
        m_connected = false;
        return fail(code, QStringLiteral("写位 %1 = %2").arg(addr.toString()).arg(value ? 1 : 0), err);
    }
    return succeed();
}

} // namespace cm8600
