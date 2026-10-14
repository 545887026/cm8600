#include "storage/Database.h"

#include "app/Logger.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QThread>
#include <QVariant>

namespace cm8600 {

namespace {

constexpr auto kIso = "yyyy-MM-dd HH:mm:ss";

QString isoOrNull(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString(QLatin1String(kIso)) : QString();
}

QDateTime fromIso(const QString &s)
{
    return s.isEmpty() ? QDateTime() : QDateTime::fromString(s, QLatin1String(kIso));
}

QString connectionNameForCurrentThread()
{
    return QStringLiteral("cm8600_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

bool reportQueryError(const QSqlQuery &q, const QString &what, QString *err)
{
    const QString msg = QStringLiteral("%1 失败：%2").arg(what, q.lastError().text());
    if (err)
        *err = msg;
    logger().error(0, msg);
    return false;
}

} // namespace

// ------------------------------------------------------------ UploadTask ---

QString UploadTask::kindText(Kind k)
{
    switch (k) {
    case Kind::Vacuum:   return QStringLiteral("vacuum");
    case Kind::Charge:   return QStringLiteral("charge");
    case Kind::Combined: return QStringLiteral("combined");
    }
    return QStringLiteral("combined");
}

UploadTask::Kind UploadTask::kindFromText(const QString &s)
{
    if (s == QLatin1String("vacuum"))
        return Kind::Vacuum;
    if (s == QLatin1String("charge"))
        return Kind::Charge;
    return Kind::Combined;
}

// -------------------------------------------------------------- Database ---

Database::~Database()
{
    close();
}

QSqlDatabase Database::connection(bool *ok, QString *err) const
{
    if (ok)
        *ok = false;

    if (m_path.isEmpty()) {
        if (err)
            *err = QStringLiteral("数据库尚未打开");
        return QSqlDatabase();
    }

    const QString name = connectionNameForCurrentThread();

    if (QSqlDatabase::contains(name)) {
        QSqlDatabase db = QSqlDatabase::database(name);
        if (db.isOpen()) {
            if (ok)
                *ok = true;
            return db;
        }
        QSqlDatabase::removeDatabase(name);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(m_path);
    if (!db.open()) {
        const QString msg = QStringLiteral("打开数据库 %1 失败：%2").arg(m_path, db.lastError().text());
        if (err)
            *err = msg;
        QSqlDatabase::removeDatabase(name);
        return QSqlDatabase();
    }

    // WAL 让读写并发，busy_timeout 让并发写自动等待而不是立刻报 database is locked
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=5000"));
    pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));

    if (ok)
        *ok = true;
    return db;
}

bool Database::open(const QString &path, QString *err)
{
    const QFileInfo fi(path);
    const QDir dir = fi.absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (err)
            *err = QStringLiteral("无法创建数据库目录 %1").arg(dir.absolutePath());
        return false;
    }

    m_path = fi.absoluteFilePath();

    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok) {
        m_path.clear();
        return false;
    }

    if (!createSchema(db, err)) {
        m_path.clear();
        return false;
    }

    logger().info(0, QStringLiteral("本地数据库就绪：%1").arg(QDir::toNativeSeparators(m_path)));
    return true;
}

