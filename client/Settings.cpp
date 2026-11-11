#include "Settings.hpp"

#include <SDL3/SDL.h>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace
{
    constexpr const char* kOrg = "rafaelholder";
    constexpr const char* kApp = "SonyHeadphonesClient";
    constexpr const char* kFile = "settings.ini";
    constexpr size_t kMaxFileSize = 16 * 1024; // Anything bigger is not ours.

    // Returns an SDL-allocated path (free with SDL_free) or nullptr.
    char* SettingsPath()
    {
        char* dir = SDL_GetPrefPath(kOrg, kApp); // Creates the folder if needed.
        if (!dir)
            return nullptr;
        char* path{};
        if (SDL_asprintf(&path, "%s%s", dir, kFile) < 0)
            path = nullptr;
        SDL_free(dir);
        return path;
    }

    void Trim(char*& begin, char*& end)
    {
        while (begin < end && std::isspace(static_cast<unsigned char>(*begin)))
            ++begin;
        while (end > begin && std::isspace(static_cast<unsigned char>(end[-1])))
            --end;
    }

    // Strict integer parse in [-100, 100]; device ranges are much narrower and are clamped when applied.
    bool ParseInt(const char* text, int32_t& out)
    {
        if (!text || !*text)
            return false;
        char* end{};
        const long value = std::strtol(text, &end, 10);
        if (*end != '\0' || value < -100 || value > 100)
            return false;
        out = static_cast<int32_t>(value);
        return true;
    }

    // "<name>|<clear bass>|<b1>,<b2>,..." -> preset. Rejects the whole line on any malformed field.
    bool ParsePreset(char* value, EqPreset& preset)
    {
        char* bar1 = std::strchr(value, '|');
        if (!bar1)
            return false;
        char* bar2 = std::strchr(bar1 + 1, '|');
        if (!bar2)
            return false;
        *bar1 = '\0';
        *bar2 = '\0';
        EqPreset parsed{};
        settingsSetPresetName(parsed, value);
        if (!parsed.name[0] || !ParseInt(bar1 + 1, parsed.clearBass))
            return false;
        char* band = bar2 + 1;
        while (*band)
        {
            char* comma = std::strchr(band, ',');
            if (comma)
                *comma = '\0';
            if (parsed.bandCount >= kMaxEqBands || !ParseInt(band, parsed.bands[parsed.bandCount]))
                return false;
            ++parsed.bandCount;
            if (!comma)
                break;
            band = comma + 1;
        }
        if (parsed.bandCount == 0)
            return false;
        preset = parsed;
        return true;
    }

    void Apply(ClientSettings& settings, const char* key, char* value)
    {
        if (std::strcmp(key, "last_device") == 0)
        {
            if (settingsIsValidAddress(value))
                SDL_strlcpy(settings.lastDevice, value, sizeof(settings.lastDevice));
        }
        else if (std::strcmp(key, "last_protocol") == 0)
        {
            if (std::strcmp(value, "v2") == 0)
                settings.lastProtocol = SavedProtocol::V2;
            else if (std::strcmp(value, "v1") == 0)
                settings.lastProtocol = SavedProtocol::V1;
            else
                settings.lastProtocol = SavedProtocol::Auto;
        }
        else if (std::strcmp(key, "auto_connect") == 0)
            settings.autoConnect = std::strcmp(value, "0") != 0;
        else if (std::strcmp(key, "eq_presets_initialized") == 0)
            settings.eqPresetsInitialized = std::strcmp(value, "0") != 0;
        else if (std::strcmp(key, "eq_preset") == 0)
        {
            if (settings.eqPresetCount < kMaxEqPresets &&
                ParsePreset(value, settings.eqPresets[settings.eqPresetCount]))
                ++settings.eqPresetCount;
        }
        // Unknown keys are ignored.
    }

    void AddStarter(ClientSettings& settings, const char* name, int32_t clearBass, const int32_t (&bands)[5])
    {
        if (settings.eqPresetCount >= kMaxEqPresets)
            return;
        EqPreset& preset = settings.eqPresets[settings.eqPresetCount++];
        settingsSetPresetName(preset, name);
        preset.clearBass = clearBass;
        preset.bandCount = 5;
        for (int i = 0; i < 5; ++i)
            preset.bands[i] = bands[i];
    }

    // Starting points for 5-band headbands such as the WH-1000XM5 (400 Hz, 1 kHz, 2.5 kHz, 6.3 kHz, 16 kHz).
    // Not tuned by ear: meant to be adjusted in the window and saved over.
    void AddStarterPresets(ClientSettings& settings)
    {
        AddStarter(settings, "Plano", 0, {0, 0, 0, 0, 0});
        AddStarter(settings, "Graves", 6, {2, 0, 0, 0, 0});
        AddStarter(settings, "Voz e podcast", -2, {-2, 1, 3, 2, 0});
        AddStarter(settings, "Jogos (passos)", -3, {-3, -1, 2, 4, 2});
    }
} // namespace

