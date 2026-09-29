#pragma once

#include <cstddef>
#include <cstdint>

namespace MemoryPatch {

bool Write(void* destination, const void* source, std::size_t size);
bool WriteRelativeJump(void* source, const void* destination,
                       std::size_t overwrittenBytes);
std::uint8_t* AllocateExecutable(std::size_t size);

} // namespace MemoryPatch

