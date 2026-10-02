#include "core/ChannelWorker.h"

#include "app/Logger.h"
#include "datasource/IParamSource.h"
#include "plc/S7Codec.h"
#include "storage/Database.h"

#include <QTimer>

#include <utility>

namespace cm8600 {

ChannelWorker::ChannelWorker(const AppConfig &cfg, const ChannelConfig &channel,
                             Database *db, IParamSource *source, QObject *parent)
    : QObject(parent)
    , m_cfg(cfg)
    , m_ch(channel)
    , m_db(db)
    , m_source(source)
    , m_plc(cfg.plc.vDbNumber)
{
}

ChannelWorker::~ChannelWorker()
{
    stop();
}

void ChannelWorker::start()
{
    if (m_running)
        return;
    m_running = true;

    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(m_cfg.plc.pollIntervalMs > 0 ? m_cfg.plc.pollIntervalMs : 200);
        connect(m_pollTimer, &QTimer::timeout, this, &ChannelWorker::onPoll);
    }
    if (!m_heartbeatTimer) {
        m_heartbeatTimer = new QTimer(this);
        m_heartbeatTimer->setInterval(m_cfg.plc.heartbeat.toggleMs > 0
                                          ? m_cfg.plc.heartbeat.toggleMs : 500);
        connect(m_heartbeatTimer, &QTimer::timeout, this, &ChannelWorker::onHeartbeat);
    }

    m_pollTimer->start();
    m_heartbeatTimer->start();

    logger().info(m_ch.id, QStringLiteral("通道启动，目标 %1 (rack=%2, slot=%3)")
                               .arg(m_ch.ip).arg(m_ch.rack).arg(m_ch.slot));
}

void ChannelWorker::stop()
{
    if (!m_running)
        return;
    m_running = false;

    if (m_pollTimer)
        m_pollTimer->stop();
    if (m_heartbeatTimer)
        m_heartbeatTimer->stop();

    m_plc.disconnect();
    setLink(false, QStringLiteral("已停止"));
    logger().info(m_ch.id, QStringLiteral("通道已停止"));
}

// ------------------------------------------------------------------ 连接 ---

void ChannelWorker::setLink(bool connected, const QString &detail)
{
    if (m_connected == connected)
        return;
    m_connected = connected;

    if (!connected) {
        // 断链时把心跳和边沿状态清掉，重连后重新建立，避免拿旧电平做边沿判断
        m_rxSeen = false;
        setPeerAlive(false);
        m_prevTrigger    = false;
        m_prevChargeDone = false;
        m_prevVacuumFlag = false;
    }

    emit linkChanged(m_ch.id, connected, detail);
    if (connected)
        logger().info(m_ch.id, QStringLiteral("已连接 %1").arg(m_ch.ip));
    else
        logger().warn(m_ch.id, QStringLiteral("连接断开：%1").arg(detail));
}

void ChannelWorker::setPeerAlive(bool alive)
{
    if (m_peerAlive == alive)
        return;
    m_peerAlive = alive;
    emit peerAliveChanged(m_ch.id, alive);

    if (alive)
        logger().info(m_ch.id, QStringLiteral("PLC 心跳恢复"));
    else
        logger().warn(m_ch.id, QStringLiteral("PLC 心跳超时（%1 ms 内未翻转）")
                                   .arg(m_cfg.plc.heartbeat.timeoutMs));
}

bool ChannelWorker::ensureConnected()
{
    if (m_plc.isConnected()) {
        setLink(true, m_ch.ip);
        return true;
    }

    setLink(false, m_plc.lastError().isEmpty() ? QStringLiteral("未连接") : m_plc.lastError());

    // 重连节流：断网时别每 200ms 撞一次，日志会刷屏、界面也卡
    const QDateTime now = QDateTime::currentDateTime();
    const int interval = m_cfg.plc.reconnectIntervalMs > 0 ? m_cfg.plc.reconnectIntervalMs : 3000;
    if (m_lastConnectAttempt.isValid() && m_lastConnectAttempt.msecsTo(now) < interval)
        return false;

    m_lastConnectAttempt = now;

    QString err;
    if (!m_plc.connectTo(m_ch.ip, m_ch.rack, m_ch.slot, m_ch.port,
                         m_cfg.plc.connectTimeoutMs, &err)) {
        setLink(false, err);
        return false;
    }

    setLink(true, m_ch.ip);
    return true;
}

