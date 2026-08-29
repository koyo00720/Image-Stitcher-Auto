#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "explorer_context_menu.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winternl.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

namespace image_stitcher::platform {
namespace {
constexpr wchar_t kRegistryKey[] =
    L"Software\\Classes\\ImageStitcherAutoContextMenu";
constexpr auto kPackageName = "ImageStitcherAuto.ContextMenu";
constexpr auto kPackageFileName = "ImageStitcherAuto.ContextMenu.msix";
constexpr auto kCommandDllFileName = "ImageStitcherExplorerCommand.dll";

QString trContext(const char* text)
{
    return QCoreApplication::translate("ExplorerContextMenu", text);
}

QString windowsErrorMessage(DWORD error)
{
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    const QString result = length > 0 && buffer
                               ? QString::fromWCharArray(buffer, length).trimmed()
                               : QStringLiteral("Windows error %1").arg(error);
    if (buffer) {
        LocalFree(buffer);
    }
    return result;
}

bool isWindows11OrLater()
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        return false;
    }
    using RtlGetVersionFunction = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFunction>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (!rtlGetVersion) {
        return false;
    }

    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (rtlGetVersion(&version) != 0) {
        return false;
    }
    return version.dwMajorVersion > 10
           || (version.dwMajorVersion == 10
               && version.dwBuildNumber >= 22000);
}

QString quotePowerShellLiteral(QString value)
{
    value.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + value + QLatin1Char('\'');
}

QString encodedPowerShellCommand(const QString& command)
{
    const QByteArray utf16Le(
        reinterpret_cast<const char*>(command.utf16()),
        command.size() * static_cast<int>(sizeof(char16_t)));
    return QString::fromLatin1(utf16Le.toBase64());
}

QString powerShellPath()
{
    const QString windowsDirectory =
        qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"));
    return QDir(windowsDirectory).filePath(
        QStringLiteral("System32/WindowsPowerShell/v1.0/powershell.exe"));
}

struct ProcessResult
{
    bool success = false;
    bool timedOut = false;
    int exitCode = -1;
    QString output;
};

ProcessResult runPowerShell(const QString& command)
{
    ProcessResult result;
    QProcess process;
    process.setProgram(powerShellPath());
    process.setArguments({
        QStringLiteral("-NoLogo"),
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-WindowStyle"),
        QStringLiteral("Hidden"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-EncodedCommand"),
        encodedPowerShellCommand(command)
    });
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* arguments) {
            arguments->flags |= CREATE_NO_WINDOW;
        });
    process.start();
    if (!process.waitForStarted(10000)) {
        result.output = process.errorString();
        return result;
    }
    if (!process.waitForFinished(60000)) {
        result.timedOut = true;
        process.kill();
        process.waitForFinished(5000);
        result.output = process.errorString();
        return result;
    }

    result.exitCode = process.exitCode();
    result.output = QString::fromLocal8Bit(process.readAll()).trimmed();
    result.success = process.exitStatus() == QProcess::NormalExit
                     && result.exitCode == 0;
    return result;
}

bool shouldRetryElevated(const ProcessResult& result)
{
    const QString output = result.output.toLower();
    return output.contains(QStringLiteral("0x80070005"))
           || output.contains(QStringLiteral("access is denied"))
           || output.contains(QStringLiteral("administrator privileges"))
           || output.contains(QStringLiteral("管理者権限"))
           || output.contains(QStringLiteral("アクセスが拒否"));
}

ProcessResult runPowerShellElevated(const QString& command)
{
    ProcessResult result;
    const std::wstring executable = powerShellPath().toStdWString();
    const QString argumentText = QStringLiteral(
        "-NoLogo -NoProfile -NonInteractive -WindowStyle Hidden "
        "-ExecutionPolicy Bypass -EncodedCommand %1")
                                     .arg(encodedPowerShellCommand(command));
    const std::wstring arguments = argumentText.toStdWString();

    SHELLEXECUTEINFOW executeInfo{};
    executeInfo.cbSize = sizeof(executeInfo);
    executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    executeInfo.lpVerb = L"runas";
    executeInfo.lpFile = executable.c_str();
    executeInfo.lpParameters = arguments.c_str();
    executeInfo.nShow = SW_HIDE;
    if (!ShellExecuteExW(&executeInfo)) {
        result.output = windowsErrorMessage(GetLastError());
        return result;
    }

    const DWORD waitResult = WaitForSingleObject(executeInfo.hProcess, 60000);
    if (waitResult == WAIT_TIMEOUT) {
        result.timedOut = true;
        CloseHandle(executeInfo.hProcess);
        return result;
    }
    if (waitResult != WAIT_OBJECT_0) {
        result.output = windowsErrorMessage(GetLastError());
        CloseHandle(executeInfo.hProcess);
        return result;
    }

    DWORD exitCode = 1;
    if (!GetExitCodeProcess(executeInfo.hProcess, &exitCode)) {
        result.output = windowsErrorMessage(GetLastError());
        CloseHandle(executeInfo.hProcess);
        return result;
    }
    CloseHandle(executeInfo.hProcess);
    result.exitCode = static_cast<int>(exitCode);
    result.success = exitCode == 0;
    return result;
}

