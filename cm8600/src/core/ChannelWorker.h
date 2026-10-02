#pragma once

#include "app/AppConfig.h"
#include "core/Records.h"
#include "plc/S7Client.h"

#include <QDateTime>
#include <QObject>

class QTimer;

namespace cm8600 {

class Database;
class IParamSource;

/**
 * 单个 PLC 通道的状态机。一个通道一个线程，两台 PLC 互不影响。
 *
 * 循环里做四件事：
 *   1) 断了就重连（按 reconnect_interval_ms 节流，不疯狂重试）
 *   2) 心跳：自己按 0.5s 翻转 tx 位；同时盯 rx 位，超过 3s 没翻转就判对方掉线
 *   3) 查询握手：PLC 置 V9741.1 -> 读条码 -> 查参数 -> 写 VD9700~9712 + VW9730
 *      -> 置查询成功位 V9740.1 或失败位 V9740.2（这两个位由 PLC 复位）
 *   4) 结果回读：加注完成位 M25.7 置起 -> 一次读 140 字节 -> 解析入库 -> 入上传队列
 *      -> 由本程序复位该位
 *
 * 第 4 步里“先入库再复位”的顺序不能反：万一复位后落库前崩了，这条记录就丢了。
 * 反过来先落库、复位前崩了，重启后会重读一遍，由 dedupKey 挡掉，代价只是一次重复读。
 */
class ChannelWorker : public QObject
{
    Q_OBJECT

public:
    ChannelWorker(const AppConfig &cfg, const ChannelConfig &channel,
                  Database *db, IParamSource *source, QObject *parent = nullptr);
    ~ChannelWorker() override;

    int     channelId() const { return m_ch.id; }
    QString channelName() const { return m_ch.name; }

public slots:
    /// 在本对象所属线程里调用（线程 started 信号触发）
    void start();
    void stop();

signals:
    /// PLC 连接状态变化
    void linkChanged(int channelId, bool connected, const QString &detail);
    /// 对方心跳存活状态变化
    void peerAliveChanged(int channelId, bool alive);
    /// 处理完一次查询握手
    void queryHandled(int channelId, const QString &barcode, bool ok,
                      const ChargeParams &params, const QString &reason);
    /// 收到并保存了一条记录；duplicate 表示这条之前已存过（重复读）
    void recordSaved(int channelId, const ResultRecord &record, bool duplicate, qint64 rowId);

private slots:
    void onPoll();
    void onHeartbeat();

private:
    bool ensureConnected();
    void pollHeartbeatRx();
    void pollQueryTrigger();
    void pollVacuumFlag();
    void pollChargeDone();

    bool handleQuery(const QString &barcode);
    bool writeParams(const ChargeParams &p, QString *err);
    bool readResultBlock(QByteArray *block, QString *err);
    void handleResult(const QByteArray &block, bool vacuumStageOnly);

    void setLink(bool connected, const QString &detail);
    void setPeerAlive(bool alive);

    const AppConfig &m_cfg;
    ChannelConfig    m_ch;
    Database        *m_db     = nullptr;
    IParamSource    *m_source = nullptr;

    S7Client m_plc;
    QTimer  *m_pollTimer      = nullptr;
    QTimer  *m_heartbeatTimer = nullptr;

    bool      m_running   = false;
    bool      m_connected = false;
    QDateTime m_lastConnectAttempt;

    // 心跳
    bool      m_txLevel = false;
    bool      m_rxLevel = false;
    bool      m_rxSeen  = false;      ///< 是否已经读到过一次 rx，用于建立初值
    QDateTime m_lastRxChange;
    bool      m_peerAlive = false;

    // 边沿检测：只在 false->true 的瞬间动作，避免电平一直为真时反复处理
    bool m_prevTrigger    = false;
    bool m_prevChargeDone = false;
    bool m_prevVacuumFlag = false;

    /// 当前工件会话：条码在扫码阶段就拿到了，不依赖结果块里的 VB9220
    QString m_sessionBarcode;
};

} // namespace cm8600