// ------------------------------------------------------------------ 心跳 ---

void ChannelWorker::onHeartbeat()
{
    if (!m_running || !m_plc.isConnected())
        return;

    // 0.5s 翻转一次，形成 1Hz 方波；按位写，不会碰到同字节里的查询成功/失败位
    m_txLevel = !m_txLevel;

    QString err;
    if (!m_plc.writeBit(m_cfg.plc.heartbeat.txBit, m_txLevel, &err))
        logger().warn(m_ch.id, QStringLiteral("写心跳位失败：%1").arg(err));
}

void ChannelWorker::pollHeartbeatRx()
{
    bool level = false;
    QString err;
    if (!m_plc.readBit(m_cfg.plc.heartbeat.rxBit, &level, &err))
        return;   // 读失败已在 S7Client 里标记为断链，下轮重连

    const QDateTime now = QDateTime::currentDateTime();

    if (!m_rxSeen) {
        m_rxSeen       = true;
        m_rxLevel      = level;
        m_lastRxChange = now;
        return;
    }

    if (level != m_rxLevel) {
        m_rxLevel      = level;
        m_lastRxChange = now;
        setPeerAlive(true);
        return;
    }

    const int timeout = m_cfg.plc.heartbeat.timeoutMs > 0 ? m_cfg.plc.heartbeat.timeoutMs : 3000;
    if (m_lastRxChange.isValid() && m_lastRxChange.msecsTo(now) > timeout)
        setPeerAlive(false);
}

// -------------------------------------------------------------- 查询握手 ---

void ChannelWorker::pollQueryTrigger()
{
    bool level = false;
    QString err;
    if (!m_plc.readBit(m_cfg.plc.query.triggerBit, &level, &err))
        return;

    const bool rising = level && !m_prevTrigger;
    m_prevTrigger = level;

    if (!rising)
        return;

    // 读条码。长度按配置（对方给的是 VB9750~9779 共 30 字节）
    QByteArray raw;
    if (!m_plc.readBytes(m_cfg.plc.query.barcodeAddr, m_cfg.plc.query.barcodeLen, &raw, &err)) {
        logger().error(m_ch.id, QStringLiteral("读条码失败：%1").arg(err));
        return;
    }

    const QString barcode = S7Codec::readAscii(raw, 0, raw.size());
    m_sessionBarcode = barcode;

    logger().info(m_ch.id, QStringLiteral("收到查询请求，条码：\"%1\"").arg(barcode));
    handleQuery(barcode);
}

bool ChannelWorker::handleQuery(const QString &barcode)
{
    ChargeParams params;
    QString reason;

    const bool found = m_source && m_source->lookup(barcode, &params, &reason);

    if (found) {
        QString err;
        if (!writeParams(params, &err)) {
            // 参数没写进去就不能报成功，否则 PLC 会拿着旧值加注
            logger().error(m_ch.id, QStringLiteral("下发参数失败：%1").arg(err));
            reason = err;
        } else {
            logger().info(m_ch.id,
                          QStringLiteral("已下发：%1 %2 g，冷冻油 %3 g，真空度 %4 Pa，抽空 %5 s（来源：%6）")
                              .arg(params.refrigerantName)
                              .arg(params.refrigerantQty, 0, 'f', 1)
                              .arg(params.oilQty, 0, 'f', 1)
                              .arg(params.vacuumLevel, 0, 'f', 1)
                              .arg(params.vacuumTime, 0, 'f', 1)
                              .arg(params.source));

            if (!m_plc.writeBit(m_cfg.plc.query.okBit, true, &err))
                logger().error(m_ch.id, QStringLiteral("置查询成功位失败：%1").arg(err));

            emit queryHandled(m_ch.id, barcode, true, params, QString());
            return true;
        }
    }

    if (reason.isEmpty())
        reason = QStringLiteral("未知原因");

    logger().warn(m_ch.id, QStringLiteral("查询失败（条码 \"%1\"）：%2").arg(barcode, reason));

    QString err;
    if (!m_plc.writeBit(m_cfg.plc.query.failBit, true, &err))
        logger().error(m_ch.id, QStringLiteral("置查询失败位失败：%1").arg(err));

    emit queryHandled(m_ch.id, barcode, false, ChargeParams(), reason);
    return false;
}

