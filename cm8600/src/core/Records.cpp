#include "core/Records.h"

#include "app/AppConfig.h"
#include "plc/S7Codec.h"

namespace cm8600 {

namespace {

constexpr auto kIsoFormat = "yyyy-MM-dd HH:mm:ss";

QString isoOrEmpty(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString(QLatin1String(kIsoFormat)) : QString();
}

/// 取字段文本，缺失时返回空串
QString textOf(const QMap<QString, QString> &fields, const char *key)
{
    return fields.value(QLatin1String(key));
}

/// 解析数值字段；失败时记一条警告，值保持 0
double numberOf(const QMap<QString, QString> &fields, const char *key,
                const QString &label, QStringList *warnings)
{
    const QString raw = textOf(fields, key);
    if (raw.isEmpty()) {
        if (warnings)
            *warnings << QStringLiteral("%1 为空").arg(label);
        return 0.0;
    }
    double v = 0.0;
    if (!S7Codec::parseNumber(raw, &v)) {
        if (warnings)
            *warnings << QStringLiteral("%1 不是数字：\"%2\"").arg(label, raw);
        return 0.0;
    }
    return v;
}

} // namespace

QString ResultRecord::dedupKey() const
{
    // 充注开始时间取到秒；同一条码重复加注时时间必然不同，足以区分。
    // 抽真空阶段单独上传时还没有充注时间，退而用抽真空开始时间，
    // 前缀 C/V 保证两个阶段的行不会互相顶掉。
    if (chargeStart.isValid())
        return QStringLiteral("%1|C|%2").arg(barcode, isoOrEmpty(chargeStart));
    if (vacuumStart.isValid())
        return QStringLiteral("%1|V|%2").arg(barcode, isoOrEmpty(vacuumStart));
    return QStringLiteral("%1|R|%2").arg(barcode, isoOrEmpty(readAt));
}

QJsonObject ResultRecord::toVacuumJson() const
{
    // 字段名是暂定的：甲方 ERP 报文规范还没给，等规范到位后在这里对齐即可，
    // 其余代码不受影响。
    return QJsonObject{
        { QStringLiteral("barcode"),        barcode },
        { QStringLiteral("channel"),        channelId },
        { QStringLiteral("vacuumStart"),    isoOrEmpty(vacuumStart) },
        { QStringLiteral("vacuumDuration"), vacuumDuration },
        { QStringLiteral("vacuumLevel"),    vacuumLevel },
        { QStringLiteral("durationUnit"),   QStringLiteral("s") },
        { QStringLiteral("levelUnit"),      QStringLiteral("Pa") },
    };
}

QJsonObject ResultRecord::toChargeJson() const
{
    return QJsonObject{
        { QStringLiteral("barcode"),         barcode },
        { QStringLiteral("channel"),         channelId },
        { QStringLiteral("chargeStart"),     isoOrEmpty(chargeStart) },
        { QStringLiteral("chargeDuration"),  chargeDuration },
        { QStringLiteral("refrigerantType"), refrigerantName },
        { QStringLiteral("refrigerantCode"), refrigerantCode },
        { QStringLiteral("refrigerantQty"),  refrigerantQty },
        { QStringLiteral("oilDuration"),     oilDuration },
        { QStringLiteral("oilQty"),          oilQty },
        { QStringLiteral("qtyUnit"),         QStringLiteral("g") },
        { QStringLiteral("durationUnit"),    QStringLiteral("s") },
    };
}

QJsonObject ResultRecord::toCombinedJson() const
{
    QJsonObject o = toVacuumJson();
    const QJsonObject c = toChargeJson();
    for (auto it = c.constBegin(); it != c.constEnd(); ++it)
        o.insert(it.key(), it.value());
    return o;
}

bool ResultRecord::parse(const AppConfig &cfg, int channelId, const QByteArray &block,
                         ResultRecord *out, QStringList *warnings)
{
    if (!out)
        return false;

    const ResultConfig &res = cfg.plc.result;

    if (block.size() != res.blockSize) {
        if (warnings) {
            *warnings << QStringLiteral("结果块长度异常：期望 %1 字节，实际 %2 字节")
                             .arg(res.blockSize).arg(block.size());
        }
        if (block.size() < res.blockSize)
            return false;   // 短了就没法按偏移取字段，直接判失败
    }

    ResultRecord r;
    r.channelId = channelId;
    r.readAt    = QDateTime::currentDateTime();
    r.rawBlock  = block;

    // 先按配置把所有字段原样取出来，原始文本一并留着，方便现场对账
    for (const ResultField &f : res.fields) {
        const int off = f.offsetIn(res.blockAddr);
        r.rawFields.insert(f.key, S7Codec::readAscii(block, off, f.len));
    }

    r.barcode = textOf(r.rawFields, "barcode");
    if (r.barcode.isEmpty() && warnings)
        *warnings << QStringLiteral("产品条码为空");

    r.vacuumStart = S7Codec::parseDateTime(textOf(r.rawFields, "vacuum_start_date"),
                                           textOf(r.rawFields, "vacuum_start_time"));
    if (!r.vacuumStart.isValid() && warnings) {
        *warnings << QStringLiteral("抽真空开始时间无法解析：\"%1 %2\"")
                         .arg(textOf(r.rawFields, "vacuum_start_date"),
                              textOf(r.rawFields, "vacuum_start_time"));
    }

    r.chargeStart = S7Codec::parseDateTime(textOf(r.rawFields, "charge_start_date"),
                                           textOf(r.rawFields, "charge_start_time"));
    if (!r.chargeStart.isValid() && warnings) {
        *warnings << QStringLiteral("充注开始时间无法解析：\"%1 %2\"")
                         .arg(textOf(r.rawFields, "charge_start_date"),
                              textOf(r.rawFields, "charge_start_time"));
    }

    r.vacuumDuration = numberOf(r.rawFields, "vacuum_duration",  QStringLiteral("抽真空时长"),   warnings);
    r.vacuumLevel    = numberOf(r.rawFields, "vacuum_level",     QStringLiteral("抽真空真空度"), warnings);
    r.chargeDuration = numberOf(r.rawFields, "charge_duration",  QStringLiteral("充注时长"),     warnings);
    r.refrigerantQty = numberOf(r.rawFields, "refrigerant_qty",  QStringLiteral("制冷剂加注量"), warnings);
    r.oilDuration    = numberOf(r.rawFields, "oil_duration",     QStringLiteral("油充注时长"),   warnings);
    r.oilQty         = numberOf(r.rawFields, "oil_qty",          QStringLiteral("油加注量"),     warnings);

    r.refrigerantName = textOf(r.rawFields, "refrigerant_type");
    r.refrigerantCode = cfg.refrigerantCode(r.refrigerantName);
    if (r.refrigerantCode == 0 && !r.refrigerantName.isEmpty() && warnings) {
        *warnings << QStringLiteral("制冷剂 \"%1\" 不在对照表里，编号按 0 处理")
                         .arg(r.refrigerantName);
    }

    *out = r;
    return true;
}

} // namespace cm8600
