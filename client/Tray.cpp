#include "Tray.hpp"
#include "Autostart.hpp"
#include "Settings.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace
{
    SDL_Tray* gTray{};
    SDL_Window* gTrayWindow{};
    SDL_TrayEntry* gEntryStatus{};
    SDL_TrayEntry* gEntryCancel{};
    SDL_TrayEntry* gEntryAmbient{};
    SDL_TrayEntry* gEntryOff{};
    SDL_TrayEntry* gEntryNext{};
    SDL_TrayEntry* gEntryWindow{};
    SDL_TrayEntry* gEntryAutoConnect{};
    SDL_TrayEntry* gEntryAutostart{};
    // -1 none, 0 off, 1 on. Applied under the lock by trayUpdate().
    std::atomic<int> gRequestedAutoConnect{-1};

    // "Equalizador" submenu, rebuilt whenever the preset list changes (clientGetEqPresetsVersion).
    SDL_TrayEntry* gEntryEq{};
    SDL_TrayMenu* gEqMenu{};
    SDL_TrayEntry* gEqEntries[kMaxEqPresets]{};
    int gEqEntryCount{};
    SDL_TrayEntry* gEqPlaceholder{};
    uint32_t gEqShownVersion{};
    std::atomic<int> gRequestedEq{-1}; // Preset index, applied under the lock by trayUpdate().

    // Written by menu callbacks (main thread, outside the worker lock), consumed by trayUpdate().
    // Holds an MDRNoiseMode, kRequestNone, or kRequestNext.
    constexpr int kRequestNone = -1;
    constexpr int kRequestNext = -2;
    std::atomic<int> gRequestedMode{kRequestNone};
    std::atomic<bool> gForceRefresh{true};
    ClientTrayStatus gShown{};

    constexpr int kIconSize = 32;

    void Wake()
    {
        SDL_Event wake{};
        wake.type = SDL_EVENT_USER;
        SDL_PushEvent(&wake);
    }

    const char* ModeLabel(MDRNoiseMode mode)
    {
        switch (mode)
        {
        case MDR_NOISE_MODE_CANCELLING:
            return "Cancelamento de ruído";
        case MDR_NOISE_MODE_AMBIENT:
            return "Som ambiente";
        case MDR_NOISE_MODE_WIND_NOISE_REDUCTION:
            return "Redução de vento";
        default:
            return "Desligado";
        }
    }

    bool SameStatus(const ClientTrayStatus& a, const ClientTrayStatus& b)
    {
        return a.autoConnect == b.autoConnect && a.hasSavedDevice == b.hasSavedDevice && a.waiting == b.waiting &&
            a.connecting == b.connecting && a.connected == b.connected && a.hasBattery == b.hasBattery &&
            a.batteryPercent == b.batteryPercent && a.charging == b.charging && a.hasNoise == b.hasNoise &&
            a.canCancel == b.canCancel && a.canAmbient == b.canAmbient && a.noiseMode == b.noiseMode &&
            a.eqUsable == b.eqUsable && a.eqBandCount == b.eqBandCount && a.eqActive == b.eqActive &&
            std::strcmp(a.modelName, b.modelName) == 0;
    }

    // 16x16 pixel-art headphone, drawn at 2x. Keep in sync with tooling/make-pixel-icon.py (app.ico).
    constexpr int kSpriteSize = 16;
    constexpr const char* kHeadphoneSprite[kSpriteSize] = {
        "................", //
        ".....######.....", //
        "...##......##...", //
        "..#..........#..", //
        ".#............#.", //
        ".#............#.", //
        ".#............#.", //
        "###..........###", //
        "####........####", //
        "####........####", //
        "####........####", //
        "####........####", //
        ".###........###.", //
        "................", //
        "................", //
        "................", //
    };

    // `size` must be a multiple of kSpriteSize (nearest-neighbour scale keeps the pixels crisp).
    bool SpritePixel(int x, int y, int size)
    {
        const int scale = size / kSpriteSize;
        if (x < 0 || y < 0 || x >= size || y >= size)
            return false;
        return kHeadphoneSprite[y / scale][x / scale] == '#';
    }

    // Black headphone + light outline, same rule as tooling/make-pixel-icon.py: outline = max(1, scale / 2) px.
    void DrawHeadphone(SDL_Surface* surface, int size, Uint8 alpha)
    {
        const int outline = std::max(1, size / kSpriteSize / 2);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                if (SpritePixel(x, y, size))
                {
                    SDL_WriteSurfacePixel(surface, x, y, 0, 0, 0, alpha);
                    continue;
                }
                bool edge = false;
                for (int dy = -outline; dy <= outline && !edge; ++dy)
                    for (int dx = -outline; dx <= outline && !edge; ++dx)
                        edge = SpritePixel(x + dx, y + dy, size);
                if (edge)
                    SDL_WriteSurfacePixel(surface, x, y, 235, 235, 235, alpha);
            }
    }

    // Black pixel-art headphone with a 1px light outline (the Windows 11 taskbar here is dark, and plain black
    // would vanish on it), plus a small battery bar underneath:
    // green > 50%, yellow 21-50%, red <= 20%, blue while charging. Faded and without the bar when disconnected.
    SDL_Surface* MakeTrayIcon(const ClientTrayStatus& status)
    {
        SDL_Surface* surface = SDL_CreateSurface(kIconSize, kIconSize, SDL_PIXELFORMAT_ARGB8888);
        if (!surface)
            return nullptr;
        SDL_FillSurfaceRect(surface, nullptr, SDL_MapSurfaceRGBA(surface, 0, 0, 0, 0));

        DrawHeadphone(surface, kIconSize, status.connected ? 255 : 110);

        if (status.connected && status.hasBattery)
        {
            const uint32_t level = status.batteryPercent > 100 ? 100 : status.batteryPercent;
            Uint32 fill;
            if (status.charging)
                fill = SDL_MapSurfaceRGBA(surface, 80, 170, 255, 255);
            else if (level > 50)
                fill = SDL_MapSurfaceRGBA(surface, 70, 200, 90, 255);
            else if (level > 20)
                fill = SDL_MapSurfaceRGBA(surface, 240, 190, 40, 255);
            else
                fill = SDL_MapSurfaceRGBA(surface, 230, 60, 50, 255);
            // Bar below the outline: light frame 3..28 x 27..31, dark track inside, fill from the left.
            const SDL_Rect frame{3, 27, 26, 5};
            const SDL_Rect track{4, 28, 24, 3};
            SDL_FillSurfaceRect(surface, &frame, SDL_MapSurfaceRGBA(surface, 235, 235, 235, 255));
            SDL_FillSurfaceRect(surface, &track, SDL_MapSurfaceRGBA(surface, 30, 30, 30, 255));
            int width = static_cast<int>((track.w * level + 50) / 100);
            if (width < 2)
                width = 2; // Still visible when nearly empty.
            const SDL_Rect levelRect{track.x, track.y, width, track.h};
            SDL_FillSurfaceRect(surface, &levelRect, fill);
        }
        return surface;
    }

    void Refresh(const ClientTrayStatus& status)
    {
        char text[128];
        if (!status.connected)
        {
            const char* label = status.connecting ? "Conectando ao fone…"
                : status.waiting                  ? "Aguardando o fone — conecta sozinho"
                                                  : "Desconectado — abra a janela para conectar";
            SDL_snprintf(text, sizeof(text), "%s", status.connecting ? "Conectando ao fone…" : "Fone desconectado");
            SDL_SetTrayEntryLabel(gEntryStatus, label);
            SDL_SetTrayTooltip(gTray, text);
        }
        else
        {
            const char* name = status.modelName[0] ? status.modelName : "Fone";
            if (status.hasBattery)
                SDL_snprintf(text, sizeof(text), "%s — %u%%%s", name, static_cast<unsigned>(status.batteryPercent),
                             status.charging ? " (carregando)" : "");
            else
                SDL_snprintf(text, sizeof(text), "%s", name);
            SDL_SetTrayEntryLabel(gEntryStatus, text);
            if (status.hasNoise)
            {
                const size_t len = SDL_strlen(text);
                SDL_snprintf(text + len, sizeof(text) - len, " · %s", ModeLabel(status.noiseMode));
            }
            SDL_SetTrayTooltip(gTray, text);
        }

        const bool noise = status.connected && status.hasNoise;
        SDL_SetTrayEntryEnabled(gEntryCancel, noise && status.canCancel);
        SDL_SetTrayEntryEnabled(gEntryAmbient, noise && status.canAmbient);
        SDL_SetTrayEntryEnabled(gEntryOff, noise);
        SDL_SetTrayEntryEnabled(gEntryNext, noise);
        SDL_SetTrayEntryChecked(gEntryCancel, noise && status.noiseMode == MDR_NOISE_MODE_CANCELLING);
        SDL_SetTrayEntryChecked(gEntryAmbient, noise && status.noiseMode == MDR_NOISE_MODE_AMBIENT);
        SDL_SetTrayEntryChecked(gEntryOff, noise && status.noiseMode == MDR_NOISE_MODE_OFF);
        SDL_SetTrayEntryEnabled(gEntryEq, status.connected && status.eqUsable);
        for (int i = 0; i < gEqEntryCount; ++i)
        {
            const bool compatible =
                clientGetEqPresetBandCount(i) == static_cast<int>(status.eqBandCount) && status.eqUsable;
            SDL_SetTrayEntryEnabled(gEqEntries[i], status.connected && compatible);
            SDL_SetTrayEntryChecked(gEqEntries[i], status.connected && status.eqActive == i);
        }
        SDL_SetTrayEntryChecked(gEntryAutoConnect, status.autoConnect);
        if (gEntryAutostart)
            SDL_SetTrayEntryChecked(gEntryAutostart, autostartIsEnabled());

        if (SDL_Surface* icon = MakeTrayIcon(status))
        {
            SDL_SetTrayIcon(gTray, icon); // SDL converts the surface to an HICON; the surface can go.
            SDL_DestroySurface(icon);
        }
    }

    void SDLCALL OnMode(void* userdata, SDL_TrayEntry*)
    {
        gRequestedMode = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
        // SDL flips a checkbox on click; force a rewrite so the menu reflects the device, not the click.
        gForceRefresh = true;
        Wake();
    }

    void SDLCALL OnNext(void*, SDL_TrayEntry*)
    {
        trayRequestNextMode();
    }

    void SDLCALL OnOpen(void*, SDL_TrayEntry*)
    {
        // Not a toggle: the open menu always steals focus, so "focused" can't be detected from here.
        trayShowWindow();
    }

    // Same order as the headphones' NC/AMB button: cancelling -> ambient -> off, skipping unsupported modes.
    MDRNoiseMode NextMode(const ClientTrayStatus& status)
    {
        const MDRNoiseMode cycle[] = {MDR_NOISE_MODE_CANCELLING, MDR_NOISE_MODE_AMBIENT, MDR_NOISE_MODE_OFF};
        constexpr int count = static_cast<int>(sizeof(cycle) / sizeof(cycle[0]));
        int current = -1; // Unknown/other modes (e.g. wind reduction) restart the cycle.
        for (int i = 0; i < count; ++i)
            if (cycle[i] == status.noiseMode)
                current = i;
        for (int step = 1; step <= count; ++step)
        {
            const MDRNoiseMode candidate = cycle[(current + step + count) % count];
            if ((candidate == MDR_NOISE_MODE_CANCELLING && !status.canCancel) ||
                (candidate == MDR_NOISE_MODE_AMBIENT && !status.canAmbient))
                continue;
            return candidate;
        }
        return MDR_NOISE_MODE_OFF;
    }

    void SDLCALL OnEqPreset(void* userdata, SDL_TrayEntry*)
    {
        gRequestedEq = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
        gForceRefresh = true; // SDL flipped the checkbox; show what the device ends up with instead.
        Wake();
    }

    // Main thread, under the lock (reads preset names from Client.cpp).
    void RebuildEqMenu()
    {
        if (!gEqMenu)
            return;
        for (int i = 0; i < gEqEntryCount; ++i)
            SDL_RemoveTrayEntry(gEqEntries[i]);
        if (gEqPlaceholder)
            SDL_RemoveTrayEntry(gEqPlaceholder);
        gEqEntryCount = 0;
        gEqPlaceholder = nullptr;
        const int count = clientGetEqPresetCount();
        for (int i = 0; i < count && i < kMaxEqPresets; ++i)
        {
            const char* name = clientGetEqPresetName(i);
            SDL_TrayEntry* entry = SDL_InsertTrayEntryAt(gEqMenu, -1, name, SDL_TRAYENTRY_CHECKBOX);
            if (!entry)
                break;
            SDL_SetTrayEntryCallback(entry, OnEqPreset, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
            gEqEntries[gEqEntryCount++] = entry;
        }
        if (gEqEntryCount == 0)
            gEqPlaceholder = SDL_InsertTrayEntryAt(gEqMenu, -1, "Nenhum preset — salve um pela janela",
                                                   SDL_TRAYENTRY_BUTTON | SDL_TRAYENTRY_DISABLED);
        gEqShownVersion = clientGetEqPresetsVersion();
        gForceRefresh = true;
    }

    void SDLCALL OnAutoConnect(void*, SDL_TrayEntry* entry)
    {
        // SDL already flipped the checkbox: its new state is what the user asked for.
        gRequestedAutoConnect = SDL_GetTrayEntryChecked(entry) ? 1 : 0;
        gForceRefresh = true;
        Wake();
    }

    void SDLCALL OnAutostart(void*, SDL_TrayEntry* entry)
    {
        // Registry only, no libmdr: safe to do right here, outside the worker lock.
        if (!autostartSetEnabled(SDL_GetTrayEntryChecked(entry)))
            SDL_Log("Autostart: unable to update the Run key");
        SDL_SetTrayEntryChecked(entry, autostartIsEnabled());
    }

    void SDLCALL OnQuit(void*, SDL_TrayEntry*)
    {
        SDL_Event quit{};
        quit.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quit);
    }

    SDL_TrayEntry* AddEntry(SDL_TrayMenu* menu, const char* label, SDL_TrayEntryFlags flags,
                            SDL_TrayCallback callback = nullptr, void* userdata = nullptr)
    {
        SDL_TrayEntry* entry = SDL_InsertTrayEntryAt(menu, -1, label, flags);
        if (entry && callback)
            SDL_SetTrayEntryCallback(entry, callback, userdata);
        return entry;
    }
} // namespace

