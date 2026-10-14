#include "ui/MainWindow.h"

#include "app/Logger.h"
#include "core/ChannelWorker.h"
#include "datasource/MockParamSource.h"
#include "plc/Snap7Lib.h"
#include "storage/Database.h"
#include "ui/ChannelPanel.h"
#include "ui/ChannelWindow.h"
#include "upload/UploadService.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QThread>

#include <utility>

namespace cm8600 {

namespace {

constexpr int kHistoryPreload = 100;

/// 相对路径一律相对程序目录，避免受快捷方式的"起始位置"影响
QString resolvePath(const QString &p)
{
    if (p.isEmpty() || QDir::isAbsolutePath(p))
        return p;
    return QDir(QCoreApplication::applicationDirPath()).filePath(p);
}

} // namespace

MainWindow::MainWindow(const AppConfig &cfg, QObject *parent)
    : QObject(parent)
    , m_cfg(cfg)
{
    connect(&logger(), &Logger::logged, this, &MainWindow::onLogged);
}

MainWindow::~MainWindow()
{
    stopChannels();

    delete m_uploader;
    m_uploader = nullptr;
    delete m_source;
    m_source = nullptr;
    delete m_db;
    m_db = nullptr;

    // worker 已经停了，panel 作为窗口的子部件会随窗口一起销毁
    qDeleteAll(m_windows);
    m_windows.clear();
    m_panels.clear();
}

bool MainWindow::initialize(QString *err)
{
    // ---- 本地数据库 ----
    m_db = new Database();
    if (!m_db->open(resolvePath(m_cfg.storage.dbPath), err)) {
        delete m_db;
        m_db = nullptr;
        return false;
    }
    m_db->purgeOldRecords(m_cfg.storage.keepDays);

    // ---- 数据源 ----
    // 甲方那边的文件格式还没定，先用模拟源把全链路跑通
    if (m_cfg.datasource.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) != 0) {
        logger().warn(0, QStringLiteral("数据源类型 \"%1\" 尚未实现，暂时退回模拟数据源")
                             .arg(m_cfg.datasource.type));
    }
    m_source = new MockParamSource(m_cfg);

    // ---- 上传服务 ----
    m_uploader = new UploadService(m_cfg.upload, m_db, this);
    connect(m_uploader, &UploadService::queueChanged, this, &MainWindow::onQueueChanged);
    m_uploader->start();

    return startChannels(err);
}

void MainWindow::showWindows()
{
    const QList<QScreen *> screens = QGuiApplication::screens();

    int idx = 0;
    for (const ChannelConfig &ch : std::as_const(m_cfg.channels)) {
        auto *panel = new ChannelPanel(ch);
        m_panels.insert(ch.id, panel);

        const bool isPrimary = (idx == 0);
        const QString title = QStringLiteral("%1 - %2#%3")
                                  .arg(m_cfg.title)
                                  .arg(ch.id)
                                  .arg(ch.name.isEmpty() ? QString()
                                                         : QStringLiteral(" %1").arg(ch.name));

        auto *window = new ChannelWindow(title, panel, isPrimary);
        m_windows.insert(ch.id, window);

        if (isPrimary)
            connect(window, &ChannelWindow::closed, this, &MainWindow::onPrimaryClosed);

        QScreen *screen = screens.value(idx, screens.isEmpty() ? nullptr : screens.first());
        if (screen) {
            window->move(screen->geometry().topLeft());
            window->showMaximized();
        } else {
            window->show();
        }

        ++idx;
    }

    // ---- 把 initialize 里拿到的几条状态灌到所有窗口 ----
    if (m_source) {
        const QString sourceText = QStringLiteral("数据源：%1").arg(m_source->describe());
        for (ChannelWindow *w : std::as_const(m_windows))
            w->setSourceText(sourceText);
    }

    if (Snap7Lib::instance().isLoaded()) {
        const QString snapText = QStringLiteral("snap7：%1")
                                     .arg(QDir::toNativeSeparators(Snap7Lib::instance().libraryPath()));
        for (ChannelWindow *w : std::as_const(m_windows))
            w->setSnap7Text(snapText, false);
    } else {
        const QString snapText = QStringLiteral("snap7：未加载，PLC 通讯不可用");
        for (ChannelWindow *w : std::as_const(m_windows))
            w->setSnap7Text(snapText, true);
    }

    // ---- 历史记录预载 ----
    if (m_db) {
        for (auto it = m_panels.constBegin(); it != m_panels.constEnd(); ++it)
            it.value()->loadHistory(m_db->recentRecords(it.key(), kHistoryPreload));
    }
}

