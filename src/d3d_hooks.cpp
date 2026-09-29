#include "d3d_hooks.hpp"

#include "config.hpp"
#include "game_patches.hpp"
#include "logger.hpp"
#include "memory_patch.hpp"
#include "window_fix.hpp"

#include <windows.h>
#include <d3d9.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace D3DHooks {
namespace {

using Direct3DCreate9Fn = IDirect3D9* (WINAPI*)(UINT);
using CreateDeviceFn = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
    D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using ResetFn = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using PresentFn = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using SetSamplerStateFn = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);

constexpr std::size_t kD3D9CreateDeviceIndex = 16;
constexpr std::size_t kDeviceResetIndex = 16;
constexpr std::size_t kDevicePresentIndex = 17;
constexpr std::size_t kDeviceSetSamplerStateIndex = 69;
constexpr std::size_t kDeviceVtableEntries = 119;

CreateDeviceFn g_originalCreateDevice = nullptr;
ResetFn g_originalReset = nullptr;
PresentFn g_originalPresent = nullptr;
SetSamplerStateFn g_originalSetSamplerState = nullptr;

void** g_originalDeviceVtable = nullptr;
void** g_clonedDeviceVtable = nullptr;

std::atomic<bool> g_deviceObserved(false);

LARGE_INTEGER g_frequency = {};
LARGE_INTEGER g_nextFrame = {};
LARGE_INTEGER g_measurementStart = {};
unsigned int g_measurementFrames = 0;
bool g_measurementWarmupComplete = false;
bool g_measurementLogged = false;
int g_effectiveFps = 0;
DWORD g_filterCaps = 0;
DWORD g_maxSupportedAnisotropy = 1;
ULONGLONG g_nextBorderlessMaintenance = 0;

bool ReplaceVtableEntry(void** table, std::size_t index, void* replacement,
                        void** original) {
    if (!table || !replacement || !original)
        return false;

    DWORD oldProtect = 0;
    if (!VirtualProtect(&table[index], sizeof(void*), PAGE_READWRITE, &oldProtect))
        return false;

    *original = table[index];
    table[index] = replacement;

    DWORD ignored = 0;
    const BOOL restored = VirtualProtect(
        &table[index], sizeof(void*), oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), &table[index], sizeof(void*));
    return restored != FALSE;
}

void PaceFrame() {
    if (g_effectiveFps <= 0 || g_frequency.QuadPart <= 0)
        return;

    static thread_local bool pacing = false;
    if (pacing)
        return;
    pacing = true;

    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    const LONGLONG frameTicks =
        std::max<LONGLONG>(1, g_frequency.QuadPart / g_effectiveFps);

    if (g_nextFrame.QuadPart == 0) {
        g_nextFrame.QuadPart = now.QuadPart + frameTicks;
        pacing = false;
        return;
    }

    if (now.QuadPart < g_nextFrame.QuadPart) {
        for (;;) {
            QueryPerformanceCounter(&now);
            const LONGLONG remaining = g_nextFrame.QuadPart - now.QuadPart;
            if (remaining <= 0)
                break;

            if (remaining > (g_frequency.QuadPart * 2) / 1000)
                Sleep(1);
            else if (!SwitchToThread())
                YieldProcessor();
        }
    }

    QueryPerformanceCounter(&now);
    if (now.QuadPart - g_nextFrame.QuadPart > frameTicks * 4)
        g_nextFrame.QuadPart = now.QuadPart + frameTicks;
    else
        g_nextFrame.QuadPart += frameTicks;

    pacing = false;
}

