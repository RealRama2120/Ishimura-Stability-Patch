#pragma once

#include <windows.h>

namespace Config {

struct Settings {
    int cpuCoreLimit = 8;

    bool physicsAndAudioSafety = true;
    int maxFps = 60;
    bool allowUnsafeHighFps = false;

    bool fixVSync = true;
    bool borderlessWindowed = true;

    bool anisotropicFiltering = true;
    int maxAnisotropy = 16;

    bool fixSubtitleScaling = true;
    int subtitleBaseHeight = 720;

    bool writeLog = true;
};

extern Settings settings;

void Load(HMODULE module);
const wchar_t* IniPath();
int EffectiveFpsLimit();

} // namespace Config
