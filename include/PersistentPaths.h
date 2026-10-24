#pragma once

#include <string>

namespace keybinder {
namespace PersistentPaths {

std::string RootDir();

std::string ConfigFile();

std::string JobsDataFile();

std::string LogFile();

std::string CrashLogFile();

std::string ImGuiIniFile();

std::string HitmarkersDir();

std::string CrosshairsDir();
std::string FistsDir();

std::string HitsoundsDir();

std::string EnsureLayout();

std::string ResolveAssetPath(const std::string& logicalPath);

} // namespace PersistentPaths
} // namespace keybinder
