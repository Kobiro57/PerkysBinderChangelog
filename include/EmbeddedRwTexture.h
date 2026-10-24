#pragma once

#include <string>

namespace keybinder {

void* CreateRwTextureFromEmbeddedPng(int resourceId, const char* logLabel, float contentScale = 1.0f);
void* CreateRwTextureFromPngFile(const std::string& path, const char* logLabel, int maxTextureSize = 1024);

} // namespace keybinder
