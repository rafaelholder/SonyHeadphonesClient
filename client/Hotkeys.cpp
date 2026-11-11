#include "Hotkeys.hpp"
#include "Tray.hpp"

#include <SDL3/SDL.h>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SDL3/SDL_system.h>

namespace
{
    constexpr int kHotkeyNextMode = 1;
    constexpr int kHotkeyToggleWindow = 2;
    HWND gHotkeyWindow{};
    HotkeysState gState{};

    // Runs on the main thread inside SDL's message pump, outside the worker lock:
    // only forwards to tray functions that queue work or touch the SDL window.
    bool SDLCALL OnWindowsMessage(void*, MSG* msg)
    {
        if (msg->message != WM_HOTKEY)
            return true;
        switch (static_cast<int>(msg->wParam))
        {
        case kHotkeyNextMode:
            trayRequestNextMode();
            return false;
        case kHotkeyToggleWindow:
            trayToggleWindow();
            return false;
        default:
            return true;
        }
    }
} // namespace

HotkeysState hotkeysInit(SDL_Window* window)
{
    gState = {};
    gHotkeyWindow = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!gHotkeyWindow)
        return gState;
    SDL_SetWindowsMessageHook(OnWindowsMessage, nullptr);
    // MOD_NOREPEAT: holding the keys down fires once, not on every auto-repeat.
    constexpr UINT kModifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
    gState.nextMode = RegisterHotKey(gHotkeyWindow, kHotkeyNextMode, kModifiers, 'H') != FALSE;
    gState.toggleWindow = RegisterHotKey(gHotkeyWindow, kHotkeyToggleWindow, kModifiers, 'J') != FALSE;
    return gState;
}

void hotkeysDestroy()
{
    if (gHotkeyWindow)
    {
        if (gState.nextMode)
            UnregisterHotKey(gHotkeyWindow, kHotkeyNextMode);
        if (gState.toggleWindow)
            UnregisterHotKey(gHotkeyWindow, kHotkeyToggleWindow);
        SDL_SetWindowsMessageHook(nullptr, nullptr);
    }
    gHotkeyWindow = nullptr;
    gState = {};
}
#else
HotkeysState hotkeysInit(SDL_Window*)
{
    return {};
}

void hotkeysDestroy()
{
}
#endif
