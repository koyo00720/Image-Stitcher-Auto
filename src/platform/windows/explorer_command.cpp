#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {
// {BF9AFB95-8EB3-4DF2-9E8C-077036336FE0}
constexpr CLSID kExplorerCommandClsid = {
    0xbf9afb95,
    0x8eb3,
    0x4df2,
    {0x9e, 0x8c, 0x07, 0x70, 0x36, 0x33, 0x6f, 0xe0}
};
constexpr wchar_t kRegistryKey[] =
    L"Software\\Classes\\ImageStitcherAutoContextMenu";
constexpr wchar_t kApplicationFileName[] = L"Image_Stitcher_Auto.exe";

HINSTANCE g_moduleInstance = nullptr;
std::atomic<long> g_objectCount{0};
std::atomic<long> g_serverLocks{0};

HRESULT duplicateString(const std::wstring& value, PWSTR* output)
{
    if (!output) {
        return E_POINTER;
    }
    *output = nullptr;
    const size_t bytes = (value.size() + 1) * sizeof(wchar_t);
    auto* copy = static_cast<PWSTR>(CoTaskMemAlloc(bytes));
    if (!copy) {
        return E_OUTOFMEMORY;
    }
    CopyMemory(copy, value.c_str(), bytes);
    *output = copy;
    return S_OK;
}

std::wstring moduleFilePath()
{
    std::vector<wchar_t> buffer(1024);
    for (;;) {
        const DWORD length = GetModuleFileNameW(
            g_moduleInstance, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size() - 1) {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring applicationPath()
{
    std::wstring path = moduleFilePath();
    const size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return kApplicationFileName;
    }
    path.resize(separator + 1);
    path += kApplicationFileName;
    return path;
}

std::wstring applicationDirectory()
{
    std::wstring path = moduleFilePath();
    const size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring()
                                           : path.substr(0, separator);
}

bool contextMenuEnabled()
{
    DWORD value = 0;
    DWORD valueSize = sizeof(value);
    const LONG result = RegGetValueW(
        HKEY_CURRENT_USER, kRegistryKey, L"Enabled",
        RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY,
        nullptr, &value, &valueSize);
    return result == ERROR_SUCCESS && value != 0;
}

bool supportedImagePath(const std::wstring& path)
{
    const size_t separator = path.find_last_of(L"\\/");
    const size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos
        || (separator != std::wstring::npos && dot < separator)) {
        return false;
    }
    std::wstring extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t value) {
                       return static_cast<wchar_t>(std::towlower(value));
                   });
    return extension == L".png" || extension == L".jpg"
           || extension == L".jpeg" || extension == L".bmp"
           || extension == L".tif" || extension == L".tiff"
           || extension == L".webp";
}

HRESULT selectedImagePaths(IShellItemArray* items,
                           std::vector<std::wstring>* paths)
{
    if (!items || !paths) {
        return E_INVALIDARG;
    }
    paths->clear();

    DWORD count = 0;
    HRESULT result = items->GetCount(&count);
    if (FAILED(result) || count == 0) {
        return FAILED(result) ? result : E_INVALIDARG;
    }
    paths->reserve(count);

    for (DWORD index = 0; index < count; ++index) {
        IShellItem* item = nullptr;
        result = items->GetItemAt(index, &item);
        if (FAILED(result) || !item) {
            return FAILED(result) ? result : E_FAIL;
        }

        SFGAOF attributes = 0;
        result = item->GetAttributes(SFGAO_FOLDER, &attributes);
        if (FAILED(result) || (attributes & SFGAO_FOLDER) != 0) {
            item->Release();
            return E_INVALIDARG;
        }

        PWSTR rawPath = nullptr;
        result = item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath);
        item->Release();
        if (FAILED(result) || !rawPath) {
            if (rawPath) {
                CoTaskMemFree(rawPath);
            }
            return FAILED(result) ? result : E_INVALIDARG;
        }

        std::wstring path(rawPath);
        CoTaskMemFree(rawPath);
        if (!supportedImagePath(path)) {
            return E_INVALIDARG;
        }
        paths->push_back(std::move(path));
    }
    return S_OK;
}

