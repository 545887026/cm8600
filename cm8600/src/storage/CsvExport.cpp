#include "storage/CsvExport.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>

namespace cm8600 {

namespace {

QString fmtTime(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
}

/// 按 CSV 的字段转义规则处理：含逗号、引号、换行就用引号包起来，内部引号翻倍。
QString csvField(const QString &s)
{
    const bool needQuote = s.contains(QLatin1Char(','))
                        || s.contains(QLatin1Char('"'))
                        || s.contains(QLatin1Char('\n'))
                        || s.contains(QLatin1Char('\r'));
    if (!needQuote)
        return s;

    QString escaped = s;
    escaped.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString joinRow(const QStringList &cells)
{
    QStringList out;
    out.reserve(cells.size());
    for (const QString &c : cells)
        out << csvField(c);
    return out.join(QLatin1Char(','));
}

} // namespace

bool exportResultRecordsCsv(const QString &path, const QVector<ResultRecord> &records,
                            QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err)
            *err = QStringLiteral("无法打开文件：%1").arg(f.errorString());
        return false;
    }

    // UTF-8 BOM：直接写字节，不要让 QTextStream 的 codec 来加
    f.write("\xEF\xBB\xBF");

    QTextStream ts(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    ts.setEncoding(QStringConverter::Utf8);
#else
    ts.setCodec("UTF-8");
#endif

    const QStringList header = {
        QStringLiteral("通道"),
        QStringLiteral("时间"),
        QStringLiteral("条码"),
        QStringLiteral("抽真空开始时间"),
        QStringLiteral("抽真空时长(s)"),
        QStringLiteral("真空度(Pa)"),
        QStringLiteral("冷冻油充注时长(s)"),
        QStringLiteral("冷冻油加注量(g)"),
        QStringLiteral("制冷剂类型"),
        QStringLiteral("制冷剂加注时间"),
        QStringLiteral("制冷剂充注时长(s)"),
        QStringLiteral("制冷剂加注量(g)"),
        QStringLiteral("读取时间"),
    };
    ts << joinRow(header) << QLatin1String("\r\n");

    for (const ResultRecord &rec : records) {
        const QDateTime when = rec.chargeStart.isValid() ? rec.chargeStart : rec.readAt;
        const QStringList row = {
            QString::number(rec.channelId),
            fmtTime(when),
            rec.barcode,
            fmtTime(rec.vacuumStart),
            QString::number(rec.vacuumDuration, 'f', 0),
            QString::number(rec.vacuumLevel, 'f', 1),
            QString::number(rec.oilDuration, 'f', 0),
            QString::number(rec.oilQty, 'f', 1),
            rec.refrigerantName,
            fmtTime(rec.chargeStart),
            QString::number(rec.chargeDuration, 'f', 0),
            QString::number(rec.refrigerantQty, 'f', 1),
            fmtTime(rec.readAt),
        };
        ts << joinRow(row) << QLatin1String("\r\n");
    }

    ts.flush();
    if (f.error() != QFile::NoError) {
        if (err)
            *err = QStringLiteral("写入失败：%1").arg(f.errorString());
        return false;
    }
    return true;
}

} // namespace cm8600
