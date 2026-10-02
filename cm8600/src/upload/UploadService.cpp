#include "upload/UploadService.h"

#include "app/Logger.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace cm8600 {

namespace {

/// baseUrl 和 path 拼接，容忍两边都带或都不带斜杠
QString joinUrl(const QString &base, const QString &path)
{
    QString b = base.trimmed();
    QString p = path.trimmed();

    while (b.endsWith(QLatin1Char('/')))
        b.chop(1);
    if (p.isEmpty())
        return b;
    if (!p.startsWith(QLatin1Char('/')))
        p.prepend(QLatin1Char('/'));

    return b + p;
}

} // namespace

UploadService::UploadService(const UploadConfig &cfg, Database *db, QObject *parent)
    : QObject(parent)
    , m_cfg(cfg)
    , m_db(db)
{
    m_timer.setInterval(2000);
    connect(&m_timer, &QTimer::timeout, this, &UploadService::pump);
}

UploadService::~UploadService()
{
    stop();
}

void UploadService::start()
{
    if (m_running)
        return;
    m_running = true;
    m_timer.start();

    emitQueueChanged();
    logger().info(0, QStringLiteral("上传服务已启动，目标 %1，模式 %2")
                         .arg(m_cfg.baseUrl,
                              m_cfg.isSplit() ? QStringLiteral("分两条（真空段/充注段）")
                                              : QStringLiteral("合并一条")));
    pump();
}

void UploadService::stop()
{
    m_running = false;
    m_timer.stop();
}

void UploadService::kick()
{
    if (m_running)
        pump();
}

QString UploadService::urlFor(UploadTask::Kind kind) const
{
    switch (kind) {
    case UploadTask::Kind::Vacuum:   return joinUrl(m_cfg.baseUrl, m_cfg.vacuumPath);
    case UploadTask::Kind::Charge:   return joinUrl(m_cfg.baseUrl, m_cfg.chargePath);
    case UploadTask::Kind::Combined: return joinUrl(m_cfg.baseUrl, m_cfg.combinedPath);
    }
    return joinUrl(m_cfg.baseUrl, m_cfg.combinedPath);
}

bool UploadService::enqueueRecord(qint64 recordId, const ResultRecord &rec, QString *err)
{
    if (!m_db) {
        if (err)
            *err = QStringLiteral("数据库未就绪");
        return false;
    }

    auto enqueue = [&](UploadTask::Kind kind, const QJsonObject &body) {
        const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
        qint64 taskId = 0;
        if (!m_db->enqueueUpload(recordId, kind, urlFor(kind), payload, &taskId, err))
            return false;
        logger().info(rec.channelId,
                      QStringLiteral("已入上传队列：任务 #%1（%2），条码 %3")
                          .arg(taskId)
                          .arg(UploadTask::kindText(kind), rec.barcode));
        return true;
    };

    if (m_cfg.isSplit()) {
        // 真空段先入队，靠自增 id 保证先发
        if (!enqueue(UploadTask::Kind::Vacuum, rec.toVacuumJson()))
            return false;
        if (!enqueue(UploadTask::Kind::Charge, rec.toChargeJson()))
            return false;
    } else {
        if (!enqueue(UploadTask::Kind::Combined, rec.toCombinedJson()))
            return false;
    }

    emitQueueChanged();
    kick();
    return true;
}

void UploadService::emitQueueChanged()
{
    if (!m_db)
        return;
    emit queueChanged(m_db->pendingUploadCount(), m_db->deadUploadCount());
}

void UploadService::pump()
{
    if (!m_running || m_busy || !m_db)
        return;

    const QVector<UploadTask> due = m_db->dueUploads(1);
    if (due.isEmpty())
        return;

    const UploadTask task = due.first();

    const QUrl url(task.url);
    if (!url.isValid() || url.host().isEmpty()) {
        const QString msg = QStringLiteral("上传地址非法：\"%1\"").arg(task.url);
        logger().error(0, msg);
        m_db->markUploadFailed(task.id, msg, m_cfg.retryIntervalMs, m_cfg.maxRetry);
        emitQueueChanged();
        return;
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    req.setTransferTimeout(m_cfg.timeoutMs > 0 ? m_cfg.timeoutMs : 10000);
    for (auto it = m_cfg.headers.constBegin(); it != m_cfg.headers.constEnd(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    m_busy = true;
    QNetworkReply *reply = m_nam.post(req, task.payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply, task] {
        onFinished(reply, task);
    });
}

void UploadService::onFinished(QNetworkReply *reply, const UploadTask &task)
{
    reply->deleteLater();
    m_busy = false;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const QString kindText = UploadTask::kindText(task.kind);

    // 只认 2xx。等甲方给了 ERP 报文规范，再在这里补对响应体的判断
    const bool success = (reply->error() == QNetworkReply::NoError)
                         && status >= 200 && status < 300;

    if (success) {
        m_db->markUploadDone(task.id);
        logger().info(0, QStringLiteral("上传成功：任务 #%1（%2）HTTP %3")
                             .arg(task.id).arg(kindText).arg(status));
        emit uploaded(task.id, kindText, true, QString::number(status));
    } else {
        const QString msg = reply->error() != QNetworkReply::NoError
            ? QStringLiteral("%1").arg(reply->errorString())
            : QStringLiteral("HTTP %1：%2").arg(status).arg(QString::fromUtf8(body.left(200)));

        m_db->markUploadFailed(task.id, msg, m_cfg.retryIntervalMs, m_cfg.maxRetry);

        logger().warn(0, QStringLiteral("上传失败：任务 #%1（%2）%3；%4 秒后重试")
                             .arg(task.id)
                             .arg(kindText, msg)
                             .arg(m_cfg.retryIntervalMs / 1000.0, 0, 'f', 1));
        emit uploaded(task.id, kindText, false, msg);
    }

    emitQueueChanged();

    // 成功就接着发下一条，别等定时器；失败则等重试间隔，避免断网时空转
    if (success)
        pump();
}

} // namespace cm8600
