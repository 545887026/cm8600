#pragma once

#include "app/AppConfig.h"
#include "core/Records.h"

#include <QHash>
#include <QMainWindow>

class QLabel;
class QPlainTextEdit;
class QThread;

namespace cm8600 {

class ChannelPanel;
class ChannelWorker;
class Database;
class IParamSource;
class UploadService;

/**
 * 主窗口：左右分屏，一边一个通道（1# / 2#），下方是全局日志和状态条。
 *
 * 线程：每个通道一个 QThread 跑 ChannelWorker；上传服务和界面在主线程
 * （QNetworkAccessManager 本身是异步的，不会卡界面）。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const AppConfig &cfg, QWidget *parent = nullptr);
    ~MainWindow() override;

    /// 打开数据库、建数据源、起通道线程。失败时 err 说明原因
    bool initialize(QString *err);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onLogged(const QDateTime &time, int level, int channelId, const QString &message);
    void onQueueChanged(int pending, int dead);

private:
    void buildUi();
    bool startChannels(QString *err);
    void stopChannels();

    AppConfig m_cfg;

    Database      *m_db       = nullptr;
    IParamSource  *m_source   = nullptr;
    UploadService *m_uploader = nullptr;

    QHash<int, ChannelPanel *>  m_panels;
    QHash<int, ChannelWorker *> m_workers;
    QHash<int, QThread *>       m_threads;

    QPlainTextEdit *m_globalLog   = nullptr;
    QLabel         *m_snapLabel   = nullptr;
    QLabel         *m_sourceLabel = nullptr;
    QLabel         *m_queueLabel  = nullptr;
};

} // namespace cm8600
