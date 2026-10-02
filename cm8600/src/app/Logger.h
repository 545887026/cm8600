#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

class QFile;
class QTextStream;

namespace cm8600 {

/**
 * 日志。一份进文件（按天切分），一份用信号发给界面。
 * 线程安全：通道线程和界面线程都会写。
 */
class Logger : public QObject
{
    Q_OBJECT

public:
    enum class Level { Debug, Info, Warn, Error };

    static Logger &instance();

    /// dir 为空则只发信号不落盘
    void init(const QString &dir, int keepDays);

    void log(Level level, int channelId, const QString &message);

    void debug(int ch, const QString &m) { log(Level::Debug, ch, m); }
    void info (int ch, const QString &m) { log(Level::Info,  ch, m); }
    void warn (int ch, const QString &m) { log(Level::Warn,  ch, m); }
    void error(int ch, const QString &m) { log(Level::Error, ch, m); }

    static QString levelText(Level level);

signals:
    /// channelId 为 0 表示不属于任何通道的全局日志
    void logged(const QDateTime &time, int level, int channelId, const QString &message);

private:
    Logger() = default;
    ~Logger() override;

    void rotateIfNeeded(const QDate &today);
    void purgeOld();

    QString m_dir;
    int     m_keepDays = 30;
    QDate   m_openedOn;
    QFile  *m_file   = nullptr;
    QTextStream *m_stream = nullptr;
};

/// 简写，代码里到处要用
inline Logger &logger() { return Logger::instance(); }

} // namespace cm8600
