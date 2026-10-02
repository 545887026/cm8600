#pragma once

#include "app/AppConfig.h"
#include "storage/Database.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

class QNetworkReply;

namespace cm8600 {

/**
 * 上传服务：把本地队列里的任务发到 MES/ERP，断网就攒着，恢复后自动补发。
 *
 * 两个刻意的设计：
 *   1) 同一时刻只发一条。这样同一件工件的“真空段”一定先于“充注段”到达 MES，
 *      顺序由数据库里的自增 id 保证。牺牲一点吞吐换顺序，加注节拍是分钟级，够用。
 *   2) 成功与否只看 HTTP 状态码是不是 2xx。甲方 ERP 的报文规范还没给，
 *      等规范到位后在 isSuccess() 里补上对响应体的判断即可。
 */
class UploadService : public QObject
{
    Q_OBJECT

public:
    UploadService(const UploadConfig &cfg, Database *db, QObject *parent = nullptr);
    ~UploadService() override;

    void start();
    void stop();

    /**
     * 把一条已落库的记录拆成上传任务入队。
     * split 模式下生成两条（真空段、充注段），combined 模式下一条。
     */
    bool enqueueRecord(qint64 recordId, const ResultRecord &rec, QString *err);

public slots:
    /// 队列有新内容时叫一下，不用等定时器
    void kick();

signals:
    void queueChanged(int pending, int dead);
    void uploaded(qint64 taskId, const QString &kind, bool success, const QString &message);

private:
    void pump();
    void onFinished(QNetworkReply *reply, const UploadTask &task);
    QString urlFor(UploadTask::Kind kind) const;
    void emitQueueChanged();

    UploadConfig           m_cfg;
    Database              *m_db = nullptr;
    QNetworkAccessManager  m_nam;
    QTimer                 m_timer;
    bool                   m_busy    = false;
    bool                   m_running = false;
};

} // namespace cm8600
