#include "game_patches.hpp"

#include "config.hpp"
#include "logger.hpp"
#include "memory_patch.hpp"
#include "pattern_scan.hpp"
#include "subtitle_policy.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace GamePatches {
namespace {

bool g_subtitleInstalled = false;
bool g_reportedWaiting = false;
ULONGLONG g_nextAttempt = 0;

alignas(4) volatile LONG g_subtitleScaleBits = 0x3F800000;
alignas(4) volatile SHORT g_effectiveRenderHeight = 0;
alignas(4) volatile LONG g_subtitleAppliedHeight = 0;
alignas(4) float g_subtitleBaseHeight = 720.0f;
alignas(4) float g_subtitleMinimumScale = SubtitlePolicy::kMinimumScale;
alignas(4) float g_subtitleMaximumScale = SubtitlePolicy::kMaximumScale;
LONG g_lastLoggedHeight = 0;
LONG g_lastLoggedScaleBits = 0x3F800000;
volatile std::uint16_t* g_gameRenderWidth = nullptr;
volatile std::uint16_t* g_gameRenderHeight = nullptr;

void Emit8(std::uint8_t* code, std::size_t& cursor, std::uint8_t value) {
    code[cursor++] = value;
}

void Emit32(std::uint8_t* code, std::size_t& cursor, std::uint32_t value) {
    std::memcpy(code + cursor, &value, sizeof(value));
    cursor += sizeof(value);
}

std::size_t EmitRelativePlaceholder(
    std::uint8_t* code, std::size_t& cursor,
    std::uint8_t first, std::uint8_t second = 0) {
    Emit8(code, cursor, first);
    if (first == 0x0F)
        Emit8(code, cursor, second);
    const std::size_t displacement = cursor;
    Emit32(code, cursor, 0);
    return displacement;
}

void PatchRelative(std::uint8_t* code, std::size_t displacement,
                   std::size_t target) {
    const std::int32_t relative = static_cast<std::int32_t>(
        target - (displacement + sizeof(std::int32_t)));
    std::memcpy(code + displacement, &relative, sizeof(relative));
}

void EmitAbsoluteFloatInstruction(
    std::uint8_t* code, std::size_t& cursor,
    const std::uint8_t* opcodes, std::size_t opcodeCount,
    const volatile void* address) {
    std::memcpy(code + cursor, opcodes, opcodeCount);
    cursor += opcodeCount;
    Emit32(code, cursor, static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(address)));
}

void EmitJumpBack(std::uint8_t* code, std::size_t& cursor,
                  const std::uint8_t* target) {
    Emit8(code, cursor, 0xE9);
    const std::intptr_t origin = reinterpret_cast<std::intptr_t>(
        code + cursor + sizeof(std::int32_t));
    const std::int32_t relative = static_cast<std::int32_t>(
        reinterpret_cast<std::intptr_t>(target) - origin);
    std::memcpy(code + cursor, &relative, sizeof(relative));
    cursor += sizeof(relative);
}

