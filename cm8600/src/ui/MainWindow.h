#pragma once

#include "app/AppConfig.h"
#include "core/Records.h"

#include <QHash>
#include <QObject>

class QThread;

namespace cm8600 {

class ChannelPanel;
class ChannelWindow;
class ChannelWorker;
class Database;
class IParamSource;
class UploadService;

/**
 * 应用控制器：拥有数据库 / 数据源 / 上传服务 / 每通道的 worker 线程，
 * 并给每个通道分配一个独立的 ChannelWindow，启动时按 QScreen 顺序投到
 * 不同显示器上（1# 主屏，2# 副屏）。
 *
 * 关闭策略：主窗口（第一个通道）关闭 → 整个程序停；
 *          副窗口关闭 → 只隐藏窗口，对应通道继续采集。
 *
 * 线程：每个通道一个 QThread 跑 ChannelWorker；上传服务和界面在主线程
 * （QNetworkAccessManager 本身是异步的，不会卡界面）。
 */
class MainWindow : public QObject
{
    Q_OBJECT

public:
    explicit MainWindow(const AppConfig &cfg, QObject *parent = nullptr);
    ~MainWindow() override;

    /// 打开数据库、建数据源、起通道线程。失败时 err 说明原因
    bool initialize(QString *err);

    /// 构建并显示每通道的窗口，按屏分配
    void showWindows();

private slots:
    void onLogged(const QDateTime &time, int level, int channelId, const QString &message);
    void onQueueChanged(int pending, int dead);
    void onPrimaryClosed();

private:
    bool startChannels(QString *err);
    void stopChannels();
    void shutdown();

    AppConfig m_cfg;

    Database      *m_db       = nullptr;
    IParamSource  *m_source   = nullptr;
    UploadService *m_uploader = nullptr;

    QHash<int, ChannelPanel *>  m_panels;
    QHash<int, ChannelWindow *> m_windows;
    QHash<int, ChannelWorker *> m_workers;
    QHash<int, QThread *>       m_threads;

    bool m_shuttingDown = false;
};

} // namespace cm8600
