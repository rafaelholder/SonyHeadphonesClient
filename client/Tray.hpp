#pragma once
// Notification-area (system tray) icon: battery level + quick noise-mode switch.
// Built on SDL_tray, which ships with the SDL already vendored by contrib/ - no new dependency.
#include <cstdint>
#include <mdr-c/Headphones.h>

struct SDL_Window;

struct ClientTrayStatus
{
    bool autoConnect{};
    bool hasSavedDevice{};
    bool waiting{}; // Not connected, auto-connect will pick the saved headphones up when they appear.
    bool connecting{}; // Control channel being opened / device initializing.
    bool connected{};
    char modelName[64]{};
    bool hasBattery{};
    uint32_t batteryPercent{};
    bool charging{};
    bool hasNoise{};
    bool canCancel{};
    bool canAmbient{};
    MDRNoiseMode noiseMode{MDR_NOISE_MODE_OFF};
    bool eqUsable{}; // Device has an equalizer and it is usable right now.
    uint32_t eqBandCount{}; // Current band layout; presets saved for another layout are greyed out.
    int eqActive{-1}; // User preset matching the device's EQ, or -1.
};

// Implemented in Client.cpp. Must be called with the client worker lock held (see SDLMain.cpp).
extern void clientGetTrayStatus(ClientTrayStatus& out);
extern bool clientSetNoiseMode(MDRNoiseMode mode);
extern void clientSetAutoConnect(bool enabled);
// User EQ presets (stored in settings.ini). Same lock rules.
extern uint32_t clientGetEqPresetsVersion();
extern int clientGetEqPresetCount();
extern const char* clientGetEqPresetName(int index);
extern int clientGetEqPresetBandCount(int index);
extern bool clientApplyEqPreset(int index);

// Main thread. Sets the pixel-art headphone as the window (title bar / taskbar) icon.
extern void traySetWindowIcon(SDL_Window* window);
// Main thread only. Returns false when the platform has no tray (the app then behaves as before).
extern bool trayInit(SDL_Window* window);
// Main thread, with the client worker lock held: applies pending menu actions and refreshes icon/menu.
extern void trayUpdate();
extern bool trayIsActive();
extern void trayShowWindow();
// Main thread, outside the lock (menu / hotkey handlers). Queued and applied by trayUpdate().
extern void trayRequestNextMode();
extern void trayToggleWindow();
// Appends the shortcut text to the matching menu entries (only for shortcuts that registered).
extern void traySetShortcutHints(bool nextMode, bool toggleWindow);
extern void trayDestroy();
