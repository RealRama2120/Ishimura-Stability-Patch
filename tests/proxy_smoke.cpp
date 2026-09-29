#include <windows.h>

#include <cstdio>
#include <cwchar>

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::fwprintf(stderr, L"usage: proxy_smoke.exe <xinput1_3.dll>\n");
        return 2;
    }

    HMODULE module = LoadLibraryW(argv[1]);
    if (!module) {
        std::fwprintf(stderr, L"LoadLibrary failed: %lu\n", GetLastError());
        return 3;
    }

    const char* namedExports[] = {
        "XInputGetState",
        "XInputSetState",
        "XInputGetCapabilities",
        "XInputEnable",
        "XInputGetDSoundAudioDeviceGuids",
        "XInputGetBatteryInformation",
        "XInputGetKeystroke"
    };

    for (const char* name : namedExports) {
        if (!GetProcAddress(module, name)) {
            std::fprintf(stderr, "missing export: %s\n", name);
            FreeLibrary(module);
            return 4;
        }
    }

    for (WORD ordinal = 100; ordinal <= 103; ++ordinal) {
        if (!GetProcAddress(module, MAKEINTRESOURCEA(ordinal))) {
            std::fprintf(stderr, "missing ordinal: %u\n", ordinal);
            FreeLibrary(module);
            return 5;
        }
    }

    using GetStateFn = DWORD (WINAPI*)(DWORD, void*);
    auto getState = reinterpret_cast<GetStateFn>(
        GetProcAddress(module, "XInputGetState"));
    unsigned char state[32] = {};
    const DWORD status = getState(3, state);
    if (status != ERROR_SUCCESS && status != ERROR_DEVICE_NOT_CONNECTED) {
        std::fprintf(stderr, "unexpected XInputGetState status: %lu\n", status);
        FreeLibrary(module);
        return 6;
    }

    FreeLibrary(module);
    std::printf("proxy smoke test passed\n");
    return 0;
}

