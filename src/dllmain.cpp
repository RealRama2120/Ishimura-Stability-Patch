#include "runtime.hpp"

#include "config.hpp"
#include "d3d_hooks.hpp"
#include "game_patches.hpp"
#include "logger.hpp"
#include "version.hpp"
#include "xinput_proxy.hpp"

#include <windows.h>

#include <cwchar>

namespace Runtime {

HMODULE module = nullptr;
bool isDeadSpace = false;
DWORD_PTR earlyAffinityMask = 0;
bool earlyAffinityApplied = false;

bool IsDeadSpaceProcess() {
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return false;

    const wchar_t* name = std::wcsrchr(path, L'\\');
    name = name ? name + 1 : path;
    return _wcsicmp(name, L"Dead Space.exe") == 0;
}

void ApplyEarlyCoreLimit(unsigned int maximumLogicalProcessors) {
    maximumLogicalProcessors =
        (maximumLogicalProcessors < 1 || maximumLogicalProcessors > 8)
        ? 8 : maximumLogicalProcessors;

    DWORD_PTR processMask = 0;
    DWORD_PTR systemMask = 0;
    if (!GetProcessAffinityMask(
            GetCurrentProcess(), &processMask, &systemMask) || processMask == 0)
        return;

    DWORD_PTR selected = 0;
    unsigned int count = 0;
    for (unsigned int bit = 0;
         bit < sizeof(DWORD_PTR) * 8 && count < maximumLogicalProcessors;
         ++bit) {
        const DWORD_PTR candidate = static_cast<DWORD_PTR>(1) << bit;
        if ((processMask & candidate) != 0) {
            selected |= candidate;
            ++count;
        }
    }

    if (selected == 0)
        return;

    earlyAffinityMask = selected;
    earlyAffinityApplied =
        SetProcessAffinityMask(GetCurrentProcess(), selected) != FALSE;
}

DWORD WINAPI WorkerThread(void*) {
    XInputProxy::EnsureLoaded();
    Config::Load(module);
    Logger::Initialise(module, Config::settings.writeLog);

    Logger::Write(L"Ishimura Stability Patch %S starting.", ISP_VERSION_STRING);
    Logger::Write(L"Configuration: %s", Config::IniPath());
    Logger::Write(L"Early CPU compatibility limit: applied=%u, mask=0x%p.",
        earlyAffinityApplied ? 1u : 0u,
        reinterpret_cast<void*>(earlyAffinityMask));

    // The earliest loader-safe pass uses eight processors. A lower configured
    // value can be applied here before normal game initialization proceeds.
    if (Config::settings.cpuCoreLimit < 8)
        ApplyEarlyCoreLimit(
            static_cast<unsigned int>(Config::settings.cpuCoreLimit));

    Logger::Write(
        L"Timing safety=%u, requested FPS=%d, effective FPS=%d, unsafe high FPS=%u.",
        Config::settings.physicsAndAudioSafety ? 1u : 0u,
        Config::settings.maxFps, Config::EffectiveFpsLimit(),
        Config::settings.allowUnsafeHighFps ? 1u : 0u);
    Logger::Write(
        L"Audio/network policy: no audio, Winsock, NetBIOS, or global timer hooks are installed.");

    D3DHooks::Install();
    GamePatches::Initialise();

    for (;;) {
        GamePatches::Update();
        Sleep(250);
    }
}

} // namespace Runtime

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        Runtime::module = module;
        DisableThreadLibraryCalls(module);

        Runtime::isDeadSpace = Runtime::IsDeadSpaceProcess();
        if (!Runtime::isDeadSpace)
            return TRUE;

        // This runs before the legacy CPU enumeration that can overflow on
        // modern high-core-count systems. It does no file or network I/O.
        Runtime::ApplyEarlyCoreLimit(8);

        HANDLE worker = CreateThread(
            nullptr, 0, Runtime::WorkerThread, nullptr, 0, nullptr);
        if (worker)
            CloseHandle(worker);
    }
    return TRUE;
}