bool writeEnabledMarker(bool enabled,
                        const QString& installDirectory,
                        QString* errorMessage)
{
    HKEY key = nullptr;
    const LONG createResult = RegCreateKeyExW(
        HKEY_CURRENT_USER, kRegistryKey, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE | KEY_WOW64_64KEY,
        nullptr, &key, nullptr);
    if (createResult != ERROR_SUCCESS) {
        if (errorMessage) {
            *errorMessage = windowsErrorMessage(
                static_cast<DWORD>(createResult));
        }
        return false;
    }

    const DWORD value = enabled ? 1U : 0U;
    LONG result = RegSetValueExW(
        key, L"Enabled", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value));
    if (result == ERROR_SUCCESS && enabled) {
        const std::wstring directory = installDirectory.toStdWString();
        result = RegSetValueExW(
            key, L"InstallDirectory", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(directory.c_str()),
            static_cast<DWORD>((directory.size() + 1) * sizeof(wchar_t)));
    }
    RegCloseKey(key);

    if (result != ERROR_SUCCESS) {
        if (errorMessage) {
            *errorMessage = windowsErrorMessage(static_cast<DWORD>(result));
        }
        return false;
    }
    return true;
}

void notifyExplorerOfAssociationChange()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
} // namespace

bool explorerContextMenuSupported()
{
    return isWindows11OrLater();
}

ExplorerContextMenuResult setExplorerContextMenuEnabled(bool enabled)
{
    const QString applicationDirectory = QCoreApplication::applicationDirPath();

    if (!enabled) {
        QString markerError;
        if (!writeEnabledMarker(false, applicationDirectory, &markerError)) {
            return {false, markerError};
        }
        notifyExplorerOfAssociationChange();
        return {true, {}};
    }

    if (!explorerContextMenuSupported()) {
        return {
            false,
            trContext("この機能はWindows 11以降でのみ利用できます。")
        };
    }

    const QString packagePath = QDir(applicationDirectory).filePath(
        QString::fromLatin1(kPackageFileName));
    const QString commandDllPath = QDir(applicationDirectory).filePath(
        QString::fromLatin1(kCommandDllFileName));
    for (const QString& requiredPath : {packagePath, commandDllPath}) {
        if (!QFileInfo::exists(requiredPath)) {
            return {
                false,
                trContext("必要なExplorer連携ファイルが見つかりません: %1")
                    .arg(QDir::toNativeSeparators(requiredPath))
            };
        }
    }

    QStringList packageVersionParts =
        QCoreApplication::applicationVersion().split(QLatin1Char('.'));
    while (packageVersionParts.size() < 4) {
        packageVersionParts.push_back(QStringLiteral("0"));
    }
    packageVersionParts = packageVersionParts.mid(0, 4);
    const QString packageVersion = packageVersionParts.join(QLatin1Char('.'));
    const QString script = QStringLiteral(
        "$ErrorActionPreference='Stop';"
        "$existing=Get-AppxPackage -Name '%1' -ErrorAction SilentlyContinue;"
        "$targetVersion=[version]'%2';"
        "if(-not $existing -or [version]$existing.Version -lt $targetVersion){"
        "Add-AppxPackage -Path %3 -ExternalLocation %4 -AllowUnsigned "
        "-ForceUpdateFromAnyVersion"
        "}")
                               .arg(QString::fromLatin1(kPackageName),
                                    packageVersion,
                                    quotePowerShellLiteral(
                                        QDir::toNativeSeparators(packagePath)),
                                    quotePowerShellLiteral(
                                        QDir::toNativeSeparators(applicationDirectory)));

    ProcessResult registration = runPowerShell(script);
    if (!registration.success && shouldRetryElevated(registration)) {
        registration = runPowerShellElevated(script);
    }
    if (!registration.success) {
        if (registration.timedOut) {
            return {
                false,
                trContext("Windowsへの登録処理がタイムアウトしました。")
            };
        }
        const QString details = registration.output.isEmpty()
                                    ? trContext("詳細なエラー情報はありません。")
                                    : registration.output;
        return {
            false,
            trContext("Windowsへの登録に失敗しました（終了コード %1）。\n%2")
                .arg(registration.exitCode)
                .arg(details)
        };
    }

    QString markerError;
    if (!writeEnabledMarker(true, applicationDirectory, &markerError)) {
        return {false, markerError};
    }
    notifyExplorerOfAssociationChange();
    return {true, {}};
}

} // namespace image_stitcher::platform
