#pragma once

#include <windows.h>

namespace Logger {

void Initialise(HMODULE module, bool enabled);
void Write(const wchar_t* format, ...);
const wchar_t* Path();

} // namespace Logger

