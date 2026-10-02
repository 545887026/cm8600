#pragma once

#include "core/Records.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

class QSqlDatabase;

namespace cm8600 {

/// 一条待上传任务
struct UploadTask
{
    enum class Kind { Vacuum, Charge, Combined };
    enum class Status { Pending = 0, Done = 1, Dead = 2 };

    qint64    id       = 0;
    qint64    recordId = 0;
    Kind      kind     = Kind::Combined;
    QString   url;
    QByteArray payload;          ///< JSON 正文
    int       attempts = 0;
    QString   lastError;
    QDateTime nextAttemptAt;

    static QString kindText(Kind k);
    static Kind    kindFromText(const QString &s);
};

/**
 * 本地存储：加注记录 + 上传重试队列（SQLite）。
 *
 * 线程模型：QSqlDatabase 的连接不能跨线程用，所以这里按线程各开一条连接
 * 指向同一个库文件，开 WAL + busy_timeout，让通道线程和上传线程能并发读写。
 * 因此本类的方法可以在任意线程直接调用，不需要排队。
 */
class Database
{
public:
    Database() = default;
    ~Database();

    Database(const Database &)            = delete;
    Database &operator=(const Database &) = delete;

    /// 打开并建表。path 为文件路径，目录不存在会自动创建
    bool open(const QString &path, QString *err);
    void close();
    bool isOpen() const { return !m_path.isEmpty(); }

    QString path() const { return m_path; }

    // ---- 加注记录 ----

    /**
     * 存一条记录。
     * @param duplicate 若因 dedup_key already exists 而未插入，置 true（不算失败）
     * @param rowId     新记录或已存在记录的主键
     */
    bool saveRecord(const ResultRecord &rec, qint64 *rowId, bool *duplicate, QString *err);

    bool hasRecord(const QString &dedupKey, bool *exists, QString *err);

    /// 最近若干条，channelId <= 0 表示不限通道。界面列表用
    QVector<ResultRecord> recentRecords(int channelId, int limit, QString *err = nullptr);

    /// 删除超过 keepDays 天的记录（只删已上传完成的，没传上去的一律留着）
    int purgeOldRecords(int keepDays, QString *err = nullptr);

    // ---- 上传队列 ----

    bool enqueueUpload(qint64 recordId, UploadTask::Kind kind, const QString &url,
                       const QByteArray &payload, qint64 *taskId, QString *err);

    /// 取到期的待发任务，按创建顺序，保证同一条记录的真空段先于充注段发出
    QVector<UploadTask> dueUploads(int limit, QString *err = nullptr);

    bool markUploadDone(qint64 taskId, QString *err = nullptr);

    /// 失败：累加次数、记错误、推迟下次尝试；maxRetry>0 且超限则标记为 Dead
    bool markUploadFailed(qint64 taskId, const QString &error, int retryIntervalMs,
                          int maxRetry, QString *err = nullptr);

    int pendingUploadCount(QString *err = nullptr);
    int deadUploadCount(QString *err = nullptr);

private:
    /// 取当前线程的连接，没有就现开一条
    QSqlDatabase connection(bool *ok, QString *err) const;
    bool createSchema(QSqlDatabase &db, QString *err);

    QString m_path;
};

} // namespace cm8600
