#include "Autostart.hpp"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cwchar>

namespace
{
    constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr const wchar_t* kValueName = L"SonyHeadphonesClient";

    // "C:\path\SonyHeadphonesClient.exe" --tray
    bool BuildCommand(wchar_t (&command)[MAX_PATH + 16])
    {
        wchar_t exe[MAX_PATH];
        const DWORD length = GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
            return false;
        return std::swprintf(command, MAX_PATH + 16, L"\"%ls\" --tray", exe) > 0;
    }
} // namespace

bool autostartSupported()
{
    return true;
}

bool autostartIsEnabled()
{
    wchar_t expected[MAX_PATH + 16];
    if (!BuildCommand(expected))
        return false;
    wchar_t current[MAX_PATH + 16]{};
    DWORD size = sizeof(current);
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, current, &size) != ERROR_SUCCESS)
        return false;
    return _wcsicmp(current, expected) == 0;
}

bool autostartSetEnabled(bool enabled)
{
    if (!enabled)
    {
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }
    wchar_t command[MAX_PATH + 16];
    if (!BuildCommand(command))
        return false;
    const DWORD bytes = static_cast<DWORD>((std::wcslen(command) + 1) * sizeof(wchar_t));
    return RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName, REG_SZ, command, bytes) == ERROR_SUCCESS;
}
#else
bool autostartSupported()
{
    return false;
}

bool autostartIsEnabled()
{
    return false;
}

bool autostartSetEnabled(bool)
{
    return false;
}
#endif
