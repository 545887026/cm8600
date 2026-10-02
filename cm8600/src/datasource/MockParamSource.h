#pragma once

#include "datasource/IParamSource.h"

#include <QHash>
#include <QMutex>

namespace cm8600 {

struct AppConfig;

/**
 * 假数据源，用于脱机联调。
 *
 * 行为刻意做得“像真的”：
 *   - 条码为空 -> 查不到（对应现场扫码枪没读到）
 *   - 条码以 "NG" 开头 -> 查不到，用来验证查询失败位和 HMI 报警那条路
 *   - 其余条码 -> 用条码哈希稳定地生成一组参数，同一条码每次查到的都一样
 * 也支持手工塞入指定条码的参数，方便做定点测试。
 */
class MockParamSource : public IParamSource
{
public:
    explicit MockParamSource(const AppConfig &cfg);

    bool      refresh(QString *err) override;
    bool      lookup(const QString &barcode, ChargeParams *out, QString *err) override;
    QString   describe() const override;
    QDateTime lastSyncAt() const override;

    /// 测试用：给指定条码预置一组参数
    void preset(const QString &barcode, const ChargeParams &params);

private:
    const AppConfig &m_cfg;

    mutable QMutex                m_mutex;
    QHash<QString, ChargeParams>  m_preset;
    QDateTime                     m_lastSync;
};

} // namespace cm8600
