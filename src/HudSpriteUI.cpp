#include "../include/HudSpriteUI.h"

#include "../include/HudSpriteManager.h"
#include "../include/PersistentPaths.h"
#include "../include/ThemeManager.h"
#include "../include/UIComponents.h"

#include "../include/imgui-1.92.7/imgui.h"

#include <algorithm>
#include <string>
#include <utility>

namespace keybinder {

namespace {

ThemeManager& Theme() {
    return ThemeManager::Instance();
}

std::string GetFileNameFromPath(const std::string& path) {
    const std::size_t slashPos = path.find_last_of("\\/");
    return slashPos == std::string::npos ? path : path.substr(slashPos + 1);
}

} // namespace

void HudSpriteUI::SetSettingsManager(SettingsManager* manager) {
    settingsManager_ = manager;
    RefreshAvailableFists();
}

void HudSpriteUI::SetHudSpriteManager(HudSpriteManager* manager) {
    hudSpriteManager_ = manager;
}

void HudSpriteUI::SetTextureManager(TextureManager* manager) {
    textureManager_ = manager;
    RefreshAvailableFists();
}

void HudSpriteUI::SetDeviceGetter(std::function<IDirect3DDevice9*()> getter) {
    deviceGetter_ = std::move(getter);
}

void HudSpriteUI::RefreshAvailableFists() {
    if (textureManager_ == nullptr) {
        availableFistPngs_.clear();
        return;
    }

    availableFistPngs_ = textureManager_->ScanDirectory(PersistentPaths::FistsDir(), "fists");
    std::sort(availableFistPngs_.begin(), availableFistPngs_.end());
}

bool HudSpriteUI::EnsureThumbnailLoaded(const std::string& path) {
    if (textureManager_ == nullptr) {
        return false;
    }

    if (textureManager_->HasTexture(path)) {
        return true;
    }

    IDirect3DDevice9* device = deviceGetter_ ? deviceGetter_() : nullptr;
    return device != nullptr && textureManager_->LoadTexture(path, device);
}


void HudSpriteUI::RenderFistLibrary() {
    if (settingsManager_ == nullptr || hudSpriteManager_ == nullptr || textureManager_ == nullptr) {
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::Danger), "Fist UI nicht initialisiert");
        return;
    }

    Settings& settings = settingsManager_->GetMutable();
    bool changed = false;

    if (availableFistPngs_.empty()) {
        RefreshAvailableFists();
    }

    UIComponents::BeginCard("fist-library-options", 0.0f);
    if (UIComponents::SecondaryButton("Fist-Ordner neu scannen")) {
        RefreshAvailableFists();
    }

    if (!settings.fist_texture_path.empty()) {
        ImGui::SameLine();
        if (UIComponents::SecondaryButton("Native Fist wiederherstellen")) {
            settings.fist_texture_path.clear();
            changed = true;
        }
    }

    ImGui::Spacing();
    const std::string activeSource = settings.fist_texture_path.empty()
        ? std::string("Native GTA-Fist")
        : GetFileNameFromPath(settings.fist_texture_path);
    UIComponents::InfoRow("Aktiv", activeSource.c_str());
    UIComponents::EndCard();

    ImGui::Spacing();
    UIComponents::Separator("Fist PNG-Auswahl");
    ImGui::Spacing();

    const float thumbSize = UIComponents::ThumbnailSize();
    const float blockWidth = UIComponents::ThumbnailCardWidth();
    const float availWidth = UIComponents::GetThumbnailLayoutWidth(blockWidth);
    float currentLineWidth = 0.0f;

    for (const auto& path : availableFistPngs_) {
        const bool selected = settings.fist_texture_path == path;
        TextureInfo* textureInfo = nullptr;
        if (EnsureThumbnailLoaded(path)) {
            textureInfo = textureManager_->GetTexture(path);
        }

        ImGui::PushID(path.c_str());
        if (currentLineWidth > 0.0f && currentLineWidth + blockWidth > availWidth) {
            currentLineWidth = 0.0f;
        }
        if (currentLineWidth > 0.0f) {
            ImGui::SameLine();
        }

        UIComponents::BeginCard("fist_item", blockWidth, selected);

        UIComponents::CenterNextItem(thumbSize);
        if (UIComponents::ThumbnailButton("##thumb", ImVec2(thumbSize, thumbSize), selected)) {
            settings.fist_texture_path = path;
            changed = true;
        }

        if (textureInfo != nullptr && textureInfo->texture != nullptr) {
            UIComponents::DrawFittedTexturePreview(*textureInfo);
        } else {
            UIComponents::DrawThumbnailTextOverlay("PNG", ImGui::GetColorU32(Theme().GetColor(ThemeColorRole::TextPrimary)));
        }

        ImGui::Spacing();
        UIComponents::DrawThumbnailCaption(GetFileNameFromPath(path).c_str(), selected);

        UIComponents::EndCard();

        currentLineWidth += blockWidth + ImGui::GetStyle().ItemSpacing.x;
        ImGui::PopID();
    }

    if (availableFistPngs_.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Keine PNG-Dateien im Ordner 'fists/' gefunden. Die native GTA-Fist bleibt aktiv.");
    }


    if (changed) {
        hudSpriteManager_->ApplySettings(settings);
        if (OnSettingsChanged) {
            OnSettingsChanged();
        }
    }
}

} // namespace keybinder
