#include "../include/TextureManager.h"
#include "../include/Logger.h"
#include "../include/PersistentPaths.h"
#include "../include/ResourceIds.h"
#include "../include/plugin-sdk/stb/stb_image.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <windows.h>

namespace keybinder {

namespace {

extern "C" IMAGE_DOS_HEADER __ImageBase;

struct EmbeddedResourceView {
    const void* data = nullptr;
    std::size_t size = 0;
};

std::string NormalizeSeparators(const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    return normalized;
}

std::string TrimLeadingSeparators(const std::string& path) {
    std::size_t firstNonSeparator = 0;
    while (firstNonSeparator < path.size() && (path[firstNonSeparator] == '\\' || path[firstNonSeparator] == '/')) {
        ++firstNonSeparator;
    }

    return path.substr(firstNonSeparator);
}

std::string TrimTrailingSeparators(const std::string& path) {
    std::size_t end = path.size();
    while (end > 0 && (path[end - 1] == '\\' || path[end - 1] == '/')) {
        --end;
    }

    return path.substr(0, end);
}

std::string NormalizeLogicalPath(const std::string& logicalPath) {
    return TrimTrailingSeparators(TrimLeadingSeparators(NormalizeSeparators(logicalPath)));
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool IsAbsolutePath(const std::string& path) {
    const std::string normalizedPath = NormalizeSeparators(path);
    return (normalizedPath.size() >= 2 && std::isalpha(static_cast<unsigned char>(normalizedPath[0])) && normalizedPath[1] == ':')
        || (normalizedPath.size() >= 2 && normalizedPath[0] == '\\' && normalizedPath[1] == '\\');
}

bool PathEndsWithSegment(const std::string& path, const std::string& segment) {
    const std::string normalizedPath = ToLowerAscii(TrimTrailingSeparators(NormalizeSeparators(path)));
    const std::string normalizedSegment = ToLowerAscii(NormalizeLogicalPath(segment));
    if (normalizedPath.empty() || normalizedSegment.empty() || normalizedPath.size() < normalizedSegment.size()) {
        return false;
    }

    if (normalizedPath.compare(normalizedPath.size() - normalizedSegment.size(), normalizedSegment.size(), normalizedSegment) != 0) {
        return false;
    }

    return normalizedPath.size() == normalizedSegment.size()
        || normalizedPath[normalizedPath.size() - normalizedSegment.size() - 1] == '\\';
}

std::string BuildAbsoluteScanDirectory(const std::string& absoluteBaseDir, const std::string& logicalName) {
    const std::string normalizedBaseDir = TrimTrailingSeparators(NormalizeSeparators(absoluteBaseDir));
    const std::string normalizedLogicalName = NormalizeLogicalPath(logicalName);
    if (normalizedBaseDir.empty()) {
        return {};
    }

    if (normalizedLogicalName.empty() || PathEndsWithSegment(normalizedBaseDir, normalizedLogicalName)) {
        return normalizedBaseDir;
    }

    return normalizedBaseDir + "\\" + normalizedLogicalName;
}

std::string ResolveTextureDiskPath(const std::string& texturePath) {
    if (texturePath.empty()) {
        return {};
    }

    if (IsAbsolutePath(texturePath)) {
        return NormalizeSeparators(texturePath);
    }

    return PersistentPaths::ResolveAssetPath(texturePath);
}

bool TryLoadEmbeddedResource(WORD resourceId, EmbeddedResourceView* outResource) {
    if (outResource == nullptr) {
        return false;
    }

    *outResource = {};

    const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    if (module == nullptr) {
        return false;
    }

    const LPCWSTR resourceType = reinterpret_cast<LPCWSTR>(RT_RCDATA);
    const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), resourceType);
    if (resource == nullptr) {
        return false;
    }

    const DWORD resourceSize = SizeofResource(module, resource);
    if (resourceSize == 0) {
        return false;
    }

    const HGLOBAL loadedResource = LoadResource(module, resource);
    if (loadedResource == nullptr) {
        return false;
    }

    const void* resourceData = LockResource(loadedResource);
    if (resourceData == nullptr) {
        return false;
    }

