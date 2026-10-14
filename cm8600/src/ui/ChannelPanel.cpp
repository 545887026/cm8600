#include "ui/ChannelPanel.h"

#include "app/Logger.h"

#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QVBoxLayout>

namespace cm8600 {

namespace {

constexpr int kMaxLogLines    = 2000;
constexpr int kMaxHistoryRows = 500;

QLabel *makeLight(QWidget *parent)
{
    auto *l = new QLabel(parent);
    l->setFixedSize(14, 14);
    l->setStyleSheet(QStringLiteral(
        "background:#9aa3ad; border-radius:7px; border:1px solid #6d7681;"));
    return l;
}

void setLightColor(QLabel *light, const QString &fill, const QString &border)
{
    light->setStyleSheet(QStringLiteral("background:%1; border-radius:7px; border:1px solid %2;")
                             .arg(fill, border));
}

QLabel *makeValue(QWidget *parent, const QString &initial = QStringLiteral("—"))
{
    auto *l = new QLabel(initial, parent);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QString fmt(double v, int decimals, const QString &unit)
{
    return QStringLiteral("%1 %2").arg(QString::number(v, 'f', decimals), unit);
}

QString fmtTime(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—");
}

} // namespace

ChannelPanel::ChannelPanel(const ChannelConfig &channel, QWidget *parent)
    : QWidget(parent)
    , m_ch(channel)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    root->addWidget(buildHeader());
    root->addWidget(buildParamsBox());
    root->addWidget(buildResultBox());
    root->addWidget(buildHistoryBox(), 1);

