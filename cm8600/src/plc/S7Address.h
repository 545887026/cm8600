#pragma once

#include <QString>

namespace cm8600 {

/**
 * PLC 地址，支持西门子 S7-200 SMART 的书写习惯：
 *
 *   VB9110   V 区字节，偏移 9110，宽度 1
 *   VW9730   V 区字，  偏移 9730，宽度 2
 *   VD9700   V 区双字，偏移 9700，宽度 4
 *   V9740.0  V 区位，  字节 9740 的第 0 位
 *   M25.7    M 区位，  字节 25 的第 7 位
 *   MB25     M 区字节
 *
 * 注意：V 区在 S7 通讯协议里没有独立区号，它落在 DB1 上，
 * 所以 VB9110 实际要按 “DB1，偏移 9110” 去读写；M 区则有独立区号。
 * 这层转换在 S7Client 里做，本结构只负责解析字面量。
 */
struct S7Address
{
    enum class Area { Unknown, V, M };

    Area area       = Area::Unknown;
    int  byteOffset = -1;   ///< 字节偏移
    int  bitOffset  = -1;   ///< 位号 0~7；-1 表示不是位地址
    int  width      = 0;    ///< 字节宽度：B=1 W=2 D=4；位地址为 0

    bool isBit()   const { return bitOffset >= 0; }
    bool isValid() const { return area != Area::Unknown && byteOffset >= 0; }

    /// 位地址在 S7 协议里按 “位序号” 寻址：字节*8 + 位号
    int bitIndex() const { return byteOffset * 8 + bitOffset; }

    /// 解析失败返回 isValid()==false，并把原因写入 err（可为 nullptr）
    static S7Address parse(const QString &text, QString *err = nullptr);

    QString toString() const;

    bool operator==(const S7Address &o) const
    {
        return area == o.area && byteOffset == o.byteOffset
            && bitOffset == o.bitOffset && width == o.width;
    }
};

} // namespace cm8600