void BuildLayoutTrampoline(
    std::uint8_t* code, volatile std::uint16_t* renderHeight,
    const std::uint8_t* returnAddress) {
    std::size_t cursor = 0;

    Emit8(code, cursor, 0x9C); // pushfd
    Emit8(code, cursor, 0x50); // push eax

    // movzx eax, word ptr [renderHeight]
    static const std::uint8_t movzxAbs[] = {0x0F,0xB7,0x05};
    EmitAbsoluteFloatInstruction(code, cursor, movzxAbs, sizeof(movzxAbs),
        renderHeight);

    Emit8(code, cursor, 0x3D); // cmp eax, minimum supported render height
    Emit32(code, cursor, SubtitlePolicy::kMinimumRenderHeight);
    const std::size_t invalidLow =
        EmitRelativePlaceholder(code, cursor, 0x0F, 0x82); // jb

    Emit8(code, cursor, 0x3D); // cmp eax, maximum supported render height
    Emit32(code, cursor, SubtitlePolicy::kMaximumRenderHeight);
    const std::size_t invalidHigh =
        EmitRelativePlaceholder(code, cursor, 0x0F, 0x87); // ja

    // Record the height used by this exact pre-layout call. The worker only
    // reads this value for diagnostics; it never changes the active scale.
    Emit8(code, cursor, 0xA3); // mov dword ptr [absolute], eax
    Emit32(code, cursor, static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&g_subtitleAppliedHeight)));

    // cvtsi2ss xmm0,eax
    static const std::uint8_t intToFloat[] = {0xF3,0x0F,0x2A,0xC0};
    std::memcpy(code + cursor, intToFloat, sizeof(intToFloat));
    cursor += sizeof(intToFloat);

    static const std::uint8_t divssAbs[] = {0xF3,0x0F,0x5E,0x05};
    EmitAbsoluteFloatInstruction(code, cursor, divssAbs, sizeof(divssAbs),
        &g_subtitleBaseHeight);

    static const std::uint8_t maxssAbs[] = {0xF3,0x0F,0x5F,0x05};
    EmitAbsoluteFloatInstruction(code, cursor, maxssAbs, sizeof(maxssAbs),
        &g_subtitleMinimumScale);

    static const std::uint8_t minssAbs[] = {0xF3,0x0F,0x5D,0x05};
    EmitAbsoluteFloatInstruction(code, cursor, minssAbs, sizeof(minssAbs),
        &g_subtitleMaximumScale);

    static const std::uint8_t movssStoreAbs[] = {0xF3,0x0F,0x11,0x05};
    EmitAbsoluteFloatInstruction(code, cursor, movssStoreAbs,
        sizeof(movssStoreAbs), &g_subtitleScaleBits);

    const std::size_t restore = cursor;
    PatchRelative(code, invalidLow, restore);
    PatchRelative(code, invalidHigh, restore);

    Emit8(code, cursor, 0x58); // pop eax
    Emit8(code, cursor, 0x9D); // popfd

    static const std::uint8_t originalMovss[] = {0xF3,0x0F,0x10,0x46,0x18};
    std::memcpy(code + cursor, originalMovss, sizeof(originalMovss));
    cursor += sizeof(originalMovss);

    static const std::uint8_t mulssAbs[] = {0xF3,0x0F,0x59,0x05};
    EmitAbsoluteFloatInstruction(code, cursor, mulssAbs, sizeof(mulssAbs),
        &g_subtitleScaleBits);
    EmitJumpBack(code, cursor, returnAddress);
    FlushInstructionCache(GetCurrentProcess(), code, cursor);
}

void BuildDrawTrampoline(std::uint8_t* code, const std::uint8_t* returnAddress) {
    std::size_t cursor = 0;

    static const std::uint8_t fldFontScale[] = {0xD9,0x47,0x18};
    static const std::uint8_t fmulAbs[] = {0xD8,0x0D};
    static const std::uint8_t middle[] = {
        0xF3,0x0F,0x10,0x46,0x34,
        0x6A,0x00,
        0xD9,0x5E,0x34,
        0x0F,0xB6,0x57,0x29
    };

    std::memcpy(code + cursor, fldFontScale, sizeof(fldFontScale));
    cursor += sizeof(fldFontScale);
    EmitAbsoluteFloatInstruction(code, cursor, fmulAbs, sizeof(fmulAbs),
        &g_subtitleScaleBits);

    std::memcpy(code + cursor, middle, sizeof(middle));
    cursor += sizeof(middle);

    std::memcpy(code + cursor, fldFontScale, sizeof(fldFontScale));
    cursor += sizeof(fldFontScale);
    EmitAbsoluteFloatInstruction(code, cursor, fmulAbs, sizeof(fmulAbs),
        &g_subtitleScaleBits);

    EmitJumpBack(code, cursor, returnAddress);
    FlushInstructionCache(GetCurrentProcess(), code, cursor);
}

