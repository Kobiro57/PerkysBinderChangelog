#include "../include/EmbeddedRwTexture.h"

#include "../include/Logger.h"

#ifndef KEYBINDER_TESTING
#include "../include/plugin-sdk/plugin_sa/game_sa/rw/rwplcore.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/rw/rwcore.h"
#include "../include/plugin-sdk/stb/stb_image.h"
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>

namespace keybinder {

#ifndef KEYBINDER_TESTING
namespace {

extern "C" IMAGE_DOS_HEADER __ImageBase;

struct EmbeddedResourceView {
    const void* data = nullptr;
    std::size_t size = 0;
};

bool TryLoadEmbeddedResource(int resourceId, EmbeddedResourceView* outResource) {
    if (outResource == nullptr) {
        return false;
    }

    *outResource = {};

    const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    if (module == nullptr) {
        return false;
    }

    const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), reinterpret_cast<LPCWSTR>(RT_RCDATA));
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

float ClampContentScale(float contentScale) {
    if (contentScale < 0.25f) {
        return 0.25f;
    }
    if (contentScale > 1.0f) {
        return 1.0f;
    }
    return contentScale;
}

struct RgbaSample {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double alpha = 0.0;
};

unsigned char ToByte(double value) {
    const double clamped = (std::max)(0.0, (std::min)(255.0, value));
    return static_cast<unsigned char>(clamped + 0.5);
}

RgbaSample SampleAreaFilteredRgba(const unsigned char* source, int width, int height, double sourceX0, double sourceY0, double sourceX1, double sourceY1) {
    const int minX = (std::max)(0, static_cast<int>(std::floor(sourceX0)));
    const int minY = (std::max)(0, static_cast<int>(std::floor(sourceY0)));
    const int maxX = (std::min)(width, static_cast<int>(std::ceil(sourceX1)));
    const int maxY = (std::min)(height, static_cast<int>(std::ceil(sourceY1)));

    double totalArea = 0.0;
    double weightedAlpha = 0.0;
    double weightedRed = 0.0;
    double weightedGreen = 0.0;
    double weightedBlue = 0.0;

    for (int sourceY = minY; sourceY < maxY; ++sourceY) {
        const double overlapY = (std::min)(sourceY1, static_cast<double>(sourceY + 1))
            - (std::max)(sourceY0, static_cast<double>(sourceY));
        if (overlapY <= 0.0) {
            continue;
        }

        for (int sourceX = minX; sourceX < maxX; ++sourceX) {
            const double overlapX = (std::min)(sourceX1, static_cast<double>(sourceX + 1))
                - (std::max)(sourceX0, static_cast<double>(sourceX));
            if (overlapX <= 0.0) {
                continue;
            }

            const double area = overlapX * overlapY;
            const std::size_t sourceOffset = (static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(width) + static_cast<std::size_t>(sourceX)) * 4u;
            const double alpha = static_cast<double>(source[sourceOffset + 3]) / 255.0;
            const double alphaArea = alpha * area;

            totalArea += area;
            weightedAlpha += alphaArea;
            weightedRed += static_cast<double>(source[sourceOffset + 0]) * alphaArea;
            weightedGreen += static_cast<double>(source[sourceOffset + 1]) * alphaArea;
            weightedBlue += static_cast<double>(source[sourceOffset + 2]) * alphaArea;
        }
    }

    RgbaSample sample;
    if (weightedAlpha > 0.0) {
        sample.red = weightedRed / weightedAlpha;
        sample.green = weightedGreen / weightedAlpha;
        sample.blue = weightedBlue / weightedAlpha;
    }
    sample.alpha = totalArea > 0.0 ? (weightedAlpha / totalArea) * 255.0 : 0.0;
    return sample;
}

std::vector<unsigned char> BuildCenteredScaledCanvas(const unsigned char* source, int width, int height, float contentScale) {
    const float scale = ClampContentScale(contentScale);
    if (source == nullptr || width <= 0 || height <= 0 || scale >= 0.995f) {
        return {};
    }

    const int scaledWidth = (std::max)(1, static_cast<int>(static_cast<float>(width) * scale + 0.5f));
    const int scaledHeight = (std::max)(1, static_cast<int>(static_cast<float>(height) * scale + 0.5f));
    const int offsetX = (width - scaledWidth) / 2;
    const int offsetY = (height - scaledHeight) / 2;
    const double sourceScaleX = static_cast<double>(width) / static_cast<double>(scaledWidth);
    const double sourceScaleY = static_cast<double>(height) / static_cast<double>(scaledHeight);

    std::vector<unsigned char> canvas(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);
    for (int y = 0; y < scaledHeight; ++y) {
        const double sourceY0 = static_cast<double>(y) * sourceScaleY;
        const double sourceY1 = static_cast<double>(y + 1) * sourceScaleY;
        for (int x = 0; x < scaledWidth; ++x) {
            const double sourceX0 = static_cast<double>(x) * sourceScaleX;
            const double sourceX1 = static_cast<double>(x + 1) * sourceScaleX;
            const RgbaSample sample = SampleAreaFilteredRgba(source, width, height, sourceX0, sourceY0, sourceX1, sourceY1);
            const std::size_t targetOffset = (static_cast<std::size_t>(offsetY + y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(offsetX + x)) * 4u;
            canvas[targetOffset + 0] = ToByte(sample.red);
            canvas[targetOffset + 1] = ToByte(sample.green);
            canvas[targetOffset + 2] = ToByte(sample.blue);
            canvas[targetOffset + 3] = ToByte(sample.alpha);
        }
    }

    return canvas;
}

RwTexture* CreateTextureFromRgbaPixels(const unsigned char* rgba, int width, int height, const char* label) {
    if (rgba == nullptr || width <= 0 || height <= 0) {
        return nullptr;
    }

    RwRaster* raster = RwRasterCreate(width, height, 0, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    if (raster == nullptr) {
        Logger::Warning("[EmbeddedRwTexture] Failed to create raster for %s", label);
        return nullptr;
    }

    auto* pixels = reinterpret_cast<unsigned char*>(RwRasterLock(raster, 0, rwRASTERLOCKWRITE));
    if (pixels == nullptr) {
        RwRasterDestroy(raster);
        Logger::Warning("[EmbeddedRwTexture] Failed to lock raster for %s", label);
        return nullptr;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t i = 0; i < pixelCount; ++i) {
        const std::size_t offset = i * 4u;
        pixels[offset + 0] = rgba[offset + 2];
        pixels[offset + 1] = rgba[offset + 1];
        pixels[offset + 2] = rgba[offset + 0];
        pixels[offset + 3] = rgba[offset + 3];
    }

    RwRasterUnlock(raster);

    RwTexture* texture = RwTextureCreate(raster);
    if (texture == nullptr) {
        RwRasterDestroy(raster);
        Logger::Warning("[EmbeddedRwTexture] Failed to create texture for %s", label);
        return nullptr;
    }

    RwTextureSetFilterMode(texture, rwFILTERLINEAR);
    return texture;
}

} // namespace
#endif

void* CreateRwTextureFromEmbeddedPng(int resourceId, const char* logLabel, float contentScale) {
#ifdef KEYBINDER_TESTING
    (void)resourceId;
    (void)logLabel;
    (void)contentScale;
    return nullptr;
#else
    const char* label = logLabel != nullptr ? logLabel : "embedded PNG";

    EmbeddedResourceView resource;
    if (!TryLoadEmbeddedResource(resourceId, &resource)) {
        Logger::Warning("[EmbeddedRwTexture] Resource missing for %s", label);
        return nullptr;
    }

    if (resource.size > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        Logger::Warning("[EmbeddedRwTexture] Resource too large for %s", label);
        return nullptr;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load_from_memory(
        static_cast<const unsigned char*>(resource.data),
        static_cast<int>(resource.size),
        &width,
        &height,
        &channels,
        4);
    if (data == nullptr) {
        Logger::Warning("[EmbeddedRwTexture] Failed to decode %s", label);
        return nullptr;
    }

    constexpr int kMaxTextureSize = 1024;
    if (width <= 0 || height <= 0 || width > kMaxTextureSize || height > kMaxTextureSize) {
        stbi_image_free(data);
        Logger::Warning("[EmbeddedRwTexture] Invalid texture size for %s: %dx%d", label, width, height);
        return nullptr;
    }

    const std::vector<unsigned char> scaledCanvas = BuildCenteredScaledCanvas(data, width, height, contentScale);
    const unsigned char* uploadPixels = scaledCanvas.empty() ? data : scaledCanvas.data();
    RwTexture* texture = CreateTextureFromRgbaPixels(uploadPixels, width, height, label);
    stbi_image_free(data);
    return texture;
#endif
}

void* CreateRwTextureFromPngFile(const std::string& path, const char* logLabel, int maxTextureSize) {
#ifdef KEYBINDER_TESTING
    (void)path;
    (void)logLabel;
    (void)maxTextureSize;
    return nullptr;
#else
    const char* label = logLabel != nullptr ? logLabel : "PNG file";

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (data == nullptr) {
        Logger::Warning("[EmbeddedRwTexture] Failed to decode %s from %s", label, path.c_str());
        return nullptr;
    }

    if (width <= 0 || height <= 0 || width > maxTextureSize || height > maxTextureSize) {
        stbi_image_free(data);
        Logger::Warning("[EmbeddedRwTexture] Invalid texture size for %s: %dx%d", label, width, height);
        return nullptr;
    }

    RwTexture* texture = CreateTextureFromRgbaPixels(data, width, height, label);
    stbi_image_free(data);
    return texture;
#endif
}

} // namespace keybinder
