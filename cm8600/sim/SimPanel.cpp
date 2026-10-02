#include "sim/SimPanel.h"

#include "plc/S7Codec.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace cm8600 {

namespace {

QLabel *makeValue(QWidget *p)
{
    auto *l = new QLabel(QStringLiteral("—"), p);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

} // namespace

SimPanel::SimPanel(const AppConfig &cfg, const ChannelConfig &channel, QWidget *parent)
    : QWidget(parent)
    , m_cfg(cfg)
    , m_ch(channel)
    , m_plc(cfg)
{
    buildUi();

    m_tick = new QTimer(this);
    m_tick->setInterval(150);
    connect(m_tick, &QTimer::timeout, this, &SimPanel::onTick);

    m_heartbeat = new QTimer(this);
    m_heartbeat->setInterval(cfg.plc.heartbeat.toggleMs > 0 ? cfg.plc.heartbeat.toggleMs : 500);
    connect(m_heartbeat, &QTimer::timeout, this, &SimPanel::onHeartbeat);

    setRunning(false);
}

SimPanel::~SimPanel()
{
    m_plc.stop();
}

void SimPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);

    // ---- 服务端 ----
    {
        auto *box = new QGroupBox(QStringLiteral("假 PLC —— %1").arg(m_ch.name), this);
        auto *row = new QHBoxLayout(box);

        m_ipEdit = new QLineEdit(QStringLiteral("127.0.0.1"), box);
        m_ipEdit->setMaximumWidth(140);

        m_portSpin = new QSpinBox(box);
        m_portSpin->setRange(1, 65535);
        // 真机是 102，但同一个 IP 上起不了两个服务端，脱机时按通道号错开
        m_portSpin->setValue(m_ch.port != 102 ? m_ch.port : 1100 + m_ch.id);

        m_startBtn = new QPushButton(QStringLiteral("启动"), box);
        m_stateLabel = new QLabel(QStringLiteral("未启动"), box);

        connect(m_startBtn, &QPushButton::clicked, this, &SimPanel::onStartStop);

        row->addWidget(new QLabel(QStringLiteral("监听："), box));
        row->addWidget(m_ipEdit);
        row->addWidget(new QLabel(QStringLiteral("端口："), box));
        row->addWidget(m_portSpin);
        row->addWidget(m_startBtn);
        row->addWidget(m_stateLabel, 1);

        root->addWidget(box);
    }

    // ---- 心跳 ----
    {
        auto *box = new QGroupBox(QStringLiteral("心跳"), this);
        auto *row = new QHBoxLayout(box);

        m_autoBeat = new QCheckBox(QStringLiteral("自动回心跳 %1")
                                       .arg(m_cfg.plc.heartbeat.rxBit.toString()), box);
        m_autoBeat->setChecked(true);
        m_autoBeat->setToolTip(QStringLiteral("取消勾选即可模拟 PLC 掉线，验证上位机 3 秒超时报警"));

        m_peerBeat = new QLabel(QStringLiteral("上位机心跳 %1：未检测到")
                                    .arg(m_cfg.plc.heartbeat.txBit.toString()), box);

        row->addWidget(m_autoBeat);
        row->addWidget(m_peerBeat, 1);
        root->addWidget(box);
    }

    // ---- 扫码查询 ----
    {
        auto *box = new QGroupBox(QStringLiteral("① 扫码 → 触发查询"), this);
        auto *layout = new QVBoxLayout(box);

        auto *row = new QHBoxLayout();
        m_barcodeEdit = new QLineEdit(
            QStringLiteral("SZ%1").arg(QDateTime::currentDateTime()
                                           .toString(QStringLiteral("yyyyMMddHHmmss"))), box);
        m_queryBtn = new QPushButton(QStringLiteral("扫码触发"), box);
        connect(m_queryBtn, &QPushButton::clicked, this, &SimPanel::onTriggerQuery);

        row->addWidget(new QLabel(QStringLiteral("条码："), box));
        row->addWidget(m_barcodeEdit, 1);
        row->addWidget(m_queryBtn);
        layout->addLayout(row);

        auto *hint = new QLabel(
            QStringLiteral("条码以 NG 开头可验证「查询失败」分支（模拟数据源里留了这个口子）"), box);
        hint->setStyleSheet(QStringLiteral("color:#5a6570;"));
        layout->addWidget(hint);

        auto *form = new QFormLayout();
        m_gotRefrigerant = makeValue(box);
        m_gotRefrigQty   = makeValue(box);
        m_gotOilQty      = makeValue(box);
        m_gotVacuumLevel = makeValue(box);
        m_gotVacuumTime  = makeValue(box);
        form->addRow(QStringLiteral("收到 制冷剂编号："), m_gotRefrigerant);
        form->addRow(QStringLiteral("收到 制冷剂量："),   m_gotRefrigQty);
        form->addRow(QStringLiteral("收到 冷冻油量："),   m_gotOilQty);
        form->addRow(QStringLiteral("收到 真空度："),     m_gotVacuumLevel);
        form->addRow(QStringLiteral("收到 抽空时间："),   m_gotVacuumTime);
        layout->addLayout(form);

        root->addWidget(box);
    }

    // ---- 加注完成 ----
    {
        auto *box = new QGroupBox(QStringLiteral("② 加注流程"), this);
        auto *row = new QHBoxLayout(box);

        m_vacuumBtn = new QPushButton(QStringLiteral("抽真空完成"), box);
        m_vacuumBtn->setToolTip(QStringLiteral("仅在 config 里 vacuum_trigger.mode = flag 时有效"));
        m_vacuumBtn->setEnabled(
            m_cfg.plc.vacuumTrigger.mode == VacuumTriggerConfig::Mode::Flag
            && m_cfg.plc.vacuumTrigger.bit.isValid());
        connect(m_vacuumBtn, &QPushButton::clicked, this, &SimPanel::onVacuumDone);

        m_chargeBtn = new QPushButton(
            QStringLiteral("加注完成（填结果块 + 置 %1）").arg(m_cfg.plc.chargeDoneBit.toString()), box);
        connect(m_chargeBtn, &QPushButton::clicked, this, &SimPanel::onChargeDone);

        row->addWidget(m_vacuumBtn);
        row->addWidget(m_chargeBtn, 1);
        root->addWidget(box);
    }

    // ---- 日志 ----
    {
        auto *box = new QGroupBox(QStringLiteral("日志"), this);
        auto *layout = new QVBoxLayout(box);
        m_log = new QPlainTextEdit(box);
        m_log->setReadOnly(true);
        m_log->setMaximumBlockCount(1000);
        m_log->setFont(QFont(QStringLiteral("Consolas"), 9));
        layout->addWidget(m_log);
        root->addWidget(box, 1);
    }
}