void settingsSetPresetName(EqPreset& preset, const char* name)
{
    size_t out = 0;
    for (const char* ch = name ? name : ""; *ch && out + 1 < sizeof(preset.name); ++ch)
    {
        const unsigned char c = static_cast<unsigned char>(*ch);
        if (c < 0x20 || c == 0x7f || c == '|')
            continue;
        preset.name[out++] = static_cast<char>(c);
    }
    // Don't leave half a UTF-8 sequence behind when the name was cut short.
    if (out > 0 && (static_cast<unsigned char>(preset.name[out - 1]) & 0x80))
    {
        size_t start = out - 1;
        while (start > 0 && (static_cast<unsigned char>(preset.name[start]) & 0xC0) == 0x80)
            --start;
        const unsigned char lead = static_cast<unsigned char>(preset.name[start]);
        const size_t expected = (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 1;
        if (out - start < expected)
            out = start;
    }
    preset.name[out] = '\0';
    // Trim surrounding spaces.
    char *begin = preset.name, *end = preset.name + out;
    Trim(begin, end);
    const size_t length = static_cast<size_t>(end - begin);
    std::memmove(preset.name, begin, length);
    preset.name[length] = '\0';
}

bool settingsIsValidAddress(const char* address)
{
    if (!address || std::strlen(address) != 17)
        return false;
    for (int i = 0; i < 17; ++i)
    {
        const char c = address[i];
        if (i % 3 == 2 ? c != ':' : !std::isxdigit(static_cast<unsigned char>(c)))
            return false;
    }
    return true;
}

ClientSettings settingsLoad()
{
    ClientSettings settings{};
    char* path = SettingsPath();
    if (!path)
        return settings;
    size_t size{};
    char* data = static_cast<char*>(SDL_LoadFile(path, &size)); // NUL-terminated by SDL.
    SDL_free(path);
    if (data && size <= kMaxFileSize)
    {
        char* line = data;
        char* const end = data + size;
        while (line < end)
        {
            char* lineEnd = line;
            while (lineEnd < end && *lineEnd != '\n')
                ++lineEnd;
            char* eq = static_cast<char*>(std::memchr(line, '=', static_cast<size_t>(lineEnd - line)));
            if (eq && *line != '#')
            {
                char *keyBegin = line, *keyEnd = eq, *valueBegin = eq + 1, *valueEnd = lineEnd;
                Trim(keyBegin, keyEnd);
                Trim(valueBegin, valueEnd);
                *keyEnd = '\0';
                *valueEnd = '\0';
                Apply(settings, keyBegin, valueBegin);
            }
            line = lineEnd + 1;
        }
    }
    SDL_free(data);
    if (!settings.eqPresetsInitialized)
    {
        if (settings.eqPresetCount == 0)
            AddStarterPresets(settings);
        settings.eqPresetsInitialized = true;
    }
    return settings;
}

bool settingsSave(const ClientSettings& settings)
{
    char* path = SettingsPath();
    if (!path)
        return false;
    const char* protocol = settings.lastProtocol == SavedProtocol::V2 ? "v2"
        : settings.lastProtocol == SavedProtocol::V1                  ? "v1"
                                                                      : "auto";
    char* text{};
    if (SDL_asprintf(&text,
                     "# SonyHeadphonesClient (rafaelholder fork)\n"
                     "last_device=%s\n"
                     "last_protocol=%s\n"
                     "auto_connect=%d\n"
                     "# eq_preset=<nome>|<clear bass>|<banda 1>,<banda 2>,...\n"
                     "eq_presets_initialized=%d\n",
                     settings.lastDevice, protocol, settings.autoConnect ? 1 : 0,
                     settings.eqPresetsInitialized ? 1 : 0) < 0)
    {
        SDL_free(path);
        return false;
    }
    for (int i = 0; i < settings.eqPresetCount; ++i)
    {
        const EqPreset& preset = settings.eqPresets[i];
        char bands[kMaxEqBands * 5 + 1]{};
        size_t used = 0;
        for (int b = 0; b < preset.bandCount; ++b)
            used += static_cast<size_t>(SDL_snprintf(bands + used, sizeof(bands) - used, b ? ",%d" : "%d",
                                                     static_cast<int>(preset.bands[b])));
        char* next{};
        if (SDL_asprintf(&next, "%seq_preset=%s|%d|%s\n", text, preset.name, static_cast<int>(preset.clearBass),
                         bands) < 0)
        {
            SDL_free(text);
            SDL_free(path);
            return false;
        }
        SDL_free(text);
        text = next;
    }
    const bool saved = SDL_SaveFile(path, text, SDL_strlen(text));
    SDL_free(text);
    SDL_free(path);
    return saved;
}