    outResource->data = resourceData;
    outResource->size = static_cast<std::size_t>(resourceSize);
    return true;
}

bool UploadDecodedTexture(const std::string& textureKey,
    unsigned char* data,
    int width,
    int height,
    IDirect3DDevice9* device,
    std::unordered_map<std::string, TextureInfo>& textures,
    std::vector<std::string>& texturePaths,
    std::unordered_set<std::string>& failedTexturePaths,
    bool trackDiskPath) {
    if (!device) {
        Logger::Warning("[TextureManager] UploadDecodedTexture: null device");
        failedTexturePaths.insert(textureKey);
        return false;
    }

    const int maxTextureSize =
        (textureKey == TextureManager::GetResourceTextureKey(IDR_GTASA_MAP)
            || textureKey == TextureManager::GetResourceTextureKey(IDR_GTASA_SATELLITE_MAP))
            ? 2048
            : TextureManager::MAX_TEXTURE_SIZE;

    if (width > maxTextureSize || height > maxTextureSize) {
        Logger::Warning("[TextureManager] Image too large (%dx%d, max %d): %s",
            width, height, maxTextureSize, textureKey.c_str());
        failedTexturePaths.insert(textureKey);
        return false;
    }

    if (width <= 0 || height <= 0) {
        Logger::Warning("[TextureManager] Invalid image dimensions (%dx%d): %s",
            width, height, textureKey.c_str());
        failedTexturePaths.insert(textureKey);
        return false;
    }

    IDirect3DTexture9* texture = nullptr;
    HRESULT hr = device->CreateTexture(
        static_cast<UINT>(width), static_cast<UINT>(height),
        1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr);
    if (FAILED(hr)) {
        Logger::Warning("[TextureManager] CreateTexture failed (0x%08X): %s",
            static_cast<unsigned>(hr), textureKey.c_str());
        failedTexturePaths.insert(textureKey);
        return false;
    }

    D3DLOCKED_RECT lockedRect;
    hr = texture->LockRect(0, &lockedRect, nullptr, 0);
    if (FAILED(hr)) {
        Logger::Warning("[TextureManager] LockRect failed (0x%08X): %s",
            static_cast<unsigned>(hr), textureKey.c_str());
        texture->Release();
        failedTexturePaths.insert(textureKey);
        return false;
    }

    unsigned char* dest = static_cast<unsigned char*>(lockedRect.pBits);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int srcIdx = (y * width + x) * 4;
            int dstIdx = y * lockedRect.Pitch + x * 4;
            dest[dstIdx + 0] = data[srcIdx + 2];
            dest[dstIdx + 1] = data[srcIdx + 1];
            dest[dstIdx + 2] = data[srcIdx + 0];
            dest[dstIdx + 3] = data[srcIdx + 3];
        }
    }
    texture->UnlockRect(0);

    auto it = textures.find(textureKey);
    if (it != textures.end() && it->second.texture) {
        it->second.texture->Release();
    }

    if (trackDiskPath && std::find(texturePaths.begin(), texturePaths.end(), textureKey) == texturePaths.end()) {
        texturePaths.push_back(textureKey);
    }

    TextureInfo info;
    info.texture = texture;
    info.width = width;
    info.height = height;
    info.filePath = textureKey;
    textures[textureKey] = info;
    failedTexturePaths.erase(textureKey);

    Logger::Info("[TextureManager] Loaded texture %dx%d: %s", width, height, textureKey.c_str());
    return true;
}

} // namespace

std::string TextureManager::GetResourceTextureKey(WORD resourceId) {
    return "__embedded/resource-" + std::to_string(static_cast<unsigned>(resourceId));
}