    auto *logBox = new QGroupBox(QStringLiteral("通道日志"), this);
    auto *logLayout = new QVBoxLayout(logBox);
    m_log = new QPlainTextEdit(logBox);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(kMaxLogLines);
    m_log->setFont(QFont(QStringLiteral("Consolas"), 9));
    logLayout->addWidget(m_log);
    logBox->setMinimumHeight(140);
    root->addWidget(logBox);
}

QWidget *ChannelPanel::buildHeader()
{
    auto *box = new QGroupBox(this);
    auto *row = new QHBoxLayout(box);

    auto *title = new QLabel(QStringLiteral("<b style='font-size:15pt'>%1</b>").arg(m_ch.name), box);
    row->addWidget(title);

    auto *ip = new QLabel(QStringLiteral("%1  rack=%2 slot=%3")
                              .arg(m_ch.ip).arg(m_ch.rack).arg(m_ch.slot), box);
    ip->setStyleSheet(QStringLiteral("color:#5a6570;"));
    row->addWidget(ip);

    row->addStretch();

    m_linkLight = makeLight(box);
    m_linkText  = new QLabel(QStringLiteral("未连接"), box);
    row->addWidget(m_linkLight);
    row->addWidget(m_linkText);

    row->addSpacing(12);

    m_beatLight = makeLight(box);
    m_beatText  = new QLabel(QStringLiteral("心跳无"), box);
    row->addWidget(m_beatLight);
    row->addWidget(m_beatText);

    return box;
}

QWidget *ChannelPanel::buildParamsBox()
{
    auto *box = new QGroupBox(QStringLiteral("扫码查询 / 下发参数"), this);
    auto *form = new QFormLayout(box);
    form->setLabelAlignment(Qt::AlignRight);

    m_barcode    = makeValue(box);
    m_barcode->setStyleSheet(QStringLiteral("font-family:Consolas; font-size:12pt;"));
    m_queryState = makeValue(box, QStringLiteral("等待扫码"));

    m_pRefrigerant = makeValue(box);
    m_pRefrigQty   = makeValue(box);
    m_pOilQty      = makeValue(box);
    m_pVacuumLevel = makeValue(box);
    m_pVacuumTime  = makeValue(box);

    form->addRow(QStringLiteral("产品条码："), m_barcode);
    form->addRow(QStringLiteral("查询结果："), m_queryState);
    form->addRow(QStringLiteral("制冷剂："),   m_pRefrigerant);
    form->addRow(QStringLiteral("加注量："),   m_pRefrigQty);
    form->addRow(QStringLiteral("冷冻油量："), m_pOilQty);
    form->addRow(QStringLiteral("真空度："),   m_pVacuumLevel);
    form->addRow(QStringLiteral("抽空时间："), m_pVacuumTime);

    return box;
}

QWidget *ChannelPanel::buildResultBox()
{
    auto *box = new QGroupBox(QStringLiteral("最近一条回传结果"), this);
    auto *row = new QHBoxLayout(box);

    auto *left  = new QFormLayout();
    auto *right = new QFormLayout();
    left->setLabelAlignment(Qt::AlignRight);
    right->setLabelAlignment(Qt::AlignRight);

    m_rVacuumStart    = makeValue(box);
    m_rVacuumDuration = makeValue(box);
    m_rVacuumLevel    = makeValue(box);
    left->addRow(QStringLiteral("抽真空开始："), m_rVacuumStart);
    left->addRow(QStringLiteral("抽真空时长："), m_rVacuumDuration);
    left->addRow(QStringLiteral("真空度："),     m_rVacuumLevel);

    m_rChargeStart    = makeValue(box);
    m_rChargeDuration = makeValue(box);
    m_rRefrigerant    = makeValue(box);
    m_rRefrigQty      = makeValue(box);
    m_rOilQty         = makeValue(box);
    right->addRow(QStringLiteral("充注开始："),   m_rChargeStart);
    right->addRow(QStringLiteral("充注时长："),   m_rChargeDuration);
    right->addRow(QStringLiteral("制冷剂："),     m_rRefrigerant);
    right->addRow(QStringLiteral("制冷剂量："),   m_rRefrigQty);
    right->addRow(QStringLiteral("冷冻油量："),   m_rOilQty);

    row->addLayout(left, 1);
    row->addLayout(right, 1);
    return box;
}

QWidget *ChannelPanel::buildHistoryBox()
{
    auto *box = new QGroupBox(QStringLiteral("历史记录"), this);
    auto *layout = new QVBoxLayout(box);

    m_history = new QTableWidget(0, 11, box);
    m_history->setHorizontalHeaderLabels({
        QStringLiteral("时间"),           QStringLiteral("条码"),
        QStringLiteral("抽真空开始时间"), QStringLiteral("抽真空时长 s"),
        QStringLiteral("真空度 Pa"),      QStringLiteral("冷冻油充注时长 s"),
        QStringLiteral("冷冻油加注量 ml"), QStringLiteral("制冷剂类型"),
        QStringLiteral("制冷剂加注时间"), QStringLiteral("制冷剂充注时长 s"),
        QStringLiteral("制冷剂加注量 g"),
    });
    m_history->verticalHeader()->setVisible(false);
    m_history->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_history->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_history->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_history->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_history->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_history->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    layout->addWidget(m_history);
    return box;
}

// ------------------------------------------------------------------ 槽 ---

void ChannelPanel::setLink(bool connected, const QString &detail)
{
    if (connected) {
        setLightColor(m_linkLight, QStringLiteral("#2ecc71"), QStringLiteral("#1e7d5a"));
        m_linkText->setText(QStringLiteral("已连接"));
    } else {
        setLightColor(m_linkLight, QStringLiteral("#e74c3c"), QStringLiteral("#a93226"));
        m_linkText->setText(QStringLiteral("未连接"));
    }
    m_linkText->setToolTip(detail);
}

void ChannelPanel::setPeerAlive(bool alive)
{
    if (alive) {
        setLightColor(m_beatLight, QStringLiteral("#2ecc71"), QStringLiteral("#1e7d5a"));
        m_beatText->setText(QStringLiteral("心跳正常"));
    } else {
        setLightColor(m_beatLight, QStringLiteral("#e74c3c"), QStringLiteral("#a93226"));
        m_beatText->setText(QStringLiteral("心跳超时"));
    }
}

void ChannelPanel::onQueryHandled(const QString &barcode, bool ok, const ChargeParams &params,
                                  const QString &reason)
{
    m_barcode->setText(barcode.isEmpty() ? QStringLiteral("—") : barcode);

    if (ok) {
        m_queryState->setText(QStringLiteral("查询成功，参数已下发"));
        m_queryState->setStyleSheet(QStringLiteral("color:#1e7d5a; font-weight:bold;"));

        m_pRefrigerant->setText(QStringLiteral("%1（编号 %2）")
                                    .arg(params.refrigerantName).arg(params.refrigerantCode));
        m_pRefrigQty->setText(fmt(params.refrigerantQty, 1, QStringLiteral("g")));
        m_pOilQty->setText(fmt(params.oilQty, 1, QStringLiteral("g")));
        m_pVacuumLevel->setText(fmt(params.vacuumLevel, 1, QStringLiteral("Pa")));
        m_pVacuumTime->setText(fmt(params.vacuumTime, 1, QStringLiteral("s")));
    } else {
        m_queryState->setText(QStringLiteral("查询失败：%1").arg(reason));
        m_queryState->setStyleSheet(QStringLiteral("color:#c0392b; font-weight:bold;"));

        for (QLabel *l : { m_pRefrigerant, m_pRefrigQty, m_pOilQty, m_pVacuumLevel, m_pVacuumTime })
            l->setText(QStringLiteral("—"));
    }
}

void ChannelPanel::onRecordSaved(const ResultRecord &record, bool duplicate)
{
    m_rVacuumStart->setText(fmtTime(record.vacuumStart));
    m_rVacuumDuration->setText(fmt(record.vacuumDuration, 0, QStringLiteral("s")));
    m_rVacuumLevel->setText(fmt(record.vacuumLevel, 1, QStringLiteral("Pa")));
    m_rChargeStart->setText(fmtTime(record.chargeStart));
    m_rChargeDuration->setText(fmt(record.chargeDuration, 0, QStringLiteral("s")));
    m_rRefrigerant->setText(record.refrigerantName.isEmpty() ? QStringLiteral("—")
                                                             : record.refrigerantName);
    m_rRefrigQty->setText(fmt(record.refrigerantQty, 1, QStringLiteral("g")));
    m_rOilQty->setText(fmt(record.oilQty, 1, QStringLiteral("g")));

    if (duplicate)
        return;   // 重复读进来的，不往历史表里再塞一行

    insertHistoryRow(0, record);
    while (m_history->rowCount() > kMaxHistoryRows)
        m_history->removeRow(m_history->rowCount() - 1);
}

void ChannelPanel::insertHistoryRow(int row, const ResultRecord &rec)
{
    m_history->insertRow(row);

    const QDateTime when = rec.chargeStart.isValid() ? rec.chargeStart : rec.readAt;
    const QStringList cells = {
        fmtTime(when),
        rec.barcode,
        fmtTime(rec.vacuumStart),
        QString::number(rec.vacuumDuration, 'f', 0),
        QString::number(rec.vacuumLevel, 'f', 1),
        QString::number(rec.oilDuration, 'f', 0),
        QString::number(rec.oilQty, 'f', 1),
        rec.refrigerantName,
        fmtTime(rec.chargeStart),
        QString::number(rec.chargeDuration, 'f', 0),
        QString::number(rec.refrigerantQty, 'f', 1),
    };

    for (int c = 0; c < cells.size(); ++c)
        m_history->setItem(row, c, new QTableWidgetItem(cells.at(c)));
}

void ChannelPanel::loadHistory(const QVector<ResultRecord> &records)
{
    m_history->setRowCount(0);
    for (const ResultRecord &r : records)
        insertHistoryRow(m_history->rowCount(), r);   // 传进来的已是倒序
}

void ChannelPanel::appendLog(const QDateTime &time, int level, const QString &message)
{
    const auto lv = static_cast<Logger::Level>(level);
    m_log->appendPlainText(QStringLiteral("%1 [%2] %3")
                               .arg(time.toString(QStringLiteral("HH:mm:ss.zzz")),
                                    Logger::levelText(lv), message));
}

} // namespace cm8600
