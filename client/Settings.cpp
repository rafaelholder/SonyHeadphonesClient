#include "Settings.hpp"

#include <SDL3/SDL.h>
#include <cctype>
#include <cstring>

namespace
{
    constexpr const char* kOrg = "rafaelholder";
    constexpr const char* kApp = "SonyHeadphonesClient";
    constexpr const char* kFile = "settings.ini";

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

    void Apply(ClientSettings& settings, const char* key, const char* value)
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
        // Unknown keys are ignored.
    }
} // namespace

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
    if (!data)
        return settings;
    constexpr size_t kMaxSize = 4096; // Anything bigger is not ours.
    if (size <= kMaxSize)
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
    char text[256];
    const int length = SDL_snprintf(text, sizeof(text),
                                    "# SonyHeadphonesClient (rafaelholder fork)\n"
                                    "last_device=%s\n"
                                    "last_protocol=%s\n"
                                    "auto_connect=%d\n",
                                    settings.lastDevice, protocol, settings.autoConnect ? 1 : 0);
    const bool saved = length > 0 && SDL_SaveFile(path, text, static_cast<size_t>(length));
    SDL_free(path);
    return saved;
}
