#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

namespace cm8600 {

/**
 * S7 数据编解码。
 *
 * 两类数据格式，别混：
 *   1) 下发方向（上位机 -> PLC）是二进制：Real 为 32 位大端 IEEE754，Int 为 16 位大端有符号。
 *   2) 回读方向（PLC -> 上位机）是裸 ASCII 定长字段，不是 S7 String（没有前两个长度字节），
 *      不足位数在左侧补空格，所以一律 trim 后再用。
 */
namespace S7Codec {

// ---- 二进制，大端 --------------------------------------------------------

float   getReal (const QByteArray &buf, int off, bool *ok = nullptr);
bool    setReal (QByteArray &buf, int off, float value);

qint16  getInt16(const QByteArray &buf, int off, bool *ok = nullptr);
bool    setInt16(QByteArray &buf, int off, qint16 value);

qint32  getDInt (const QByteArray &buf, int off, bool *ok = nullptr);
bool    setDInt (QByteArray &buf, int off, qint32 value);

bool    getBit  (const QByteArray &buf, int byteOff, int bitOff);
bool    setBit  (QByteArray &buf, int byteOff, int bitOff, bool value);

// ---- 裸 ASCII 定长字段 ---------------------------------------------------

/// 取 [off, off+len) 并去掉两端空白与 \0；越界返回空串
QString readAscii(const QByteArray &buf, int off, int len);

/// 生成定长 ASCII 字段（左侧补 pad），超长则截断。主要给模拟器用
QByteArray makeAscii(const QString &text, int len, char pad = ' ');

// ---- 业务层解析 ----------------------------------------------------------

/// 把 "2026/09/13" + "18:22:05" 合成 QDateTime；兼容 - 分隔与缺前导零
QDateTime parseDateTime(const QString &date, const QString &time);

/// 解析数值字段；失败返回 false（字段为空或非数字）
bool parseNumber(const QString &text, double *out);

} // namespace S7Codec
} // namespace cm8600
