#include "core/Injector.h"

#include "core/injectcore/Inject.h"

#include <QFile>
#include <QFileInfo>

namespace {

// Map the engine's English diagnostics to actionable Chinese messages.
QString translateEngineMessage(const QString& raw, quint32 winError)
{
    if (raw.contains(QLatin1String("Failed to open the DLL file")))
        return QStringLiteral("无法打开 DLL 文件，请检查路径是否有效");
    if (raw.contains(QLatin1String("Failed to get the DLL file size")))
        return QStringLiteral("无法读取 DLL 文件大小，文件可能已损坏");
    if (raw.contains(QLatin1String("Failed to read the DLL file")))
        return QStringLiteral("读取 DLL 文件失败");
    if (raw.contains(QLatin1String("Failed to open the target process")))
        return QStringLiteral("打开目标进程失败（游戏可能以管理员身份运行，请以管理员身份重启本程序）");
    if (raw.contains(QLatin1String("Failed to inject the DLL")))
        return QStringLiteral("注入 DLL 失败（错误码 %1），请确认目标进程与系统版本匹配").arg(winError);
    if (raw.contains(QLatin1String("Could not signal resident proxy")))
        return QStringLiteral("向驻留代理发送恢复信号失败（错误码 %1）").arg(winError);
    if (raw.contains(QLatin1String("Reactivation requested")))
        return QStringLiteral("已通知驻留代理恢复运行（未重新加载磁盘 DLL）");
    if (raw.contains(QLatin1String("Injected the DLL")))
        return QStringLiteral("DLL 注入成功");
    if (!raw.isEmpty())
        return raw;
    return QStringLiteral("未知错误");
}

} // namespace

namespace Injector {

bool fileReadable(const QString& dllPath)
{
    if (dllPath.trimmed().isEmpty())
        return false;
    const QFileInfo info(dllPath);
    return info.exists() && info.isFile() && info.isReadable();
}

InjectionResult run(DWORD pid, const QString& dllPath)
{
    InjectionResult result;

    if (!fileReadable(dllPath))
    {
        result.message = QStringLiteral("找不到代理 DLL：%1").arg(dllPath);
        return result;
    }

    char message[1024] = {0};
    BOOL resumed = FALSE;
    const BOOL ok = DoInjectEx(pid, dllPath.toLocal8Bit().constData(), message,
                               int(sizeof(message)), &resumed);

    result.ok = (ok == TRUE);
    result.resumed = (resumed == TRUE);
    result.injected = result.ok && !result.resumed;

    QString raw = QString::fromLatin1(message);
    quint32 winError = 0;
    const int idx = raw.lastIndexOf(QLatin1String("Error="));
    if (idx >= 0)
        winError = raw.mid(idx + 6).toUInt();

    result.winError = winError;
    result.message = translateEngineMessage(raw, winError);
    return result;
}

} // namespace Injector
