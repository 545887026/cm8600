#pragma once

#include <QMainWindow>
#include <QString>

class QLabel;
class QPlainTextEdit;

namespace cm8600 {

class ChannelPanel;

/**
 * 单通道的顶层窗口：承载一个 ChannelPanel + 全局日志 + 状态条。
 * 多显示器环境下每通道一个，由 MainWindow 创建并分屏投放。
 */
class ChannelWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// panel 的父对象会被转到本窗口的 central widget 上
    ChannelWindow(const QString &title, ChannelPanel *panel, bool isPrimary,
                  QWidget *parent = nullptr);

    void appendGlobalLog(const QString &line);
    void setSnap7Text(const QString &text, bool alert);
    void setSourceText(const QString &text);
    void setQueueText(const QString &text, bool alert);

signals:
    /// 仅主窗口在关闭时发出，副窗口关闭只隐藏自己
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    bool            m_isPrimary;
    QPlainTextEdit *m_globalLog   = nullptr;
    QLabel         *m_snapLabel   = nullptr;
    QLabel         *m_sourceLabel = nullptr;
    QLabel         *m_queueLabel  = nullptr;
};

} // namespace cm8600
