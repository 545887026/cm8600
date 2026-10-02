#pragma once

#include "core/Records.h"

#include <QDateTime>
#include <QString>

namespace cm8600 {

/**
 * 按条码查加注参数的数据源。
 *
 * 甲方侧的形态还没定（对方说是“服务器上的一个文件，可能是 Excel”），
 * 所以这里抽一层接口：现在用 Mock 打通全链路，规范到位后加一个实现类、
 * 改 config.json 的 datasource.type 即可，上层一行不动。
 *
 * 实现必须是线程安全的：两个通道线程会同时查。
 */
class IParamSource
{
public:
    virtual ~IParamSource() = default;

    /// 同步/重载数据源（比如把服务器上的文件拉到本地缓存）。失败不应清空已有缓存
    virtual bool refresh(QString *err) = 0;

    /**
     * 按条码查参数。
     * @return true 表示查到了；false 表示没查到或出错，err 写明原因。
     *         注意“没查到”是正常业务分支——要给 PLC 置查询失败位，让工人重扫。
     */
    virtual bool lookup(const QString &barcode, ChargeParams *out, QString *err) = 0;

    /// 数据源描述，显示在界面上
    virtual QString describe() const = 0;

    /// 最近一次成功同步的时间；界面据此提示数据是否已经过期
    virtual QDateTime lastSyncAt() const = 0;
};

} // namespace cm8600
