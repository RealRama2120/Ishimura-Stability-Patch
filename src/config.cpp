#include "config.hpp"

#include <algorithm>
#include <string>

namespace Config {

Settings settings;

namespace {

std::wstring g_iniPath;

int ReadInt(const wchar_t* section, const wchar_t* key, int fallback) {
    return static_cast<int>(GetPrivateProfileIntW(
        section, key, fallback, g_iniPath.c_str()));
}

bool ReadBool(const wchar_t* section, const wchar_t* key, bool fallback) {
    return ReadInt(section, key, fallback ? 1 : 0) != 0;
}

std::wstring ModuleDirectory(HMODULE module) {
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return L".";

    std::wstring result(path, length);
    const std::wstring::size_type separator = result.find_last_of(L"\\/");
    if (separator == std::wstring::npos)
        return L".";
    result.resize(separator);
    return result;
}

} // namespace

void Load(HMODULE module) {
    g_iniPath = ModuleDirectory(module) + L"\\IshimuraStabilityPatch.ini";

    settings.cpuCoreLimit = std::clamp(
        ReadInt(L"Compatibility", L"CpuCoreLimit", 8), 1, 8);

    settings.physicsAndAudioSafety = ReadBool(
        L"Timing", L"PhysicsAndAudioSafety", true);
    settings.maxFps = std::clamp(
        ReadInt(L"Timing", L"MaxFPS", 60), 0, 1000);
    settings.allowUnsafeHighFps = ReadBool(
        L"Timing", L"AllowUnsafeHighFPS", false);

    settings.fixVSync = ReadBool(L"Display", L"FixVSync", true);
    settings.borderlessWindowed = ReadBool(
        L"Display", L"BorderlessWindowed", true);

    settings.anisotropicFiltering = ReadBool(
        L"Graphics", L"AnisotropicFiltering", true);
    settings.maxAnisotropy = std::clamp(
        ReadInt(L"Graphics", L"MaxAnisotropy", 16), 1, 16);

    settings.fixSubtitleScaling = ReadBool(
        L"Fixes", L"FixSubtitleScaling", true);
    settings.subtitleBaseHeight = std::clamp(
        ReadInt(L"Fixes", L"SubtitleBaseHeight", 720), 240, 4320);

    settings.writeLog = ReadBool(L"Diagnostics", L"WriteLog", true);
}

const wchar_t* IniPath() {
    return g_iniPath.c_str();
}

int EffectiveFpsLimit() {
    int requested = settings.maxFps;
    if (settings.physicsAndAudioSafety) {
        if (requested <= 0)
            requested = 60;
        if (!settings.allowUnsafeHighFps)
            requested = std::min(requested, 60);
    }
    return requested;
}

} // namespace Config
