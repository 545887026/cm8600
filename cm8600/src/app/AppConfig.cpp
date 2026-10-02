#include "app/AppConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>


namespace cm8600 {

const ResultField *ResultConfig::field(const QString &key) const
{
    for (const ResultField &f : fields) {
        if (f.key == key)
            return &f;
    }
    return nullptr;
}

int AppConfig::refrigerantCode(const QString &name) const
{
    const QString target = name.trimmed();
    if (target.isEmpty())
        return 0;

    for (auto it = refrigerants.constBegin(); it != refrigerants.constEnd(); ++it) {
        if (it.value().compare(target, Qt::CaseInsensitive) == 0)
            return it.key();
    }
    return 0;
}

QString AppConfig::refrigerantName(int code) const
{
    return refrigerants.value(code);
}

namespace {

/// 收集所有配置问题，一次性报给用户，省得改一条重启一次
class Collector
{
public:
    QStringList errors;

    void add(const QString &msg) { errors << msg; }

    S7Address addr(const QJsonObject &obj, const QString &key, const QString &where,
                   bool required = true)
    {
        const QString text = obj.value(key).toString();
        if (text.trimmed().isEmpty()) {
            if (required)
                add(QStringLiteral("%1.%2 未填写").arg(where, key));
            return {};
        }
        QString err;
        const S7Address a = S7Address::parse(text, &err);
        if (!a.isValid())
            add(QStringLiteral("%1.%2：%3").arg(where, key, err));
        return a;
    }

    int intOr(const QJsonObject &obj, const QString &key, int fallback) const
    {
        const QJsonValue v = obj.value(key);
        return v.isDouble() ? v.toInt() : fallback;
    }