bool ResolveGameRenderDimensions() {
    if (g_gameRenderWidth && g_gameRenderHeight)
        return true;

    static const std::uint8_t resolutionPattern[] = {
        0x0F,0xB7,0x05,0,0,0,0,
        0xF3,0x0F,0x10,0x05,0,0,0,0,
        0xF3,0x0F,0x2A,0xC8,0x8B,0x44,0x24,0x04,
        0x0F,0x28,0xD0,0xF3,0x0F,0x5E,0xD1,0xF3,0x0F,0x11,0x11,
        0x0F,0xB7,0x15,0,0,0,0,
        0xF3,0x0F,0x2A,0xCA,0xF3,0x0F,0x5E,0xC1,
        0xF3,0x0F,0x11,0x00,0xC3
    };
    static const char resolutionMask[] =
        "xxx????xxxx????xxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxxx";

    const PatternScan::Result resolution =
        PatternScan::FindUniqueInExecutableSections(
            resolutionPattern, resolutionMask);
    if (!resolution.address)
        return false;

    std::uint32_t renderWidthAddress = 0;
    std::uint32_t renderHeightAddress = 0;
    std::memcpy(&renderWidthAddress, resolution.address + 3,
        sizeof(renderWidthAddress));
    std::memcpy(&renderHeightAddress, resolution.address + 37,
        sizeof(renderHeightAddress));
    auto* renderWidth = reinterpret_cast<volatile std::uint16_t*>(
        static_cast<std::uintptr_t>(renderWidthAddress));
    auto* renderHeight = reinterpret_cast<volatile std::uint16_t*>(
        static_cast<std::uintptr_t>(renderHeightAddress));
    if (!PatternScan::IsReadable(
            const_cast<std::uint16_t*>(renderWidth), sizeof(std::uint16_t)) ||
        !PatternScan::IsReadable(
            const_cast<std::uint16_t*>(renderHeight), sizeof(std::uint16_t)))
        return false;

    g_gameRenderWidth = renderWidth;
    g_gameRenderHeight = renderHeight;
    return true;
}