bool TextureManager::LoadTexture(const std::string& filePath, IDirect3DDevice9* device) {
    if (failedTexturePaths_.find(filePath) != failedTexturePaths_.end()) {
        return false;
    }

    if (!device) {
        Logger::Warning("[TextureManager] LoadTexture: null device");
        failedTexturePaths_.insert(filePath);
        return false;
    }

    const std::string resolvedFilePath = ResolveTextureDiskPath(filePath);
    if (resolvedFilePath.empty()) {
        Logger::Warning("[TextureManager] Failed to resolve texture path: %s", filePath.c_str());
        failedTexturePaths_.insert(filePath);
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(resolvedFilePath.c_str(), &width, &height, &channels, 4);
    if (!data) {
        Logger::Warning("[TextureManager] Failed to load image: %s (resolved: %s)", filePath.c_str(), resolvedFilePath.c_str());
        failedTexturePaths_.insert(filePath);
        return false;
    }

    const bool result = UploadDecodedTexture(filePath, data, width, height, device, textures_, texturePaths_, failedTexturePaths_, true);
    stbi_image_free(data);
    return result;
}

bool TextureManager::LoadTextureFromMemory(const std::string& textureKey, const unsigned char* imageData, int imageDataSize, IDirect3DDevice9* device) {
    if (failedTexturePaths_.find(textureKey) != failedTexturePaths_.end()) {
        return false;
    }

    if (imageData == nullptr || imageDataSize <= 0) {
        Logger::Warning("[TextureManager] LoadTextureFromMemory: invalid image data for %s", textureKey.c_str());
        failedTexturePaths_.insert(textureKey);
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load_from_memory(imageData, imageDataSize, &width, &height, &channels, 4);
    if (!data) {
        Logger::Warning("[TextureManager] Failed to decode embedded image: %s", textureKey.c_str());
        failedTexturePaths_.insert(textureKey);
        return false;
    }

    std::vector<unsigned char> ownedBytes(imageData, imageData + imageDataSize);
    memoryTextureSources_[textureKey] = std::move(ownedBytes);

    auto texturePathIt = std::find(texturePaths_.begin(), texturePaths_.end(), textureKey);
    if (texturePathIt != texturePaths_.end()) {
        texturePaths_.erase(texturePathIt);
    }

    // Embedded textures are reloaded from memoryTextureSources_ on device reset.
    // Treating their keys as file paths causes ReloadAll() to try a bogus disk load
    // first (for example "__embedded/sidebar-logo"), which then blocks the real
    // in-memory reload path for the same key.
    const bool result = UploadDecodedTexture(textureKey, data, width, height, device, textures_, texturePaths_, failedTexturePaths_, false);
    stbi_image_free(data);
    return result;
}

bool TextureManager::LoadTextureFromResource(WORD resourceId, IDirect3DDevice9* device) {
    const std::string textureKey = GetResourceTextureKey(resourceId);
    if (HasTexture(textureKey)) {
        return true;
    }

    EmbeddedResourceView resource;
    if (!TryLoadEmbeddedResource(resourceId, &resource)) {
        Logger::Warning("[TextureManager] Resource texture not found: %u", static_cast<unsigned>(resourceId));
        failedTexturePaths_.insert(textureKey);
        return false;
    }

    if (resource.size > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        Logger::Warning("[TextureManager] Resource texture too large: %u", static_cast<unsigned>(resourceId));
        failedTexturePaths_.insert(textureKey);
        return false;
    }

    return LoadTextureFromMemory(
        textureKey,
        static_cast<const unsigned char*>(resource.data),
        static_cast<int>(resource.size),
        device);
}

TextureInfo* TextureManager::GetTexture(const std::string& filePath) {
    auto it = textures_.find(filePath);
    if (it != textures_.end()) {
        return &it->second;
    }
    return nullptr;
}

void TextureManager::ReleaseAll() {
    // NOTE: Our textures use D3DPOOL_MANAGED which survives D3D9 Reset()
    // automatically. However, we release and reload as a safety net for
    // edge cases (device recreation, driver bugs) and to align with the
    // overlay's invalidate/recreate lifecycle.
    for (auto& pair : textures_) {
        if (pair.second.texture) {
            pair.second.texture->Release();
            pair.second.texture = nullptr;
        }
    }
    textures_.clear();
}

void TextureManager::ReloadAll(IDirect3DDevice9* device) {
    failedTexturePaths_.clear();

    if (!device) {
        Logger::Warning("[TextureManager] ReloadAll: null device");
        return;
    }

    for (const auto& path : texturePaths_) {
        if (!LoadTexture(path, device)) {
            Logger::Warning("[TextureManager] ReloadAll: failed to reload %s", path.c_str());
        }
    }

    for (const auto& memoryTexture : memoryTextureSources_) {
        if (!LoadTextureFromMemory(
                memoryTexture.first,
                memoryTexture.second.data(),
                static_cast<int>(memoryTexture.second.size()),
                device)) {
            Logger::Warning("[TextureManager] ReloadAll: failed to reload embedded texture %s", memoryTexture.first.c_str());
        }
    }

    Logger::Info("[TextureManager] Reloaded %zu texture(s)", texturePaths_.size() + memoryTextureSources_.size());
}

bool TextureManager::HasTexture(const std::string& filePath) const {
    return textures_.find(filePath) != textures_.end();
}

std::vector<std::string> TextureManager::ScanDirectory(const std::string& dirPath) {
    const std::string logicalName = NormalizeLogicalPath(dirPath);
    if (logicalName.empty()) {
        return {};
    }

    return ScanDirectory(PersistentPaths::ResolveAssetPath(logicalName), logicalName);
}

std::vector<std::string> TextureManager::ScanDirectory(const std::string& absoluteBaseDir, const std::string& logicalName) {
    std::vector<std::string> results;

    const std::string normalizedLogicalName = NormalizeLogicalPath(logicalName);
    const std::string fullPath = BuildAbsoluteScanDirectory(absoluteBaseDir, normalizedLogicalName);
    if (normalizedLogicalName.empty() || fullPath.empty()) {
        return results;
    }

    if (!EnsureDirectory(fullPath)) {
        Logger::Warning("[TextureManager] ScanDirectory: failed to ensure directory %s", fullPath.c_str());
        return results;
    }

    std::string searchPattern = fullPath + "\\*.png";

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return results;
    }

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        results.push_back(normalizedLogicalName + "\\" + findData.cFileName);
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

    Logger::Debug("[TextureManager] ScanDirectory: found %zu PNG file(s) in %s (logical root %s)",
                  results.size(), fullPath.c_str(), normalizedLogicalName.c_str());
    return results;
}

bool TextureManager::EnsureDirectory(const std::string& dirPath) {
    if (CreateDirectoryA(dirPath.c_str(), nullptr)) {
        return true;
    }

    const DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS) {
        return true;
    }

    Logger::Warning("[TextureManager] EnsureDirectory: failed to create %s (error=%lu)", dirPath.c_str(), error);
    return false;
}

} // namespace keybinder