void RecordMeasuredFrameRate() {
    if (g_measurementLogged || g_frequency.QuadPart <= 0)
        return;

    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    if (g_measurementStart.QuadPart == 0) {
        g_measurementStart = now;
        g_measurementFrames = 0;
        return;
    }

    if (!g_measurementWarmupComplete) {
        if (now.QuadPart - g_measurementStart.QuadPart <
            g_frequency.QuadPart * 20)
            return;
        g_measurementWarmupComplete = true;
        g_measurementStart = now;
        g_measurementFrames = 0;
        return;
    }

    ++g_measurementFrames;
    const LONGLONG elapsed = now.QuadPart - g_measurementStart.QuadPart;
    if (elapsed < g_frequency.QuadPart * 5)
        return;

    const double measured =
        static_cast<double>(g_measurementFrames) *
        static_cast<double>(g_frequency.QuadPart) /
        static_cast<double>(elapsed);
    Logger::Write(L"Measured presentation rate after pacing: %.2f FPS.", measured);
    g_measurementLogged = true;
}

void FixPresentationInterval(D3DPRESENT_PARAMETERS& parameters) {
    if (!Config::settings.fixVSync)
        return;

    if (parameters.PresentationInterval == D3DPRESENT_INTERVAL_TWO ||
        parameters.PresentationInterval == D3DPRESENT_INTERVAL_THREE ||
        parameters.PresentationInterval == D3DPRESENT_INTERVAL_FOUR) {
        Logger::Write(
            L"VSync interval corrected from %u refreshes to one refresh.",
            parameters.PresentationInterval);
        parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    }
}

void PrepareBorderlessPresentation(
    D3DPRESENT_PARAMETERS& parameters, HWND fallbackWindow) {
    if (!Config::settings.borderlessWindowed || !parameters.Windowed)
        return;

    HWND window = parameters.hDeviceWindow;
    if (!window)
        window = fallbackWindow;
    if (!window)
        window = WindowFix::TrackedWindow();

    RECT bounds = {};
    if (!WindowFix::GetBorderlessBounds(window, bounds)) {
        Logger::Write(
            L"Could not resolve the target monitor before borderless D3D9 setup; preserving requested backbuffer size.");
        return;
    }

    const UINT width = static_cast<UINT>(bounds.right - bounds.left);
    const UINT height = static_cast<UINT>(bounds.bottom - bounds.top);
    if (!GamePatches::PrepareBorderlessRenderSize(width, height)) {
        Logger::Write(
            L"Native monitor-sized backbuffer was not forced because the engine render dimensions could not be safely synchronized.");
        return;
    }
    if (parameters.BackBufferWidth != width ||
        parameters.BackBufferHeight != height) {
        Logger::Write(
            L"Corrected borderless backbuffer from %ux%u to native monitor size %ux%u.",
            parameters.BackBufferWidth, parameters.BackBufferHeight,
            width, height);
    }
    parameters.BackBufferWidth = width;
    parameters.BackBufferHeight = height;
    parameters.FullScreen_RefreshRateInHz = 0;
}

void ObservePresentationParameters(
    IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS& parameters,
    HWND fallbackWindow) {
    g_deviceObserved.store(true);
    Logger::Write(
        L"D3D9 device observed: %ux%u, windowed=%u, interval=%u.",
        parameters.BackBufferWidth, parameters.BackBufferHeight,
        parameters.Windowed, parameters.PresentationInterval);
    GamePatches::ObserveRenderHeight(parameters.BackBufferHeight);

    D3DCAPS9 caps = {};
    if (device && SUCCEEDED(device->GetDeviceCaps(&caps))) {
        g_filterCaps = caps.TextureFilterCaps;
        g_maxSupportedAnisotropy = std::max<DWORD>(1, caps.MaxAnisotropy);
    }

    if (Config::settings.borderlessWindowed && parameters.Windowed) {
        HWND window = parameters.hDeviceWindow;
        if (!window)
            window = fallbackWindow;
        if (!window)
            window = WindowFix::TrackedWindow();

        if (!WindowFix::ApplyBorderless(window))
            Logger::Write(L"Windowed D3D9 mode was detected, but verified borderless activation failed.");
        g_nextBorderlessMaintenance = GetTickCount64() + 250;
    } else {
        WindowFix::StopBorderlessMaintenance();
    }
}