void traySetWindowIcon(SDL_Window* window)
{
    // Explicit WM_SETICON with several sizes, so the taskbar/title bar never fall back to a stale cached
    // .exe icon. SDL picks the closest size per DPI from the alternates.
    constexpr int kSizes[] = {32, 16, 48, 64, 128, 256};
    SDL_Surface* icon{};
    for (const int size : kSizes)
    {
        const int drawn = size < kSpriteSize ? kSpriteSize : size;
        SDL_Surface* surface = SDL_CreateSurface(drawn, drawn, SDL_PIXELFORMAT_ARGB8888);
        if (!surface)
            continue;
        SDL_FillSurfaceRect(surface, nullptr, SDL_MapSurfaceRGBA(surface, 0, 0, 0, 0));
        DrawHeadphone(surface, drawn, 255);
        if (!icon)
            icon = surface;
        else
        {
            SDL_AddSurfaceAlternateImage(icon, surface);
            SDL_DestroySurface(surface); // The primary surface keeps its own reference.
        }
    }
    if (icon)
    {
        if (!SDL_SetWindowIcon(window, icon))
            SDL_Log("SDL_SetWindowIcon: %s", SDL_GetError());
        SDL_DestroySurface(icon);
    }
}

bool trayInit(SDL_Window* window)
{
#ifdef _WIN32
    gTrayWindow = window;
    ClientTrayStatus disconnected{};
    SDL_Surface* icon = MakeTrayIcon(disconnected);
    gTray = SDL_CreateTray(icon, "Fone desconectado");
    SDL_DestroySurface(icon);
    if (!gTray)
        return false;
    SDL_TrayMenu* menu = SDL_CreateTrayMenu(gTray);
    if (!menu)
    {
        trayDestroy();
        return false;
    }
    gEntryStatus = AddEntry(menu, "Desconectado", SDL_TRAYENTRY_BUTTON | SDL_TRAYENTRY_DISABLED);
    AddEntry(menu, nullptr, SDL_TRAYENTRY_BUTTON); // separator
    gEntryCancel = AddEntry(menu, ModeLabel(MDR_NOISE_MODE_CANCELLING), SDL_TRAYENTRY_CHECKBOX, OnMode,
                            reinterpret_cast<void*>(static_cast<intptr_t>(MDR_NOISE_MODE_CANCELLING)));
    gEntryAmbient = AddEntry(menu, ModeLabel(MDR_NOISE_MODE_AMBIENT), SDL_TRAYENTRY_CHECKBOX, OnMode,
                             reinterpret_cast<void*>(static_cast<intptr_t>(MDR_NOISE_MODE_AMBIENT)));
    gEntryOff = AddEntry(menu, ModeLabel(MDR_NOISE_MODE_OFF), SDL_TRAYENTRY_CHECKBOX, OnMode,
                         reinterpret_cast<void*>(static_cast<intptr_t>(MDR_NOISE_MODE_OFF)));
    gEntryNext = AddEntry(menu, "Próximo modo", SDL_TRAYENTRY_BUTTON, OnNext);
    gEntryEq = AddEntry(menu, "Equalizador", SDL_TRAYENTRY_SUBMENU);
    gEqMenu = gEntryEq ? SDL_CreateTraySubmenu(gEntryEq) : nullptr;
    gEqShownVersion = 0; // Filled by the first trayUpdate(), which holds the lock.
    AddEntry(menu, nullptr, SDL_TRAYENTRY_BUTTON); // separator
    AddEntry(menu, nullptr, SDL_TRAYENTRY_BUTTON); // separator
    gEntryAutoConnect = AddEntry(menu, "Reconectar automaticamente", SDL_TRAYENTRY_CHECKBOX, OnAutoConnect);
    if (autostartSupported())
        gEntryAutostart = AddEntry(menu, "Iniciar com o Windows", SDL_TRAYENTRY_CHECKBOX, OnAutostart);
    AddEntry(menu, nullptr, SDL_TRAYENTRY_BUTTON); // separator
    gEntryWindow = AddEntry(menu, "Abrir janela", SDL_TRAYENTRY_BUTTON, OnOpen);
    AddEntry(menu, "Sair", SDL_TRAYENTRY_BUTTON, OnQuit);
    gForceRefresh = true;
    return true;
#else
    (void)window;
    return false;
#endif
}

