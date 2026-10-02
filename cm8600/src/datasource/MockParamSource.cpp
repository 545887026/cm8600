#include "datasource/MockParamSource.h"

#include "app/AppConfig.h"

#include <QCryptographicHash>
#include <QMutexLocker>

namespace cm8600 {

MockParamSource::MockParamSource(const AppConfig &cfg)
    : m_cfg(cfg)
{
    m_lastSync = QDateTime::currentDateTime();
}

bool MockParamSource::refresh(QString *err)
{
    Q_UNUSED(err)
    QMutexLocker lock(&m_mutex);
    m_lastSync = QDateTime::currentDateTime();
    return true;
}

void MockParamSource::preset(const QString &barcode, const ChargeParams &params)
{
    QMutexLocker lock(&m_mutex);
    m_preset.insert(barcode.trimmed(), params);
}

bool MockParamSource::lookup(const QString &barcode, ChargeParams *out, QString *err)
{
    if (!out)
        return false;

    const QString key = barcode.trimmed();

    if (key.isEmpty()) {
        if (err)
            *err = QStringLiteral("条码为空");
        return false;
    }

    {
        QMutexLocker lock(&m_mutex);
        const auto it = m_preset.constFind(key);
        if (it != m_preset.constEnd()) {
            *out = it.value();
            out->barcode = key;
            out->source  = QStringLiteral("模拟数据源（预置）");
            return true;
        }
    }

    // 留一条制造“查不到”的口子，用来验证查询失败位这条路
    if (key.startsWith(QLatin1String("NG"), Qt::CaseInsensitive)) {
        if (err)
            *err = QStringLiteral("条码 %1 在数据源中不存在").arg(key);
        return false;
    }

    // 拿条码哈希生成稳定的参数：同一条码每次查到的都一样，便于反复验证
    const QByteArray h = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5);
    auto byteAt = [&h](int i) { return static_cast<quint8>(h.at(i)); };

    ChargeParams p;
    p.barcode = key;

    // 制冷剂从对照表里挑一个真实存在的编号（跳过第 13 项“备用”）
    QList<int> codes;
    for (auto it = m_cfg.refrigerants.constBegin(); it != m_cfg.refrigerants.constEnd(); ++it) {
        if (it.value().compare(QStringLiteral("备用"), Qt::CaseInsensitive) != 0)
            codes << it.key();
    }
    if (codes.isEmpty())
        codes << 1;

    p.refrigerantCode = codes.at(byteAt(0) % codes.size());
    p.refrigerantName = m_cfg.refrigerantName(p.refrigerantCode);

    p.refrigerantQty = 500.0 + (byteAt(1) % 101) * 5.0;   // 500 ~ 1000 g
    p.oilQty         = 30.0  + (byteAt(2) % 71);          // 30 ~ 100 g
    p.vacuumLevel    = 30.0  + (byteAt(3) % 101);         // 30 ~ 130 Pa，与技术协议一致
    p.vacuumTime     = 60.0  + (byteAt(4) % 121);         // 60 ~ 180 s
    p.source         = QStringLiteral("模拟数据源（按条码生成）");

    *out = p;
    return true;
}

QString MockParamSource::describe() const
{
    return QStringLiteral("模拟数据源（脱机联调用，条码以 NG 开头则模拟查不到）");
}

QDateTime MockParamSource::lastSyncAt() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastSync;
}

} // namespace cm8600