HRESULT STDMETHODCALLTYPE HookedReset(
    IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters) {
    if (!g_originalReset || !parameters)
        return D3DERR_INVALIDCALL;

    D3DPRESENT_PARAMETERS adjusted = *parameters;
    HWND window = adjusted.hDeviceWindow;
    if (!window)
        window = WindowFix::TrackedWindow();
    FixPresentationInterval(adjusted);
    PrepareBorderlessPresentation(adjusted, window);
    const HRESULT result = g_originalReset(device, &adjusted);
    if (SUCCEEDED(result)) {
        *parameters = adjusted;
        g_nextFrame.QuadPart = 0;
        ObservePresentationParameters(device, adjusted, window);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookedPresent(
    IDirect3DDevice9* device, const RECT* source, const RECT* destination,
    HWND overrideWindow, const RGNDATA* dirtyRegion) {
    if (!g_originalPresent)
        return D3DERR_INVALIDCALL;

    const HRESULT result = g_originalPresent(
        device, source, destination, overrideWindow, dirtyRegion);

    if (Config::settings.borderlessWindowed &&
        WindowFix::IsBorderlessActive()) {
        const ULONGLONG now = GetTickCount64();
        if (now >= g_nextBorderlessMaintenance) {
            WindowFix::MaintainBorderless();
            g_nextBorderlessMaintenance = now + 250;
        }
    }

    PaceFrame();
    RecordMeasuredFrameRate();
    return result;
}

HRESULT STDMETHODCALLTYPE HookedSetSamplerState(
    IDirect3DDevice9* device, DWORD sampler,
    D3DSAMPLERSTATETYPE type, DWORD value) {
    if (!g_originalSetSamplerState)
        return D3DERR_INVALIDCALL;

    if (!Config::settings.anisotropicFiltering || sampler > 3)
        return g_originalSetSamplerState(device, sampler, type, value);

    const DWORD requested = std::min<DWORD>(
        static_cast<DWORD>(Config::settings.maxAnisotropy),
        g_maxSupportedAnisotropy);

    if (type == D3DSAMP_MINFILTER && value == D3DTEXF_LINEAR &&
        (g_filterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0) {
        g_originalSetSamplerState(
            device, sampler, D3DSAMP_MAXANISOTROPY, requested);
        value = D3DTEXF_ANISOTROPIC;
    } else if (type == D3DSAMP_MAGFILTER && value == D3DTEXF_LINEAR &&
        (g_filterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0) {
        g_originalSetSamplerState(
            device, sampler, D3DSAMP_MAXANISOTROPY, requested);
        value = D3DTEXF_ANISOTROPIC;
    } else if (type == D3DSAMP_MIPFILTER && value == D3DTEXF_POINT &&
               (g_filterCaps & D3DPTFILTERCAPS_MIPFLINEAR) != 0) {
        value = D3DTEXF_LINEAR;
    }

    return g_originalSetSamplerState(device, sampler, type, value);
}

bool HookDevice(IDirect3DDevice9* device) {
    if (!device)
        return false;

    void** current = *reinterpret_cast<void***>(device);
    if (!current)
        return false;

    if (g_clonedDeviceVtable) {
        *reinterpret_cast<void***>(device) = g_clonedDeviceVtable;
        return true;
    }

    const std::size_t bytes = kDeviceVtableEntries * sizeof(void*);
    void** clone = static_cast<void**>(VirtualAlloc(
        nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!clone)
        return false;

    std::memcpy(clone, current, bytes);
    g_originalDeviceVtable = current;
    g_originalReset = reinterpret_cast<ResetFn>(clone[kDeviceResetIndex]);
    g_originalPresent = reinterpret_cast<PresentFn>(clone[kDevicePresentIndex]);
    g_originalSetSamplerState = reinterpret_cast<SetSamplerStateFn>(
        clone[kDeviceSetSamplerStateIndex]);

    clone[kDeviceResetIndex] = reinterpret_cast<void*>(&HookedReset);
    clone[kDevicePresentIndex] = reinterpret_cast<void*>(&HookedPresent);
    clone[kDeviceSetSamplerStateIndex] =
        reinterpret_cast<void*>(&HookedSetSamplerState);

    DWORD oldProtect = 0;
    VirtualProtect(clone, bytes, PAGE_READONLY, &oldProtect);
    g_clonedDeviceVtable = clone;
    *reinterpret_cast<void***>(device) = clone;

    Logger::Write(L"Installed per-device D3D9 hooks without patching D3D code.");
    return true;
}

HRESULT STDMETHODCALLTYPE HookedCreateDevice(
    IDirect3D9* direct3D, UINT adapter, D3DDEVTYPE deviceType,
    HWND focusWindow, DWORD behaviorFlags,
    D3DPRESENT_PARAMETERS* parameters, IDirect3DDevice9** outputDevice) {
    if (!g_originalCreateDevice || !parameters || !outputDevice)
        return D3DERR_INVALIDCALL;

    D3DPRESENT_PARAMETERS adjusted = *parameters;
    FixPresentationInterval(adjusted);

    bool preparedBorderlessWindow = false;
    if (Config::settings.borderlessWindowed && adjusted.Windowed) {
        HWND window = adjusted.hDeviceWindow;
        if (!window)
            window = focusWindow;
        preparedBorderlessWindow = WindowFix::ApplyBorderless(window);
        if (!preparedBorderlessWindow) {
            Logger::Write(
                L"Could not prepare the verified borderless window before D3D9 device creation.");
        }
    }
    PrepareBorderlessPresentation(adjusted, focusWindow);

    const HRESULT result = g_originalCreateDevice(
        direct3D, adapter, deviceType, focusWindow, behaviorFlags,
        &adjusted, outputDevice);
    if (FAILED(result) && preparedBorderlessWindow)
        WindowFix::RestoreOriginalWindow();
    if (SUCCEEDED(result) && *outputDevice) {
        *parameters = adjusted;
        HookDevice(*outputDevice);
        ObservePresentationParameters(*outputDevice, adjusted, focusWindow);
    }
    return result;
}

} // namespace

bool Install() {
    g_effectiveFps = Config::EffectiveFpsLimit();
    QueryPerformanceFrequency(&g_frequency);
    if (g_effectiveFps > 0)
        timeBeginPeriod(1);

    HMODULE d3d9 = GetModuleHandleW(L"d3d9.dll");
    if (!d3d9)
        d3d9 = LoadLibraryW(L"d3d9.dll");
    if (!d3d9) {
        Logger::Write(L"D3D9 could not be loaded; display/timing hooks disabled.");
        return false;
    }

    auto create9 = reinterpret_cast<Direct3DCreate9Fn>(
        GetProcAddress(d3d9, "Direct3DCreate9"));
    if (!create9) {
        Logger::Write(L"Direct3DCreate9 export not found.");
        return false;
    }

    IDirect3D9* probe = create9(D3D_SDK_VERSION);
    if (!probe) {
        Logger::Write(L"D3D9 probe interface could not be created.");
        return false;
    }

    void** table = *reinterpret_cast<void***>(probe);
    void* original = nullptr;
    const bool installed = ReplaceVtableEntry(
        table, kD3D9CreateDeviceIndex,
        reinterpret_cast<void*>(&HookedCreateDevice), &original);
    if (installed) {
        g_originalCreateDevice = reinterpret_cast<CreateDeviceFn>(original);
        Logger::Write(
            L"D3D9 creation hook installed. Effective FPS limit: %d.",
            g_effectiveFps);
    } else {
        Logger::Write(L"D3D9 creation hook installation failed.");
    }

    probe->Release();
    return installed;
}

bool DeviceWasObserved() {
    return g_deviceObserved.load();
}

} // namespace D3DHooks