void trayUpdate()
{
    if (!gTray)
        return;
    const int autoConnect = gRequestedAutoConnect.exchange(-1);
    if (autoConnect >= 0)
        clientSetAutoConnect(autoConnect == 1);
    if (gEqShownVersion != clientGetEqPresetsVersion())
        RebuildEqMenu();
    const int eqPreset = gRequestedEq.exchange(-1);
    if (eqPreset >= 0 && !clientApplyEqPreset(eqPreset))
        SDL_Log("EQ preset %d could not be applied", eqPreset);

    ClientTrayStatus status{};
    clientGetTrayStatus(status);
    const int requested = gRequestedMode.exchange(kRequestNone);
    const bool applied =
        (requested == kRequestNext && status.connected && status.hasNoise && clientSetNoiseMode(NextMode(status))) ||
        (requested >= 0 && clientSetNoiseMode(static_cast<MDRNoiseMode>(requested)));
    if (applied)
        clientGetTrayStatus(status); // DESIRED state now holds the new mode.
    if (!gForceRefresh.exchange(false) && SameStatus(status, gShown))
        return;
    gShown = status;
    Refresh(status);
}

bool trayIsActive()
{
    return gTray != nullptr;
}

void trayShowWindow()
{
    if (!gTrayWindow)
        return;
    SDL_ShowWindow(gTrayWindow);
    SDL_RestoreWindow(gTrayWindow);
    SDL_RaiseWindow(gTrayWindow);
    Wake();
}

