#include "pattern_scan.hpp"

#include <windows.h>

#include <cstring>

namespace PatternScan {

Result FindUniqueInExecutableSections(
    const std::uint8_t* pattern,
    const char* mask) {
    Result result;
    if (!pattern || !mask)
        return result;

    const std::size_t patternLength = std::strlen(mask);
    if (patternLength == 0)
        return result;

    auto* imageBase = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (!imageBase)
        return result;

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return result;

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        imageBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
        return result;

    const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (WORD index = 0; index < nt->FileHeader.NumberOfSections;
         ++index, ++section) {
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
            continue;

        auto* start = imageBase + section->VirtualAddress;
        const std::size_t size = section->Misc.VirtualSize;
        if (size < patternLength)
            continue;

        for (std::size_t offset = 0;
             offset <= size - patternLength; ++offset) {
            bool matches = true;
            for (std::size_t byte = 0; byte < patternLength; ++byte) {
                if (mask[byte] == 'x' &&
                    start[offset + byte] != pattern[byte]) {
                    matches = false;
                    break;
                }
            }
            if (!matches)
                continue;

            ++result.count;
            if (result.count == 1)
                result.address = start + offset;
            else
                result.address = nullptr;
        }
    }

    return result;
}

bool IsReadable(const void* address, std::size_t size) {
    if (!address || size == 0)
        return false;

    MEMORY_BASIC_INFORMATION info = {};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info))
        return false;
    if (info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
        info.Protect == PAGE_NOACCESS)
        return false;

    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = start + size;
    const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) +
        info.RegionSize;
    return end >= start && end <= regionEnd;
}

} // namespace PatternScan

