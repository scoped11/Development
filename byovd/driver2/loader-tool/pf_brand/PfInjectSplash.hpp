#pragma once
#include <functional>

// Top-center splash: palm fixed, image stretches horizontally with progress (finger loading bar).
void RunPfInjectSplash(float durationSeconds, const std::function<void()>& work);
