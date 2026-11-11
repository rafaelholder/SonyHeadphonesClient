#pragma once
// Notification-area (system tray) icon: battery level + quick noise-mode switch.
// Built on SDL_tray, which ships with the SDL already vendored by contrib/ - no new dependency.
#include <cstdint>
#include <mdr-c/Headphones.h>

struct SDL_Window;

struct ClientTrayStatus
{
    bool connected{};
    char modelName[64]{};
    bool hasBattery{};
    uint32_t batteryPercent{};
    bool charging{};
    bool hasNoise{};
    bool canCancel{};
    bool canAmbient{};
    MDRNoiseMode noiseMode{MDR_NOISE_MODE_OFF};
};

// Implemented in Client.cpp. Must be called with the client worker lock held (see SDLMain.cpp).
extern void clientGetTrayStatus(ClientTrayStatus& out);
extern bool clientSetNoiseMode(MDRNoiseMode mode);

// Main thread only. Returns false when the platform has no tray (the app then behaves as before).
extern bool trayInit(SDL_Window* window);
// Main thread, with the client worker lock held: applies pending menu actions and refreshes icon/menu.
extern void trayUpdate();
extern bool trayIsActive();
extern void trayShowWindow();
extern void trayDestroy();
