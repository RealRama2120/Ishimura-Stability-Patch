#include "logger.hpp"

#include <cstdarg>
#include <cstdio>
#include <string>

namespace Logger {

namespace {

CRITICAL_SECTION g_lock;
bool g_lockReady = false;
bool g_enabled = false;
std::wstring g_path;

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

void Initialise(HMODULE module, bool enabled) {
    if (!g_lockReady) {
        InitializeCriticalSection(&g_lock);
        g_lockReady = true;
    }

    g_enabled = enabled;
    g_path = ModuleDirectory(module) + L"\\IshimuraStabilityPatch.log";
    if (!g_enabled)
        return;

    HANDLE file = CreateFileW(g_path.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        static const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        DWORD written = 0;
        WriteFile(file, bom, sizeof(bom), &written, nullptr);
        CloseHandle(file);
    }
}

void Write(const wchar_t* format, ...) {
    if (!g_enabled || !format)
        return;

    wchar_t message[2048] = {};
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(message, _countof(message), _TRUNCATE, format, args);
    va_end(args);

    SYSTEMTIME time = {};
    GetLocalTime(&time);

    wchar_t line[2304] = {};
    _snwprintf_s(line, _countof(line), _TRUNCATE,
        L"[%02u:%02u:%02u.%03u] [T%lu] %s\r\n",
        time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
        GetCurrentThreadId(), message);

    const int bytesRequired = WideCharToMultiByte(
        CP_UTF8, 0, line, -1, nullptr, 0, nullptr, nullptr);
    if (bytesRequired <= 1)
        return;

    std::string utf8(static_cast<size_t>(bytesRequired), '\0');
    WideCharToMultiByte(CP_UTF8, 0, line, -1,
        utf8.data(), bytesRequired, nullptr, nullptr);
    utf8.resize(static_cast<size_t>(bytesRequired - 1));

    EnterCriticalSection(&g_lock);
    HANDLE file = CreateFileW(g_path.c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()),
            &written, nullptr);
        CloseHandle(file);
    }
    LeaveCriticalSection(&g_lock);
}

const wchar_t* Path() {
    return g_path.c_str();
}

} // namespace Logger
