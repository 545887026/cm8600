#include "ui/MainWindow.h"

#include "app/Logger.h"
#include "core/ChannelWorker.h"
#include "datasource/MockParamSource.h"
#include "plc/Snap7Lib.h"
#include "storage/Database.h"
#include "ui/ChannelPanel.h"
#include "upload/UploadService.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace cm8600 {

namespace {

constexpr int kHistoryPreload = 100;

/// 相对路径一律相对程序目录，避免受快捷方式的“起始位置”影响
QString resolvePath(const QString &p)
{
    if (p.isEmpty() || QDir::isAbsolutePath(p))
        return p;
    return QDir(QCoreApplication::applicationDirPath()).filePath(p);
}

} // namespace

MainWindow::MainWindow(const AppConfig &cfg, QWidget *parent)
    : QMainWindow(parent)
    , m_cfg(cfg)
{
    setWindowTitle(m_cfg.title);
    resize(1600, 950);
    buildUi();

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
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(6, 6, 6, 6);

    // 左右分屏：一边一个通道
    auto *splitter = new QSplitter(Qt::Horizontal, central);
    for (const ChannelConfig &ch : std::as_const(m_cfg.channels)) {
        auto *panel = new ChannelPanel(ch, splitter);
        splitter->addWidget(panel);
        m_panels.insert(ch.id, panel);
    }
    for (int i = 0; i < splitter->count(); ++i)
        splitter->setStretchFactor(i, 1);

    root->addWidget(splitter, 1);

    auto *logBox = new QGroupBox(QStringLiteral("系统日志"), central);
    auto *logLayout = new QVBoxLayout(logBox);
    m_globalLog = new QPlainTextEdit(logBox);
    m_globalLog->setReadOnly(true);
    m_globalLog->setMaximumBlockCount(3000);
    m_globalLog->setFont(QFont(QStringLiteral("Consolas"), 9));
    logLayout->addWidget(m_globalLog);
    logBox->setMaximumHeight(160);
    root->addWidget(logBox);

    setCentralWidget(central);

    m_snapLabel   = new QLabel(this);
    m_sourceLabel = new QLabel(this);
    m_queueLabel  = new QLabel(this);
    statusBar()->addWidget(m_snapLabel);
    statusBar()->addWidget(new QLabel(QStringLiteral("  |  "), this));
    statusBar()->addWidget(m_sourceLabel);
    statusBar()->addPermanentWidget(m_queueLabel);
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
    m_sourceLabel->setText(QStringLiteral("数据源：%1").arg(m_source->describe()));

    // ---- 上传服务 ----
    m_uploader = new UploadService(m_cfg.upload, m_db, this);
    connect(m_uploader, &UploadService::queueChanged, this, &MainWindow::onQueueChanged);
    m_uploader->start();

    // ---- snap7 状态 ----
    if (Snap7Lib::instance().isLoaded()) {
        m_snapLabel->setText(QStringLiteral("snap7：%1")
                                 .arg(QDir::toNativeSeparators(Snap7Lib::instance().libraryPath())));
    } else {
        m_snapLabel->setText(QStringLiteral("snap7：未加载，PLC 通讯不可用"));
        m_snapLabel->setStyleSheet(QStringLiteral("color:#c0392b; font-weight:bold;"));
    }

    // ---- 历史记录预载 ----
    for (auto it = m_panels.constBegin(); it != m_panels.constEnd(); ++it)
        it.value()->loadHistory(m_db->recentRecords(it.key(), kHistoryPreload));

    return startChannels(err);
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

        ChannelPanel *panel = m_panels.value(ch.id, nullptr);
        if (panel) {
            connect(worker, &ChannelWorker::linkChanged, panel,
                    [panel](int, bool connected, const QString &detail) {
                        panel->setLink(connected, detail);
                    });
            connect(worker, &ChannelWorker::peerAliveChanged, panel,
                    [panel](int, bool alive) { panel->setPeerAlive(alive); });
            connect(worker, &ChannelWorker::queryHandled, panel,
                    [panel](int, const QString &barcode, bool ok, const ChargeParams &p,
                            const QString &reason) {
                        panel->onQueryHandled(barcode, ok, p, reason);
                    });
            connect(worker, &ChannelWorker::recordSaved, panel,
                    [panel](int, const ResultRecord &rec, bool duplicate, qint64) {
                        panel->onRecordSaved(rec, duplicate);
                    });
        }

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

void MainWindow::closeEvent(QCloseEvent *event)
{
    stopChannels();
    if (m_uploader)
        m_uploader->stop();
    event->accept();
}

void MainWindow::onLogged(const QDateTime &time, int level, int channelId, const QString &message)
{
    if (ChannelPanel *panel = m_panels.value(channelId, nullptr))
        panel->appendLog(time, level, message);

    const auto lv = static_cast<Logger::Level>(level);
    const QString chText = channelId > 0 ? QStringLiteral("%1#").arg(channelId)
                                         : QStringLiteral("--");
    m_globalLog->appendPlainText(QStringLiteral("%1 [%2] %3 %4")
                                     .arg(time.toString(QStringLiteral("HH:mm:ss.zzz")),
                                          Logger::levelText(lv), chText, message));
}

void MainWindow::onQueueChanged(int pending, int dead)
{
    QString text = QStringLiteral("待上传：%1").arg(pending);
    if (dead > 0)
        text += QStringLiteral("    已放弃：%1").arg(dead);

    m_queueLabel->setText(text);
    m_queueLabel->setStyleSheet(pending > 0 || dead > 0
                                    ? QStringLiteral("color:#c0392b; font-weight:bold;")
                                    : QString());
}

} // namespace cm8600
