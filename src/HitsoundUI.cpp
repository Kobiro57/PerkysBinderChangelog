#include "../include/HitsoundUI.h"

#include "../include/HitsoundPlayer.h"
#include "../include/PersistentPaths.h"
#include "../include/ThemeManager.h"
#include "../include/UIComponents.h"
#include "../include/imgui-1.92.7/imgui.h"

#include <algorithm>
#include <string>
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

std::string GetFileNameFromPath(const std::string& path) {
    const std::size_t slashPos = path.find_last_of("\\/");
    if (slashPos == std::string::npos) {
        return path;
    }

    return path.substr(slashPos + 1);
}

bool IsEmbeddedDefaultSelected(const std::string& path) {
    return path.empty();
}

} // namespace

void HitsoundUI::SetSettingsManager(SettingsManager* manager) {
    settingsManager_ = manager;
    RefreshAvailableWavs();
}

void HitsoundUI::SetHitsoundPlayer(HitsoundPlayer* player) {
    hitsoundPlayer_ = player;
}

void HitsoundUI::RefreshAvailableWavs() {
    availableWavs_.clear();

    const std::string dir = PersistentPaths::HitsoundsDir();
    if (dir.empty()) {
        return;
    }

    WIN32_FIND_DATAA findData = {};
    const std::string searchPattern = dir + "\\*";
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        const std::string fileName = findData.cFileName;
        const std::size_t len = fileName.size();
        if (len > 4) {
            const std::string ext = fileName.substr(len - 4);
            if (ext == ".wav" || ext == ".WAV" || ext == ".Wav") {
                availableWavs_.push_back("hitsounds\\" + fileName);
            }
        }
    } while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);

    std::sort(availableWavs_.begin(), availableWavs_.end(),
        [](const std::string& a, const std::string& b) {
            return GetFileNameFromPath(a) < GetFileNameFromPath(b);
        });
}

void HitsoundUI::Render() {
    if (settingsManager_ == nullptr) {
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::Danger), "Hitsound UI nicht initialisiert");
        return;
    }

    Settings& settings = settingsManager_->GetMutable();
    bool changed = false;

    if (availableWavs_.empty()) {
        RefreshAvailableWavs();
    }

    UIComponents::BeginCard("hitsound_outgoing", 0.0f);
    UIComponents::CardHeader("Ausgehende Treffer", "Sound beim Zufügen von Schaden");

    if (UIComponents::Toggle("Ausgehenden Hitsound aktivieren", &settings.hitsound_outgoing_enabled)) {
        changed = true;
    }

    float outgoingVolumePercent = settings.hitsound_outgoing_volume * 100.0f;
    if (UIComponents::SliderFloat("Lautstärke##hitsound_outgoing", &outgoingVolumePercent, 0.0f, 100.0f, "%.0f%%")) {
        settings.hitsound_outgoing_volume = outgoingVolumePercent / 100.0f;
        changed = true;
    }

    ImGui::Spacing();
    if (UIComponents::SecondaryButton("Standard##hitsound_outgoing", ImVec2(0.0f, 0.0f), IsEmbeddedDefaultSelected(settings.hitsound_outgoing_path))) {
        settings.hitsound_outgoing_path.clear();
        changed = true;
    }

    ImGui::SameLine();
    if (UIComponents::GhostButton("Vorschau##hitsound_outgoing")) {
        if (hitsoundPlayer_ != nullptr) {
            hitsoundPlayer_->PreviewOutgoing();
        }
    }

    UIComponents::EndCard();

    ImGui::Spacing();

    UIComponents::BeginCard("hitsound_incoming", 0.0f);
    UIComponents::CardHeader("Eingehende Treffer", "Sound beim Empfang von Schaden");

    if (UIComponents::Toggle("Eingehenden Hitsound aktivieren", &settings.hitsound_incoming_enabled)) {
        changed = true;
    }

    float incomingVolumePercent = settings.hitsound_incoming_volume * 100.0f;
    if (UIComponents::SliderFloat("Lautstärke##hitsound_incoming", &incomingVolumePercent, 0.0f, 100.0f, "%.0f%%")) {
        settings.hitsound_incoming_volume = incomingVolumePercent / 100.0f;
        changed = true;
    }

    ImGui::Spacing();
    if (UIComponents::SecondaryButton("Standard##hitsound_incoming", ImVec2(0.0f, 0.0f), IsEmbeddedDefaultSelected(settings.hitsound_incoming_path))) {
        settings.hitsound_incoming_path.clear();
        changed = true;
    }

    ImGui::SameLine();
    if (UIComponents::GhostButton("Vorschau##hitsound_incoming")) {
        if (hitsoundPlayer_ != nullptr) {
            hitsoundPlayer_->PreviewIncoming();
        }
    }

    UIComponents::EndCard();

    ImGui::Spacing();
    UIComponents::Separator("WAV-Auswahl");
    ImGui::Spacing();

    if (UIComponents::SecondaryButton("Ordner neu scannen##hitsound_library")) {
        RefreshAvailableWavs();
    }

    ImGui::Spacing();

    if (availableWavs_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme().GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Keine WAV-Dateien im Ordner 'hitsounds/' gefunden.");
        ImGui::PopStyleColor();
    } else {
        for (const auto& wavPath : availableWavs_) {
            const std::string fileName = GetFileNameFromPath(wavPath);
            const bool isOutgoingSelected = settings.hitsound_outgoing_path == wavPath;
            const bool isIncomingSelected = settings.hitsound_incoming_path == wavPath;

            ImGui::PushID(wavPath.c_str());

            UIComponents::BeginCard("hitsound_wav_item", 0.0f, isOutgoingSelected || isIncomingSelected);

            ImGui::PushStyleColor(ImGuiCol_Text, Theme().GetColor(ThemeColorRole::TextPrimary));
            ImGui::TextUnformatted(fileName.c_str());
            ImGui::PopStyleColor();

            if (isOutgoingSelected) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, Theme().GetColor(ThemeColorRole::Accent));
                ImGui::TextUnformatted("(Ausgehend)");
                ImGui::PopStyleColor();
            }

            if (isIncomingSelected) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, Theme().GetColor(ThemeColorRole::Success));
                ImGui::TextUnformatted("(Eingehend)");
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();

            if (UIComponents::SecondaryButton("Als ausgehend")) {
                settings.hitsound_outgoing_enabled = true;
                settings.hitsound_outgoing_path = wavPath;
                changed = true;
            }

            ImGui::SameLine();

            if (UIComponents::SecondaryButton("Als eingehend")) {
                settings.hitsound_incoming_enabled = true;
                settings.hitsound_incoming_path = wavPath;
                changed = true;
            }

            ImGui::SameLine();

            if (UIComponents::GhostButton("Vorschau")) {
                if (hitsoundPlayer_ != nullptr) {
                    // Preview the specific WAV file using the player's
                    // dedicated preview playback state so the configured
                    // outgoing/incoming slots remain untouched.
                    const bool useIncoming = isIncomingSelected && !isOutgoingSelected;
                    const HitsoundPlayer::Direction dir = useIncoming ? HitsoundPlayer::Direction::Incoming : HitsoundPlayer::Direction::Outgoing;
                    hitsoundPlayer_->PreviewPath(dir, wavPath);
                }
            }

            UIComponents::EndCard();

            ImGui::Spacing();
            ImGui::PopID();
        }
    }

    if (changed) {
        if (hitsoundPlayer_ != nullptr) {
            hitsoundPlayer_->ApplySettings(settings);
        }

        if (OnSettingsChanged) {
            OnSettingsChanged();
        }
    }
}

} // namespace keybinder
