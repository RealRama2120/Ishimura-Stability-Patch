#include "xinput_proxy.hpp"

#include "logger.hpp"

#include <windows.h>
#include <xinput.h>

#include <string>

namespace {

INIT_ONCE g_initialiseOnce = INIT_ONCE_STATIC_INIT;
HMODULE g_realModule = nullptr;

using EnableFn = void (WINAPI*)(BOOL);
using GetStateFn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
using SetStateFn = DWORD (WINAPI*)(DWORD, XINPUT_VIBRATION*);
using GetCapabilitiesFn = DWORD (WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*);

struct ProxyBatteryInformation {
    BYTE batteryType;
    BYTE batteryLevel;
};

struct ProxyKeystroke {
    WORD virtualKey;
    WCHAR unicode;
    WORD flags;
    BYTE userIndex;
    BYTE hidCode;
};

using GetBatteryFn = DWORD (WINAPI*)(DWORD, BYTE, ProxyBatteryInformation*);
using GetDSoundFn = DWORD (WINAPI*)(DWORD, GUID*, GUID*);
using GetKeystrokeFn = DWORD (WINAPI*)(DWORD, DWORD, ProxyKeystroke*);
using WaitGuideFn = DWORD (WINAPI*)(DWORD, DWORD, void*);
using CancelGuideFn = DWORD (WINAPI*)(DWORD);
using PowerOffFn = DWORD (WINAPI*)(DWORD);

EnableFn g_enable = nullptr;
GetStateFn g_getState = nullptr;
SetStateFn g_setState = nullptr;
GetCapabilitiesFn g_getCapabilities = nullptr;
GetBatteryFn g_getBattery = nullptr;
GetDSoundFn g_getDSound = nullptr;
GetKeystrokeFn g_getKeystroke = nullptr;
GetStateFn g_getStateEx = nullptr;
WaitGuideFn g_waitGuide = nullptr;
CancelGuideFn g_cancelGuide = nullptr;
PowerOffFn g_powerOff = nullptr;

FARPROC ResolveOrdinal(WORD ordinal) {
    return g_realModule
        ? GetProcAddress(g_realModule, MAKEINTRESOURCEA(ordinal))
        : nullptr;
}

BOOL CALLBACK Initialise(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t systemDirectory[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return TRUE;

    std::wstring path(systemDirectory, length);
    path += L"\\xinput1_3.dll";
    g_realModule = LoadLibraryW(path.c_str());
    if (!g_realModule)
        return TRUE;

    g_enable = reinterpret_cast<EnableFn>(
        GetProcAddress(g_realModule, "XInputEnable"));
    g_getState = reinterpret_cast<GetStateFn>(
        GetProcAddress(g_realModule, "XInputGetState"));
    g_setState = reinterpret_cast<SetStateFn>(
        GetProcAddress(g_realModule, "XInputSetState"));
    g_getCapabilities = reinterpret_cast<GetCapabilitiesFn>(
        GetProcAddress(g_realModule, "XInputGetCapabilities"));
    g_getBattery = reinterpret_cast<GetBatteryFn>(
        GetProcAddress(g_realModule, "XInputGetBatteryInformation"));
    g_getDSound = reinterpret_cast<GetDSoundFn>(
        GetProcAddress(g_realModule, "XInputGetDSoundAudioDeviceGuids"));
    g_getKeystroke = reinterpret_cast<GetKeystrokeFn>(
        GetProcAddress(g_realModule, "XInputGetKeystroke"));

    g_getStateEx = reinterpret_cast<GetStateFn>(ResolveOrdinal(100));
    g_waitGuide = reinterpret_cast<WaitGuideFn>(ResolveOrdinal(101));
    g_cancelGuide = reinterpret_cast<CancelGuideFn>(ResolveOrdinal(102));
    g_powerOff = reinterpret_cast<PowerOffFn>(ResolveOrdinal(103));
    return TRUE;
}

} // namespace

namespace XInputProxy {

bool EnsureLoaded() {
    InitOnceExecuteOnce(&g_initialiseOnce, Initialise, nullptr, nullptr);
    return g_realModule != nullptr;
}

} // namespace XInputProxy

extern "C" {

void WINAPI XInputEnable(BOOL enable) {
    XInputProxy::EnsureLoaded();
    if (g_enable)
        g_enable(enable);
}

DWORD WINAPI XInputGetState(DWORD index, XINPUT_STATE* state) {
    XInputProxy::EnsureLoaded();
    return g_getState ? g_getState(index, state) : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputSetState(DWORD index, XINPUT_VIBRATION* vibration) {
    XInputProxy::EnsureLoaded();
    return g_setState ? g_setState(index, vibration) : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetCapabilities(
    DWORD index, DWORD flags, XINPUT_CAPABILITIES* capabilities) {
    XInputProxy::EnsureLoaded();
    return g_getCapabilities
        ? g_getCapabilities(index, flags, capabilities)
        : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetBatteryInformation(
    DWORD index, BYTE deviceType, ProxyBatteryInformation* battery) {
    XInputProxy::EnsureLoaded();
    return g_getBattery
        ? g_getBattery(index, deviceType, battery)
        : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetDSoundAudioDeviceGuids(
    DWORD index, GUID* renderGuid, GUID* captureGuid) {
    XInputProxy::EnsureLoaded();
    return g_getDSound
        ? g_getDSound(index, renderGuid, captureGuid)
        : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetKeystroke(
    DWORD index, DWORD reserved, ProxyKeystroke* keystroke) {
    XInputProxy::EnsureLoaded();
    return g_getKeystroke
        ? g_getKeystroke(index, reserved, keystroke)
        : ERROR_EMPTY;
}

DWORD WINAPI XInputGetStateEx(DWORD index, XINPUT_STATE* state) {
    XInputProxy::EnsureLoaded();
    if (g_getStateEx)
        return g_getStateEx(index, state);
    return g_getState ? g_getState(index, state) : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputWaitForGuideButton(
    DWORD index, DWORD flags, void* overlapped) {
    XInputProxy::EnsureLoaded();
    return g_waitGuide
        ? g_waitGuide(index, flags, overlapped)
        : ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD WINAPI XInputCancelGuideButtonWait(DWORD index) {
    XInputProxy::EnsureLoaded();
    return g_cancelGuide ? g_cancelGuide(index) : ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD WINAPI XInputPowerOffController(DWORD index) {
    XInputProxy::EnsureLoaded();
    return g_powerOff ? g_powerOff(index) : ERROR_CALL_NOT_IMPLEMENTED;
}

} // extern "C"
