#include "../include/HitmarkerUI.h"
#include "../include/Hitmarker.h"
#include "../include/PersistentPaths.h"
#include "../include/ThemeManager.h"
#include "../include/TextureManager.h"
#include "../include/UIComponents.h"
#include "../include/imgui-1.92.7/imgui.h"

#include <algorithm>
#include <vector>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace keybinder {

namespace {

ThemeManager& Theme() {
    return ThemeManager::Instance();
}

constexpr int kMaxSavedHitmarkerPresets = 5;

std::string GetFileNameFromPath(const std::string& path) {
    const std::size_t slashPos = path.find_last_of("\\/");
    if (slashPos == std::string::npos) {
        return path;
    }

    return path.substr(slashPos + 1);
}

int ToShapeSettingValue(HitmarkerShape shape) {
    switch (shape) {
    case HitmarkerShape::CrossX:
        return 0;
    case HitmarkerShape::Plus:
        return 1;
    case HitmarkerShape::Circle:
        return 2;
    case HitmarkerShape::Diamond:
        return 3;
    case HitmarkerShape::CustomPNG:
        return 4;
    default:
        return 0;
    }
}

ImU32 MakeOutlineColor(ImU32 color, float alphaScale = 0.45f) {
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
    const ImVec4 bgBase = Theme().GetColor(ThemeColorRole::Background);
    rgba.x = bgBase.x;
    rgba.y = bgBase.y;
    rgba.z = bgBase.z;
    rgba.w *= alphaScale;
    return ImGui::ColorConvertFloat4ToU32(rgba);
}

void DrawLineWithOutline(ImDrawList* drawList, const ImVec2& start, const ImVec2& end, ImU32 color, float thickness) {
    drawList->AddLine(start, end, MakeOutlineColor(color), thickness + 2.0f);
    drawList->AddLine(start, end, color, thickness);
}

void DrawShapePreview(ImDrawList* drawList, const ImVec2& topLeft, const ImVec2& bottomRight,
                      HitmarkerShape shape, ImU32 color) {
    const ImVec2 center((topLeft.x + bottomRight.x) * 0.5f, (topLeft.y + bottomRight.y) * 0.5f);
    const float size = std::min(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y) * 0.56f;
    const float half = size * 0.5f;
    const float thickness = std::max(2.0f, size / 7.0f);

    switch (shape) {
    case HitmarkerShape::CrossX: {
        const float scale = size / 20.0f;
        const float innerGap = std::max(1.6f, 4.0f * scale);
        const float armLength = std::max(4.0f, 10.0f * scale);
        const float lineThickness = std::max(1.0f, 2.0f * scale);
        drawList->AddLine(ImVec2(center.x - innerGap, center.y - innerGap), ImVec2(center.x - innerGap - armLength, center.y - innerGap - armLength), color, lineThickness);
        drawList->AddLine(ImVec2(center.x + innerGap, center.y - innerGap), ImVec2(center.x + innerGap + armLength, center.y - innerGap - armLength), color, lineThickness);
        drawList->AddLine(ImVec2(center.x - innerGap, center.y + innerGap), ImVec2(center.x - innerGap - armLength, center.y + innerGap + armLength), color, lineThickness);
        drawList->AddLine(ImVec2(center.x + innerGap, center.y + innerGap), ImVec2(center.x + innerGap + armLength, center.y + innerGap + armLength), color, lineThickness);
        break;
    }
    case HitmarkerShape::Plus: {
        const float gap = std::max(3.0f, size * 0.18f);
        DrawLineWithOutline(drawList, ImVec2(center.x - half, center.y), ImVec2(center.x - gap, center.y), color, thickness);
        DrawLineWithOutline(drawList, ImVec2(center.x + gap, center.y), ImVec2(center.x + half, center.y), color, thickness);
        DrawLineWithOutline(drawList, ImVec2(center.x, center.y - half), ImVec2(center.x, center.y - gap), color, thickness);
        DrawLineWithOutline(drawList, ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + half), color, thickness);
        break;
    }
    case HitmarkerShape::Circle: {
        const float radius = size * 0.46f;
        drawList->AddCircle(center, radius, MakeOutlineColor(color), 48, thickness + 2.0f);
        drawList->AddCircle(center, radius, color, 48, thickness);
        break;
    }
    case HitmarkerShape::Diamond: {
        const ImVec2 top(center.x, center.y - half);
        const ImVec2 right(center.x + half, center.y);
        const ImVec2 bottom(center.x, center.y + half);
        const ImVec2 left(center.x - half, center.y);
        DrawLineWithOutline(drawList, top, right, color, thickness);
        DrawLineWithOutline(drawList, right, bottom, color, thickness);
        DrawLineWithOutline(drawList, bottom, left, color, thickness);
        DrawLineWithOutline(drawList, left, top, color, thickness);
        break;
    }
    case HitmarkerShape::CustomPNG:
        break;
    }
}

