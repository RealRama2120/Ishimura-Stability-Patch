#include "window_fix.hpp"

#include "logger.hpp"

namespace WindowFix {
namespace {

constexpr LONG_PTR kRemovedStyle =
    WS_CAPTION | WS_BORDER | WS_DLGFRAME | WS_THICKFRAME |
    WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
constexpr LONG_PTR kRemovedExStyle =
    WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE |
    WS_EX_STATICEDGE | WS_EX_TOOLWINDOW;

HWND g_window = nullptr;
LONG_PTR g_savedStyle = 0;
LONG_PTR g_savedExStyle = 0;
RECT g_savedRect = {};
bool g_savedStateValid = false;
bool g_active = false;

bool SetWindowLongValue(HWND window, int index, LONG_PTR value) {
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrW(window, index, value);
    return previous != 0 || GetLastError() == ERROR_SUCCESS;
}

bool RectEquals(const RECT& left, const RECT& right) {
    return left.left == right.left && left.top == right.top &&
           left.right == right.right && left.bottom == right.bottom;
}

bool GetMonitorBoundsInternal(HWND window, RECT& bounds) {
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    const HMONITOR nearest = MonitorFromWindow(
        window, MONITOR_DEFAULTTONEAREST);
    if (!nearest || !GetMonitorInfoW(nearest, &monitor))
        return false;
    bounds = monitor.rcMonitor;
    return true;
}

bool ApplyStylesAndGeometry(HWND window, const RECT& bounds) {
    const LONG_PTR style =
        (GetWindowLongPtrW(window, GWL_STYLE) & ~kRemovedStyle) | WS_POPUP;
    const LONG_PTR exStyle =
        (GetWindowLongPtrW(window, GWL_EXSTYLE) & ~kRemovedExStyle) |
        WS_EX_APPWINDOW;

    if (!SetWindowLongValue(window, GWL_STYLE, style) ||
        !SetWindowLongValue(window, GWL_EXSTYLE, exStyle))
        return false;

    if (!SetWindowPos(window, nullptr,
            bounds.left, bounds.top,
            bounds.right - bounds.left, bounds.bottom - bounds.top,
            SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER |
            SWP_FRAMECHANGED))
        return false;

    RECT actual = {};
    RECT client = {};
    if (!GetWindowRect(window, &actual) || !GetClientRect(window, &client))
        return false;

    const LONG_PTR actualStyle = GetWindowLongPtrW(window, GWL_STYLE);
    const LONG_PTR actualExStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    const LONG width = bounds.right - bounds.left;
    const LONG height = bounds.bottom - bounds.top;
    return (actualStyle & kRemovedStyle) == 0 &&
           (actualStyle & WS_POPUP) != 0 &&
           (actualExStyle & kRemovedExStyle) == 0 &&
           (actualExStyle & WS_EX_APPWINDOW) != 0 &&
           RectEquals(actual, bounds) &&
           client.left == 0 && client.top == 0 &&
           client.right == width && client.bottom == height;
}

void RestoreSavedState() {
    if (!g_savedStateValid || !g_window || !IsWindow(g_window))
        return;

    SetWindowLongValue(g_window, GWL_STYLE, g_savedStyle);
    SetWindowLongValue(g_window, GWL_EXSTYLE, g_savedExStyle);
    SetWindowPos(g_window, nullptr,
        g_savedRect.left, g_savedRect.top,
        g_savedRect.right - g_savedRect.left,
        g_savedRect.bottom - g_savedRect.top,
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER |
        SWP_FRAMECHANGED);
}

} // namespace

bool GetBorderlessBounds(HWND window, RECT& bounds) {
    return window && IsWindow(window) &&
        GetMonitorBoundsInternal(window, bounds);
}

bool ApplyBorderless(HWND window) {
    if (!window || !IsWindow(window))
        return false;

    const bool newWindow = window != g_window;
    if (newWindow) {
        g_window = window;
        g_active = false;
        g_savedStateValid = false;
    }

    if (!g_savedStateValid) {
        g_savedStyle = GetWindowLongPtrW(window, GWL_STYLE);
        g_savedExStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
        g_savedStateValid = GetWindowRect(window, &g_savedRect) != FALSE;
        if (!g_savedStateValid)
            return false;
    }

    RECT bounds = {};
    if (!GetMonitorBoundsInternal(window, bounds))
        return false;

    const bool wasActive = g_active;
    if (!ApplyStylesAndGeometry(window, bounds)) {
        if (!wasActive)
            RestoreSavedState();
        Logger::Write(L"Borderless activation failed verification; original window state was preserved.");
        return false;
    }

    g_active = true;
    Logger::Write(
        L"Verified borderless window at %ld,%ld (%ldx%ld client and monitor).",
        bounds.left, bounds.top,
        bounds.right - bounds.left, bounds.bottom - bounds.top);
    return true;
}

bool MaintainBorderless() {
    if (!g_active || !g_window || !IsWindow(g_window))
        return false;

    // Do not fight a deliberate minimize during alt-tab. Presentation resumes
    // after restoration, at which point the next maintenance tick repairs any
    // style or geometry change made by the game.
    if (IsIconic(g_window))
        return true;

    RECT bounds = {};
    RECT actual = {};
    RECT client = {};
    if (!GetMonitorBoundsInternal(g_window, bounds) ||
        !GetWindowRect(g_window, &actual) ||
        !GetClientRect(g_window, &client))
        return false;

    const LONG_PTR style = GetWindowLongPtrW(g_window, GWL_STYLE);
    const LONG_PTR exStyle = GetWindowLongPtrW(g_window, GWL_EXSTYLE);
    const LONG width = bounds.right - bounds.left;
    const LONG height = bounds.bottom - bounds.top;
    const bool incorrect =
        (style & kRemovedStyle) != 0 ||
        (style & WS_POPUP) == 0 ||
        (exStyle & kRemovedExStyle) != 0 ||
        (exStyle & WS_EX_APPWINDOW) == 0 ||
        !RectEquals(actual, bounds) ||
        client.left != 0 || client.top != 0 ||
        client.right != width || client.bottom != height;

    if (!incorrect)
        return true;

    const bool repaired = ApplyStylesAndGeometry(g_window, bounds);
    Logger::Write(repaired
        ? L"Repaired a game-initiated borderless style or size change."
        : L"Could not repair a game-initiated borderless style or size change.");
    return repaired;
}

void RestoreOriginalWindow() {
    RestoreSavedState();
    StopBorderlessMaintenance();
}

void StopBorderlessMaintenance() {
    g_window = nullptr;
    g_savedStyle = 0;
    g_savedExStyle = 0;
    g_savedRect = {};
    g_savedStateValid = false;
    g_active = false;
}

bool IsBorderlessActive() {
    return g_active && g_window && IsWindow(g_window);
}

HWND TrackedWindow() {
    return g_window;
}

} // namespace WindowFix
