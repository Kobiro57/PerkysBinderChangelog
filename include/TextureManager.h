#pragma once

#include "PersistentPaths.h"

#include <d3d9.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace keybinder {

struct TextureInfo {
    IDirect3DTexture9* texture = nullptr;
    int width = 0;
    int height = 0;
    std::string filePath;
};

class TextureManager {
public:
    static constexpr int MAX_TEXTURE_SIZE = 1024;

    static std::string GetResourceTextureKey(WORD resourceId);

    bool LoadTexture(const std::string& filePath, IDirect3DDevice9* device);
    bool LoadTextureFromMemory(const std::string& textureKey, const unsigned char* imageData, int imageDataSize, IDirect3DDevice9* device);
    bool LoadTextureFromResource(WORD resourceId, IDirect3DDevice9* device);
    TextureInfo* GetTexture(const std::string& filePath);
    void ReleaseAll();
    void ReloadAll(IDirect3DDevice9* device);
    bool HasTexture(const std::string& filePath) const;
    std::vector<std::string> ScanDirectory(const std::string& dirPath);
    std::vector<std::string> ScanDirectory(const std::string& absoluteBaseDir, const std::string& logicalName);
    bool EnsureDirectory(const std::string& dirPath);

private:
    std::unordered_map<std::string, TextureInfo> textures_;
    std::vector<std::string> texturePaths_;
    std::unordered_map<std::string, std::vector<unsigned char>> memoryTextureSources_;
    std::unordered_set<std::string> failedTexturePaths_;
};

} // namespace keybinder
