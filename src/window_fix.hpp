#pragma once

#include <windows.h>

namespace WindowFix {

bool GetBorderlessBounds(HWND window, RECT& bounds);
bool ApplyBorderless(HWND window);
bool MaintainBorderless();
void RestoreOriginalWindow();
void StopBorderlessMaintenance();
bool IsBorderlessActive();
HWND TrackedWindow();

} // namespace WindowFix
