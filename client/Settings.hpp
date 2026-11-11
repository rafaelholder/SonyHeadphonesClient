#pragma once
// Small persistent settings file: %APPDATA%\rafaelholder\SonyHeadphonesClient\settings.ini (SDL_GetPrefPath).
// Plain "key=value" lines: last device, auto-connect switch and the user's EQ presets.
#include <cstdint>

enum class SavedProtocol : int
{
    Auto = 0,
    V2 = 1,
    V1 = 2,
};

constexpr int kMaxEqPresets = 16;
constexpr int kMaxEqBands = 10;
constexpr int kEqPresetNameSize = 48; // bytes, UTF-8, NUL included

// One line per preset: eq_preset=<name>|<clear bass>|<band 1>,<band 2>,...
struct EqPreset
{
    char name[kEqPresetNameSize]{};
    int32_t clearBass{};
    int bandCount{};
    int32_t bands[kMaxEqBands]{};
};

struct ClientSettings
{
    char lastDevice[18]{}; // "XX:XX:XX:XX:XX:XX", empty when none
    SavedProtocol lastProtocol{SavedProtocol::Auto};
    bool autoConnect{true};

    EqPreset eqPresets[kMaxEqPresets]{};
    int eqPresetCount{};
    bool eqPresetsInitialized{}; // Starter presets were offered once; deleting them all doesn't bring them back.
};

// Missing or malformed values fall back to the defaults above. On first run, starter EQ presets are added.
extern ClientSettings settingsLoad();
extern bool settingsSave(const ClientSettings& settings);
extern bool settingsIsValidAddress(const char* address);
// Copies a user-typed name into a preset, dropping the separator '|' and control characters.
extern void settingsSetPresetName(EqPreset& preset, const char* name);