std::wstring quoteCommandLineArgument(const std::wstring& argument)
{
    std::wstring quoted;
    quoted.push_back(L'"');
    size_t backslashes = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        if (character == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
        } else {
            quoted.append(backslashes, L'\\');
            quoted.push_back(character);
        }
        backslashes = 0;
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

class ExplorerCommand final : public IExplorerCommand
{
public:
    ExplorerCommand() { ++g_objectCount; }
    ~ExplorerCommand() { --g_objectCount; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (IsEqualIID(iid, IID_IUnknown)
            || IsEqualIID(iid, IID_IExplorerCommand)) {
            *object = static_cast<IExplorerCommand*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&referenceCount_));
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const LONG count = InterlockedDecrement(&referenceCount_);
        if (count == 0) {
            delete this;
        }
        return static_cast<ULONG>(count);
    }

    HRESULT STDMETHODCALLTYPE GetTitle(IShellItemArray*, PWSTR* title) override
    {
        return duplicateString(L"Open in IS Auto", title);
    }

    HRESULT STDMETHODCALLTYPE GetIcon(IShellItemArray*, PWSTR* icon) override
    {
        return duplicateString(applicationPath(), icon);
    }

    HRESULT STDMETHODCALLTYPE GetToolTip(IShellItemArray*, PWSTR* toolTip) override
    {
        if (!toolTip) {
            return E_POINTER;
        }
        *toolTip = nullptr;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetCanonicalName(GUID* canonicalName) override
    {
        if (!canonicalName) {
            return E_POINTER;
        }
        *canonicalName = kExplorerCommandClsid;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetState(IShellItemArray* items,
                                       BOOL,
                                       EXPCMDSTATE* state) override
    {
        if (!state) {
            return E_POINTER;
        }
        *state = ECS_HIDDEN;
        if (!contextMenuEnabled()) {
            return S_OK;
        }

        std::vector<std::wstring> paths;
        if (SUCCEEDED(selectedImagePaths(items, &paths))) {
            *state = ECS_ENABLED;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(IShellItemArray* items, IBindCtx*) override
    {
        std::vector<std::wstring> paths;
        HRESULT result = selectedImagePaths(items, &paths);
        if (FAILED(result)) {
            return result;
        }

        std::wstring arguments = L"--";
        for (const std::wstring& path : paths) {
            arguments.push_back(L' ');
            arguments += quoteCommandLineArgument(path);
        }
        if (arguments.size() >= 30000) {
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        }

        const std::wstring executable = applicationPath();
        const std::wstring workingDirectory = applicationDirectory();
        const HINSTANCE launchResult = ShellExecuteW(
            nullptr, L"open", executable.c_str(), arguments.c_str(),
            workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
            SW_SHOWNORMAL);
        const auto launchCode = reinterpret_cast<INT_PTR>(launchResult);
        if (launchCode <= 32) {
            return HRESULT_FROM_WIN32(
                launchCode > 0 ? static_cast<DWORD>(launchCode)
                               : ERROR_GEN_FAILURE);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetFlags(EXPCMDFLAGS* flags) override
    {
        if (!flags) {
            return E_POINTER;
        }
        *flags = ECF_DEFAULT;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE EnumSubCommands(
        IEnumExplorerCommand** commands) override
    {
        if (!commands) {
            return E_POINTER;
        }
        *commands = nullptr;
        return E_NOTIMPL;
    }

private:
    volatile LONG referenceCount_ = 1;
};

class ExplorerCommandFactory final : public IClassFactory
{
public:
    ExplorerCommandFactory() { ++g_objectCount; }
    ~ExplorerCommandFactory() { --g_objectCount; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (IsEqualIID(iid, IID_IUnknown)
            || IsEqualIID(iid, IID_IClassFactory)) {
            *object = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&referenceCount_));
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const LONG count = InterlockedDecrement(&referenceCount_);
        if (count == 0) {
            delete this;
        }
        return static_cast<ULONG>(count);
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,
                                             REFIID iid,
                                             void** object) override
    {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (outer) {
            return CLASS_E_NOAGGREGATION;
        }
        auto* command = new (std::nothrow) ExplorerCommand;
        if (!command) {
            return E_OUTOFMEMORY;
        }
        const HRESULT result = command->QueryInterface(iid, object);
        command->Release();
        return result;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override
    {
        if (lock) {
            ++g_serverLocks;
        } else {
            --g_serverLocks;
        }
        return S_OK;
    }

private:
    volatile LONG referenceCount_ = 1;
};
} // namespace

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_moduleInstance = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID classId,
                                                REFIID iid,
                                                void** object)
{
    if (!object) {
        return E_POINTER;
    }
    *object = nullptr;
    if (!IsEqualCLSID(classId, kExplorerCommandClsid)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new (std::nothrow) ExplorerCommandFactory;
    if (!factory) {
        return E_OUTOFMEMORY;
    }
    const HRESULT result = factory->QueryInterface(iid, object);
    factory->Release();
    return result;
}

extern "C" HRESULT __stdcall DllCanUnloadNow()
{
    return g_objectCount.load() == 0 && g_serverLocks.load() == 0
               ? S_OK
               : S_FALSE;
}
