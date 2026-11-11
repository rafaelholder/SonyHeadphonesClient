#include "Tray.hpp"

#include <SDL3/SDL.h>
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

    // Written by menu callbacks (main thread, outside the worker lock), consumed by trayUpdate().
    std::atomic<int> gRequestedMode{-1};
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
        return a.connected == b.connected && a.hasBattery == b.hasBattery && a.batteryPercent == b.batteryPercent &&
            a.charging == b.charging && a.hasNoise == b.hasNoise && a.canCancel == b.canCancel &&
            a.canAmbient == b.canAmbient && a.noiseMode == b.noiseMode &&
            std::strcmp(a.modelName, b.modelName) == 0;
    }

    // Draws a battery glyph: outline + fill proportional to the level.
    // Green > 50%, yellow 21-50%, red <= 20%, blue while charging, outline only when disconnected.
    SDL_Surface* MakeBatteryIcon(const ClientTrayStatus& status)
    {
        SDL_Surface* surface = SDL_CreateSurface(kIconSize, kIconSize, SDL_PIXELFORMAT_ARGB8888);
        if (!surface)
            return nullptr;
        SDL_FillSurfaceRect(surface, nullptr, SDL_MapSurfaceRGBA(surface, 0, 0, 0, 0));

        const bool active = status.connected && status.hasBattery;
        const Uint32 outline =
            active ? SDL_MapSurfaceRGBA(surface, 240, 240, 240, 255) : SDL_MapSurfaceRGBA(surface, 140, 140, 140, 255);
        const Uint32 clear = SDL_MapSurfaceRGBA(surface, 0, 0, 0, 0);

        // Body 2..27 x 8..24, 2px stroke; terminal 28..30 x 12..20.
        const SDL_Rect body{2, 8, 26, 17};
        const SDL_Rect inner{4, 10, 22, 13};
        const SDL_Rect terminal{28, 12, 3, 9};
        SDL_FillSurfaceRect(surface, &body, outline);
        SDL_FillSurfaceRect(surface, &inner, clear);
        SDL_FillSurfaceRect(surface, &terminal, outline);

        if (active)
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
            // Inside the stroke with a 1px gap; at least 2px so a low battery is still visible.
            const int maxWidth = inner.w - 2;
            int width = static_cast<int>((maxWidth * level + 50) / 100);
            if (width < 2)
                width = 2;
            const SDL_Rect level_rect{inner.x + 1, inner.y + 1, width, inner.h - 2};
            SDL_FillSurfaceRect(surface, &level_rect, fill);
        }
        return surface;
    }

    void Refresh(const ClientTrayStatus& status)
    {
        char text[128];
        if (!status.connected)
        {
            SDL_snprintf(text, sizeof(text), "Fone desconectado");
            SDL_SetTrayEntryLabel(gEntryStatus, "Desconectado — abra a janela para conectar");
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
        SDL_SetTrayEntryChecked(gEntryCancel, noise && status.noiseMode == MDR_NOISE_MODE_CANCELLING);
        SDL_SetTrayEntryChecked(gEntryAmbient, noise && status.noiseMode == MDR_NOISE_MODE_AMBIENT);
        SDL_SetTrayEntryChecked(gEntryOff, noise && status.noiseMode == MDR_NOISE_MODE_OFF);

        if (SDL_Surface* icon = MakeBatteryIcon(status))
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

    void SDLCALL OnOpen(void*, SDL_TrayEntry*)
    {
        trayShowWindow();
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

bool trayInit(SDL_Window* window)
{
#ifdef _WIN32
    gTrayWindow = window;
    ClientTrayStatus disconnected{};
    SDL_Surface* icon = MakeBatteryIcon(disconnected);
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
    AddEntry(menu, nullptr, SDL_TRAYENTRY_BUTTON); // separator
    AddEntry(menu, "Abrir janela", SDL_TRAYENTRY_BUTTON, OnOpen);
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
    const int requested = gRequestedMode.exchange(-1);
    if (requested >= 0)
        clientSetNoiseMode(static_cast<MDRNoiseMode>(requested));

    ClientTrayStatus status{};
    clientGetTrayStatus(status);
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

void trayDestroy()
{
    if (gTray)
        SDL_DestroyTray(gTray);
    gTray = nullptr;
    gTrayWindow = nullptr;
    gEntryStatus = gEntryCancel = gEntryAmbient = gEntryOff = nullptr;
}
