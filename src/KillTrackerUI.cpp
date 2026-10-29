#include "../include/MessageVariablesUI.h"
/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Kill Tracker UI Implementation
*/
#include "../include/KillTrackerUI.h"
#include "../include/SAMPBridge.h"
#include "../include/KillTracker.h"
#include "../include/WeaponNames.h"
#include "../include/UIComponents.h"
#include "../include/ThemeManager.h"

#include "imgui.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace keybinder {

KillTrackerUI::KillTrackerUI() {
    strncpy(customLocalKillMessageBuf_, settings_.customLocalKillMessage.c_str(), sizeof(customLocalKillMessageBuf_) - 1);
    customLocalKillMessageBuf_[sizeof(customLocalKillMessageBuf_) - 1] = '\0';
    strncpy(customLocalDeathMessageBuf_, settings_.customLocalDeathMessage.c_str(), sizeof(customLocalDeathMessageBuf_) - 1);
    customLocalDeathMessageBuf_[sizeof(customLocalDeathMessageBuf_) - 1] = '\0';
    strncpy(customServerKillMessageBuf_, settings_.customServerKillMessage.c_str(), sizeof(customServerKillMessageBuf_) - 1);
    customServerKillMessageBuf_[sizeof(customServerKillMessageBuf_) - 1] = '\0';
    strncpy(customServerDeathMessageBuf_, settings_.customServerDeathMessage.c_str(), sizeof(customServerDeathMessageBuf_) - 1);
    customServerDeathMessageBuf_[sizeof(customServerDeathMessageBuf_) - 1] = '\0';
}

void KillTrackerUI::SetSettings(const KillTrackerSettings& settings) {
    settings_ = settings;
    strncpy(customLocalKillMessageBuf_, settings_.customLocalKillMessage.c_str(), sizeof(customLocalKillMessageBuf_) - 1);
    customLocalKillMessageBuf_[sizeof(customLocalKillMessageBuf_) - 1] = '\0';
    strncpy(customLocalDeathMessageBuf_, settings_.customLocalDeathMessage.c_str(), sizeof(customLocalDeathMessageBuf_) - 1);
    customLocalDeathMessageBuf_[sizeof(customLocalDeathMessageBuf_) - 1] = '\0';
    strncpy(customServerKillMessageBuf_, settings_.customServerKillMessage.c_str(), sizeof(customServerKillMessageBuf_) - 1);
    customServerKillMessageBuf_[sizeof(customServerKillMessageBuf_) - 1] = '\0';
    strncpy(customServerDeathMessageBuf_, settings_.customServerDeathMessage.c_str(), sizeof(customServerDeathMessageBuf_) - 1);
    customServerDeathMessageBuf_[sizeof(customServerDeathMessageBuf_) - 1] = '\0';
}

void KillTrackerUI::Render() {
    UIComponents::BeginSubTabs("KillTrackerTabs");
    if (UIComponents::SubTabItem("Statistiken", selectedTab_ == 0)) {
        selectedTab_ = 0;
    }
    if (UIComponents::SubTabItem("Verlauf", selectedTab_ == 1)) {
        selectedTab_ = 1;
    }
    if (UIComponents::SubTabItem("Einstellungen", selectedTab_ == 2)) {
        selectedTab_ = 2;
    }
    UIComponents::EndSubTabs();

    if (selectedTab_ == 0) {
        RenderStatsTab();
    } else if (selectedTab_ == 1) {
        RenderHistoryTab();
    } else if (selectedTab_ == 2) {
        RenderSettingsTab();
    }
}

