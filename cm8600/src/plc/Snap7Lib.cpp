#include "plc/Snap7Lib.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QStringList>

#include <type_traits>
#include <utility>

namespace cm8600 {

Snap7Lib &Snap7Lib::instance()
{
    static Snap7Lib s_instance;
    return s_instance;
}

Snap7Lib::~Snap7Lib()
{
    delete m_lib;   // 进程退出时才走到这里，不主动 unload，避免还有客户端句柄在用
}

bool Snap7Lib::load(const QString &preferredPath, QString *err)
{
    if (m_loaded)
        return true;

    QStringList candidates;
    if (!preferredPath.trimmed().isEmpty())
        candidates << preferredPath.trimmed();

    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << appDir + QStringLiteral("/snap7.dll")
               << appDir + QStringLiteral("/lib/snap7.dll")
               << QStringLiteral("snap7");   // 交给系统按 PATH 找

    QStringList tried;
    for (const QString &cand : std::as_const(candidates)) {
        // 带路径的候选先看文件在不在，免得把“文件不存在”和“加载失败”混为一谈
        const bool looksLikePath = cand.contains(QLatin1Char('/')) || cand.contains(QLatin1Char('\\'));
        if (looksLikePath && !QFileInfo::exists(cand)) {
            tried << QStringLiteral("%1（文件不存在）").arg(QDir::toNativeSeparators(cand));
            continue;
        }

        auto *lib = new QLibrary(cand);
        if (!lib->load()) {
            tried << QStringLiteral("%1（%2）").arg(QDir::toNativeSeparators(cand), lib->errorString());
            delete lib;
            continue;
        }

        m_lib  = lib;
        m_path = lib->fileName();

        QString resolveErr;
        if (!resolveAll(&resolveErr)) {
            tried << QStringLiteral("%1（%2）").arg(QDir::toNativeSeparators(cand), resolveErr);
            m_lib->unload();
            delete m_lib;
            m_lib = nullptr;
            m_path.clear();
            continue;
        }

        m_loaded = true;
        return true;
    }

    if (err) {
        *err = QStringLiteral("加载 snap7.dll 失败。请把 64 位的 snap7.dll 放到程序目录，"
                              "或在 config.json 的 snap7.dll_path 里指定绝对路径。\n已尝试：\n  - %1")
                   .arg(tried.join(QStringLiteral("\n  - ")));
    }
    return false;
}

bool Snap7Lib::resolveAll(QString *err)
{
    QStringList missing;

    auto bind = [&](auto &fnPtr, const char *name) {
        using FnType = std::remove_reference_t<decltype(fnPtr)>;
        fnPtr = reinterpret_cast<FnType>(m_lib->resolve(name));
        if (!fnPtr)
            missing << QString::fromLatin1(name);
    };

    bind(Cli_Create,       "Cli_Create");
    bind(Cli_Destroy,      "Cli_Destroy");
    bind(Cli_ConnectTo,    "Cli_ConnectTo");
    // SetConnectionType 不是所有旧版 snap7 都导出；缺了也不报错，连接时跳过
    Cli_SetConnectionType = reinterpret_cast<decltype(Cli_SetConnectionType)>(
        m_lib->resolve("Cli_SetConnectionType"));
    bind(Cli_Disconnect,   "Cli_Disconnect");
    bind(Cli_GetConnected, "Cli_GetConnected");
    bind(Cli_SetParam,     "Cli_SetParam");
    bind(Cli_ReadArea,     "Cli_ReadArea");
    bind(Cli_WriteArea,    "Cli_WriteArea");
    bind(Cli_ErrorText,    "Cli_ErrorText");

    // 服务端接口只给模拟器用，缺了不影响主程序跑，单独宽容处理
    Srv_Create       = reinterpret_cast<decltype(Srv_Create)>(m_lib->resolve("Srv_Create"));
    Srv_Destroy      = reinterpret_cast<decltype(Srv_Destroy)>(m_lib->resolve("Srv_Destroy"));
    Srv_SetParam     = reinterpret_cast<decltype(Srv_SetParam)>(m_lib->resolve("Srv_SetParam"));
    Srv_RegisterArea = reinterpret_cast<decltype(Srv_RegisterArea)>(m_lib->resolve("Srv_RegisterArea"));
    Srv_Start        = reinterpret_cast<decltype(Srv_Start)>(m_lib->resolve("Srv_Start"));
    Srv_StartTo      = reinterpret_cast<decltype(Srv_StartTo)>(m_lib->resolve("Srv_StartTo"));
    Srv_Stop         = reinterpret_cast<decltype(Srv_Stop)>(m_lib->resolve("Srv_Stop"));
    Srv_LockArea     = reinterpret_cast<decltype(Srv_LockArea)>(m_lib->resolve("Srv_LockArea"));
    Srv_UnlockArea   = reinterpret_cast<decltype(Srv_UnlockArea)>(m_lib->resolve("Srv_UnlockArea"));
    Srv_ErrorText    = reinterpret_cast<decltype(Srv_ErrorText)>(m_lib->resolve("Srv_ErrorText"));

    if (!missing.isEmpty()) {
        if (err) {
            *err = QStringLiteral("缺少符号：%1；该文件可能不是 snap7，或不是 64 位版本")
                       .arg(missing.join(QStringLiteral(", ")));
        }
        return false;
    }
    return true;
}

QString Snap7Lib::errorText(int code) const
{
    if (code == 0)
        return QStringLiteral("成功");

    if (!m_loaded || !Cli_ErrorText)
        return QStringLiteral("snap7 错误码 0x%1").arg(static_cast<uint>(code), 8, 16, QLatin1Char('0'));

    char buf[1024] = {0};
    Cli_ErrorText(code, buf, static_cast<int>(sizeof(buf)));

    QString text = QString::fromLatin1(buf).trimmed();
    if (text.isEmpty())
        text = QStringLiteral("未知错误");

    return QStringLiteral("%1 (0x%2)").arg(text).arg(static_cast<uint>(code), 8, 16, QLatin1Char('0'));
}

} // namespace cm8600