    QString strOr(const QJsonObject &obj, const QString &key, const QString &fallback) const
    {
        const QJsonValue v = obj.value(key);
        return v.isString() ? v.toString() : fallback;
    }
};

DownloadItem readDownloadItem(Collector &c, const QJsonObject &parent, const QString &key,
                              const QString &where)
{
    DownloadItem item;
    const QJsonObject obj = parent.value(key).toObject();
    if (obj.isEmpty()) {
        c.add(QStringLiteral("%1.%2 缺失").arg(where, key));
        return item;
    }
    item.addr = c.addr(obj, QStringLiteral("addr"), QStringLiteral("%1.%2").arg(where, key));
    item.type = c.strOr(obj, QStringLiteral("type"), QStringLiteral("real"));
    item.unit = c.strOr(obj, QStringLiteral("unit"), QString());
    return item;
}

} // namespace

bool AppConfig::load(const QString &path, AppConfig *out, QString *err)
{
    if (!out)
        return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err)
            *err = QStringLiteral("打不开配置文件 %1：%2").arg(path, f.errorString());
        return false;
    }

    QJsonParseError parseErr{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseErr);
    f.close();

    if (parseErr.error != QJsonParseError::NoError) {
        if (err) {
            *err = QStringLiteral("配置文件 %1 不是合法 JSON（偏移 %2）：%3")
                       .arg(path)
                       .arg(parseErr.offset)
                       .arg(parseErr.errorString());
        }
        return false;
    }
    if (!doc.isObject()) {
        if (err)
            *err = QStringLiteral("配置文件 %1 的顶层必须是对象").arg(path);
        return false;
    }

    const QJsonObject root = doc.object();
    AppConfig cfg;
    Collector c;

    // ---- app ----
    {
        const QJsonObject o = root.value(QStringLiteral("app")).toObject();
        cfg.title       = c.strOr(o, QStringLiteral("title"), cfg.title);
        cfg.logDir      = c.strOr(o, QStringLiteral("log_dir"), cfg.logDir);
        cfg.logKeepDays = c.intOr(o, QStringLiteral("log_keep_days"), cfg.logKeepDays);
    }

    // ---- snap7 ----
    cfg.snap7DllPath = root.value(QStringLiteral("snap7")).toObject()
                           .value(QStringLiteral("dll_path")).toString();

    // ---- channels ----
    {
        const QJsonArray arr = root.value(QStringLiteral("channels")).toArray();
        if (arr.isEmpty())
            c.add(QStringLiteral("channels 为空，至少要配置一个通道"));

        for (int i = 0; i < arr.size(); ++i) {
            const QJsonObject o = arr.at(i).toObject();
            const QString where = QStringLiteral("channels[%1]").arg(i);

            ChannelConfig ch;
            ch.id      = c.intOr(o, QStringLiteral("id"), i + 1);
            ch.name    = c.strOr(o, QStringLiteral("name"), QStringLiteral("%1# 系统").arg(ch.id));
            ch.ip      = o.value(QStringLiteral("ip")).toString().trimmed();
            ch.rack    = c.intOr(o, QStringLiteral("rack"), 0);
            ch.slot    = c.intOr(o, QStringLiteral("slot"), 1);
            ch.port    = c.intOr(o, QStringLiteral("port"), 102);
            ch.enabled = o.value(QStringLiteral("enabled")).toBool(true);

            if (ch.ip.isEmpty())
                c.add(QStringLiteral("%1.ip 未填写").arg(where));

            cfg.channels << ch;
        }
    }

    // ---- plc ----
    {
        const QJsonObject plc = root.value(QStringLiteral("plc")).toObject();
        if (plc.isEmpty())
            c.add(QStringLiteral("plc 节点缺失"));

        cfg.plc.vDbNumber           = c.intOr(plc, QStringLiteral("v_db_number"), 1);
        cfg.plc.pollIntervalMs      = c.intOr(plc, QStringLiteral("poll_interval_ms"), 200);
        cfg.plc.connectTimeoutMs    = c.intOr(plc, QStringLiteral("connect_timeout_ms"), 3000);
        cfg.plc.reconnectIntervalMs = c.intOr(plc, QStringLiteral("reconnect_interval_ms"), 3000);

        // heartbeat
        {
            const QJsonObject o = plc.value(QStringLiteral("heartbeat")).toObject();
            const QString where = QStringLiteral("plc.heartbeat");
            cfg.plc.heartbeat.txBit     = c.addr(o, QStringLiteral("tx_bit"), where);
            cfg.plc.heartbeat.rxBit     = c.addr(o, QStringLiteral("rx_bit"), where);
            cfg.plc.heartbeat.toggleMs  = c.intOr(o, QStringLiteral("toggle_ms"), 500);
            cfg.plc.heartbeat.timeoutMs = c.intOr(o, QStringLiteral("timeout_ms"), 3000);

            if (cfg.plc.heartbeat.timeoutMs <= cfg.plc.heartbeat.toggleMs * 2) {
                c.add(QStringLiteral("%1.timeout_ms (%2) 必须明显大于 toggle_ms 的两倍 (%3)，否则正常翻转也会误判断链")
                          .arg(where)
                          .arg(cfg.plc.heartbeat.timeoutMs)
                          .arg(cfg.plc.heartbeat.toggleMs * 2));
            }
        }

        // query
        {
            const QJsonObject o = plc.value(QStringLiteral("query")).toObject();
            const QString where = QStringLiteral("plc.query");
            cfg.plc.query.triggerBit  = c.addr(o, QStringLiteral("trigger_bit"), where);
            cfg.plc.query.barcodeAddr = c.addr(o, QStringLiteral("barcode_addr"), where);
            cfg.plc.query.barcodeLen  = c.intOr(o, QStringLiteral("barcode_len"), 30);
            cfg.plc.query.okBit       = c.addr(o, QStringLiteral("ok_bit"), where);
            cfg.plc.query.failBit     = c.addr(o, QStringLiteral("fail_bit"), where);

            if (cfg.plc.query.barcodeLen <= 0)
                c.add(QStringLiteral("%1.barcode_len 必须大于 0").arg(where));
        }

        // download
        {
            const QJsonObject o = plc.value(QStringLiteral("download")).toObject();
            const QString where = QStringLiteral("plc.download");
            cfg.plc.download.blockAddr = c.addr(o, QStringLiteral("block_addr"), where);
            cfg.plc.download.blockSize = c.intOr(o, QStringLiteral("block_size"), 16);

            cfg.plc.download.oilQty          = readDownloadItem(c, o, QStringLiteral("oil_qty"), where);
            cfg.plc.download.refrigerantQty  = readDownloadItem(c, o, QStringLiteral("refrigerant_qty"), where);
            cfg.plc.download.vacuumLevel     = readDownloadItem(c, o, QStringLiteral("vacuum_level"), where);
            cfg.plc.download.vacuumTime      = readDownloadItem(c, o, QStringLiteral("vacuum_time"), where);
            cfg.plc.download.refrigerantType = readDownloadItem(c, o, QStringLiteral("refrigerant_type"), where);

            // 四个 Real 必须落在 block 内，否则一次性写块会越界
            const S7Address blk = cfg.plc.download.blockAddr;
            const int blkSize   = cfg.plc.download.blockSize;
            const DownloadItem reals[] = { cfg.plc.download.oilQty, cfg.plc.download.refrigerantQty,
                                           cfg.plc.download.vacuumLevel, cfg.plc.download.vacuumTime };
            if (blk.isValid()) {
                for (const DownloadItem &it : reals) {
                    if (!it.addr.isValid())
                        continue;
                    const int off = it.addr.byteOffset - blk.byteOffset;
                    if (off < 0 || off + 4 > blkSize) {
                        c.add(QStringLiteral("%1：%2 不在下发块 %3+%4 字节范围内")
                                  .arg(where, it.addr.toString(), blk.toString())
                                  .arg(blkSize));
                    }
                }
            }
        }

        // charge_done_bit
        {
            QString e;
            const QString text = plc.value(QStringLiteral("charge_done_bit")).toString();
            cfg.plc.chargeDoneBit = S7Address::parse(text, &e);
            if (!cfg.plc.chargeDoneBit.isValid())
                c.add(QStringLiteral("plc.charge_done_bit：%1").arg(e));
        }

        // vacuum_trigger
        {
            const QJsonObject o = plc.value(QStringLiteral("vacuum_trigger")).toObject();
            const QString mode  = c.strOr(o, QStringLiteral("mode"), QStringLiteral("on_charge_done"));

            if (mode.compare(QStringLiteral("flag"), Qt::CaseInsensitive) == 0) {
                cfg.plc.vacuumTrigger.mode = VacuumTriggerConfig::Mode::Flag;
                cfg.plc.vacuumTrigger.bit  = c.addr(o, QStringLiteral("bit"),
                                                    QStringLiteral("plc.vacuum_trigger"));
            } else if (mode.compare(QStringLiteral("on_charge_done"), Qt::CaseInsensitive) == 0) {
                cfg.plc.vacuumTrigger.mode = VacuumTriggerConfig::Mode::OnChargeDone;
            } else {
                c.add(QStringLiteral("plc.vacuum_trigger.mode 只能是 flag 或 on_charge_done，当前为 \"%1\"").arg(mode));
            }
        }

        // result
        {
            const QJsonObject o = plc.value(QStringLiteral("result")).toObject();
            const QString where = QStringLiteral("plc.result");
            cfg.plc.result.blockAddr = c.addr(o, QStringLiteral("block_addr"), where);
            cfg.plc.result.blockSize = c.intOr(o, QStringLiteral("block_size"), 140);

            const QJsonArray arr = o.value(QStringLiteral("fields")).toArray();
            if (arr.isEmpty())
                c.add(QStringLiteral("%1.fields 为空").arg(where));

            for (int i = 0; i < arr.size(); ++i) {
                const QJsonObject fo = arr.at(i).toObject();
                const QString fwhere = QStringLiteral("%1.fields[%2]").arg(where).arg(i);

                ResultField fld;
                fld.key   = fo.value(QStringLiteral("key")).toString();
                fld.label = c.strOr(fo, QStringLiteral("label"), fld.key);
                fld.addr  = c.addr(fo, QStringLiteral("addr"), fwhere);
                fld.len   = c.intOr(fo, QStringLiteral("len"), 10);
                fld.type  = c.strOr(fo, QStringLiteral("type"), QStringLiteral("ascii"));
                fld.unit  = c.strOr(fo, QStringLiteral("unit"), QString());
                fld.hint  = c.strOr(fo, QStringLiteral("hint"), QString());

                if (fld.key.isEmpty())
                    c.add(QStringLiteral("%1.key 未填写").arg(fwhere));
                if (fld.len <= 0)
                    c.add(QStringLiteral("%1.len 必须大于 0").arg(fwhere));

                // 字段必须落在结果块内，否则解析时会读到缓冲区外
                if (cfg.plc.result.blockAddr.isValid() && fld.addr.isValid()) {
                    const int off = fld.offsetIn(cfg.plc.result.blockAddr);
                    if (off < 0 || off + fld.len > cfg.plc.result.blockSize) {
                        c.add(QStringLiteral("%1：%2 长度 %3 超出结果块 %4+%5 字节")
                                  .arg(fwhere, fld.addr.toString())
                                  .arg(fld.len)
                                  .arg(cfg.plc.result.blockAddr.toString())
                                  .arg(cfg.plc.result.blockSize));
                    }
                }

                cfg.plc.result.fields << fld;
            }
        }
    }

    // ---- refrigerants ----
    {
        const QJsonObject o = root.value(QStringLiteral("refrigerants")).toObject();
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            bool ok = false;
            const int code = it.key().toInt(&ok);
            if (!ok)
                continue;   // "_说明" 之类的注释键，跳过
            cfg.refrigerants.insert(code, it.value().toString().trimmed());
        }
        if (cfg.refrigerants.isEmpty())
            c.add(QStringLiteral("refrigerants 对照表为空"));
    }

    // ---- datasource ----
    {
        const QJsonObject o = root.value(QStringLiteral("datasource")).toObject();
        cfg.datasource.type              = c.strOr(o, QStringLiteral("type"), QStringLiteral("mock"));
        cfg.datasource.filePath          = c.strOr(o, QStringLiteral("file_path"), QString());
        cfg.datasource.refreshIntervalMs = c.intOr(o, QStringLiteral("refresh_interval_ms"), 60000);
        cfg.datasource.cacheDb           = c.strOr(o, QStringLiteral("cache_db"), QStringLiteral("param_cache.db"));
    }

    // ---- upload ----
    {
        const QJsonObject o = root.value(QStringLiteral("upload")).toObject();
        cfg.upload.mode            = c.strOr(o, QStringLiteral("mode"), QStringLiteral("split"));
        cfg.upload.baseUrl         = c.strOr(o, QStringLiteral("base_url"), QString());
        cfg.upload.vacuumPath      = c.strOr(o, QStringLiteral("vacuum_path"), QString());
        cfg.upload.chargePath      = c.strOr(o, QStringLiteral("charge_path"), QString());
        cfg.upload.combinedPath    = c.strOr(o, QStringLiteral("combined_path"), QString());
        cfg.upload.timeoutMs       = c.intOr(o, QStringLiteral("timeout_ms"), 10000);
        cfg.upload.retryIntervalMs = c.intOr(o, QStringLiteral("retry_interval_ms"), 30000);
        cfg.upload.maxRetry        = c.intOr(o, QStringLiteral("max_retry"), 0);

        const QJsonObject h = o.value(QStringLiteral("headers")).toObject();
        for (auto it = h.constBegin(); it != h.constEnd(); ++it)
            cfg.upload.headers.insert(it.key(), it.value().toString());
    }

    // ---- storage ----
    {
        const QJsonObject o = root.value(QStringLiteral("storage")).toObject();
        cfg.storage.dbPath   = c.strOr(o, QStringLiteral("db_path"), QStringLiteral("cm8600.db"));
        cfg.storage.keepDays = c.intOr(o, QStringLiteral("keep_days"), 365);
    }

    if (!c.errors.isEmpty()) {
        if (err) {
            *err = QStringLiteral("配置文件 %1 有 %2 处问题：\n  - %3")
                       .arg(path)
                       .arg(c.errors.size())
                       .arg(c.errors.join(QStringLiteral("\n  - ")));
        }
        return false;
    }

    *out = cfg;
    return true;
}

} // namespace cm8600
