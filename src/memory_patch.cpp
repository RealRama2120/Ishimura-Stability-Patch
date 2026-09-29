#include "memory_patch.hpp"

#include <windows.h>

#include <climits>
#include <cstring>
#include <new>

namespace MemoryPatch {

bool Write(void* destination, const void* source, std::size_t size) {
    if (!destination || !source || size == 0)
        return false;

    DWORD oldProtect = 0;
    if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    std::memcpy(destination, source, size);
    FlushInstructionCache(GetCurrentProcess(), destination, size);

    DWORD ignored = 0;
    const BOOL restored = VirtualProtect(destination, size, oldProtect, &ignored);
    return restored != FALSE;
}

bool WriteRelativeJump(void* source, const void* destination,
                       std::size_t overwrittenBytes) {
    if (!source || !destination || overwrittenBytes < 5)
        return false;

    auto* patch = new (std::nothrow) std::uint8_t[overwrittenBytes];
    if (!patch)
        return false;

    std::memset(patch, 0x90, overwrittenBytes);
    patch[0] = 0xE9;

    const std::intptr_t from = reinterpret_cast<std::intptr_t>(source) + 5;
    const std::intptr_t to = reinterpret_cast<std::intptr_t>(destination);
    const std::int64_t displacement64 = static_cast<std::int64_t>(to - from);
    if (displacement64 < INT32_MIN || displacement64 > INT32_MAX) {
        delete[] patch;
        return false;
    }

    const std::int32_t displacement = static_cast<std::int32_t>(displacement64);
    std::memcpy(patch + 1, &displacement, sizeof(displacement));
    const bool result = Write(source, patch, overwrittenBytes);
    delete[] patch;
    return result;
}

std::uint8_t* AllocateExecutable(std::size_t size) {
    return static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
}

} // namespace MemoryPatch
