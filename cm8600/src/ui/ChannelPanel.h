#pragma once

#include "app/AppConfig.h"
#include "core/Records.h"

#include <QDateTime>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QTableWidget;

namespace cm8600 {

class Database;

/**
 * 单通道面板。左右分屏各放一个，分别对应 1# / 2# 系统。
 * 只管显示，不碰通讯；所有数据由 MainWindow 从对应通道的 worker 转发过来。
 *
 * @param db 可选，仅"导出历史记录"用。为空时导出按钮会被隐藏。
 */
class ChannelPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ChannelPanel(const ChannelConfig &channel, Database *db = nullptr,
                          QWidget *parent = nullptr);

    int channelId() const { return m_ch.id; }

public slots:
    void setLink(bool connected, const QString &detail);
    void setPeerAlive(bool alive);
    void onQueryHandled(const QString &barcode, bool ok, const ChargeParams &params,
                        const QString &reason);
    void onRecordSaved(const ResultRecord &record, bool duplicate);
    void appendLog(const QDateTime &time, int level, const QString &message);

    /// 启动时把历史记录填进表格
    void loadHistory(const QVector<ResultRecord> &records);

private:
    QWidget *buildHeader();
    QWidget *buildParamsBox();
    QWidget *buildResultBox();
    QWidget *buildHistoryBox();
    void     insertHistoryRow(int row, const ResultRecord &rec);
    void     exportHistory();

    ChannelConfig m_ch;
    Database     *m_db = nullptr;

    QLabel *m_linkLight   = nullptr;
    QLabel *m_linkText    = nullptr;
    QLabel *m_beatLight   = nullptr;
    QLabel *m_beatText    = nullptr;

    QLabel *m_barcode     = nullptr;
    QLabel *m_queryState  = nullptr;

    QLabel *m_pRefrigerant = nullptr;
    QLabel *m_pRefrigQty   = nullptr;
    QLabel *m_pOilQty      = nullptr;
    QLabel *m_pVacuumLevel = nullptr;
    QLabel *m_pVacuumTime  = nullptr;

    QLabel *m_rVacuumStart    = nullptr;
    QLabel *m_rVacuumDuration = nullptr;
    QLabel *m_rVacuumLevel    = nullptr;
    QLabel *m_rChargeStart    = nullptr;
    QLabel *m_rChargeDuration = nullptr;
    QLabel *m_rRefrigerant    = nullptr;
    QLabel *m_rRefrigQty      = nullptr;
    QLabel *m_rOilQty         = nullptr;

    QTableWidget   *m_history = nullptr;
    QPlainTextEdit *m_log     = nullptr;
};

} // namespace cm8600
