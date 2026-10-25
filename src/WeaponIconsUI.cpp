#include "../include/WeaponIconsUI.h"
#include "../include/WeaponIconManager.h"
#include "../include/WeaponIconCatalog.h"
#include "../include/PersistentPaths.h"
#include "../include/SettingsManager.h"
#include "../include/TextureManager.h"
#include "../include/ThemeManager.h"
#include "../include/UIComponents.h"
#include "../include/imgui-1.92.7/imgui.h"

#include <algorithm>
#include <utility>
#include <vector>
#ifndef KEYBINDER_TESTING
#include <shellapi.h>
#endif

namespace keybinder {
namespace {
WeaponIconSelection& SelectionFor(Settings& settings, int weaponId) {
    for (auto& selection : settings.weapon_icon_selections)
        if (selection.weaponId == weaponId) return selection;
    settings.weapon_icon_selections.push_back({weaponId, {}, true});
    return settings.weapon_icon_selections.back();
}

const WeaponIconSelection* FindSelection(const Settings& settings, int weaponId) {
    for (const auto& selection : settings.weapon_icon_selections)
        if (selection.weaponId == weaponId) return &selection;
    return nullptr;
}

std::string FileName(const std::string& path) {
    const auto separator = path.find_last_of("\\/");
    return separator == std::string::npos ? path : path.substr(separator + 1);
}
} // namespace

void WeaponIconsUI::PersistAndApply() {
    if (weaponIconManager_ && settingsManager_) weaponIconManager_->ApplySettings(settingsManager_->Get());
    if (OnSettingsChanged) OnSettingsChanged();
}

bool WeaponIconsUI::EnsureThumbnailLoaded(const std::string& path) {
    if (!textureManager_) return false;
    if (textureManager_->HasTexture(path)) return true;
    auto* device = deviceGetter_ ? deviceGetter_() : nullptr;
    return device && textureManager_->LoadTexture(path, device);
}

void WeaponIconsUI::Render() {
    if (!settingsManager_ || !weaponIconManager_ || !textureManager_) {
        ImGui::TextDisabled("Waffenicons werden vorbereitet.");
        return;
    }
    auto& settings = settingsManager_->GetMutable();
    bool changed = false;
    UIComponents::BeginCard("weapon-icons-options", 0.0f);
    changed |= UIComponents::Toggle("Waffenicons aktivieren", &settings.weapon_icons_enabled);
    ImGui::TextWrapped("Lege PNG-Bilder in NLBinder/Waffenicons/ ab, jeweils im Ordner der passenden Waffe. Mehrere Bilder pro Waffe sind möglich.");
    ImGui::Spacing();
    if (UIComponents::SecondaryButton("Ordner neu einlesen")) {
        weaponIconManager_->RefreshLibrary();
        weaponIconManager_->ApplySettings(settings);
    }
    ImGui::SameLine();
    if (UIComponents::SecondaryButton("Waffenicons-Ordner öffnen")) {
#ifndef KEYBINDER_TESTING
        const auto directory = PersistentPaths::WeaponIconsDir();
        ShellExecuteA(nullptr, "open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
    }
    ImGui::TextDisabled("Ohne eigene PNG bleibt das originale Waffenicon erhalten. Die Fist findest du weiterhin unter Hud.");
    UIComponents::EndCard();

    ImGui::Spacing();
    UIComponents::BeginCard("weapon-icon-selection", 0.0f);
    std::vector<const char*> names;
    int selectedIndex = 0;
    for (const auto& definition : kWeaponIconCatalog) {
        if (definition.weaponId == selectedWeaponId_) selectedIndex = static_cast<int>(names.size());
        names.push_back(definition.name);
    }
    if (UIComponents::Combo("Waffe", &selectedIndex, names.data(), static_cast<int>(names.size())))
        selectedWeaponId_ = kWeaponIconCatalog[selectedIndex].weaponId;
    const auto* definition = FindWeaponIconDefinition(selectedWeaponId_);
    if (!definition) { UIComponents::EndCard(); return; }
    const auto* selection = FindSelection(settings, selectedWeaponId_);
    bool weaponEnabled = !selection || selection->enabled;
    if (UIComponents::Toggle("Eigenes Icon für diese Waffe", &weaponEnabled)) {
        SelectionFor(settings, selectedWeaponId_).enabled = weaponEnabled;
        changed = true;
    }
    const std::string folder = std::string("Waffenicons/") + definition->folder + "/";
    UIComponents::InfoRow("Ordner", folder.c_str());
    const auto available = weaponIconManager_->GetAvailableIcons(selectedWeaponId_);
    const std::string effective = weaponIconManager_->GetConfiguredIcon(selectedWeaponId_);
    const std::string status = !settings.weapon_icons_enabled ? "Waffenicons ausgeschaltet"
        : !weaponEnabled ? "Originales Waffenicon"
        : effective.empty() ? "Originales Waffenicon (keine passende PNG vorhanden)"
        : !weaponIconManager_->IsAppliedForWeapon(selectedWeaponId_) ? "Originales Waffenicon (PNG nicht geladen)" : FileName(effective);
    UIComponents::InfoRow("Aktiv", status.c_str());
    if (selection && !selection->path.empty()
        && std::none_of(available.begin(), available.end(), [&](const std::string& path) {
            return LowerWeaponIconPath(path) == LowerWeaponIconPath(selection->path);
        }))
        ImGui::TextWrapped("Die gespeicherte PNG fehlt. Wähle ein vorhandenes Bild oder die automatische Auswahl.");
    if (UIComponents::SecondaryButton("Automatisch erste PNG verwenden")) {
        auto& editable = SelectionFor(settings, selectedWeaponId_);
        editable.path.clear();
        editable.enabled = true;
        changed = true;
    }
    ImGui::SameLine();
    if (UIComponents::SecondaryButton("Original für diese Waffe")) {
        SelectionFor(settings, selectedWeaponId_).enabled = false;
        changed = true;
    }
    if (!weaponIconManager_->GetLastError().empty())
        ImGui::TextWrapped("%s", weaponIconManager_->GetLastError().c_str());
    UIComponents::EndCard();

    ImGui::Spacing();
    UIComponents::Separator("PNG-Auswahl");
    const float thumbSize = UIComponents::ThumbnailSize();
    const float cardWidth = UIComponents::ThumbnailCardWidth();
    const float availableWidth = UIComponents::GetThumbnailLayoutWidth(cardWidth);
    float lineWidth = 0.0f;
    for (const auto& path : available) {
        const bool selected = weaponEnabled && effective == path;
        TextureInfo* texture = EnsureThumbnailLoaded(path) ? textureManager_->GetTexture(path) : nullptr;
        ImGui::PushID(path.c_str());
        if (lineWidth > 0.0f && lineWidth + cardWidth > availableWidth) lineWidth = 0.0f;
        if (lineWidth > 0.0f) ImGui::SameLine();
        UIComponents::BeginCard("weapon-icon-item", cardWidth, selected);
        UIComponents::CenterNextItem(thumbSize);
        if (UIComponents::ThumbnailButton("##weapon-icon", ImVec2(thumbSize, thumbSize), selected)) {
            auto& editable = SelectionFor(settings, selectedWeaponId_);
            editable.path = path;
            editable.enabled = true;
            changed = true;
        }
        if (texture && texture->texture) UIComponents::DrawFittedTexturePreview(*texture);
        else UIComponents::DrawThumbnailTextOverlay("PNG", ImGui::GetColorU32(ThemeManager::Instance().GetColor(ThemeColorRole::TextPrimary)));
        ImGui::Spacing();
        UIComponents::DrawThumbnailCaption(FileName(path).c_str(), selected);
        UIComponents::EndCard();
        lineWidth += cardWidth + ImGui::GetStyle().ItemSpacing.x;
        ImGui::PopID();
    }
    if (available.empty()) ImGui::TextWrapped("Noch keine PNG für %s. Lege dein Bild in %s und klicke auf 'Ordner neu einlesen'.", definition->name, folder.c_str());
    if (changed) PersistAndApply();
}
} // namespace keybinder
