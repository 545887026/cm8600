#pragma once

#include "app/AppConfig.h"
#include "sim/SimPlc.h"

#include <QWidget>
#include <QDateTime>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;

namespace cm8600 {

/**
 * 一台假 PLC 的操作面板：手动走一遍现场流程。
 *
 * 一次完整的自测：
 *   启动 -> 填条码 -> 「扫码触发」 -> 看上位机是否回「查询成功」并把参数写进来
 *        -> 「加注完成」 -> 看上位机是否读走结果块、入库、复位 M25.7
 * 条码填 NG 开头可以验证查询失败这条路（模拟数据源里做了这个口子）。
 */
class SimPanel : public QWidget
{
    Q_OBJECT

public:
    SimPanel(const AppConfig &cfg, const ChannelConfig &channel, QWidget *parent = nullptr);
    ~SimPanel() override;

private slots:
    void onStartStop();
    void onTriggerQuery();
    void onVacuumDone();
    void onChargeDone();
    void onTick();
    void onHeartbeat();

private:
    void buildUi();
    void log(const QString &text);
    void setRunning(bool running);
    QByteArray buildResultBlock() const;

    const AppConfig &m_cfg;
    ChannelConfig    m_ch;
    SimPlc           m_plc;

    QTimer *m_tick      = nullptr;
    QTimer *m_heartbeat = nullptr;

    QLineEdit   *m_ipEdit      = nullptr;
    QSpinBox    *m_portSpin    = nullptr;
    QPushButton *m_startBtn    = nullptr;
    QLabel      *m_stateLabel  = nullptr;

    QCheckBox   *m_autoBeat    = nullptr;
    QLabel      *m_peerBeat    = nullptr;

    QLineEdit   *m_barcodeEdit = nullptr;
    QPushButton *m_queryBtn    = nullptr;
    QPushButton *m_vacuumBtn   = nullptr;
    QPushButton *m_chargeBtn   = nullptr;

    QLabel *m_gotRefrigerant = nullptr;
    QLabel *m_gotRefrigQty   = nullptr;
    QLabel *m_gotOilQty      = nullptr;
    QLabel *m_gotVacuumLevel = nullptr;
    QLabel *m_gotVacuumTime  = nullptr;

    QPlainTextEdit *m_log = nullptr;

    bool m_txBeat        = false;
    bool m_prevPeerBeat  = false;
    bool m_peerBeatSeen  = false;
    int  m_peerBeatCount = 0;

    bool m_chargeFlagSet = false;   ///< 我们置起了 M25.7，正在等上位机复位
    bool m_vacuumFlagSet = false;

    // 上位机下发下来的参数，用来生成回传结果块，验证数据闭环
    double m_pOilQty = 0, m_pRefrigQty = 0, m_pVacuumLevel = 0, m_pVacuumTime = 0;
    int    m_pRefrigCode = 0;
    QDateTime m_vacuumStart;
};

} // namespace cm8600