void KillTrackerUI::RenderStatsTab() {
    auto& theme = ThemeManager::Instance();

    if (!bridge_) {
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Danger), "Killzähler nicht initialisiert");
        return;
    }

    auto* tracker = bridge_->GetKillTracker();
    if (!tracker) {
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Danger), "Killzähler nicht verfügbar");
        return;
    }

    // PERF: Cache all stats ONCE per frame instead of calling getters multiple times
    cachedStats_ = tracker->GetAllWeaponStats();
    cachedTotalDamage_ = 0.0f;
    cachedTotalKills_ = tracker->GetTotalKills();
    for (const auto& stat : cachedStats_) {
        cachedTotalDamage_ += stat.totalDamage;
    }
    
    // Sort once for the table
    std::sort(cachedStats_.begin(), cachedStats_.end(), [](const auto& a, const auto& b) {
        if (a.killCount != b.killCount) {
            return a.killCount > b.killCount;
        }
        return a.totalDamage > b.totalDamage;
    });

    RenderSummaryCards();
    ImGui::Dummy(ImVec2(0.0f, ThemeManager::Instance().GetSpacing(ThemeSpacingRole::CardGap)));
    RenderWeaponStatsTable();
}

void KillTrackerUI::RenderSummaryCards() {
    auto* tracker = bridge_ != nullptr ? bridge_->GetKillTracker() : nullptr;
    auto& theme = ThemeManager::Instance();

    const float totalDamage = cachedTotalDamage_;
    const int totalKills = cachedTotalKills_;
    const int totalDeaths = tracker != nullptr ? tracker->GetTotalDeaths() : 0;
    const float kd = totalDeaths > 0 ? static_cast<float>(totalKills) / totalDeaths : static_cast<float>(totalKills);
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float metricWidth = (ImGui::GetContentRegionAvail().x - gap * 3.0f) / 4.0f;
    const ImVec4 successColor = theme.GetColor(ThemeColorRole::Success);
    const ImVec4 dangerColor = theme.GetColor(ThemeColorRole::Danger);

    UIComponents::MetricCard("KillsCard", "Kills", std::to_string(totalKills), "", &successColor, nullptr, metricWidth);
    ImGui::SameLine();
    UIComponents::MetricCard("DeathsCard", "Tode", std::to_string(totalDeaths), "", &dangerColor, nullptr, metricWidth);
    ImGui::SameLine();

    char kdBuffer[32] = {};
    std::snprintf(kdBuffer, sizeof(kdBuffer), "%.2f", kd);
    UIComponents::MetricCard("KDCard", "K/D", kdBuffer, "", nullptr, nullptr, metricWidth);
    ImGui::SameLine();

    UIComponents::MetricCard("DamageCard", "Schaden", FormatDamage(totalDamage), "", nullptr, nullptr, metricWidth);
}

void KillTrackerUI::RenderWeaponStatsTable() {
    UIComponents::SectionHeader("Waffen-Statistiken", "Kills, Schaden und Genauigkeit pro Waffe");

    // PERF: Use cached and pre-sorted stats
    const auto& stats = cachedStats_;
    float totalDamage = cachedTotalDamage_;

    if (stats.empty()) {
        UIComponents::BeginCard("WeaponStatsEmpty", 0.0f, false);
        UIComponents::CardHeader("Waffen-Statistiken", "Sobald Daten vorhanden sind, erscheinen hier pro Waffe kompakte Leistungs-Karten");
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeManager::Instance().GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Noch keine Kills aufgezeichnet. Spiele eine Runde, um Schadensanteil, Kills und Leistung pro Waffe zu vergleichen.");
        ImGui::PopStyleColor();
        UIComponents::EndCard();
        return;
    }

    float windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    ImGuiStyle& style = ImGui::GetStyle();
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    float cardWidth = (availableWidth - style.ItemSpacing.x) * 0.5f;
    if (cardWidth < 280.0f) {
        cardWidth = availableWidth;
    }
    
    for (size_t i = 0; i < stats.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        
        RenderWeaponStatCard(stats[i], static_cast<int>(i), totalDamage, cardWidth);
        
        float lastCardX2 = ImGui::GetItemRectMax().x;
        float nextCardX2 = lastCardX2 + style.ItemSpacing.x + cardWidth; // Expected position if next card was on same line
        
        if (i + 1 < stats.size() && nextCardX2 < windowVisibleX2) {
            ImGui::SameLine();
        }
        
        ImGui::PopID();
    }
}