bool ChannelWorker::writeParams(const ChargeParams &p, QString *err)
{
    const DownloadConfig &d = m_cfg.plc.download;

    // 四个 Real 连续存放，凑成一块一次写下去：
    // 要么四个参数全到位，要么一个都不改，不会出现“加注量改了但真空度还是旧值”
    QByteArray block(d.blockSize, '\0');
    const int base = d.blockAddr.byteOffset;

    auto put = [&](const DownloadItem &item, double value) {
        return S7Codec::setReal(block, item.addr.byteOffset - base, static_cast<float>(value));
    };

    if (!put(d.oilQty, p.oilQty) || !put(d.refrigerantQty, p.refrigerantQty)
        || !put(d.vacuumLevel, p.vacuumLevel) || !put(d.vacuumTime, p.vacuumTime)) {
        if (err)
            *err = QStringLiteral("下发块内偏移计算越界，请检查 config.json 的 plc.download");
        return false;
    }

    if (!m_plc.writeBytes(d.blockAddr, block, err))
        return false;

    // 制冷剂编号单独一个字，不和上面那块连续
    QByteArray word(2, '\0');
    S7Codec::setInt16(word, 0, static_cast<qint16>(p.refrigerantCode));
    if (!m_plc.writeBytes(d.refrigerantType.addr, word, err))
        return false;

    return true;
}

// -------------------------------------------------------------- 结果回读 ---

void ChannelWorker::pollVacuumFlag()
{
    if (m_cfg.plc.vacuumTrigger.mode != VacuumTriggerConfig::Mode::Flag)
        return;
    if (!m_cfg.plc.vacuumTrigger.bit.isValid())
        return;

    bool level = false;
    QString err;
    if (!m_plc.readBit(m_cfg.plc.vacuumTrigger.bit, &level, &err))
        return;

    const bool rising = level && !m_prevVacuumFlag;
    m_prevVacuumFlag = level;
    if (!rising)
        return;

    logger().info(m_ch.id, QStringLiteral("抽真空完成标志位置起，读取真空段数据"));

    QByteArray block;
    if (!readResultBlock(&block, &err)) {
        logger().error(m_ch.id, QStringLiteral("读结果块失败：%1").arg(err));
        return;   // 不复位标志位，下轮重试；数据宁可晚到也别丢
    }

    handleResult(block, /*vacuumStageOnly=*/true);

    if (!m_plc.writeBit(m_cfg.plc.vacuumTrigger.bit, false, &err))
        logger().error(m_ch.id, QStringLiteral("复位抽真空完成标志位失败：%1").arg(err));
}

