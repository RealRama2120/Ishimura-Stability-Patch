#pragma once

#include <windows.h>

namespace WindowFix {

bool GetBorderlessBounds(HWND window, RECT& bounds);
bool GetBorderlessBackbufferSize(HWND window, UINT& width, UINT& height);
bool ApplyBorderless(HWND window);
bool MaintainBorderless();
void RestoreOriginalWindow();
void StopBorderlessMaintenance();
bool IsBorderlessActive();
HWND TrackedWindow();

} // namespace WindowFix