void SimPanel::log(const QString &text)
{
    m_log->appendPlainText(QStringLiteral("%1  %2")
                               .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                                    text));
}

void SimPanel::setRunning(bool running)
{
    m_startBtn->setText(running ? QStringLiteral("停止") : QStringLiteral("启动"));
    m_ipEdit->setEnabled(!running);
    m_portSpin->setEnabled(!running);

    m_queryBtn->setEnabled(running);
    m_chargeBtn->setEnabled(running);
    m_vacuumBtn->setEnabled(running
                            && m_cfg.plc.vacuumTrigger.mode == VacuumTriggerConfig::Mode::Flag
                            && m_cfg.plc.vacuumTrigger.bit.isValid());

    if (running) {
        m_stateLabel->setText(QStringLiteral("运行中 %1").arg(m_plc.endpoint()));
        m_stateLabel->setStyleSheet(QStringLiteral("color:#1e7d5a; font-weight:bold;"));
    } else {
        m_stateLabel->setText(QStringLiteral("未启动"));
        m_stateLabel->setStyleSheet(QStringLiteral("color:#5a6570;"));
    }
}

void SimPanel::onStartStop()
{
    if (m_plc.isRunning()) {
        m_tick->stop();
        m_heartbeat->stop();
        m_plc.stop();
        setRunning(false);
        log(QStringLiteral("已停止"));
        return;
    }

    QString err;
    if (!m_plc.start(m_ipEdit->text().trimmed(), m_portSpin->value(), &err)) {
        log(QStringLiteral("启动失败：%1").arg(err));
        return;
    }

    m_peerBeatSeen  = false;
    m_peerBeatCount = 0;
    m_chargeFlagSet = false;
    m_vacuumFlagSet = false;

    m_tick->start();
    m_heartbeat->start();
    setRunning(true);

    log(QStringLiteral("已在 %1 启动。把上位机 config.json 的该通道改成同样的 ip/port 即可连上")
            .arg(m_plc.endpoint()));
}