bool MainWindow::startChannels(QString *err)
{
    Q_UNUSED(err)

    for (const ChannelConfig &ch : std::as_const(m_cfg.channels)) {
        if (!ch.enabled) {
            logger().info(ch.id, QStringLiteral("通道已在配置中停用，不启动"));
            continue;
        }

        auto *thread = new QThread(this);
        thread->setObjectName(QStringLiteral("channel-%1").arg(ch.id));

        auto *worker = new ChannelWorker(m_cfg, ch, m_db, m_source);
        worker->moveToThread(thread);

        connect(thread, &QThread::started,  worker, &ChannelWorker::start);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);

        // worker 的信号用 lambda 通过 m_panels.value(id) 查目标 panel；
        // 这里不能捕获 panel 指针，因为 showWindows() 还没执行时它还不存在。
        const int chId = ch.id;
        connect(worker, &ChannelWorker::linkChanged, this,
                [this, chId](int, bool connected, const QString &detail) {
                    if (ChannelPanel *p = m_panels.value(chId, nullptr))
                        p->setLink(connected, detail);
                });
        connect(worker, &ChannelWorker::peerAliveChanged, this,
                [this, chId](int, bool alive) {
                    if (ChannelPanel *p = m_panels.value(chId, nullptr))
                        p->setPeerAlive(alive);
                });
        connect(worker, &ChannelWorker::queryHandled, this,
                [this, chId](int, const QString &barcode, bool ok, const ChargeParams &p,
                             const QString &reason) {
                    if (ChannelPanel *panel = m_panels.value(chId, nullptr))
                        panel->onQueryHandled(barcode, ok, p, reason);
                });
        connect(worker, &ChannelWorker::recordSaved, this,
                [this, chId](int, const ResultRecord &rec, bool duplicate, qint64) {
                    if (ChannelPanel *panel = m_panels.value(chId, nullptr))
                        panel->onRecordSaved(rec, duplicate);
                });

        // 记录入库后立刻排上传；重复读进来的不再发
        connect(worker, &ChannelWorker::recordSaved, this,
                [this](int channelId, const ResultRecord &rec, bool duplicate, qint64 rowId) {
                    if (duplicate || !m_uploader)
                        return;
                    QString e;
                    if (!m_uploader->enqueueRecord(rowId, rec, &e))
                        logger().error(channelId, QStringLiteral("入上传队列失败：%1").arg(e));
                });

        m_workers.insert(ch.id, worker);
        m_threads.insert(ch.id, thread);
        thread->start();
    }

    if (m_workers.isEmpty())
        logger().warn(0, QStringLiteral("没有任何通道被启用，检查 config.json 的 channels.enabled"));

    return true;
}

void MainWindow::stopChannels()
{
    for (auto it = m_workers.constBegin(); it != m_workers.constEnd(); ++it) {
        // stop() 必须在 worker 自己的线程里跑，阻塞等它收尾再退线程
        QMetaObject::invokeMethod(it.value(), "stop", Qt::BlockingQueuedConnection);
    }

    for (auto it = m_threads.constBegin(); it != m_threads.constEnd(); ++it) {
        QThread *t = it.value();
        t->quit();
        if (!t->wait(5000))
            logger().warn(it.key(), QStringLiteral("通道线程未在 5 秒内退出，强制终止"));
    }

    m_workers.clear();
    m_threads.clear();
}

void MainWindow::shutdown()
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;

    stopChannels();
    if (m_uploader)
        m_uploader->stop();

    // 副窗口的 closeEvent 会把事件 ignore 掉改成 hide，不能靠
    // quitOnLastWindowClosed；主窗口关的时候直接退 app
    QCoreApplication::quit();
}

void MainWindow::onPrimaryClosed()
{
    shutdown();
}

void MainWindow::onLogged(const QDateTime &time, int level, int channelId, const QString &message)
{
    if (ChannelPanel *panel = m_panels.value(channelId, nullptr))
        panel->appendLog(time, level, message);

    const auto lv = static_cast<Logger::Level>(level);
    const QString chText = channelId > 0 ? QStringLiteral("%1#").arg(channelId)
                                         : QStringLiteral("--");
    const QString line = QStringLiteral("%1 [%2] %3 %4")
                             .arg(time.toString(QStringLiteral("HH:mm:ss.zzz")),
                                  Logger::levelText(lv), chText, message);

    for (ChannelWindow *w : std::as_const(m_windows))
        w->appendGlobalLog(line);
}

void MainWindow::onQueueChanged(int pending, int dead)
{
    QString text = QStringLiteral("待上传：%1").arg(pending);
    if (dead > 0)
        text += QStringLiteral("    已放弃：%1").arg(dead);

    const bool alert = pending > 0 || dead > 0;
    for (ChannelWindow *w : std::as_const(m_windows))
        w->setQueueText(text, alert);
}

} // namespace cm8600