void Database::close()
{
    if (m_path.isEmpty())
        return;

    // 只能关掉当前线程这条；其它线程的连接随其线程结束一起释放
    const QString name = connectionNameForCurrentThread();
    if (QSqlDatabase::contains(name)) {
        {
            QSqlDatabase db = QSqlDatabase::database(name);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(name);
    }
    m_path.clear();
}

bool Database::createSchema(QSqlDatabase &db, QString *err)
{
    static const char *ddl[] = {
        "CREATE TABLE IF NOT EXISTS record ("
        "  id               INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  dedup_key        TEXT    NOT NULL UNIQUE,"
        "  channel_id       INTEGER NOT NULL,"
        "  barcode          TEXT    NOT NULL,"
        "  vacuum_start     TEXT,"
        "  vacuum_duration  REAL,"
        "  vacuum_level     REAL,"
        "  charge_start     TEXT,"
        "  charge_duration  REAL,"
        "  refrigerant_name TEXT,"
        "  refrigerant_code INTEGER,"
        "  refrigerant_qty  REAL,"
        "  oil_duration     REAL,"
        "  oil_qty          REAL,"
        "  raw_fields       TEXT,"
        "  raw_block        BLOB,"
        "  read_at          TEXT    NOT NULL,"
        "  created_at       TEXT    NOT NULL)",

        "CREATE INDEX IF NOT EXISTS idx_record_barcode ON record(barcode)",
        "CREATE INDEX IF NOT EXISTS idx_record_read_at ON record(read_at)",

        "CREATE TABLE IF NOT EXISTS upload_task ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  record_id       INTEGER NOT NULL REFERENCES record(id),"
        "  kind            TEXT    NOT NULL,"
        "  url             TEXT    NOT NULL,"
        "  payload         TEXT    NOT NULL,"
        "  status          INTEGER NOT NULL DEFAULT 0,"
        "  attempts        INTEGER NOT NULL DEFAULT 0,"
        "  last_error      TEXT,"
        "  next_attempt_at TEXT    NOT NULL,"
        "  created_at      TEXT    NOT NULL,"
        "  done_at         TEXT)",

        "CREATE INDEX IF NOT EXISTS idx_upload_due ON upload_task(status, next_attempt_at, id)",
    };

    QSqlQuery q(db);
    for (const char *sql : ddl) {
        if (!q.exec(QLatin1String(sql)))
            return reportQueryError(q, QStringLiteral("建表"), err);
    }
    return true;
}

// ------------------------------------------------------------------ 记录 ---

bool Database::saveRecord(const ResultRecord &rec, qint64 *rowId, bool *duplicate, QString *err)
{
    if (duplicate)
        *duplicate = false;

    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return false;

    const QString key = rec.dedupKey();

    // 先查重。条码+充注开始时间重复，说明是“读完但没来得及复位标志位”导致的重读
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT id FROM record WHERE dedup_key = ?"));
        q.addBindValue(key);
        if (!q.exec())
            return reportQueryError(q, QStringLiteral("查重"), err);
        if (q.next()) {
            if (rowId)
                *rowId = q.value(0).toLongLong();
            if (duplicate)
                *duplicate = true;
            return true;
        }
    }

    QJsonObject rawObj;
    for (auto it = rec.rawFields.constBegin(); it != rec.rawFields.constEnd(); ++it)
        rawObj.insert(it.key(), it.value());

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO record (dedup_key, channel_id, barcode, vacuum_start, vacuum_duration,"
        " vacuum_level, charge_start, charge_duration, refrigerant_name, refrigerant_code,"
        " refrigerant_qty, oil_duration, oil_qty, raw_fields, raw_block, read_at, created_at)"
        " VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));

    q.addBindValue(key);
    q.addBindValue(rec.channelId);
    q.addBindValue(rec.barcode);
    q.addBindValue(isoOrNull(rec.vacuumStart));
    q.addBindValue(rec.vacuumDuration);
    q.addBindValue(rec.vacuumLevel);
    q.addBindValue(isoOrNull(rec.chargeStart));
    q.addBindValue(rec.chargeDuration);
    q.addBindValue(rec.refrigerantName);
    q.addBindValue(rec.refrigerantCode);
    q.addBindValue(rec.refrigerantQty);
    q.addBindValue(rec.oilDuration);
    q.addBindValue(rec.oilQty);
    q.addBindValue(QString::fromUtf8(QJsonDocument(rawObj).toJson(QJsonDocument::Compact)));
    q.addBindValue(rec.rawBlock);
    q.addBindValue(isoOrNull(rec.readAt.isValid() ? rec.readAt : QDateTime::currentDateTime()));
    q.addBindValue(QDateTime::currentDateTime().toString(QLatin1String(kIso)));

    if (!q.exec())
        return reportQueryError(q, QStringLiteral("写入加注记录"), err);

    if (rowId)
        *rowId = q.lastInsertId().toLongLong();
    return true;
}

bool Database::hasRecord(const QString &dedupKey, bool *exists, QString *err)
{
    if (exists)
        *exists = false;

    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT 1 FROM record WHERE dedup_key = ? LIMIT 1"));
    q.addBindValue(dedupKey);
    if (!q.exec())
        return reportQueryError(q, QStringLiteral("查重"), err);

    if (exists)
        *exists = q.next();
    return true;
}

QVector<ResultRecord> Database::recentRecords(int channelId, int limit, QString *err)
{
    QVector<ResultRecord> list;

    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return list;

    QSqlQuery q(db);
    if (channelId > 0) {
        q.prepare(QStringLiteral(
            "SELECT channel_id, barcode, vacuum_start, vacuum_duration, vacuum_level,"
            " charge_start, charge_duration, refrigerant_name, refrigerant_code,"
            " refrigerant_qty, oil_duration, oil_qty, read_at"
            " FROM record WHERE channel_id = ? ORDER BY id DESC LIMIT ?"));
        q.addBindValue(channelId);
    } else {
        q.prepare(QStringLiteral(
            "SELECT channel_id, barcode, vacuum_start, vacuum_duration, vacuum_level,"
            " charge_start, charge_duration, refrigerant_name, refrigerant_code,"
            " refrigerant_qty, oil_duration, oil_qty, read_at"
            " FROM record ORDER BY id DESC LIMIT ?"));
    }
    q.addBindValue(limit > 0 ? limit : 200);

    if (!q.exec()) {
        reportQueryError(q, QStringLiteral("查询历史记录"), err);
        return list;
    }

    while (q.next()) {
        ResultRecord r;
        r.channelId        = q.value(0).toInt();
        r.barcode          = q.value(1).toString();
        r.vacuumStart      = fromIso(q.value(2).toString());
        r.vacuumDuration   = q.value(3).toDouble();
        r.vacuumLevel      = q.value(4).toDouble();
        r.chargeStart      = fromIso(q.value(5).toString());
        r.chargeDuration   = q.value(6).toDouble();
        r.refrigerantName  = q.value(7).toString();
        r.refrigerantCode  = q.value(8).toInt();
        r.refrigerantQty   = q.value(9).toDouble();
        r.oilDuration      = q.value(10).toDouble();
        r.oilQty           = q.value(11).toDouble();
        r.readAt           = fromIso(q.value(12).toString());
        list << r;
    }
    return list;
}

QVector<ResultRecord> Database::recordsInRange(int channelId, const QDateTime &from,
                                               const QDateTime &to, QString *err)
{
    QVector<ResultRecord> list;

    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return list;

    // 按需拼 WHERE 子句，而不是写死四个版本的 prepare
    QString sql = QStringLiteral(
        "SELECT channel_id, barcode, vacuum_start, vacuum_duration, vacuum_level,"
        " charge_start, charge_duration, refrigerant_name, refrigerant_code,"
        " refrigerant_qty, oil_duration, oil_qty, read_at"
        " FROM record");

    QStringList where;
    QVector<QVariant> binds;
    if (channelId > 0) {
        where << QStringLiteral("channel_id = ?");
        binds << channelId;
    }
    if (from.isValid()) {
        where << QStringLiteral("read_at >= ?");
        binds << from.toString(QLatin1String(kIso));
    }
    if (to.isValid()) {
        where << QStringLiteral("read_at <= ?");
        binds << to.toString(QLatin1String(kIso));
    }
    if (!where.isEmpty())
        sql += QStringLiteral(" WHERE ") + where.join(QStringLiteral(" AND "));
    sql += QStringLiteral(" ORDER BY read_at ASC, id ASC");

    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &v : binds)
        q.addBindValue(v);

    if (!q.exec()) {
        reportQueryError(q, QStringLiteral("按时间段查询历史记录"), err);
        return list;
    }

    while (q.next()) {
        ResultRecord r;
        r.channelId        = q.value(0).toInt();
        r.barcode          = q.value(1).toString();
        r.vacuumStart      = fromIso(q.value(2).toString());
        r.vacuumDuration   = q.value(3).toDouble();
        r.vacuumLevel      = q.value(4).toDouble();
        r.chargeStart      = fromIso(q.value(5).toString());
        r.chargeDuration   = q.value(6).toDouble();
        r.refrigerantName  = q.value(7).toString();
        r.refrigerantCode  = q.value(8).toInt();
        r.refrigerantQty   = q.value(9).toDouble();
        r.oilDuration      = q.value(10).toDouble();
        r.oilQty           = q.value(11).toDouble();
        r.readAt           = fromIso(q.value(12).toString());
        list << r;
    }
    return list;
}

