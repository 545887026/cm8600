// 不接 PLC 的自测：验证地址解析、大端编解码、结果块 ASCII 解析、配置加载。
// 用法：cm8600_selftest [config.json 路径]

#include "app/AppConfig.h"
#include "plc/S7Address.h"
#include "plc/S7Codec.h"

#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

using namespace cm8600;

namespace {

int g_pass = 0;
int g_fail = 0;

QTextStream &out()
{
    static QTextStream s(stdout);
    static bool init = [] {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        s.setEncoding(QStringConverter::Utf8);
#else
        s.setCodec("UTF-8");
#endif
        return true;
    }();
    Q_UNUSED(init)
    return s;
}

void check(bool ok, const QString &what, const QString &detail = {})
{
    if (ok) {
        ++g_pass;
        out() << "  [ok]   " << what << Qt::endl;
    } else {
        ++g_fail;
        out() << "  [FAIL] " << what;
        if (!detail.isEmpty())
            out() << "   -> " << detail;
        out() << Qt::endl;
    }
}

void testAddress()
{
    out() << "地址解析" << Qt::endl;

    auto a = S7Address::parse(QStringLiteral("VB9110"));
    check(a.isValid() && a.area == S7Address::Area::V && a.byteOffset == 9110
              && a.width == 1 && !a.isBit(),
          QStringLiteral("VB9110 -> V 区字节 9110"));

    a = S7Address::parse(QStringLiteral("VW9730"));
    check(a.isValid() && a.width == 2 && a.byteOffset == 9730, QStringLiteral("VW9730 -> 字"));

    a = S7Address::parse(QStringLiteral("VD9700"));
    check(a.isValid() && a.width == 4 && a.byteOffset == 9700, QStringLiteral("VD9700 -> 双字"));

    a = S7Address::parse(QStringLiteral("V9740.1"));
    check(a.isValid() && a.isBit() && a.byteOffset == 9740 && a.bitOffset == 1
              && a.bitIndex() == 9740 * 8 + 1,
          QStringLiteral("V9740.1 -> 位，位序号 %1").arg(9740 * 8 + 1));

    a = S7Address::parse(QStringLiteral("M25.7"));
    check(a.isValid() && a.area == S7Address::Area::M && a.byteOffset == 25 && a.bitOffset == 7,
          QStringLiteral("M25.7 -> M 区位"));

    check(!S7Address::parse(QStringLiteral("V9740")).isValid(),
          QStringLiteral("V9740 缺宽度应判为非法"));
    check(!S7Address::parse(QStringLiteral("M25.8")).isValid(),
          QStringLiteral("M25.8 位号越界应判为非法"));
    check(!S7Address::parse(QStringLiteral("DB1.DBX0.0")).isValid(),
          QStringLiteral("暂不支持的写法应判为非法"));

    check(S7Address::parse(QStringLiteral("VD9700")).toString() == QStringLiteral("VD9700")
              && S7Address::parse(QStringLiteral("M25.7")).toString() == QStringLiteral("M25.7"),
          QStringLiteral("toString 往返一致"));
}

void testCodec()
{
    out() << "大端编解码" << Qt::endl;

    QByteArray buf(16, '\0');

    S7Codec::setReal(buf, 0, 1250.5f);
    S7Codec::setReal(buf, 4, 850.0f);
    S7Codec::setReal(buf, 8, 30.0f);
    S7Codec::setReal(buf, 12, 120.0f);

    bool ok = false;
    check(qFuzzyCompare(S7Codec::getReal(buf, 0, &ok), 1250.5f) && ok,
          QStringLiteral("Real 往返 1250.5"));
    check(qFuzzyCompare(S7Codec::getReal(buf, 12, &ok), 120.0f) && ok,
          QStringLiteral("Real 往返 120.0"));

    // 1250.5f 的 IEEE754 = 0x449C5000，大端存储首字节应是 0x44
    check(static_cast<quint8>(buf.at(0)) == 0x44 && static_cast<quint8>(buf.at(1)) == 0x9C
              && static_cast<quint8>(buf.at(2)) == 0x50 && static_cast<quint8>(buf.at(3)) == 0x00,
          QStringLiteral("Real 字节序为大端 44 9C 50 00"),
          QStringLiteral("实际 %1").arg(QString::fromLatin1(buf.left(4).toHex(' '))));

    QByteArray w(2, '\0');
    S7Codec::setInt16(w, 0, 12);
    check(static_cast<quint8>(w.at(0)) == 0x00 && static_cast<quint8>(w.at(1)) == 0x0C
              && S7Codec::getInt16(w, 0) == 12,
          QStringLiteral("Int16 往返 12（R454C 编号）为大端 00 0C"));

    S7Codec::setInt16(w, 0, -1);
    check(S7Codec::getInt16(w, 0) == -1, QStringLiteral("Int16 负数往返 -1"));

    QByteArray bits(1, '\0');
    S7Codec::setBit(bits, 0, 7, true);
    check(static_cast<quint8>(bits.at(0)) == 0x80 && S7Codec::getBit(bits, 0, 7),
          QStringLiteral("位操作 bit7"));
    S7Codec::setBit(bits, 0, 7, false);
    check(static_cast<quint8>(bits.at(0)) == 0x00, QStringLiteral("位清零"));

    // 越界必须安全返回，不能崩
    check(!S7Codec::setReal(buf, 14, 1.0f), QStringLiteral("越界写入被拒绝"));
    S7Codec::getReal(buf, 99, &ok);
    check(!ok, QStringLiteral("越界读取返回 ok=false"));
}

void testResultBlock(const AppConfig &cfg)
{
    out() << "结果块解析（VB9110~VB9249，140 字节裸 ASCII，左侧补空格）" << Qt::endl;

    const auto &res = cfg.plc.result;
    if (!res.blockAddr.isValid() || res.fields.isEmpty()) {
        check(false, QStringLiteral("结果块配置可用"));
        return;
    }

    // 按配置造一份 PLC 会发出来的样例数据
    const QMap<QString, QString> sample = {
        { QStringLiteral("vacuum_start_date"), QStringLiteral("2026/09/13") },
        { QStringLiteral("vacuum_start_time"), QStringLiteral("18:22:05")   },
        { QStringLiteral("vacuum_duration"),   QStringLiteral("120")        },
        { QStringLiteral("vacuum_level"),      QStringLiteral("30")         },
        { QStringLiteral("charge_duration"),   QStringLiteral("95")         },
        { QStringLiteral("refrigerant_type"),  QStringLiteral("R134a")      },
        { QStringLiteral("refrigerant_qty"),   QStringLiteral("850.5")      },
        { QStringLiteral("charge_start_date"), QStringLiteral("2026/09/13") },
        { QStringLiteral("charge_start_time"), QStringLiteral("18:24:10")   },
        { QStringLiteral("oil_duration"),      QStringLiteral("12")         },
        { QStringLiteral("oil_qty"),           QStringLiteral("60")         },
        { QStringLiteral("barcode"),           QStringLiteral("SZ20260913000123") },
    };

    QByteArray block(res.blockSize, ' ');
    for (const ResultField &f : res.fields) {
        const int off = f.offsetIn(res.blockAddr);
        block.replace(off, f.len, S7Codec::makeAscii(sample.value(f.key), f.len));
    }

    check(block.size() == 140, QStringLiteral("结果块长度 140 字节"),
          QStringLiteral("实际 %1").arg(block.size()));

    // 左侧补空格：抽真空时长字段应为 7 个空格 + "120"
    const ResultField *dur = res.field(QStringLiteral("vacuum_duration"));
    if (dur) {
        const QByteArray raw = block.mid(dur->offsetIn(res.blockAddr), dur->len);
        check(raw == QByteArray("       120"),
              QStringLiteral("时长字段左侧补空格"),
              QStringLiteral("实际 \"%1\"").arg(QString::fromLatin1(raw)));
    }

    // 逐字段解析回来，trim 后应与原值一致
    bool allMatch = true;
    QString mismatch;
    for (const ResultField &f : res.fields) {
        const QString got = S7Codec::readAscii(block, f.offsetIn(res.blockAddr), f.len);
        if (got != sample.value(f.key)) {
            allMatch = false;
            mismatch += QStringLiteral("%1: 期望 \"%2\" 得到 \"%3\"; ")
                            .arg(f.key, sample.value(f.key), got);
        }
    }
    check(allMatch, QStringLiteral("全部 %1 个字段解析回原值").arg(res.fields.size()), mismatch);

    // 日期时间合成
    const QDateTime dt = S7Codec::parseDateTime(
        S7Codec::readAscii(block, res.field(QStringLiteral("charge_start_date"))->offsetIn(res.blockAddr), 10),
        S7Codec::readAscii(block, res.field(QStringLiteral("charge_start_time"))->offsetIn(res.blockAddr), 10));
    check(dt.isValid() && dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                              == QStringLiteral("2026-09-13 18:24:10"),
          QStringLiteral("充注开始时间合成为 2026-09-13 18:24:10"),
          dt.toString(Qt::ISODate));

    // 数值字段
    double qty = 0;
    check(S7Codec::parseNumber(
              S7Codec::readAscii(block, res.field(QStringLiteral("refrigerant_qty"))->offsetIn(res.blockAddr), 10),
              &qty)
              && qFuzzyCompare(qty, 850.5),
          QStringLiteral("制冷剂加注量解析为 850.5 g"));

    // 条码 30 字节
    const ResultField *bc = res.field(QStringLiteral("barcode"));
    check(bc && bc->len == 30, QStringLiteral("产品条码字段为 30 字节"));
}

void testRefrigerantTable(const AppConfig &cfg)
{
    out() << "制冷剂对照表（下发用编号，回读是名称）" << Qt::endl;

    check(cfg.refrigerants.size() == 13, QStringLiteral("共 13 项"),
          QStringLiteral("实际 %1").arg(cfg.refrigerants.size()));
    check(cfg.refrigerantName(1) == QStringLiteral("R134A"), QStringLiteral("1 -> R134A"));
    check(cfg.refrigerantName(12) == QStringLiteral("R454C"), QStringLiteral("12 -> R454C"));
    check(cfg.refrigerantName(9) == QStringLiteral("R1234YF"), QStringLiteral("9 -> R1234YF"));

    // PLC 回读的是 "R134a"，大小写与表里不同，反查必须忽略大小写
    check(cfg.refrigerantCode(QStringLiteral("R134a")) == 1,
          QStringLiteral("按名称反查忽略大小写：R134a -> 1"));
    check(cfg.refrigerantCode(QStringLiteral("R999")) == 0,
          QStringLiteral("未知名称返回 0"));
}

} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
#endif
    QCoreApplication app(argc, argv);

    QString configPath = (argc > 1)
        ? QString::fromLocal8Bit(argv[1])
        : QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/config.json"));

    out() << "=== CM8600 上位机 自测 ===" << Qt::endl;
    out() << "配置文件：" << QDir::toNativeSeparators(configPath) << Qt::endl << Qt::endl;

    testAddress();
    out() << Qt::endl;
    testCodec();
    out() << Qt::endl;

    out() << "配置加载" << Qt::endl;
    AppConfig cfg;
    QString err;
    const bool loaded = AppConfig::load(configPath, &cfg, &err);
    check(loaded, QStringLiteral("config.json 加载并通过校验"), err);

    if (loaded) {
        check(cfg.channels.size() == 2
                  && cfg.channels.at(0).ip == QStringLiteral("192.168.2.1")
                  && cfg.channels.at(1).ip == QStringLiteral("192.168.2.10"),
              QStringLiteral("两个通道 192.168.2.1 / 192.168.2.10"));
        check(cfg.plc.chargeDoneBit.area == S7Address::Area::V
                  && cfg.plc.chargeDoneBit.byteOffset == 9741
                  && cfg.plc.chargeDoneBit.bitOffset == 2,
              QStringLiteral("加注完成标志位 = V9741.2（2026-09-23 由 M25.7 改来）"),
              cfg.plc.chargeDoneBit.toString());

        // 这三个位同在 VB9741 一个字节里，而 .0/.1 归 PLC 写、.2 归我们复位。
        // 所以复位时必须按位写，绝不能读整字节改完再写回去，否则会覆盖 PLC 的心跳和触发位。
        check(cfg.plc.chargeDoneBit.byteOffset == cfg.plc.heartbeat.rxBit.byteOffset
                  && cfg.plc.chargeDoneBit.byteOffset == cfg.plc.query.triggerBit.byteOffset,
              QStringLiteral("加注完成位与 PLC 的心跳位/触发位同在 VB9741，故必须按位写"));

        check(cfg.plc.vacuumTrigger.mode == VacuumTriggerConfig::Mode::OnChargeDone,
              QStringLiteral("不新增抽真空完成位，只认加注完成标志位"));
        check(!cfg.upload.isSplit(),
              QStringLiteral("上传方式 = 合并一条记录"));
        check(cfg.plc.vDbNumber == 1, QStringLiteral("V 区映射 DB1"));

        out() << Qt::endl;
        testResultBlock(cfg);
        out() << Qt::endl;
        testRefrigerantTable(cfg);
    }

    out() << Qt::endl
          << "=== 通过 " << g_pass << " 项，失败 " << g_fail << " 项 ===" << Qt::endl;
    out().flush();

    return g_fail == 0 ? 0 : 1;
}
