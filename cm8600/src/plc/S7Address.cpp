#include "plc/S7Address.h"

#include <QRegularExpression>

namespace cm8600 {

S7Address S7Address::parse(const QString &text, QString *err)
{
    S7Address addr;
    const QString s = text.trimmed().toUpper();

    auto fail = [&](const QString &why) {
        if (err)
            *err = QStringLiteral("地址 \"%1\" 无法解析：%2").arg(text, why);
        return S7Address{};
    };

    if (s.isEmpty())
        return fail(QStringLiteral("为空"));

    // [区号 V/M] [宽度 B/W/D 可选] [字节偏移] [.位号 可选]
    static const QRegularExpression re(
        QStringLiteral("^([VM])([BWD])?(\\d+)(?:\\.([0-7]))?$"));

    const auto m = re.match(s);
    if (!m.hasMatch())
        return fail(QStringLiteral("格式应为 VB1234 / VW1234 / VD1234 / V1234.0 / M25.7"));

    addr.area       = (m.captured(1) == QLatin1String("V")) ? Area::V : Area::M;
    addr.byteOffset = m.captured(3).toInt();

    const QString widthChar = m.captured(2);
    const QString bitChar   = m.captured(4);

    if (!bitChar.isEmpty()) {
        // 位地址：V9740.0、M25.7、（也容忍 VB9740.0）
        if (!widthChar.isEmpty() && widthChar != QLatin1String("B"))
            return fail(QStringLiteral("位地址不能带 W/D 宽度"));
        addr.bitOffset = bitChar.toInt();
        addr.width     = 0;
    } else {
        if (widthChar.isEmpty())
            return fail(QStringLiteral("缺少宽度字母（B/W/D）或位号"));
        addr.width = (widthChar == QLatin1String("B")) ? 1
                   : (widthChar == QLatin1String("W")) ? 2
                                                       : 4;
        addr.bitOffset = -1;
    }

    return addr;
}

QString S7Address::toString() const
{
    if (!isValid())
        return QStringLiteral("<无效地址>");

    const QChar areaChar = (area == Area::V) ? QLatin1Char('V') : QLatin1Char('M');

    if (isBit())
        return QStringLiteral("%1%2.%3").arg(areaChar).arg(byteOffset).arg(bitOffset);

    const QChar widthChar = (width == 1) ? QLatin1Char('B')
                          : (width == 2) ? QLatin1Char('W')
                                         : QLatin1Char('D');
    return QStringLiteral("%1%2%3").arg(areaChar).arg(widthChar).arg(byteOffset);
}

} // namespace cm8600
