#include "../include/PersistentPaths.h"

#include <algorithm>
#include <string>

#include <windows.h>
#include <shlobj.h>

#pragma comment(lib, "Shell32.lib")

namespace keybinder {
namespace PersistentPaths {

namespace {

constexpr const char* kGameUserFilesDir = "GTA San Andreas User Files";
constexpr const char* kRootFolderName = "NLBinder";
constexpr const char* kConfigFileName = "keybinder.json";
constexpr const char* kJobsDataFileName = "jobs_data.json";
constexpr const char* kLogFileName = "keybinder.log";
constexpr const char* kCrashLogFileName = "keybinder_crash.log";
constexpr const char* kImGuiIniFileName = "imgui.ini";
constexpr const char* kHitmarkersFolderName = "hitmarkers";
constexpr const char* kCrosshairsFolderName = "crosshairs";
constexpr const char* kFistsFolderName = "fists";
constexpr const char* kWeaponIconsFolderName = "Waffenicons";
constexpr const char* kHitsoundsFolderName = "hitsounds";

bool IsSeparator(char ch) {
    return ch == '\\' || ch == '/';
}

std::string NormalizeSeparators(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    return path;
}

std::string TrimLeadingSeparators(std::string path) {
    while (!path.empty() && IsSeparator(path.front())) {
        path.erase(path.begin());
    }

    return path;
}

std::string TrimTrailingSeparators(std::string path) {
    path = NormalizeSeparators(std::move(path));

    while (path.size() > 3 && IsSeparator(path.back())) {
        path.pop_back();
    }

    return path;
}

std::string JoinPath(const std::string& left, const std::string& right) {
    if (left.empty()) {
        return TrimTrailingSeparators(NormalizeSeparators(right));
    }

    if (right.empty()) {
        return TrimTrailingSeparators(NormalizeSeparators(left));
    }

    std::string normalizedLeft = TrimTrailingSeparators(NormalizeSeparators(left));
    std::string normalizedRight = TrimLeadingSeparators(NormalizeSeparators(right));

    if (!normalizedLeft.empty() && !IsSeparator(normalizedLeft.back())) {
        normalizedLeft.push_back('\\');
    }

    normalizedLeft += normalizedRight;
    return normalizedLeft;
}

bool DirectoryExists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsDriveRoot(const std::string& path) {
    if (path.size() == 2) {
        return path[1] == ':';
    }

    return path.size() == 3 && path[1] == ':' && IsSeparator(path[2]);
}

bool EnsureDirectoryRecursive(const std::string& rawPath) {
    const std::string path = TrimTrailingSeparators(rawPath);
    if (path.empty() || IsDriveRoot(path)) {
        return true;
    }

    if (DirectoryExists(path)) {
        return true;
    }

    const std::size_t separator = path.find_last_of('\\');
    if (separator != std::string::npos) {
        const std::string parent = path.substr(0, separator);
        if (!parent.empty() && !IsDriveRoot(parent) && !EnsureDirectoryRecursive(parent)) {
            return false;
        }
    }

    if (CreateDirectoryA(path.c_str(), nullptr) != 0) {
        return true;
    }

    const DWORD error = GetLastError();
    return error == ERROR_ALREADY_EXISTS && DirectoryExists(path);
}

std::string DocumentsDir() {
#ifdef KEYBINDER_TESTING
    // The standalone harness must never create or remove assets in the user's
    // actual game profile. Keep each test process in its own temporary tree.
    char temporaryPath[MAX_PATH] = {};
    const DWORD length = GetTempPathA(MAX_PATH, temporaryPath);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return JoinPath(temporaryPath, "NLBinderTests-" + std::to_string(GetCurrentProcessId()));
#else
    char documentsPath[MAX_PATH] = {};
    const HRESULT result = SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, documentsPath);
    if (FAILED(result) || documentsPath[0] == '\0') {
        return {};
    }

    return TrimTrailingSeparators(documentsPath);
#endif
}

} // namespace

std::string RootDir() {
    const std::string documentsDir = DocumentsDir();
    if (documentsDir.empty()) {
        return {};
    }

    return JoinPath(JoinPath(documentsDir, kGameUserFilesDir), kRootFolderName);
}

std::string ConfigFile() {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    return JoinPath(rootDir, kConfigFileName);
}

std::string JobsDataFile() {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    return JoinPath(rootDir, kJobsDataFileName);
}

std::string LogFile() {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    return JoinPath(rootDir, kLogFileName);
}

std::string CrashLogFile() {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    return JoinPath(rootDir, kCrashLogFileName);
}

std::string ImGuiIniFile() {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    return JoinPath(rootDir, kImGuiIniFileName);
}

std::string HitmarkersDir() {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    return JoinPath(rootDir, kHitmarkersFolderName);
}

std::string CrosshairsDir() {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    return JoinPath(rootDir, kCrosshairsFolderName);
}

std::string FistsDir() {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    return JoinPath(rootDir, kFistsFolderName);
}

std::string HitsoundsDir() {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    return JoinPath(rootDir, kHitsoundsFolderName);
}

std::string WeaponIconsDir() {
    const std::string rootDir = RootDir();
    return rootDir.empty() ? std::string() : JoinPath(rootDir, kWeaponIconsFolderName);
}

std::string EnsureLayout() {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    if (!EnsureDirectoryRecursive(rootDir)) {
        return {};
    }

    if (!EnsureDirectoryRecursive(HitmarkersDir())) {
        return {};
    }

    if (!EnsureDirectoryRecursive(CrosshairsDir())) {
        return {};
    }

    if (!EnsureDirectoryRecursive(FistsDir())) {
        return {};
    }

    if (!EnsureDirectoryRecursive(WeaponIconsDir())) {
        return {};
    }

    if (!EnsureDirectoryRecursive(HitsoundsDir())) {
        return {};
    }

    return rootDir;
}

std::string ResolveAssetPath(const std::string& logicalPath) {
    const std::string rootDir = RootDir();
    if (rootDir.empty()) {
        return {};
    }

    const std::string normalizedLogicalPath = TrimLeadingSeparators(NormalizeSeparators(logicalPath));
    if (normalizedLogicalPath.empty()) {
        return rootDir;
    }

    return JoinPath(rootDir, normalizedLogicalPath);
}

} // namespace PersistentPaths
} // namespace keybinder
