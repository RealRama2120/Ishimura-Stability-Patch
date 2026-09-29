#pragma once

#include <cstddef>
#include <cstdint>

namespace PatternScan {

struct Result {
    std::uint8_t* address = nullptr;
    std::size_t count = 0;
};

Result FindUniqueInExecutableSections(
    const std::uint8_t* pattern,
    const char* mask);

bool IsReadable(const void* address, std::size_t size);

} // namespace PatternScan