void EnsureSavedPresetSlots(std::vector<HitmarkerPreset>& presets) {
    if (presets.size() > kMaxSavedHitmarkerPresets) {
        presets.resize(kMaxSavedHitmarkerPresets);
    }

    while (presets.size() < kMaxSavedHitmarkerPresets) {
        presets.emplace_back();
    }
}

HitmarkerPreset BuildPresetFromSettings(const Settings& settings) {
    HitmarkerPreset preset;
    preset.shape = static_cast<HitmarkerShape>(settings.hitmarker_shape);
    preset.color = HitmarkerColor{
        settings.hitmarker_color_r,
        settings.hitmarker_color_g,
        settings.hitmarker_color_b,
        settings.hitmarker_color_a
    };
    preset.killConfirmedColor = HitmarkerColor{
        settings.hitmarker_kill_color_r,
        settings.hitmarker_kill_color_g,
        settings.hitmarker_kill_color_b,
        settings.hitmarker_kill_color_a
    };
    preset.size = settings.hitmarker_size;
    preset.opacity = settings.hitmarker_opacity;
    preset.duration = settings.hitmarker_duration;
    preset.fadeOut = settings.hitmarker_fade_out;
    preset.customImagePath = preset.shape == HitmarkerShape::CustomPNG
        ? settings.hitmarker_custom_image_path
        : std::string{};
    return preset;
}

void ApplyPresetToSettings(const HitmarkerPreset& preset, Settings& settings) {
    settings.hitmarker_shape = ToShapeSettingValue(preset.shape);
    settings.hitmarker_size = preset.size;
    settings.hitmarker_opacity = preset.opacity;
    settings.hitmarker_duration = preset.duration;
    settings.hitmarker_fade_out = preset.fadeOut;
    settings.hitmarker_color_r = preset.color.r;
    settings.hitmarker_color_g = preset.color.g;
    settings.hitmarker_color_b = preset.color.b;
    settings.hitmarker_color_a = preset.color.a;
    settings.hitmarker_kill_color_r = preset.killConfirmedColor.r;
    settings.hitmarker_kill_color_g = preset.killConfirmedColor.g;
    settings.hitmarker_kill_color_b = preset.killConfirmedColor.b;
    settings.hitmarker_kill_color_a = preset.killConfirmedColor.a;
    settings.hitmarker_custom_image_path = preset.customImagePath;
}

std::string GetPresetSlotLabel(const HitmarkerPreset& preset, int slotIndex) {
    if (!preset.name.empty()) {
        return preset.name;
    }

    return "Slot " + std::to_string(slotIndex + 1) + " (leer)";
}

} // namespace

void HitmarkerUI::SetSettingsManager(SettingsManager* manager) {
    settingsManager_ = manager;
    RefreshAvailablePngs();
}

void HitmarkerUI::SetHitmarker(Hitmarker* hitmarker) {
    hitmarker_ = hitmarker;
}

void HitmarkerUI::SetTextureManager(TextureManager* manager) {
    textureManager_ = manager;
    RefreshAvailablePngs();
}

void HitmarkerUI::SetDeviceGetter(std::function<IDirect3DDevice9*()> getter) {
    deviceGetter_ = std::move(getter);
}

void HitmarkerUI::RefreshAvailablePngs() {
    if (textureManager_ == nullptr) {
        availablePngs_.clear();
        return;
    }

    availablePngs_ = textureManager_->ScanDirectory(PersistentPaths::HitmarkersDir(), "hitmarkers");
    std::sort(availablePngs_.begin(), availablePngs_.end());
}