void SimPanel::onHeartbeat()
{
    if (!m_plc.isRunning() || !m_autoBeat->isChecked())
        return;

    m_txBeat = !m_txBeat;
    m_plc.setBit(m_cfg.plc.heartbeat.rxBit, m_txBeat);
}

void SimPanel::onTriggerQuery()
{
    const QString barcode = m_barcodeEdit->text().trimmed();
    if (barcode.isEmpty()) {
        log(QStringLiteral("条码为空，先填一个"));
        return;
    }

    // 和真机一致：条码写进 VB9750（30 字节，左侧补空格），然后置触发位
    m_plc.writeAscii(m_cfg.plc.query.barcodeAddr, m_cfg.plc.query.barcodeLen, barcode);
    m_plc.setBit(m_cfg.plc.query.triggerBit, true);

    m_vacuumStart = QDateTime::currentDateTime();
    log(QStringLiteral("已写入条码 \"%1\" 并置 %2，等待上位机应答")
            .arg(barcode, m_cfg.plc.query.triggerBit.toString()));
}

void SimPanel::onVacuumDone()
{
    if (m_cfg.plc.vacuumTrigger.mode != VacuumTriggerConfig::Mode::Flag)
        return;

    m_plc.writeBlock(m_cfg.plc.result.blockAddr, buildResultBlock());
    m_plc.setBit(m_cfg.plc.vacuumTrigger.bit, true);
    m_vacuumFlagSet = true;

    log(QStringLiteral("已填结果块并置抽真空完成位 %1，等待上位机读取并复位")
            .arg(m_cfg.plc.vacuumTrigger.bit.toString()));
}

void SimPanel::onChargeDone()
{
    m_plc.writeBlock(m_cfg.plc.result.blockAddr, buildResultBlock());
    m_plc.setBit(m_cfg.plc.chargeDoneBit, true);
    m_chargeFlagSet = true;

    log(QStringLiteral("已填结果块 %1 共 %2 字节，并置 %3，等待上位机读取并复位")
            .arg(m_cfg.plc.result.blockAddr.toString())
            .arg(m_cfg.plc.result.blockSize)
            .arg(m_cfg.plc.chargeDoneBit.toString()));
}

QByteArray SimPanel::buildResultBlock() const
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime vacStart = m_vacuumStart.isValid() ? m_vacuumStart : now.addSecs(-200);

    // 回传的值直接用上位机刚下发的那组，这样能验证整条链路的数据闭环：
    // 查到什么 -> 下发什么 -> PLC 回什么 -> 库里存什么 -> 上传什么
    const QString refrigerantName = m_cfg.refrigerantName(m_pRefrigCode);

    const QMap<QString, QString> values = {
        { QStringLiteral("vacuum_start_date"), vacStart.toString(QStringLiteral("yyyy/MM/dd")) },
        { QStringLiteral("vacuum_start_time"), vacStart.toString(QStringLiteral("HH:mm:ss")) },
        { QStringLiteral("vacuum_duration"),   QString::number(m_pVacuumTime, 'f', 0) },
        { QStringLiteral("vacuum_level"),      QString::number(m_pVacuumLevel, 'f', 0) },
        { QStringLiteral("charge_duration"),   QString::number(vacStart.secsTo(now)) },
        { QStringLiteral("refrigerant_type"),  refrigerantName },
        { QStringLiteral("refrigerant_qty"),   QString::number(m_pRefrigQty, 'f', 1) },
        { QStringLiteral("charge_start_date"), now.toString(QStringLiteral("yyyy/MM/dd")) },
        { QStringLiteral("charge_start_time"), now.toString(QStringLiteral("HH:mm:ss")) },
        { QStringLiteral("oil_duration"),      QStringLiteral("12") },
        { QStringLiteral("oil_qty"),           QString::number(m_pOilQty, 'f', 1) },
        { QStringLiteral("barcode"),           m_barcodeEdit->text().trimmed() },
    };

    QByteArray block(m_cfg.plc.result.blockSize, ' ');
    for (const ResultField &f : m_cfg.plc.result.fields) {
        const int off = f.offsetIn(m_cfg.plc.result.blockAddr);
        if (off < 0 || off + f.len > block.size())
            continue;
        block.replace(off, f.len, S7Codec::makeAscii(values.value(f.key), f.len));
    }
    return block;
}

