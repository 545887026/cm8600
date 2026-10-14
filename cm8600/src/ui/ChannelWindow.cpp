#include "ui/ChannelWindow.h"

#include "ui/ChannelPanel.h"

#include <QCloseEvent>
#include <QFont>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace cm8600 {

ChannelWindow::ChannelWindow(const QString &title, ChannelPanel *panel, bool isPrimary,
                             QWidget *parent)
    : QMainWindow(parent)
    , m_isPrimary(isPrimary)
{
    setWindowTitle(title);
    resize(1200, 950);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(6, 6, 6, 6);

    panel->setParent(central);
    root->addWidget(panel, 1);

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

void ChannelWindow::appendGlobalLog(const QString &line)
{
    m_globalLog->appendPlainText(line);
}

void ChannelWindow::setSnap7Text(const QString &text, bool alert)
{
    m_snapLabel->setText(text);
    m_snapLabel->setStyleSheet(alert ? QStringLiteral("color:#c0392b; font-weight:bold;")
                                     : QString());
}

void ChannelWindow::setSourceText(const QString &text)
{
    m_sourceLabel->setText(text);
}

void ChannelWindow::setQueueText(const QString &text, bool alert)
{
    m_queueLabel->setText(text);
    m_queueLabel->setStyleSheet(alert ? QStringLiteral("color:#c0392b; font-weight:bold;")
                                      : QString());
}

void ChannelWindow::closeEvent(QCloseEvent *event)
{
    if (m_isPrimary) {
        emit closed();
        QMainWindow::closeEvent(event);
        return;
    }

    // 副窗口：关掉只是隐藏，对应通道继续在后台跑
    event->ignore();
    hide();
}

} // namespace cm8600
