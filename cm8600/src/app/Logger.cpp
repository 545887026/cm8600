#include "app/Logger.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace cm8600 {

namespace {
QMutex g_mutex;   // 递归不需要，日志调用之间不会互相嵌套
}

Logger &Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::~Logger()
{
    delete m_stream;
    if (m_file) {
        m_file->close();
        delete m_file;
    }
}

QString Logger::levelText(Level level)
{
    switch (level) {
    case Level::Debug: return QStringLiteral("调试");
    case Level::Info:  return QStringLiteral("信息");
    case Level::Warn:  return QStringLiteral("警告");
    case Level::Error: return QStringLiteral("错误");
    }
    return QStringLiteral("信息");
}

void Logger::init(const QString &dir, int keepDays)
{
    QMutexLocker lock(&g_mutex);

    m_keepDays = keepDays > 0 ? keepDays : 30;
    m_dir      = dir.trimmed();

    if (m_dir.isEmpty())
        return;

    QDir d(m_dir);
    if (!d.exists() && !d.mkpath(QStringLiteral("."))) {
        qWarning() << "无法创建日志目录" << m_dir << "，日志将只显示在界面上";
        m_dir.clear();
        return;
    }

    purgeOld();
}

void Logger::rotateIfNeeded(const QDate &today)
{
    if (m_dir.isEmpty())
        return;
    if (m_file && m_file->isOpen() && m_openedOn == today)
        return;

    delete m_stream;
    m_stream = nullptr;
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    const QString path = QDir(m_dir).filePath(
        QStringLiteral("cm8600-%1.log").arg(today.toString(QStringLiteral("yyyyMMdd"))));

    auto *f = new QFile(path);
    if (!f->open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "打不开日志文件" << path << f->errorString();
        delete f;
        return;
    }

    m_file     = f;
    m_stream   = new QTextStream(m_file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_stream->setEncoding(QStringConverter::Utf8);
#else
    m_stream->setCodec("UTF-8");
#endif
    m_openedOn = today;

    purgeOld();
}

void Logger::purgeOld()
{
    if (m_dir.isEmpty())
        return;

    const QDate cutoff = QDate::currentDate().addDays(-m_keepDays);
    const QFileInfoList list =
        QDir(m_dir).entryInfoList({QStringLiteral("cm8600-*.log")}, QDir::Files);

    for (const QFileInfo &fi : list) {
        const QString stamp = fi.completeBaseName().section(QLatin1Char('-'), 1, 1);
        const QDate d = QDate::fromString(stamp, QStringLiteral("yyyyMMdd"));
        if (d.isValid() && d < cutoff)
            QFile::remove(fi.absoluteFilePath());
    }
}

void Logger::log(Level level, int channelId, const QString &message)
{
    const QDateTime now = QDateTime::currentDateTime();

    {
        QMutexLocker lock(&g_mutex);
        rotateIfNeeded(now.date());

        if (m_stream) {
            const QString chText = channelId > 0 ? QStringLiteral("%1#").arg(channelId)
                                                 : QStringLiteral("--");
            *m_stream << now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                      << QLatin1Char(' ') << levelText(level)
                      << QLatin1Char(' ') << chText
                      << QLatin1Char(' ') << message << Qt::endl;
        }
    }

    emit logged(now, static_cast<int>(level), channelId, message);
}

} // namespace cm8600
