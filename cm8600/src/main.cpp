#include "app/AppConfig.h"
#include "app/Logger.h"
#include "core/Records.h"
#include "plc/Snap7Lib.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>

using namespace cm8600;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("CM8600Host"));
    QApplication::setOrganizationName(QStringLiteral("CM8600"));

    // 通道线程用队列连接把这两个结构发给界面线程
    qRegisterMetaType<ChargeParams>("ChargeParams");
    qRegisterMetaType<ResultRecord>("ResultRecord");
    qRegisterMetaType<ChargeParams>("cm8600::ChargeParams");
    qRegisterMetaType<ResultRecord>("cm8600::ResultRecord");

    // 可以用命令行指定配置文件，方便用 config.offline.json 接模拟器跑
    QString configPath =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/config.json"));
    if (argc > 1) {
        const QString arg = QString::fromLocal8Bit(argv[1]);
        configPath = QDir::isAbsolutePath(arg)
                         ? arg
                         : QDir(QCoreApplication::applicationDirPath()).filePath(arg);
    }

    AppConfig cfg;
    QString err;
    if (!AppConfig::load(configPath, &cfg, &err)) {
        QMessageBox::critical(nullptr, QStringLiteral("配置错误"), err);
        return 1;
    }

    // 日志目录相对程序目录，避免受工作目录影响
    const QString logDir = QDir::isAbsolutePath(cfg.logDir)
                               ? cfg.logDir
                               : QDir(QCoreApplication::applicationDirPath()).filePath(cfg.logDir);
    logger().init(logDir, cfg.logKeepDays);
    logger().info(0, QStringLiteral("程序启动，配置来自 %1").arg(QDir::toNativeSeparators(configPath)));

    // snap7 缺失不阻止启动：界面照样能开，日志和状态条上会写明通讯不可用
    QString snapErr;
    if (!Snap7Lib::instance().load(cfg.snap7DllPath, &snapErr))
        logger().error(0, snapErr);
    else
        logger().info(0, QStringLiteral("已加载 %1").arg(Snap7Lib::instance().libraryPath()));

    MainWindow w(cfg);
    if (!w.initialize(&err)) {
        QMessageBox::critical(nullptr, QStringLiteral("启动失败"), err);
        return 2;
    }

    w.show();
    return app.exec();
}
