#pragma once

namespace SubtitlePolicy {

constexpr int kMinimumRenderHeight = 240;
constexpr int kMaximumRenderHeight = 10000;
constexpr int kMinimumBaseHeight = 240;
constexpr int kMaximumBaseHeight = 4320;
constexpr float kMinimumScale = 1.0f;
constexpr float kMaximumScale = 8.0f;

inline float ScaleForHeight(int renderHeight, int baseHeight) {
    if (renderHeight < kMinimumRenderHeight ||
        renderHeight > kMaximumRenderHeight ||
        baseHeight < kMinimumBaseHeight ||
        baseHeight > kMaximumBaseHeight)
        return 1.0f;

    const float scale = static_cast<float>(renderHeight) /
        static_cast<float>(baseHeight);
    if (scale < kMinimumScale)
        return kMinimumScale;
    if (scale > kMaximumScale)
        return kMaximumScale;
    return scale;
}

} // namespace SubtitlePolicy
