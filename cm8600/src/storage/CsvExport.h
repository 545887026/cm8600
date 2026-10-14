#pragma once

#include "core/Records.h"

#include <QString>
#include <QVector>

namespace cm8600 {

/**
 * 把一批加注记录写成 CSV。
 *
 * 编码：UTF-8 + BOM。加 BOM 是为了 Windows Excel 直接双击能正确识别中文，
 * 否则默认按 GBK 解会乱码。
 *
 * 字段顺序与 ChannelPanel 历史表基本一致，另外在最前面加了 "通道"，
 * 最后加了 "读取时间"（用于对账 PLC 回传时刻和充注开始时刻的差异）。
 *
 * @return 写文件失败时返回 false，并把原因写进 err。
 */
bool exportResultRecordsCsv(const QString &path, const QVector<ResultRecord> &records,
                            QString *err);

} // namespace cm8600