void ChannelWorker::pollChargeDone()
{
    bool level = false;
    QString err;
    if (!m_plc.readBit(m_cfg.plc.chargeDoneBit, &level, &err))
        return;

    const bool rising = level && !m_prevChargeDone;
    m_prevChargeDone = level;
    if (!rising)
        return;

    logger().info(m_ch.id, QStringLiteral("加注完成标志位置起，读取结果块"));

    QByteArray block;
    if (!readResultBlock(&block, &err)) {
        logger().error(m_ch.id, QStringLiteral("读结果块失败：%1").arg(err));
        return;   // 同上：读不到就别复位，下轮再来
    }

    handleResult(block, /*vacuumStageOnly=*/false);

    // 先入库再复位。顺序反了会丢数据，见头文件注释
    if (!m_plc.writeBit(m_cfg.plc.chargeDoneBit, false, &err))
        logger().error(m_ch.id, QStringLiteral("复位加注完成标志位失败：%1").arg(err));
    else
        logger().info(m_ch.id, QStringLiteral("已复位加注完成标志位 %1")
                                   .arg(m_cfg.plc.chargeDoneBit.toString()));
}

bool ChannelWorker::readResultBlock(QByteArray *block, QString *err)
{
    return m_plc.readBytes(m_cfg.plc.result.blockAddr, m_cfg.plc.result.blockSize, block, err);
}

void ChannelWorker::handleResult(const QByteArray &block, bool vacuumStageOnly)
{
    ResultRecord rec;
    QStringList warnings;

    if (!ResultRecord::parse(m_cfg, m_ch.id, block, &rec, &warnings)) {
        logger().error(m_ch.id, QStringLiteral("结果块解析失败：%1")
                                    .arg(warnings.join(QStringLiteral("；"))));
        return;
    }

    for (const QString &w : std::as_const(warnings))
        logger().warn(m_ch.id, QStringLiteral("结果块：%1").arg(w));

    // 条码兜底：真空阶段 PLC 可能还没往 VB9220 写条码，用扫码时记下的那个
    if (rec.barcode.isEmpty() && !m_sessionBarcode.isEmpty()) {
        rec.barcode = m_sessionBarcode;
        logger().info(m_ch.id, QStringLiteral("结果块条码为空，改用扫码时记录的条码 \"%1\"")
                                   .arg(m_sessionBarcode));
    }

    if (vacuumStageOnly) {
        // 只上传真空段，充注字段此刻还没产生，清掉免得把空值发给 MES
        rec.chargeStart = QDateTime();
        rec.chargeDuration = 0;
        rec.refrigerantName.clear();
        rec.refrigerantCode = 0;
        rec.refrigerantQty = 0;
        rec.oilDuration = 0;
        rec.oilQty = 0;
    }

    if (!m_db) {
        logger().error(m_ch.id, QStringLiteral("数据库未就绪，记录无法保存"));
        return;
    }

    qint64 rowId = 0;
    bool duplicate = false;
    QString err;
    if (!m_db->saveRecord(rec, &rowId, &duplicate, &err)) {
        logger().error(m_ch.id, QStringLiteral("保存记录失败：%1").arg(err));
        return;
    }

    if (duplicate) {
        // 上一轮已经读过并入库，只是没来得及复位标志位。不重复上传
        logger().warn(m_ch.id, QStringLiteral("重复记录（条码 %1），已忽略，不再上传")
                                   .arg(rec.barcode));
    } else {
        logger().info(m_ch.id, QStringLiteral("记录已保存 #%1：条码 %2，%3 %4 g，真空度 %5 Pa")
                                   .arg(rowId)
                                   .arg(rec.barcode, rec.refrigerantName)
                                   .arg(rec.refrigerantQty, 0, 'f', 1)
                                   .arg(rec.vacuumLevel, 0, 'f', 1));
    }

    emit recordSaved(m_ch.id, rec, duplicate, rowId);
}

// -------------------------------------------------------------- 主循环 ---

void ChannelWorker::onPoll()
{
    if (!m_running)
        return;

    if (!ensureConnected())
        return;

    pollHeartbeatRx();
    if (!m_plc.isConnected())
        return;

    pollQueryTrigger();
    if (!m_plc.isConnected())
        return;

    pollVacuumFlag();
    if (!m_plc.isConnected())
        return;

    pollChargeDone();
}

} // namespace cm8600