bool HitmarkerUI::EnsureThumbnailLoaded(const std::string& path) {
    if (textureManager_ == nullptr) {
        return false;
    }

    const std::string resolvedPath = PersistentPaths::ResolveAssetPath(path);
    if (resolvedPath.empty()) {
        return false;
    }

    if (textureManager_->HasTexture(resolvedPath)) {
        return true;
    }

    IDirect3DDevice9* device = deviceGetter_ ? deviceGetter_() : nullptr;
    return device != nullptr && textureManager_->LoadTexture(resolvedPath, device);
}

void HitmarkerUI::Render() {
    if (settingsManager_ == nullptr || hitmarker_ == nullptr || textureManager_ == nullptr) {
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::Danger), "Hitmarker oder SettingsManager nicht initialisiert");
        return;
    }

    Settings& settings = settingsManager_->GetMutable();
    EnsureSavedPresetSlots(settings.hitmarker_saved_presets);

    bool changed = false;

    UIComponents::BeginCard("hitmarker_main_opts", 0.0f);
    if (UIComponents::Toggle("Hitmarker aktivieren", &settings.hitmarker_enabled)) {
        hitmarker_->SetEnabled(settings.hitmarker_enabled);
        changed = true;
    }
    UIComponents::EndCard();

    ImGui::Dummy(ImVec2(0.0f, Theme().GetSpacing(ThemeSpacingRole::CardGap) * 0.5f));

    if (!settings.hitmarker_enabled) {
        ImGui::BeginDisabled();
    }

    UIComponents::BeginCard("hitmarker_presets", 0.0f);
    UIComponents::CardHeader("Presets");

    int selectedSlot = settings.hitmarker_active_preset_index;
    if (selectedSlot < 0) {
        selectedSlot = 0;
    } else if (selectedSlot >= kMaxSavedHitmarkerPresets) {
        selectedSlot = kMaxSavedHitmarkerPresets - 1;
    }

    std::vector<std::string> slotLabels;
    std::vector<const char*> slotLabelPtrs;
    slotLabels.reserve(kMaxSavedHitmarkerPresets);
    slotLabelPtrs.reserve(kMaxSavedHitmarkerPresets);

    for (int i = 0; i < kMaxSavedHitmarkerPresets; ++i) {
        slotLabels.push_back(GetPresetSlotLabel(settings.hitmarker_saved_presets[static_cast<std::size_t>(i)], i));
        slotLabelPtrs.push_back(slotLabels.back().c_str());
    }

    if (UIComponents::Combo("Preset-Slot", &selectedSlot, slotLabelPtrs.data(), kMaxSavedHitmarkerPresets)) {
        settings.hitmarker_active_preset_index = selectedSlot;

        const HitmarkerPreset& selectedPreset = settings.hitmarker_saved_presets[static_cast<std::size_t>(selectedSlot)];
        if (!selectedPreset.name.empty()) {
            ApplyPresetToSettings(selectedPreset, settings);
            changed = true;
        }
    }

    HitmarkerPreset& currentSlot = settings.hitmarker_saved_presets[static_cast<std::size_t>(selectedSlot)];
    std::string presetNameStr = currentSlot.name;
    if (UIComponents::InputText("Preset-Name", &presetNameStr, "Name eingeben...")) {
        currentSlot.name = presetNameStr;
        if (!currentSlot.name.empty()) {
            changed = true;
        }
    }

    ImGui::Spacing();
    if (UIComponents::PrimaryButton("Aktuelle Einstellungen in Slot speichern")) {
        HitmarkerPreset savedPreset = BuildPresetFromSettings(settings);
        savedPreset.name = currentSlot.name.empty()
            ? ("Preset " + std::to_string(selectedSlot + 1))
            : currentSlot.name;
        currentSlot = savedPreset;
        settings.hitmarker_active_preset_index = selectedSlot;
        changed = true;
    }

    ImGui::SameLine();
    if (UIComponents::SecondaryButton("Slot laden")) {
        if (!currentSlot.name.empty()) {
            ApplyPresetToSettings(currentSlot, settings);
            changed = true;
        }
    }

    ImGui::SameLine();
    if (UIComponents::DangerButton("Slot löschen")) {
        currentSlot = HitmarkerPreset{};
        changed = true;
    }

    if (currentSlot.name.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme().GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Dieser Slot ist leer. Stelle den Hitmarker ein und speichere ihn hier.");
        ImGui::PopStyleColor();
    }
    UIComponents::EndCard();

    ImGui::Dummy(ImVec2(0.0f, Theme().GetSpacing(ThemeSpacingRole::CardGap) * 0.5f));
    UIComponents::Separator("Erscheinungsbild");

    const float thumbSize = UIComponents::ThumbnailSize();
    const float blockWidth = UIComponents::ThumbnailCardWidth();
    const float availWidth = UIComponents::GetThumbnailLayoutWidth(blockWidth);
    float currentLineWidth = 0.0f;

    const auto getShapeLabel = [](HitmarkerShape shape) {
        switch (shape) {
        case HitmarkerShape::CrossX:
            return "Kreuz (X)";
        case HitmarkerShape::Plus:
            return "Plus (+)";
        case HitmarkerShape::Circle:
            return "Kreis";
        case HitmarkerShape::Diamond:
            return "Diamant";
        case HitmarkerShape::CustomPNG:
            return "Eigenes PNG";
        default:
            return "Unbekannt";
        }
    };

    auto beginThumbnailCard = [&](const char* id, bool selected) {
        ImGui::PushID(id);
        if (currentLineWidth > 0.0f && currentLineWidth + blockWidth > availWidth) {
            currentLineWidth = 0.0f;
        }
        if (currentLineWidth > 0.0f) {
            ImGui::SameLine();
        }

        UIComponents::BeginCard("hitmarker_item", blockWidth, selected);
    };

    auto endThumbnailCard = [&]() {
        UIComponents::EndCard();
        currentLineWidth += blockWidth + Theme().GetSpacing(ThemeSpacingRole::ItemSpacingX);
        ImGui::PopID();
    };

    auto renderShapeCard = [&](HitmarkerShape shape) {
        const bool selected = settings.hitmarker_shape == ToShapeSettingValue(shape);
        beginThumbnailCard(getShapeLabel(shape), selected);

        TextureInfo* textureInfo = nullptr;
        if (shape == HitmarkerShape::CustomPNG && !settings.hitmarker_custom_image_path.empty() && EnsureThumbnailLoaded(settings.hitmarker_custom_image_path)) {
            textureInfo = textureManager_->GetTexture(PersistentPaths::ResolveAssetPath(settings.hitmarker_custom_image_path));
        }

        UIComponents::CenterNextItem(thumbSize);
        if (UIComponents::ThumbnailButton("##thumb", ImVec2(thumbSize, thumbSize), selected)) {
            settings.hitmarker_shape = ToShapeSettingValue(shape);
            changed = true;
        }

        if (textureInfo != nullptr && textureInfo->texture != nullptr) {
            UIComponents::DrawFittedTexturePreview(*textureInfo);
        } else {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            if (shape == HitmarkerShape::CustomPNG) {
                UIComponents::DrawThumbnailTextOverlay("PNG", ImGui::GetColorU32(Theme().GetColor(ThemeColorRole::TextPrimary)));
            } else {
                DrawShapePreview(drawList, min, max, shape, ImGui::GetColorU32(Theme().GetColor(ThemeColorRole::TextPrimary)));
            }
        }

        ImGui::Spacing();
        UIComponents::DrawThumbnailCaption(getShapeLabel(shape), selected);
        endThumbnailCard();
    };

    ImGui::PushStyleColor(ImGuiCol_Text, Theme().GetColor(ThemeColorRole::TextSecondary));
    ImGui::Text("Form auswählen");
    ImGui::PopStyleColor();
    renderShapeCard(HitmarkerShape::CrossX);
    renderShapeCard(HitmarkerShape::Plus);
    renderShapeCard(HitmarkerShape::Circle);
    renderShapeCard(HitmarkerShape::Diamond);
    renderShapeCard(HitmarkerShape::CustomPNG);

    if (settings.hitmarker_shape == static_cast<int>(HitmarkerShape::CustomPNG)) {
        currentLineWidth = 0.0f;
        ImGui::Spacing();

        if (UIComponents::SecondaryButton("PNG-Ordner neu scannen")) {
            RefreshAvailablePngs();
        }

        if (!availablePngs_.empty()) {
            for (const auto& path : availablePngs_) {
                const bool selected = settings.hitmarker_custom_image_path == path;
                TextureInfo* textureInfo = nullptr;
                if (EnsureThumbnailLoaded(path)) {
                    textureInfo = textureManager_->GetTexture(PersistentPaths::ResolveAssetPath(path));
                }

                beginThumbnailCard(path.c_str(), selected);
                UIComponents::CenterNextItem(thumbSize);
                if (UIComponents::ThumbnailButton("##thumb", ImVec2(thumbSize, thumbSize), selected)) {
                    settings.hitmarker_custom_image_path = path;
                    settings.hitmarker_shape = ToShapeSettingValue(HitmarkerShape::CustomPNG);
                    changed = true;
                }

                if (textureInfo != nullptr && textureInfo->texture != nullptr) {
                    UIComponents::DrawFittedTexturePreview(*textureInfo);
                } else {
                    UIComponents::DrawThumbnailTextOverlay("PNG", ImGui::GetColorU32(Theme().GetColor(ThemeColorRole::TextPrimary)));
                }

                ImGui::Spacing();
                const std::string fileName = GetFileNameFromPath(path);
                UIComponents::DrawThumbnailCaption(fileName.c_str(), selected);
                endThumbnailCard();
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, Theme().GetColor(ThemeColorRole::TextMuted));
            ImGui::TextWrapped("Keine PNG-Dateien im Ordner 'hitmarkers/' gefunden.");
            ImGui::PopStyleColor();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, Theme().GetSpacing(ThemeSpacingRole::CardGap) * 0.5f));
    
    UIComponents::BeginCard("hitmarker_properties", 0.0f);
    UIComponents::CardHeader("Eigenschaften");
    if (UIComponents::SliderFloat("Größe", &settings.hitmarker_size, 8.0f, 64.0f, "%.0f")) {
        changed = true;
    }

    if (UIComponents::SliderFloat("Deckkraft", &settings.hitmarker_opacity, 0.1f, 1.0f, "%.2f")) {
        changed = true;
    }

    if (UIComponents::SliderFloat("Anzeigedauer (s)", &settings.hitmarker_duration, 0.1f, 1.5f, "%.2f s")) {
        changed = true;
    }

    if (UIComponents::Toggle("Ausblenden (Fade-out)", &settings.hitmarker_fade_out)) {
        changed = true;
    }
    UIComponents::EndCard();

    ImGui::Dummy(ImVec2(0.0f, Theme().GetSpacing(ThemeSpacingRole::CardGap) * 0.5f));
    UIComponents::BeginCard("hitmarker_colors", 0.0f);
    UIComponents::CardHeader("Farben");

    if (ImGui::BeginTable("hitmarker_colors_table", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, Theme().GetColor(ThemeColorRole::Border));
        ImGui::PushStyleColor(ImGuiCol_TableBorderLight, Theme().GetColor(ThemeColorRole::Border));
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, Theme().GetColor(ThemeColorRole::SurfaceAlt));
        ImGui::TableNextColumn();
        ImVec4 colorVec(settings.hitmarker_color_r, settings.hitmarker_color_g, settings.hitmarker_color_b, settings.hitmarker_color_a);
        if (UIComponents::ColorPicker("Standard Treffer", &colorVec)) {
            settings.hitmarker_color_r = colorVec.x;
            settings.hitmarker_color_g = colorVec.y;
            settings.hitmarker_color_b = colorVec.z;
            settings.hitmarker_color_a = colorVec.w;
            changed = true;
        }

        ImGui::TableNextColumn();
        ImVec4 killColorVec(settings.hitmarker_kill_color_r, settings.hitmarker_kill_color_g, settings.hitmarker_kill_color_b, settings.hitmarker_kill_color_a);
        if (UIComponents::ColorPicker("Tödlicher Treffer", &killColorVec)) {
            settings.hitmarker_kill_color_r = killColorVec.x;
            settings.hitmarker_kill_color_g = killColorVec.y;
            settings.hitmarker_kill_color_b = killColorVec.z;
            settings.hitmarker_kill_color_a = killColorVec.w;
            changed = true;
        }
        ImGui::EndTable();
        ImGui::PopStyleColor(3);
    }
    UIComponents::EndCard();

    if (!settings.hitmarker_enabled) {
        ImGui::EndDisabled();
    }

    if (changed) {
        HitmarkerPreset updatedPreset = BuildPresetFromSettings(settings);
        updatedPreset.name = currentSlot.name;
        currentSlot = updatedPreset;

        hitmarker_->ApplySettings(settings);

        if (OnSettingsChanged) {
            OnSettingsChanged();
        }
    }
}

} // namespace keybinder


