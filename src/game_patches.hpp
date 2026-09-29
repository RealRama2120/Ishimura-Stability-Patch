#pragma once

namespace GamePatches {

void Initialise();
void Update();
bool PrepareBorderlessRenderSize(unsigned int width, unsigned int height);
void ObserveRenderHeight(unsigned int height);
bool SubtitleFixInstalled();

} // namespace GamePatches