bool InstallSubtitleFix() {
    static const std::uint8_t resolutionPattern[] = {
        0x0F,0xB7,0x05,0,0,0,0,
        0xF3,0x0F,0x10,0x05,0,0,0,0,
        0xF3,0x0F,0x2A,0xC8,0x8B,0x44,0x24,0x04,
        0x0F,0x28,0xD0,0xF3,0x0F,0x5E,0xD1,0xF3,0x0F,0x11,0x11,
        0x0F,0xB7,0x15,0,0,0,0,
        0xF3,0x0F,0x2A,0xCA,0xF3,0x0F,0x5E,0xC1,
        0xF3,0x0F,0x11,0x00,0xC3
    };
    static const char resolutionMask[] =
        "xxx????xxxx????xxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxxx";

    static const std::uint8_t layoutPattern[] = {
        0x8B,0x74,0x24,0x30,
        0xF3,0x0F,0x10,0x46,0x18,
        0x57,0x8B,0xF8,0x8B,0x46,0x10
    };
    static const char layoutMask[] = "xxxxxxxxxxxxxxx";

    static const std::uint8_t drawPattern[] = {
        0xD9,0x47,0x18,
        0xF3,0x0F,0x10,0x46,0x34,
        0x6A,0x00,
        0xD9,0x5E,0x34,
        0x0F,0xB6,0x57,0x29,
        0xD9,0x47,0x18,
        0x8B,0x47,0x38
    };
    static const char drawMask[] = "xxxxxxxxxxxxxxxxxxxxxxx";

    const PatternScan::Result resolution =
        PatternScan::FindUniqueInExecutableSections(
            resolutionPattern, resolutionMask);
    const PatternScan::Result layout =
        PatternScan::FindUniqueInExecutableSections(layoutPattern, layoutMask);
    const PatternScan::Result draw =
        PatternScan::FindUniqueInExecutableSections(drawPattern, drawMask);

    if (!resolution.address || !layout.address || !draw.address) {
        if (!g_reportedWaiting) {
            Logger::Write(
                L"Waiting for unpacked subtitle code (resolution=%u, layout=%u, draw=%u).",
                static_cast<unsigned>(resolution.count),
                static_cast<unsigned>(layout.count),
                static_cast<unsigned>(draw.count));
            g_reportedWaiting = true;
        }
        return false;
    }

    std::uint32_t renderWidthAddress = 0;
    std::memcpy(&renderWidthAddress, resolution.address + 3,
        sizeof(renderWidthAddress));
    auto* renderWidth = reinterpret_cast<volatile std::uint16_t*>(
        static_cast<std::uintptr_t>(renderWidthAddress));

    std::uint32_t renderHeightAddress = 0;
    std::memcpy(&renderHeightAddress, resolution.address + 37,
        sizeof(renderHeightAddress));
    auto* renderHeight = reinterpret_cast<volatile std::uint16_t*>(
        static_cast<std::uintptr_t>(renderHeightAddress));
    if (!PatternScan::IsReadable(
            const_cast<std::uint16_t*>(renderWidth), sizeof(std::uint16_t)) ||
        !PatternScan::IsReadable(
            const_cast<std::uint16_t*>(renderHeight), sizeof(std::uint16_t)))
        return false;

    const std::uint16_t gameHeight = *renderHeight;
    if (gameHeight < SubtitlePolicy::kMinimumRenderHeight ||
        gameHeight > SubtitlePolicy::kMaximumRenderHeight)
        return false;

    SHORT effectiveHeight = InterlockedCompareExchange16(
        &g_effectiveRenderHeight, 0, 0);
    if (effectiveHeight < SubtitlePolicy::kMinimumRenderHeight ||
        effectiveHeight > SubtitlePolicy::kMaximumRenderHeight) {
        effectiveHeight = static_cast<SHORT>(gameHeight);
        InterlockedExchange16(&g_effectiveRenderHeight, effectiveHeight);
    }
    const std::uint16_t height = static_cast<std::uint16_t>(effectiveHeight);
    g_gameRenderWidth = renderWidth;
    g_gameRenderHeight = renderHeight;

    std::uint8_t* layoutSite = layout.address + 4;
    std::uint8_t* drawSite = draw.address;
    std::uint8_t layoutOriginal[5] = {};
    std::uint8_t drawOriginal[20] = {};
    std::memcpy(layoutOriginal, layoutSite, sizeof(layoutOriginal));
    std::memcpy(drawOriginal, drawSite, sizeof(drawOriginal));

    std::uint8_t* layoutTrampoline = MemoryPatch::AllocateExecutable(192);
    std::uint8_t* drawTrampoline = MemoryPatch::AllocateExecutable(96);
    if (!layoutTrampoline || !drawTrampoline)
        return false;

    g_subtitleBaseHeight = static_cast<float>(
        Config::settings.subtitleBaseHeight);
    const float initialScale = SubtitlePolicy::ScaleForHeight(
        height, Config::settings.subtitleBaseHeight);
    LONG initialScaleBits = 0;
    static_assert(sizeof(initialScaleBits) == sizeof(initialScale),
        "subtitle scale storage must remain 32-bit");
    std::memcpy(&initialScaleBits, &initialScale, sizeof(initialScaleBits));
    InterlockedExchange(&g_subtitleScaleBits, initialScaleBits);
    InterlockedExchange(&g_subtitleAppliedHeight, height);
    g_lastLoggedHeight = height;
    g_lastLoggedScaleBits = initialScaleBits;
    BuildLayoutTrampoline(
        layoutTrampoline,
        reinterpret_cast<volatile std::uint16_t*>(&g_effectiveRenderHeight),
        layoutSite + sizeof(layoutOriginal));
    BuildDrawTrampoline(
        drawTrampoline, drawSite + sizeof(drawOriginal));

    if (!MemoryPatch::WriteRelativeJump(
            layoutSite, layoutTrampoline, sizeof(layoutOriginal)))
        return false;

    if (!MemoryPatch::WriteRelativeJump(
            drawSite, drawTrampoline, sizeof(drawOriginal))) {
        MemoryPatch::Write(layoutSite, layoutOriginal, sizeof(layoutOriginal));
        return false;
    }

    auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    Logger::Write(
        L"Full subtitle layout/draw scaling installed at RVAs 0x%08X and 0x%08X (height=%u, scale=%.3f).",
        static_cast<unsigned>(layoutSite - base),
        static_cast<unsigned>(drawSite - base), height, initialScale);
    return true;
}

} // namespace

void Initialise() {
    g_subtitleBaseHeight = static_cast<float>(
        Config::settings.subtitleBaseHeight);
    g_nextAttempt = 0;
}

