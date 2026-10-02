// CM8600 假 PLC 模拟器。
//
// 起一个（或两个）S7 服务端，手动走扫码触发 / 加注完成的流程，
// 让上位机在不接设备的情况下跑通整条链路。
//
// 用法：cm8600_sim [config.json 路径]
// 地址表从配置里读，和上位机用同一份，改地址两边同时生效。

#include "app/AppConfig.h"
#include "plc/Snap7Lib.h"
#include "sim/SimPanel.h"

#include <QApplication>
#include <QDir>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSplitter>
#include <QWidget>

#include <utility>

using namespace cm8600;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("CM8600Sim"));

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

    // 模拟器没有 snap7 就什么都做不了，这里直接拦住
    QString snapErr;
    if (!Snap7Lib::instance().load(cfg.snap7DllPath, &snapErr)) {
        QMessageBox::critical(nullptr, QStringLiteral("snap7 未加载"), snapErr);
        return 2;
    }
    if (!Snap7Lib::instance().Srv_Create) {
        QMessageBox::critical(nullptr, QStringLiteral("snap7 不支持服务端"),
                              QStringLiteral("当前 snap7.dll 里没有 Srv_* 接口，模拟器无法运行。\n"
                                             "请使用完整版 snap7（snap7-full）。"));
        return 3;
    }

    QWidget window;
    window.setWindowTitle(QStringLiteral("CM8600 假 PLC 模拟器 —— 配置：%1")
                              .arg(QDir::toNativeSeparators(configPath)));
    window.resize(1400, 900);

    auto *layout = new QHBoxLayout(&window);
    auto *splitter = new QSplitter(Qt::Horizontal, &window);

    for (const ChannelConfig &ch : std::as_const(cfg.channels))
        splitter->addWidget(new SimPanel(cfg, ch, splitter));

    for (int i = 0; i < splitter->count(); ++i)
        splitter->setStretchFactor(i, 1);

    layout->addWidget(splitter);
    window.show();

    return app.exec();
}