int Database::purgeOldRecords(int keepDays, QString *err)
{
    if (keepDays <= 0)
        return 0;

    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return 0;

    const QString cutoff =
        QDateTime::currentDateTime().addDays(-keepDays).toString(QLatin1String(kIso));

    // 只删“所有上传任务都已完成”的记录；还没传上去的一条都不能删
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "DELETE FROM record WHERE read_at < ? AND id NOT IN"
        " (SELECT record_id FROM upload_task WHERE status <> 1)"));
    q.addBindValue(cutoff);

    if (!q.exec()) {
        reportQueryError(q, QStringLiteral("清理历史记录"), err);
        return 0;
    }
    return q.numRowsAffected();
}

// -------------------------------------------------------------- 上传队列 ---

bool Database::enqueueUpload(qint64 recordId, UploadTask::Kind kind, const QString &url,
                             const QByteArray &payload, qint64 *taskId, QString *err)
{
    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return false;

    const QString now = QDateTime::currentDateTime().toString(QLatin1String(kIso));

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO upload_task (record_id, kind, url, payload, status, attempts,"
        " next_attempt_at, created_at) VALUES (?,?,?,?,0,0,?,?)"));
    q.addBindValue(recordId);
    q.addBindValue(UploadTask::kindText(kind));
    q.addBindValue(url);
    q.addBindValue(QString::fromUtf8(payload));
    q.addBindValue(now);   // 立刻可发
    q.addBindValue(now);

    if (!q.exec())
        return reportQueryError(q, QStringLiteral("加入上传队列"), err);

    if (taskId)
        *taskId = q.lastInsertId().toLongLong();
    return true;
}

