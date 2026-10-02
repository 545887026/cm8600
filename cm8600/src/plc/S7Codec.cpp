#include "plc/S7Codec.h"

#include <QLocale>
#include <QtEndian>

#include <cstring>

namespace cm8600 {
namespace S7Codec {

namespace {

bool canRead(const QByteArray &buf, int off, int len)
{
    return off >= 0 && len > 0 && off + len <= buf.size();
}

bool canWrite(const QByteArray &buf, int off, int len)
{
    return off >= 0 && len > 0 && off + len <= buf.size();
}

} // namespace

// ---------------------------------------------------------------- Real ----

float getReal(const QByteArray &buf, int off, bool *ok)
{
    if (!canRead(buf, off, 4)) {
        if (ok) *ok = false;
        return 0.0f;
    }
    quint32 raw = 0;
    std::memcpy(&raw, buf.constData() + off, 4);
    raw = qFromBigEndian(raw);

    float value = 0.0f;
    std::memcpy(&value, &raw, 4);   // 不能用 reinterpret_cast，会违反严格别名

    if (ok) *ok = true;
    return value;
}

bool setReal(QByteArray &buf, int off, float value)
{
    if (!canWrite(buf, off, 4))
        return false;

    quint32 raw = 0;
    std::memcpy(&raw, &value, 4);
    raw = qToBigEndian(raw);
    std::memcpy(buf.data() + off, &raw, 4);
    return true;
}

// --------------------------------------------------------------- Int16 ----

qint16 getInt16(const QByteArray &buf, int off, bool *ok)
{
    if (!canRead(buf, off, 2)) {
        if (ok) *ok = false;
        return 0;
    }
    quint16 raw = 0;
    std::memcpy(&raw, buf.constData() + off, 2);
    raw = qFromBigEndian(raw);

    if (ok) *ok = true;
    return static_cast<qint16>(raw);
}

bool setInt16(QByteArray &buf, int off, qint16 value)
{
    if (!canWrite(buf, off, 2))
        return false;

    quint16 raw = qToBigEndian(static_cast<quint16>(value));
    std::memcpy(buf.data() + off, &raw, 2);
    return true;
}

// ---------------------------------------------------------------- DInt ----

qint32 getDInt(const QByteArray &buf, int off, bool *ok)
{
    if (!canRead(buf, off, 4)) {
        if (ok) *ok = false;
        return 0;
    }
    quint32 raw = 0;
    std::memcpy(&raw, buf.constData() + off, 4);
    raw = qFromBigEndian(raw);

    if (ok) *ok = true;
    return static_cast<qint32>(raw);
}

bool setDInt(QByteArray &buf, int off, qint32 value)
{
    if (!canWrite(buf, off, 4))
        return false;

    quint32 raw = qToBigEndian(static_cast<quint32>(value));
    std::memcpy(buf.data() + off, &raw, 4);
    return true;
}

// ----------------------------------------------------------------- Bit ----

bool getBit(const QByteArray &buf, int byteOff, int bitOff)
{
    if (!canRead(buf, byteOff, 1) || bitOff < 0 || bitOff > 7)
        return false;
    return (static_cast<quint8>(buf.at(byteOff)) >> bitOff) & 0x01;
}

bool setBit(QByteArray &buf, int byteOff, int bitOff, bool value)
{
    if (!canWrite(buf, byteOff, 1) || bitOff < 0 || bitOff > 7)
        return false;

    quint8 b = static_cast<quint8>(buf.at(byteOff));
    if (value)
        b |= static_cast<quint8>(1u << bitOff);
    else
        b &= static_cast<quint8>(~(1u << bitOff));
    buf[byteOff] = static_cast<char>(b);
    return true;
}

// --------------------------------------------------------------- ASCII ----

QString readAscii(const QByteArray &buf, int off, int len)
{
    if (!canRead(buf, off, len))
        return {};

    QByteArray raw = buf.mid(off, len);

    // PLC 侧可能用 \0 填充，截到第一个 \0
    const int nul = raw.indexOf('\0');
    if (nul >= 0)
        raw.truncate(nul);

    return QString::fromLatin1(raw).trimmed();
}

QByteArray makeAscii(const QString &text, int len, char pad)
{
    if (len <= 0)
        return {};

    QByteArray raw = text.toLatin1();
    if (raw.size() > len)
        raw = raw.left(len);            // 超长截断，宁可丢尾也不越界写邻居字段

    return QByteArray(len - raw.size(), pad) + raw;   // 左侧补齐，与 PLC 一致
}

// ------------------------------------------------------------ 业务解析 ----

QDateTime parseDateTime(const QString &date, const QString &time)
{
    const QString d = date.trimmed();
    const QString t = time.trimmed();
    if (d.isEmpty())
        return {};

    static const char *dateFormats[] = { "yyyy/MM/dd", "yyyy-MM-dd", "yyyy/M/d", "yyyy-M-d" };
    static const char *timeFormats[] = { "HH:mm:ss", "H:m:s", "HH:mm" };

    QDate qd;
    for (const char *f : dateFormats) {
        qd = QDate::fromString(d, QLatin1String(f));
        if (qd.isValid())
            break;
    }
    if (!qd.isValid())
        return {};

    QTime qt(0, 0, 0);
    if (!t.isEmpty()) {
        for (const char *f : timeFormats) {
            const QTime parsed = QTime::fromString(t, QLatin1String(f));
            if (parsed.isValid()) {
                qt = parsed;
                break;
            }
        }
    }

    return QDateTime(qd, qt);
}

bool parseNumber(const QString &text, double *out)
{
    const QString s = text.trimmed();
    if (s.isEmpty())
        return false;

    bool ok = false;
    const double v = QLocale::c().toDouble(s, &ok);
    if (!ok)
        return false;

    if (out)
        *out = v;
    return true;
}

} // namespace S7Codec
} // namespace cm8600
