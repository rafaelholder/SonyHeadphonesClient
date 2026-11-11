#pragma once
// Small persistent settings file: %APPDATA%\rafaelholder\SonyHeadphonesClient\settings.ini (SDL_GetPrefPath).
// Plain "key=value" lines. Holds only the last device's Bluetooth address and the auto-connect switch.

enum class SavedProtocol : int
{
    Auto = 0,
    V2 = 1,
    V1 = 2,
};

struct ClientSettings
{
    char lastDevice[18]{}; // "XX:XX:XX:XX:XX:XX", empty when none
    SavedProtocol lastProtocol{SavedProtocol::Auto};
    bool autoConnect{true};
};

// Missing or malformed values fall back to the defaults above.
extern ClientSettings settingsLoad();
extern bool settingsSave(const ClientSettings& settings);
extern bool settingsIsValidAddress(const char* address);
