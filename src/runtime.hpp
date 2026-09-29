#pragma once

#include <windows.h>

namespace Runtime {

extern HMODULE module;
extern bool isDeadSpace;
extern DWORD_PTR earlyAffinityMask;
extern bool earlyAffinityApplied;

bool IsDeadSpaceProcess();
void ApplyEarlyCoreLimit(unsigned int maximumLogicalProcessors);
DWORD WINAPI WorkerThread(void*);

} // namespace Runtime

