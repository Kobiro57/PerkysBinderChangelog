#include "../include/CrosshairUI.h"

#include "../include/CrosshairSwitcher.h"
#include "../include/PersistentPaths.h"
#include "../include/ThemeManager.h"
#include "../include/TextureManager.h"
#include "../include/UIComponents.h"

#include "../include/imgui-1.92.7/imgui.h"

#include <algorithm>

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

void CrosshairUI::SetSettingsManager(SettingsManager* manager) {
    settingsManager_ = manager;
    RefreshAvailablePngs();
}

void CrosshairUI::SetCrosshairSwitcher(CrosshairSwitcher* switcher) {
    crosshairSwitcher_ = switcher;
}

void CrosshairUI::SetTextureManager(TextureManager* manager) {
    textureManager_ = manager;
    RefreshAvailablePngs();
}

void CrosshairUI::SetDeviceGetter(std::function<IDirect3DDevice9*()> getter) {
    deviceGetter_ = std::move(getter);
}

void CrosshairUI::RefreshAvailablePngs() {
    if (textureManager_ == nullptr) {
        availablePngs_.clear();
        return;
    }

    availablePngs_ = textureManager_->ScanDirectory(PersistentPaths::CrosshairsDir(), "crosshairs");
    std::sort(availablePngs_.begin(), availablePngs_.end());
}

bool CrosshairUI::EnsureThumbnailLoaded(const std::string& path) {
    if (textureManager_ == nullptr) {
        return false;
    }

    if (textureManager_->HasTexture(path)) {
        return true;
    }

    IDirect3DDevice9* device = deviceGetter_ ? deviceGetter_() : nullptr;
    return device != nullptr && textureManager_->LoadTexture(path, device);
}

void CrosshairUI::Render() {
    if (settingsManager_ == nullptr || crosshairSwitcher_ == nullptr || textureManager_ == nullptr) {
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::Danger), "Crosshair UI nicht initialisiert");
        return;
    }

    Settings& settings = settingsManager_->GetMutable();
    bool changed = false;

    if (availablePngs_.empty()) {
        RefreshAvailablePngs();
    }

    UIComponents::BeginCard("crosshair-options", 0.0f);
    if (UIComponents::Toggle("Custom Crosshair aktivieren", &settings.crosshair_enabled)) {
        changed = true;
    }

    ImGui::Spacing();
    if (UIComponents::SecondaryButton("Crosshair-Ordner neu scannen")) {
        RefreshAvailablePngs();
    }

    std::string activeSource;
    if (!settings.crosshair_enabled) {
        activeSource = "Deaktiviert";
    } else if (settings.crosshair_texture_path.empty()) {
        activeSource = "Kein PNG ausgewaehlt";
    } else {
        activeSource = GetFileNameFromPath(settings.crosshair_texture_path);
    }

    UIComponents::InfoRow("Aktiv", activeSource.c_str());
    UIComponents::EndCard();

    ImGui::Spacing();
    UIComponents::Separator("PNG-Auswahl");
    ImGui::Spacing();

    const float thumbSize = UIComponents::ThumbnailSize();
    const float blockWidth = UIComponents::ThumbnailCardWidth();
    const float availWidth = UIComponents::GetThumbnailLayoutWidth(blockWidth);
    float currentLineWidth = 0.0f;

    auto renderPathSelection = [&](const std::string& path, TextureInfo* textureInfo, const char* fallbackLabel) {
        const bool selected = settings.crosshair_enabled && settings.crosshair_texture_path == path;

        ImGui::PushID(path.c_str());
        if (currentLineWidth > 0.0f && currentLineWidth + blockWidth > availWidth) {
            currentLineWidth = 0.0f;
        }
        if (currentLineWidth > 0.0f) {
            ImGui::SameLine();
        }

        UIComponents::BeginCard("crosshair_item", blockWidth, selected);

        UIComponents::CenterNextItem(thumbSize);
        if (UIComponents::ThumbnailButton("##thumb", ImVec2(thumbSize, thumbSize), selected)) {
            settings.crosshair_enabled = true;
            settings.crosshair_texture_path = path;
            changed = true;
        }

        if (textureInfo != nullptr && textureInfo->texture != nullptr) {
            UIComponents::DrawFittedTexturePreview(*textureInfo);
        } else {
            UIComponents::DrawThumbnailTextOverlay("PNG", ImGui::GetColorU32(Theme().GetColor(ThemeColorRole::TextPrimary)));
        }

        ImGui::Spacing();
        UIComponents::DrawThumbnailCaption(fallbackLabel, selected);

        UIComponents::EndCard();

        currentLineWidth += blockWidth + ImGui::GetStyle().ItemSpacing.x;
        ImGui::PopID();
    };

    for (const auto& path : availablePngs_) {
        TextureInfo* textureInfo = nullptr;
        if (EnsureThumbnailLoaded(path)) {
            textureInfo = textureManager_->GetTexture(path);
        }
        renderPathSelection(path, textureInfo, GetFileNameFromPath(path).c_str());
    }

    if (availablePngs_.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Keine PNG-Dateien im Ordner 'crosshairs/' gefunden.");
    }

    if (changed) {
        crosshairSwitcher_->ApplySettings(settings);
        if (OnSettingsChanged) {
            OnSettingsChanged();
        }
    }
}

} // namespace keybinder
