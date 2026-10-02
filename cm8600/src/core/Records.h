#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace cm8600 {

struct AppConfig;

/**
 * 按条码查到的加注参数，要下发给 PLC 的那一组。
 * 单位统一：加注量 g，真空度 Pa，时间 s。
 */
struct ChargeParams
{
    QString barcode;
    double  oilQty          = 0.0;   ///< 冷冻油加注量 g   -> VD9700
    double  refrigerantQty  = 0.0;   ///< 制冷剂加注量 g   -> VD9704
    double  vacuumLevel     = 0.0;   ///< 真空度目标 Pa    -> VD9708
    double  vacuumTime      = 0.0;   ///< 抽空时间 s       -> VD9712
    int     refrigerantCode = 0;     ///< 制冷剂编号       -> VW9730
    QString refrigerantName;         ///< 便于界面显示与核对

    QString source;                  ///< 数据来自哪里，写进日志便于现场排查

    bool isValid() const { return !barcode.isEmpty() && refrigerantCode > 0; }
};

/**
 * PLC 回传的一条完整加注记录（结果块 VB9110~VB9249 解析后的结果）。
 *
 * 本地始终存完整一条；上传时按配置决定是拆成真空段/充注段两条发，
 * 还是合并成一条发。
 */
struct ResultRecord
{
    int       channelId = 0;
    QString   barcode;

    // 真空段
    QDateTime vacuumStart;
    double    vacuumDuration = 0.0;  ///< s
    double    vacuumLevel    = 0.0;  ///< Pa

    // 充注段
    QDateTime chargeStart;
    double    chargeDuration = 0.0;  ///< s
    QString   refrigerantName;       ///< PLC 回传的是名称字符串，如 "R134a"
    int       refrigerantCode = 0;   ///< 由名称反查对照表得到，查不到为 0
    double    refrigerantQty  = 0.0; ///< g
    double    oilDuration     = 0.0; ///< s
    double    oilQty          = 0.0; ///< g

    // 原始数据，出问题时用来对账
    QMap<QString, QString> rawFields;
    QByteArray             rawBlock;
    QDateTime              readAt;

    /**
     * 去重键 = 条码 + 充注开始时间。
     *
     * 必要性：加注完成标志位由上位机复位，如果“读完数据”和“复位标志位”之间
     * 程序崩了/断网了，重启后会把同一件再读一遍。靠这个键挡掉。
     */
    QString dedupKey() const;

    bool isValid() const { return !barcode.isEmpty(); }

    /// 只含真空段的上传体
    QJsonObject toVacuumJson() const;
    /// 只含充注段的上传体
    QJsonObject toChargeJson() const;
    /// 合并的上传体
    QJsonObject toCombinedJson() const;

    /**
     * 从结果块解析。block 长度须等于 cfg.plc.result.blockSize。
     * 字段缺失或格式不对时写入 warnings 但不判失败——现场宁可少个字段也要把记录留下来。
     */
    static bool parse(const AppConfig &cfg, int channelId, const QByteArray &block,
                      ResultRecord *out, QStringList *warnings);
};

} // namespace cm8600

// 通道线程通过队列连接把这两个结构发给界面线程，必须注册为元类型
Q_DECLARE_METATYPE(cm8600::ChargeParams)
Q_DECLARE_METATYPE(cm8600::ResultRecord)