void KillTrackerUI::RenderWeaponStatCard(const killtracker::WeaponStats& stats, int index, float totalDamage, float cardWidth) {
    auto& theme = ThemeManager::Instance();
    
    UIComponents::BeginCard("WeaponCard", cardWidth, true);
    
    std::string weaponName = killtracker::GetWeaponName(stats.weaponId);
    UIComponents::CardHeader(weaponName.c_str());
    
    // Stats rows
    std::string killsStr = std::to_string(stats.killCount);
    UIComponents::InfoRow("Kills", killsStr.c_str());
    
    std::string dmgStr = FormatDamage(stats.totalDamage);
    UIComponents::InfoRow("Schaden", dmgStr.c_str());
    
    float avgDamage = stats.killCount > 0 ? stats.totalDamage / stats.killCount : 0.0f;
    char avgBuf[32];
    snprintf(avgBuf, sizeof(avgBuf), "%.1f", avgDamage);
    UIComponents::InfoRow("Ø Schaden/Kill", avgBuf);
    
    UIComponents::EndCard();
}

void KillTrackerUI::RenderSettingsTab() {
    UIComponents::SectionHeader("Killzähler Einstellungen", "Konfiguriere Tracking-Verhalten und Nachrichten");

    RenderGeneralSettings();
    UIComponents::Separator();
    RenderMessageSettings();
    UIComponents::Separator();
    RenderDataManagement();
}

void KillTrackerUI::RenderGeneralSettings() {
    UIComponents::SectionHeader("Allgemein");

    // Enable toggle
    if (UIComponents::Toggle("Killz\xC3\xA4hler aktivieren", &settings_.enabled)) {
        if (bridge_) {
            bridge_->SetKillTrackerEnabled(settings_.enabled);
        }
        if (OnSettingsChanged) OnSettingsChanged();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Killz\xC3\xA4hler ein-/ausschalten");
    }

    ImGui::Spacing();

    // Track damage
    bool countersChanged = UIComponents::Toggle("Kills erfassen", &settings_.countKills);
    countersChanged |= UIComponents::Toggle("Tode erfassen", &settings_.countDeaths);
    if (countersChanged && OnSettingsChanged) OnSettingsChanged();

    if (UIComponents::Toggle("Verursachten Schaden tracken", &settings_.trackDamage)) {
        if (bridge_) {
            bridge_->SetKillTrackerTrackDamage(settings_.trackDamage);
        }
        if (OnSettingsChanged) OnSettingsChanged();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Schaden aus RPC-Paketen tracken (experimentell)");
    }
}