void Update() {
    const ULONGLONG now = GetTickCount64();
    if (now < g_nextAttempt)
        return;
    g_nextAttempt = now + 250;

    if (Config::settings.fixSubtitleScaling && !g_subtitleInstalled) {
        g_subtitleInstalled = InstallSubtitleFix();
        return;
    }

    if (g_subtitleInstalled) {
        const LONG appliedHeight = InterlockedCompareExchange(
            &g_subtitleAppliedHeight, 0, 0);
        const LONG scaleBits = InterlockedCompareExchange(
            &g_subtitleScaleBits, 0, 0);
        if (appliedHeight >= SubtitlePolicy::kMinimumRenderHeight &&
            appliedHeight <= SubtitlePolicy::kMaximumRenderHeight &&
            (appliedHeight != g_lastLoggedHeight ||
             scaleBits != g_lastLoggedScaleBits)) {
            float scale = 1.0f;
            std::memcpy(&scale, &scaleBits, sizeof(scale));
            Logger::Write(
                L"Subtitle pre-layout synchronized to render height %ld (scale=%.3f).",
                appliedHeight, scale);
            g_lastLoggedHeight = appliedHeight;
            g_lastLoggedScaleBits = scaleBits;
        }
    }
}

void ObserveRenderHeight(unsigned int height) {
    if (height < static_cast<unsigned>(SubtitlePolicy::kMinimumRenderHeight) ||
        height > static_cast<unsigned>(SubtitlePolicy::kMaximumRenderHeight))
        return;
    InterlockedExchange16(
        &g_effectiveRenderHeight, static_cast<SHORT>(height));
}

bool PrepareBorderlessRenderSize(unsigned int width, unsigned int height) {
    if (width < 320 || width > 10000 ||
        height < static_cast<unsigned>(SubtitlePolicy::kMinimumRenderHeight) ||
        height > static_cast<unsigned>(SubtitlePolicy::kMaximumRenderHeight))
        return false;

    // D3D creation and the worker can race by a few milliseconds while the
    // packed EA executable finishes exposing its text code. Resolve only the
    // two validated render globals here; never wait indefinitely.
    for (unsigned int attempt = 0;
         attempt < 10 && !ResolveGameRenderDimensions(); ++attempt) {
        Sleep(5);
    }
    if (!g_gameRenderWidth || !g_gameRenderHeight)
        return false;

    auto* renderWidth = const_cast<std::uint16_t*>(g_gameRenderWidth);
    auto* renderHeight = const_cast<std::uint16_t*>(g_gameRenderHeight);
    if (!PatternScan::IsReadable(renderWidth, sizeof(*renderWidth)) ||
        !PatternScan::IsReadable(renderHeight, sizeof(*renderHeight)))
        return false;

    const std::uint16_t oldWidth = *renderWidth;
    const std::uint16_t oldHeight = *renderHeight;
    const std::uint16_t newWidth = static_cast<std::uint16_t>(width);
    const std::uint16_t newHeight = static_cast<std::uint16_t>(height);

    if (!MemoryPatch::Write(renderWidth, &newWidth, sizeof(newWidth)) ||
        !MemoryPatch::Write(renderHeight, &newHeight, sizeof(newHeight))) {
        MemoryPatch::Write(renderWidth, &oldWidth, sizeof(oldWidth));
        MemoryPatch::Write(renderHeight, &oldHeight, sizeof(oldHeight));
        return false;
    }

    if (*renderWidth != newWidth || *renderHeight != newHeight) {
        MemoryPatch::Write(renderWidth, &oldWidth, sizeof(oldWidth));
        MemoryPatch::Write(renderHeight, &oldHeight, sizeof(oldHeight));
        return false;
    }

    InterlockedExchange16(
        &g_effectiveRenderHeight, static_cast<SHORT>(newHeight));
    Logger::Write(
        L"Synchronized engine render size from %ux%u to %ux%u for native borderless presentation.",
        oldWidth, oldHeight, newWidth, newHeight);
    return true;
}

bool SubtitleFixInstalled() {
    return g_subtitleInstalled;
}

} // namespace GamePatches