void trayRequestNextMode()
{
    gRequestedMode = kRequestNext;
    gForceRefresh = true;
    Wake();
}

void trayToggleWindow()
{
    if (!gTrayWindow)
        return;
    const SDL_WindowFlags flags = SDL_GetWindowFlags(gTrayWindow);
    const bool visible = !(flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED));
    if (visible && (flags & SDL_WINDOW_INPUT_FOCUS))
        SDL_HideWindow(gTrayWindow);
    else
        trayShowWindow(); // Hidden, minimized, or behind other windows: bring it to the front.
}

void traySetShortcutHints(bool nextMode, bool toggleWindow)
{
    // A tab in a Win32 menu label right-aligns the text after it, like an accelerator column.
    if (gEntryNext && nextMode)
        SDL_SetTrayEntryLabel(gEntryNext, "Próximo modo\tCtrl+Alt+H");
    if (gEntryWindow && toggleWindow)
        SDL_SetTrayEntryLabel(gEntryWindow, "Abrir janela\tCtrl+Alt+J");
}

void trayDestroy()
{
    if (gTray)
        SDL_DestroyTray(gTray);
    gTray = nullptr;
    gTrayWindow = nullptr;
    gEntryStatus = gEntryCancel = gEntryAmbient = gEntryOff = gEntryNext = gEntryWindow = nullptr;
    gEntryAutoConnect = gEntryAutostart = nullptr;
    gEntryEq = nullptr;
    gEqMenu = nullptr;
    gEqEntryCount = 0;
    gEqPlaceholder = nullptr;
    gEqShownVersion = 0;
}