void KillTrackerUI::RenderMessageSettings() {
    auto& theme = ThemeManager::Instance();

    UIComponents::SectionHeader("Nachrichten");
    if (UIComponents::GhostButton("Variablen##KillTrackerMessages")) {
        showMessageVariables_ = true;
    }
    RenderMessageVariablesModal("Verfügbare Variablen##KillTrackerMessages", &showMessageVariables_, true);

    if (UIComponents::Toggle("Lokale Kill-Nachricht anzeigen", &settings_.showLocalKillMessage)) {
        if (bridge_) {
            bridge_->SetKillTrackerLocalKillMessageEnabled(settings_.showLocalKillMessage);
        }
        if (OnSettingsChanged) OnSettingsChanged();
    }

    ImGui::TextColored(theme.GetColor(ThemeColorRole::Warning), "Lokale Kill-Nachricht");
    
    int localKillLen = static_cast<int>(strlen(customLocalKillMessageBuf_));
    std::string strLocalKill = customLocalKillMessageBuf_;
    if (UIComponents::InputText("##customlocalkillmessage", &strLocalKill)) {
        strncpy(customLocalKillMessageBuf_, strLocalKill.c_str(), sizeof(customLocalKillMessageBuf_) - 1);
        customLocalKillMessageBuf_[sizeof(customLocalKillMessageBuf_) - 1] = '\0';
    }
    
    ImVec4 localKillLenColor = localKillLen > 144 ? theme.GetColor(ThemeColorRole::Danger) : theme.GetColor(ThemeColorRole::TextMuted);
    ImGui::TextColored(localKillLenColor, "%d / 144 Zeichen", localKillLen);

    ImGui::PushID("LocalKillActions");
    if (UIComponents::PrimaryButton("Übernehmen")) {
        settings_.customLocalKillMessage = customLocalKillMessageBuf_;
        if (bridge_) bridge_->SetKillTrackerLocalKillMessageTemplate(settings_.customLocalKillMessage);
        if (OnSettingsChanged) OnSettingsChanged();
    }
    ImGui::SameLine();
    if (UIComponents::SecondaryButton("Standard")) {
        settings_.customLocalKillMessage = kDefaultLocalKillMessageTemplate;
        strncpy(customLocalKillMessageBuf_, settings_.customLocalKillMessage.c_str(), sizeof(customLocalKillMessageBuf_) - 1);
        customLocalKillMessageBuf_[sizeof(customLocalKillMessageBuf_) - 1] = '\0';
        if (bridge_) bridge_->SetKillTrackerLocalKillMessageTemplate(settings_.customLocalKillMessage);
        if (OnSettingsChanged) OnSettingsChanged();
    }
    ImGui::PopID();

    ImGui::Spacing();

    UIComponents::InfoRow("Todeszähler", "Jeder erfasste Tod wird automatisch nur lokal gemeldet.");
    ImGui::Spacing();

    if (UIComponents::Toggle("Kill-Nachricht an Server senden", &settings_.sendServerKillMessage)) {
        if (bridge_) {
            bridge_->SetKillTrackerServerKillMessageEnabled(settings_.sendServerKillMessage);
        }
        if (OnSettingsChanged) OnSettingsChanged();
    }

    ImGui::TextColored(theme.GetColor(ThemeColorRole::Info), "Server Kill-Nachricht");
    
    int serverKillLen = static_cast<int>(strlen(customServerKillMessageBuf_));
    std::string strServerKill = customServerKillMessageBuf_;
    if (UIComponents::InputText("##customserverkillmessage", &strServerKill)) {
        strncpy(customServerKillMessageBuf_, strServerKill.c_str(), sizeof(customServerKillMessageBuf_) - 1);
        customServerKillMessageBuf_[sizeof(customServerKillMessageBuf_) - 1] = '\0';
    }
    
    ImVec4 serverKillLenColor = serverKillLen > 144 ? theme.GetColor(ThemeColorRole::Danger) : theme.GetColor(ThemeColorRole::TextMuted);
    ImGui::TextColored(serverKillLenColor, "%d / 144 Zeichen", serverKillLen);

    ImGui::PushID("ServerKillActions");
    if (UIComponents::PrimaryButton("Übernehmen")) {
        settings_.customServerKillMessage = customServerKillMessageBuf_;
        if (bridge_) bridge_->SetKillTrackerServerKillMessageTemplate(settings_.customServerKillMessage);
        if (OnSettingsChanged) OnSettingsChanged();
    }
    ImGui::SameLine();
    if (UIComponents::SecondaryButton("Standard")) {
        settings_.customServerKillMessage = "Kill in {city}, {bezirk}. Meine Kills: {total_kills}";
        strncpy(customServerKillMessageBuf_, settings_.customServerKillMessage.c_str(), sizeof(customServerKillMessageBuf_) - 1);
        customServerKillMessageBuf_[sizeof(customServerKillMessageBuf_) - 1] = '\0';
        if (bridge_) bridge_->SetKillTrackerServerKillMessageTemplate(settings_.customServerKillMessage);
        if (OnSettingsChanged) OnSettingsChanged();
    }
    ImGui::PopID();

    ImGui::Spacing();

    ImGui::TextColored(theme.GetColor(ThemeColorRole::Info), "Verf\xC3\xBCgbare Variablen:");
    ImGui::Columns(2, "vars", false);
    for (const auto& variable : kCombatMessageVariables) {
        ImGui::TextDisabled("{%s}", variable.tag); ImGui::NextColumn();
        ImGui::TextDisabled("%s", variable.description); ImGui::NextColumn();
    }
    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::TextColored(theme.GetColor(ThemeColorRole::Warning), "Farben: {RRGGBB} nur lokal. Server-Nachrichten senden keine Farbcodes. Max. 144 Zeichen.");
}

