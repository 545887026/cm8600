#pragma once

#include "plc/S7Address.h"

#include <QHash>
#include <QMap>
#include <QString>
#include <QVector>

namespace cm8600 {

struct ChannelConfig
{
    int     id      = 0;
    QString name;
    QString ip;
    int     rack    = 0;
    int     slot    = 1;
    int     port    = 102;   ///< 真机固定 102；脱机接模拟器时可改，便于两台同跑在 127.0.0.1
    bool    enabled = true;
};

struct HeartbeatConfig
{
    S7Address txBit;        ///< 上位机写，1Hz 方波
    S7Address rxBit;        ///< PLC 写，监视其翻转
    int       toggleMs  = 500;
    int       timeoutMs = 3000;
};

struct QueryConfig
{
    S7Address triggerBit;   ///< PLC 置位，表示“扫码完成，请查参数”
    S7Address barcodeAddr;  ///< 条码起始地址
    int       barcodeLen = 30;
    S7Address okBit;        ///< 上位机置位：查询成功（由 PLC 复位）
    S7Address failBit;      ///< 上位机置位：查询失败（由 PLC 复位）
};

struct DownloadItem
{
    S7Address addr;
    QString   type;         ///< real / int16
    QString   unit;
};

struct DownloadConfig
{
    S7Address    blockAddr;         ///< VD9700
    int          blockSize = 16;    ///< 4 个 Real 连续 16 字节，一次写完
    DownloadItem oilQty;
    DownloadItem refrigerantQty;
    DownloadItem vacuumLevel;
    DownloadItem vacuumTime;
    DownloadItem refrigerantType;   ///< VW9730，单独写
};

struct ResultField
{
    QString   key;
    QString   label;
    S7Address addr;
    int       len  = 10;
    QString   type;         ///< 目前全是 ascii
    QString   unit;
    QString   hint;

    /// 该字段在结果块缓冲区中的下标
    int offsetIn(const S7Address &blockStart) const
    {
        return addr.byteOffset - blockStart.byteOffset;
    }
};

struct ResultConfig
{
    S7Address           blockAddr;          ///< VB9110
    int                 blockSize = 140;    ///< VB9110~VB9249
    QVector<ResultField> fields;

    const ResultField *field(const QString &key) const;
};

struct VacuumTriggerConfig
{
    enum class Mode {
        Flag,          ///< PLC 给独立的“抽真空完成”标志位
        OnChargeDone,  ///< 不新增位：充注完成后一次读满，本地拆两条
    };
    Mode      mode = Mode::OnChargeDone;
    S7Address bit;
};

struct PlcConfig
{
    int vDbNumber           = 1;
    int pollIntervalMs      = 200;
    int connectTimeoutMs    = 3000;
    int reconnectIntervalMs = 3000;

    HeartbeatConfig     heartbeat;
    QueryConfig         query;
    DownloadConfig      download;
    S7Address           chargeDoneBit;      ///< M25.7
    VacuumTriggerConfig vacuumTrigger;
    ResultConfig        result;
};

struct DataSourceConfig
{
    QString type = QStringLiteral("mock");  ///< mock / file
    QString filePath;
    int     refreshIntervalMs = 60000;
    QString cacheDb;
};

struct UploadConfig
{
    QString mode = QStringLiteral("split"); ///< split = 真空段/充注段各发一条；combined = 合并一条
    QString baseUrl;
    QString vacuumPath;
    QString chargePath;
    QString combinedPath;
    int     timeoutMs       = 10000;
    int     retryIntervalMs = 30000;
    int     maxRetry        = 0;            ///< 0 = 一直重试
    QMap<QString, QString> headers;

    bool isSplit() const { return mode.compare(QStringLiteral("combined"), Qt::CaseInsensitive) != 0; }
};

struct StorageConfig
{
    QString dbPath = QStringLiteral("cm8600.db");
    int     keepDays = 365;
};

/**
 * 全局配置。PLC 地址、单位、制冷剂对照表、上传方式全部走配置，
 * 现场对方改了地址或接口形态时，改 config.json 即可，不用重新编译。
 */
struct AppConfig
{
    QString title = QStringLiteral("CM8600 冷媒加注机 上位机");
    QString logDir = QStringLiteral("logs");
    int     logKeepDays = 30;
    QString snap7DllPath;

    QVector<ChannelConfig> channels;
    PlcConfig              plc;
    QMap<int, QString>     refrigerants;   ///< 编号 -> 名称
    DataSourceConfig       datasource;
    UploadConfig           upload;
    StorageConfig          storage;

    /// 读取并校验配置。err 里会列出所有问题，而不是只报第一个
    static bool load(const QString &path, AppConfig *out, QString *err);

    /// 按名称反查编号（忽略大小写）；找不到返回 0
    int refrigerantCode(const QString &name) const;

    /// 按编号取名称；找不到返回空串
    QString refrigerantName(int code) const;
};

} // namespace cm8600