void SimPanel::onTick()
{
    if (!m_plc.isRunning())
        return;

    // ---- 盯上位机心跳 ----
    const bool beat = m_plc.getBit(m_cfg.plc.heartbeat.txBit);
    if (!m_peerBeatSeen) {
        m_peerBeatSeen = true;
        m_prevPeerBeat = beat;
    } else if (beat != m_prevPeerBeat) {
        m_prevPeerBeat = beat;
        ++m_peerBeatCount;
        m_peerBeat->setText(QStringLiteral("上位机心跳 %1：正常（已翻转 %2 次）")
                                .arg(m_cfg.plc.heartbeat.txBit.toString())
                                .arg(m_peerBeatCount));
    }

    // ---- 查询应答：上位机置了成功位或失败位 ----
    const bool ok   = m_plc.getBit(m_cfg.plc.query.okBit);
    const bool fail = m_plc.getBit(m_cfg.plc.query.failBit);

    if (ok || fail) {
        if (ok) {
            // 把上位机写进来的参数读出来显示，顺便记下来用于生成回传数据
            m_pOilQty      = m_plc.readReal(m_cfg.plc.download.oilQty.addr);
            m_pRefrigQty   = m_plc.readReal(m_cfg.plc.download.refrigerantQty.addr);
            m_pVacuumLevel = m_plc.readReal(m_cfg.plc.download.vacuumLevel.addr);
            m_pVacuumTime  = m_plc.readReal(m_cfg.plc.download.vacuumTime.addr);
            m_pRefrigCode  = m_plc.readInt16(m_cfg.plc.download.refrigerantType.addr);

            m_gotRefrigerant->setText(QStringLiteral("%1（%2）")
                                          .arg(m_pRefrigCode)
                                          .arg(m_cfg.refrigerantName(m_pRefrigCode)));
            m_gotRefrigQty->setText(QStringLiteral("%1 g").arg(m_pRefrigQty, 0, 'f', 1));
            m_gotOilQty->setText(QStringLiteral("%1 g").arg(m_pOilQty, 0, 'f', 1));
            m_gotVacuumLevel->setText(QStringLiteral("%1 Pa").arg(m_pVacuumLevel, 0, 'f', 1));
            m_gotVacuumTime->setText(QStringLiteral("%1 s").arg(m_pVacuumTime, 0, 'f', 1));

            log(QStringLiteral("上位机报【查询成功】，参数已收到；按真机约定由 PLC 侧复位标志位"));
        } else {
            log(QStringLiteral("上位机报【查询失败】，真机此时 HMI 报警提示补数据或重扫"));
        }

        // 按对方确认的约定：触发位和成功/失败位都由 PLC 复位
        m_plc.setBit(m_cfg.plc.query.triggerBit, false);
        m_plc.setBit(m_cfg.plc.query.okBit, false);
        m_plc.setBit(m_cfg.plc.query.failBit, false);
    }

    // ---- 看上位机有没有把完成位复位掉 ----
    if (m_chargeFlagSet && !m_plc.getBit(m_cfg.plc.chargeDoneBit)) {
        m_chargeFlagSet = false;
        log(QStringLiteral("上位机已复位 %1，本件流程结束")
                .arg(m_cfg.plc.chargeDoneBit.toString()));
    }
    if (m_vacuumFlagSet && m_cfg.plc.vacuumTrigger.bit.isValid()
        && !m_plc.getBit(m_cfg.plc.vacuumTrigger.bit)) {
        m_vacuumFlagSet = false;
        log(QStringLiteral("上位机已复位抽真空完成位 %1")
                .arg(m_cfg.plc.vacuumTrigger.bit.toString()));
    }
}

} // namespace cm8600