void KillTrackerUI::RenderDataManagement() {
    auto& theme = ThemeManager::Instance();

    UIComponents::SectionHeader("Datenverwaltung");

    // Export button
    if (UIComponents::GhostButton("Stats in Zwischenablage exportieren")) {
        if (bridge_ && bridge_->GetKillTracker()) {
            auto stats = bridge_->GetKillTracker()->GetAllWeaponStats();
            std::stringstream ss;
            ss << "=== Killzähler Stats ===\n";
            ss << "Kills gesamt: " << GetTotalKills() << "\n";
            ss << "Schaden gesamt: " << FormatDamage(GetTotalDamage()) << "\n\n";
            ss << "Waffen-Stats:\n";
            for (const auto& stat : stats) {
                if (stat.killCount > 0 || stat.totalDamage > 0) {
                    ss << "  " << killtracker::GetWeaponName(stat.weaponId) 
                       << ": " << stat.killCount << " Kills, " 
                       << FormatDamage(stat.totalDamage) << " Schaden\n";
                }
            }
            ImGui::SetClipboardText(ss.str().c_str());
            ImGui::OpenPopup("Kopiert!");
        }
    }

    if (ImGui::BeginPopupModal("Kopiert!", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Stats in Zwischenablage kopiert!");
        if (UIComponents::PrimaryButton("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Import button (placeholder - would need file dialog)
    if (UIComponents::GhostButton("Stats importieren")) {
        ImGui::OpenPopup("Importieren");
    }

    if (ImGui::BeginPopupModal("Importieren", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(theme.GetColor(ThemeColorRole::TextSecondary), "Import-Funktion kommt bald!");
        if (UIComponents::PrimaryButton("Schließen", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
    ImGui::TextWrapped("Setzt alle gespeicherten Kills, Tode und Waffenstatistiken des Killzählers zurück.");
    ImGui::PopStyleColor();
    if (UIComponents::GhostButton("Alle Stats zurücksetzen", ImVec2(0.0f, 0.0f))) {
        ImGui::OpenPopup("Stats zurücksetzen bestätigen");
    }

    if (ImGui::BeginPopupModal("Stats zurücksetzen bestätigen", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Möchtest du wirklich alle Statistiken zurücksetzen?");
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Warning), "Diese Aktion kann nicht rückgängig gemacht werden!");
        UIComponents::Separator();

        if (UIComponents::DangerButton("Ja, zurücksetzen", ImVec2(120, 0))) {
            if (bridge_ && bridge_->GetKillTracker()) {
                bridge_->GetKillTracker()->ResetStats();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (UIComponents::SecondaryButton("Abbrechen", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();

    // Manual edits toggle
    if (UIComponents::Toggle("Manuelle Stat-Bearbeitung erlauben", &settings_.allowManualEdits)) {
        if (OnSettingsChanged) OnSettingsChanged();
    }

    // Manual edit section
    if (settings_.allowManualEdits) {
        static bool openKillsSetPopup = false;
        static bool openDeathsSetPopup = false;
        static bool openAddKillsPopup = false;

        ImGui::Spacing();
        UIComponents::BeginCard("ManualEditCard", 0.0f, false);
        UIComponents::CardHeader("Manuelle Bearbeitung", "Setze Werte direkt oder f\xC3\xBCge Kills hinzu");

        // Warning note
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::Warning));
        ImGui::TextWrapped("Warnung: Manuelle Bearbeitung kann die Genauigkeit der Statistiken beeintr\xC3\xA4" "chtigen.");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Get current stats
        int currentKills = 0;
        int currentDeaths = 0;
        if (bridge_ && bridge_->GetKillTracker()) {
            currentKills = bridge_->GetKillTracker()->GetTotalKills();
            currentDeaths = bridge_->GetKillTracker()->GetTotalDeaths();
        }

        // Stats display row
        float cardWidth = ImGui::GetContentRegionAvail().x;
        float statBoxWidth = (cardWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        // Kills box
        UIComponents::BeginCard("KillsStatBox", statBoxWidth, false);
        ImGui::TextColored(theme.GetColor(ThemeColorRole::TextSecondary), "Kills gesamt");
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Success), "%d", currentKills);
        ImGui::SetWindowFontScale(1.0f);
        if (UIComponents::SecondaryButton("Setzen##Kills", ImVec2(-1.0f, 0.0f))) {
            openKillsSetPopup = true;
        }
        UIComponents::EndCard();

        ImGui::SameLine();

        // Deaths box
        UIComponents::BeginCard("DeathsStatBox", statBoxWidth, false);
        ImGui::TextColored(theme.GetColor(ThemeColorRole::TextSecondary), "Tode gesamt");
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Danger), "%d", currentDeaths);
        ImGui::SetWindowFontScale(1.0f);
        if (UIComponents::SecondaryButton("Setzen##Deaths", ImVec2(-1.0f, 0.0f))) {
            openDeathsSetPopup = true;
        }
        UIComponents::EndCard();

        ImGui::Spacing();

        // Add kills button (full width)
        if (UIComponents::PrimaryButton("Kills manuell hinzuf\xC3\xBCgen", ImVec2(-1.0f, 0.0f))) {
            openAddKillsPopup = true;
        }

        UIComponents::EndCard();

        if (openKillsSetPopup) {
            ImGui::OpenPopup("Kills setzen");
            openKillsSetPopup = false;
        }

        if (openDeathsSetPopup) {
            ImGui::OpenPopup("Tode setzen");
            openDeathsSetPopup = false;
        }

        if (openAddKillsPopup) {
            ImGui::OpenPopup("Kills hinzufügen");
            openAddKillsPopup = false;
        }

        // Kills set popup
        if (ImGui::BeginPopupModal("Kills setzen", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static int editedKills = 0;

            if (ImGui::IsWindowAppearing()) {
                editedKills = currentKills;
            }

            UIComponents::InputInt("Kills gesamt", &editedKills);
            if (editedKills < 0) {
                editedKills = 0;
            }

            ImGui::PushID("SaveTotalKillsModal");
            if (UIComponents::PrimaryButton("Speichern")) {
                if (bridge_) {
                    bridge_->ImportKillCount(editedKills);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UIComponents::SecondaryButton("Abbrechen")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::EndPopup();
        }

        // Deaths set popup
        if (ImGui::BeginPopupModal("Tode setzen", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static int editedDeaths = 0;

            if (ImGui::IsWindowAppearing()) {
                editedDeaths = currentDeaths;
            }

            UIComponents::InputInt("Tode gesamt", &editedDeaths);
            if (editedDeaths < 0) {
                editedDeaths = 0;
            }

            ImGui::PushID("SaveTotalDeathsModal");
            if (UIComponents::PrimaryButton("Speichern")) {
                if (bridge_) {
                    bridge_->ImportDeathCount(editedDeaths);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UIComponents::SecondaryButton("Abbrechen")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::EndPopup();
        }

        // Add kills popup
        if (ImGui::BeginPopupModal("Kills hinzufügen", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static int weaponId = 24;
            static int killCount = 1;
            static float damage = 100.0f;

            UIComponents::InputInt("Waffen-ID", &weaponId);
            UIComponents::InputInt("Kill-Anzahl", &killCount);
            UIComponents::InputFloat("Schaden", &damage);

            if (UIComponents::PrimaryButton("Hinzuf\xC3\xBCgen")) {
                // Add to stats (would need implementation)
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UIComponents::SecondaryButton("Abbrechen")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

std::string KillTrackerUI::FormatDamage(float damage) const {
    std::ostringstream oss;
    if (damage >= 1000000.0f) {
        oss << std::fixed << std::setprecision(1) << (damage / 1000000.0f) << "M";
    } else if (damage >= 1000.0f) {
        oss << std::fixed << std::setprecision(1) << (damage / 1000.0f) << "K";
    } else {
        oss << std::fixed << std::setprecision(0) << damage;
    }
    return oss.str();
}

std::string KillTrackerUI::FormatPercentage(float value, float total) const {
    if (total <= 0.0f) return "0.0%";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (value / total * 100.0f) << "%";
    return oss.str();
}

std::string KillTrackerUI::GetWeaponIcon(int weaponId) const {
    // Could return icon codes or emoji for weapons
    return "";
}

ImVec4 KillTrackerUI::GetWeaponColor(int weaponId) const {
    auto& theme = ThemeManager::Instance();
    if (weaponId <= 15) return theme.GetColor(ThemeColorRole::TextMuted);       // Melee
    if (weaponId <= 21) return theme.GetColor(ThemeColorRole::TextDisabled);    // Special
    if (weaponId <= 32) return theme.GetColor(ThemeColorRole::Warning);          // Firearms
    if (weaponId <= 38) return theme.GetColor(ThemeColorRole::Danger);           // Heavy
    if (weaponId <= 46) return theme.GetColor(ThemeColorRole::Info);             // Special weapons
    return theme.GetColor(ThemeColorRole::TextDisabled);                         // Unknown
}

std::vector<killtracker::WeaponStats> KillTrackerUI::GetSortedWeaponStats() const {
    std::vector<killtracker::WeaponStats> stats;
    
    if (!bridge_ || !bridge_->GetKillTracker()) {
        return stats;
    }

    stats = bridge_->GetKillTracker()->GetAllWeaponStats();

    // Sort by total kills (descending), then by total damage
    std::sort(stats.begin(), stats.end(), [](const auto& a, const auto& b) {
        if (a.killCount != b.killCount) {
            return a.killCount > b.killCount;
        }
        return a.totalDamage > b.totalDamage;
    });

    return stats;
}

float KillTrackerUI::GetTotalDamage() const {
    if (!bridge_ || !bridge_->GetKillTracker()) return 0.0f;
    return bridge_->GetKillTracker()->GetTotalDamage();
}

int KillTrackerUI::GetTotalKills() const {
    if (!bridge_ || !bridge_->GetKillTracker()) return 0;
    return bridge_->GetKillTracker()->GetTotalKills();
}

int KillTrackerUI::GetTotalWeapons() const {
    if (!bridge_ || !bridge_->GetKillTracker()) return 0;
    auto stats = bridge_->GetKillTracker()->GetAllWeaponStats();
    return static_cast<int>(stats.size());
}

void KillTrackerUI::RenderHistoryTab() {
    auto& theme = ThemeManager::Instance();

    if (!bridge_) {
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Danger), "Killzähler nicht initialisiert");
        return;
    }

    auto* tracker = bridge_->GetKillTracker();
    if (!tracker) {
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Danger), "Killzähler nicht verfügbar");
        return;
    }

    float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    UIComponents::BeginCardFillHeight("KillHistoryPanel", halfWidth, false);
    UIComponents::CardHeader("Letzte Kills", "Deine letzten 50 Kills");
    ImGui::BeginChild("KillHistoryScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_None);
    RenderKillHistoryTable();
    ImGui::EndChild();
    UIComponents::EndCard();

    ImGui::SameLine();

    UIComponents::BeginCardFillHeight("DeathHistoryPanel", halfWidth, false);
    UIComponents::CardHeader("Letzte Tode", "Deine letzten 50 Tode");
    ImGui::BeginChild("DeathHistoryScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_None);
    RenderDeathHistoryTable();
    ImGui::EndChild();
    UIComponents::EndCard();
}

void KillTrackerUI::RenderKillHistoryTable() {
    // Table header
    ImGui::Columns(4, "killhistorycolumns");
    ImGui::Separator();

    ImGui::Text("Zeit"); ImGui::NextColumn();
    ImGui::Text("Opfer"); ImGui::NextColumn();
    ImGui::Text("Waffe"); ImGui::NextColumn();
    ImGui::Text("Schaden"); ImGui::NextColumn();

    ImGui::Separator();

    // Get recent kills from tracker
    auto recentKills = bridge_->GetKillTracker()->GetRecentKills();

    // Sort by timestamp descending (most recent first)
    std::sort(recentKills.begin(), recentKills.end(), [](const auto& a, const auto& b) {
        return a.timestamp > b.timestamp;
    });

    int index = 0;
    for (const auto& kill : recentKills) {
        RenderKillHistoryRow(kill, index++);
    }

    ImGui::Columns(1);
    ImGui::Separator();

    if (recentKills.empty()) {
        ImGui::TextColored(ThemeManager::Instance().GetColor(ThemeColorRole::TextMuted),
            "Noch keine Kills aufgezeichnet. Töte Spieler, um deine Kill-Historie zu sehen!");
    }
}

void KillTrackerUI::RenderDeathHistoryTable() {
    auto& theme = ThemeManager::Instance();

    // Table header
    ImGui::Columns(3, "deathhistorycolumns");
    ImGui::Separator();

    ImGui::Text("Zeit"); ImGui::NextColumn();
    ImGui::Text("Killer"); ImGui::NextColumn();
    ImGui::Text("Waffe"); ImGui::NextColumn();

    ImGui::Separator();

    auto recentDeaths = bridge_->GetKillTracker()->GetRecentDeaths();

    // Sort by timestamp descending (most recent first)
    std::sort(recentDeaths.begin(), recentDeaths.end(), [](const auto& a, const auto& b) {
        return a.timestamp > b.timestamp;
    });

    int index = 0;
    for (const auto& death : recentDeaths) {
        ImGui::PushID(1000 + index);

        std::time_t now = std::time(nullptr);
        double diff = std::difftime(now, death.timestamp);
        std::string timeStr;
        if (diff < 60) {
            timeStr = "Gerade";
        } else if (diff < 3600) {
            timeStr = std::to_string(static_cast<int>(diff / 60)) + " min";
        } else if (diff < 86400) {
            timeStr = std::to_string(static_cast<int>(diff / 3600)) + " h";
        } else {
            timeStr = std::to_string(static_cast<int>(diff / 86400)) + " d";
        }
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Danger), "%s", timeStr.c_str());
        ImGui::NextColumn();

        if (death.killer.empty()) {
            ImGui::TextDisabled("Unbekannt");
        } else {
            ImGui::Text("%s", death.killer.c_str());
        }
        ImGui::NextColumn();

        ImGui::Text("%s", killtracker::GetWeaponName(death.weaponId).c_str());
        ImGui::NextColumn();

        ImGui::PopID();
        index++;
    }

    ImGui::Columns(1);
    ImGui::Separator();

    if (recentDeaths.empty()) {
        ImGui::TextColored(theme.GetColor(ThemeColorRole::TextMuted),
            "Noch keine Tode aufgezeichnet.");
    }
}

void KillTrackerUI::RenderKillHistoryRow(const killtracker::KillEvent& kill, int index) {
    ImGui::PushID(index);

    // Format timestamp as relative time (e.g., "2 min ago")
    std::time_t now = std::time(nullptr);
    double diff = std::difftime(now, kill.timestamp);
    std::string timeStr;
    if (diff < 60) {
        timeStr = "Gerade";
    } else if (diff < 3600) {
        timeStr = std::to_string(static_cast<int>(diff / 60)) + " min";
    } else if (diff < 86400) {
        timeStr = std::to_string(static_cast<int>(diff / 3600)) + " h";
    } else {
        timeStr = std::to_string(static_cast<int>(diff / 86400)) + " d";
    }
    ImGui::Text("%s", timeStr.c_str());
    ImGui::NextColumn();

    // Victim name
    ImGui::Text("%s", kill.victim.c_str());
    ImGui::NextColumn();

    // Weapon name
    ImGui::Text("%s", killtracker::GetWeaponName(kill.weaponId).c_str());
    ImGui::NextColumn();

    // Damage dealt
    if (kill.damageDealt > 0.0f) {
        ImGui::Text("%.1f", kill.damageDealt);
    } else {
        ImGui::TextDisabled("-");
    }
    ImGui::NextColumn();

    ImGui::PopID();
}

} // namespace keybinder