QVector<UploadTask> Database::dueUploads(int limit, QString *err)
{
    QVector<UploadTask> list;

    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return list;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, record_id, kind, url, payload, attempts, last_error, next_attempt_at"
        " FROM upload_task WHERE status = 0 AND next_attempt_at <= ?"
        " ORDER BY id ASC LIMIT ?"));   // 按 id 升序：同一条记录的真空段先于充注段发出
    q.addBindValue(QDateTime::currentDateTime().toString(QLatin1String(kIso)));
    q.addBindValue(limit > 0 ? limit : 20);

    if (!q.exec()) {
        reportQueryError(q, QStringLiteral("取待上传任务"), err);
        return list;
    }

    while (q.next()) {
        UploadTask t;
        t.id            = q.value(0).toLongLong();
        t.recordId      = q.value(1).toLongLong();
        t.kind          = UploadTask::kindFromText(q.value(2).toString());
        t.url           = q.value(3).toString();
        t.payload       = q.value(4).toString().toUtf8();
        t.attempts      = q.value(5).toInt();
        t.lastError     = q.value(6).toString();
        t.nextAttemptAt = fromIso(q.value(7).toString());
        list << t;
    }
    return list;
}

bool Database::markUploadDone(qint64 taskId, QString *err)
{
    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE upload_task SET status = 1, last_error = NULL, done_at = ? WHERE id = ?"));
    q.addBindValue(QDateTime::currentDateTime().toString(QLatin1String(kIso)));
    q.addBindValue(taskId);

    if (!q.exec())
        return reportQueryError(q, QStringLiteral("标记上传成功"), err);
    return true;
}

bool Database::markUploadFailed(qint64 taskId, const QString &error, int retryIntervalMs,
                                int maxRetry, QString *err)
{
    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return false;

    const int delayMs = retryIntervalMs > 0 ? retryIntervalMs : 30000;
    const QString next =
        QDateTime::currentDateTime().addMSecs(delayMs).toString(QLatin1String(kIso));

    QSqlQuery q(db);
    if (maxRetry > 0) {
        // 超过次数上限标记为 Dead，不再自动重发，但记录仍留在库里等人工处理
        q.prepare(QStringLiteral(
            "UPDATE upload_task SET attempts = attempts + 1, last_error = ?,"
            " next_attempt_at = ?,"
            " status = CASE WHEN attempts + 1 >= ? THEN 2 ELSE 0 END"
            " WHERE id = ?"));
        q.addBindValue(error);
        q.addBindValue(next);
        q.addBindValue(maxRetry);
        q.addBindValue(taskId);
    } else {
        q.prepare(QStringLiteral(
            "UPDATE upload_task SET attempts = attempts + 1, last_error = ?,"
            " next_attempt_at = ? WHERE id = ?"));
        q.addBindValue(error);
        q.addBindValue(next);
        q.addBindValue(taskId);
    }

    if (!q.exec())
        return reportQueryError(q, QStringLiteral("标记上传失败"), err);
    return true;
}

int Database::pendingUploadCount(QString *err)
{
    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return 0;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM upload_task WHERE status = 0"))) {
        reportQueryError(q, QStringLiteral("统计待上传"), err);
        return 0;
    }
    return q.next() ? q.value(0).toInt() : 0;
}

int Database::deadUploadCount(QString *err)
{
    bool ok = false;
    QSqlDatabase db = connection(&ok, err);
    if (!ok)
        return 0;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM upload_task WHERE status = 2"))) {
        reportQueryError(q, QStringLiteral("统计放弃的上传"), err);
        return 0;
    }
    return q.next() ? q.value(0).toInt() : 0;
}

} // namespace cm8600
