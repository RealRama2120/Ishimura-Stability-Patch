#include <windows.h>

#include "../src/subtitle_policy.hpp"
#include "../src/window_fix.hpp"

#include <cmath>
#include <cstdio>

namespace Logger {
void Write(const wchar_t*, ...) {}
}

namespace {

bool NearlyEqual(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
}

bool CheckScale(int height, int baseHeight, float expected) {
    const float actual = SubtitlePolicy::ScaleForHeight(height, baseHeight);
    if (!NearlyEqual(actual, expected)) {
        std::fprintf(stderr,
            "subtitle scale mismatch: height=%d base=%d expected=%.3f actual=%.3f\n",
            height, baseHeight, expected, actual);
        return false;
    }
    return true;
}

bool TestSubtitlePolicy() {
    if (!CheckScale(480, 720, 1.0f) ||
        !CheckScale(720, 720, 1.0f) ||
        !CheckScale(1080, 720, 1.5f) ||
        !CheckScale(1440, 720, 2.0f) ||
        !CheckScale(2160, 720, 3.0f) ||
        !CheckScale(4320, 720, 6.0f) ||
        !CheckScale(5760, 720, 8.0f) ||
        !CheckScale(10000, 720, 8.0f) ||
        !CheckScale(10001, 720, 1.0f) ||
        !CheckScale(2160, 100, 1.0f))
        return false;

    // A resolution change must select a fresh absolute scale. Repeated calls
    // must never compound a previous scale into the next subtitle box.
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (!NearlyEqual(
                SubtitlePolicy::ScaleForHeight(2160, 720), 3.0f) ||
            !NearlyEqual(
                SubtitlePolicy::ScaleForHeight(720, 720), 1.0f))
            return false;
    }
    return true;
}

LRESULT CALLBACK TestWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(window, message, wParam, lParam);
}

bool HasCorrectBorderlessState(HWND window) {
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    const HMONITOR nearest = MonitorFromWindow(
        window, MONITOR_DEFAULTTONEAREST);
    RECT actual = {};
    RECT client = {};
    if (!nearest || !GetMonitorInfoW(nearest, &monitor) ||
        !GetWindowRect(window, &actual) || !GetClientRect(window, &client))
        return false;

    constexpr LONG_PTR removedStyle =
        WS_CAPTION | WS_BORDER | WS_DLGFRAME | WS_THICKFRAME |
        WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    constexpr LONG_PTR removedExStyle =
        WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE |
        WS_EX_STATICEDGE | WS_EX_TOOLWINDOW;
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    const LONG_PTR exStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    const RECT& bounds = monitor.rcMonitor;

    return (style & removedStyle) == 0 &&
           (style & WS_POPUP) != 0 &&
           (exStyle & removedExStyle) == 0 &&
           (exStyle & WS_EX_APPWINDOW) != 0 &&
           actual.left == bounds.left && actual.top == bounds.top &&
           actual.right == bounds.right && actual.bottom == bounds.bottom &&
           client.left == 0 && client.top == 0 &&
           client.right == bounds.right - bounds.left &&
           client.bottom == bounds.bottom - bounds.top;
}

bool TestBorderlessRepair() {
    const wchar_t className[] = L"IshimuraBorderlessRegressionWindow";
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = TestWindowProcedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);

    HWND window = CreateWindowExW(
        WS_EX_WINDOWEDGE, className, L"Borderless regression",
        WS_OVERLAPPEDWINDOW,
        100, 100, 800, 600,
        nullptr, nullptr, windowClass.hInstance, nullptr);
    if (!window)
        return false;

    RECT targetBounds = {};
    const bool targetResolved =
        WindowFix::GetBorderlessBounds(window, targetBounds) &&
        targetBounds.right > targetBounds.left &&
        targetBounds.bottom > targetBounds.top;

    UINT physicalWidth = 0;
    UINT physicalHeight = 0;
    const bool physicalResolved =
        WindowFix::GetBorderlessBackbufferSize(
            window, physicalWidth, physicalHeight) &&
        physicalWidth >= 320 && physicalHeight >= 200;
    std::printf("monitor coordinates=%ldx%ld, backbuffer pixels=%ux%u\n",
        targetBounds.right - targetBounds.left,
        targetBounds.bottom - targetBounds.top,
        physicalWidth, physicalHeight);

    const bool initiallyApplied = targetResolved && physicalResolved &&
        WindowFix::ApplyBorderless(window) &&
        WindowFix::IsBorderlessActive() &&
        WindowFix::TrackedWindow() == window &&
        HasCorrectBorderlessState(window);

    // Reproduce Dead Space rewriting the frame and dimensions after startup.
    SetWindowLongPtrW(window, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    SetWindowLongPtrW(window, GWL_EXSTYLE, WS_EX_WINDOWEDGE);
    SetWindowPos(window, nullptr, 120, 140, 640, 480,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);

    const bool repaired = WindowFix::MaintainBorderless() &&
        HasCorrectBorderlessState(window);

    WindowFix::RestoreOriginalWindow();
    const bool restored = !WindowFix::IsBorderlessActive() &&
        WindowFix::TrackedWindow() == nullptr;

    WindowFix::StopBorderlessMaintenance();
    const bool stopped = !WindowFix::IsBorderlessActive() &&
        WindowFix::TrackedWindow() == nullptr;
    DestroyWindow(window);
    UnregisterClassW(className, windowClass.hInstance);
    return initiallyApplied && repaired && restored && stopped;
}

} // namespace

int wmain() {
    if (!TestSubtitlePolicy()) {
        std::fprintf(stderr, "subtitle policy regression test failed\n");
        return 1;
    }
    if (!TestBorderlessRepair()) {
        std::fprintf(stderr, "borderless repair regression test failed\n");
        return 2;
    }

    std::printf("subtitle and borderless regression tests passed\n");
    return 0;
}
