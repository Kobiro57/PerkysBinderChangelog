#include "../include/KeybindEditorUI.h"
#include "../include/KeyInputUtils.h"
#include "../include/ThemeManager.h"
#include "../include/UIComponents.h"
#include "../include/imgui-1.92.7/imgui.h"
#include <windows.h>
#include <algorithm>
#include <cctype>

namespace keybinder {

namespace {

ThemeManager& Theme() {
    return ThemeManager::Instance();
}

void RenderMutedWrappedText(const char* text) {
    auto& theme = ThemeManager::Instance();
    const float edgeInset = (std::max)(2.0f * theme.GetAppliedScale(), 2.0f);
    ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - edgeInset);
    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
}

} // namespace

static inline float GetLayoutScale() {
    return ThemeManager::Instance().GetAppliedScale();
}

static inline float Scale(float value) {
    return value * GetLayoutScale();
}

std::string KeybindEditorUI::GetKeyName(int vk) const {
    return GetLocalizedVirtualKeyName(vk);
}

KeybindEditorUI::KeybindEditorUI()
    : m_isOpen(false)
    , m_isEditing(false)
    , m_capturingKey(false)
    , m_captureKeyFrame(-1)
    , m_setFocus(false)
    , m_showDeleteConfirm(false)
    , m_showVariables(false)
    , m_actionEditorOpen(false)
    , m_isEditingAction(false)
    , m_editingActionIndex(0)
    , m_capturingActionKey(false)
    , m_captureActionKeyFrame(-1)
    , m_capturedKeys()
    , m_captureIgnoreKeys()
    , m_actionCaptureIgnoreKeys()
    , m_multiKeyMode(false)
{
}

void KeybindEditorUI::OpenForCreate() {
    m_currentKeybind = Keybind();
    m_currentKeybind.enabled = true;
    m_isOpen = true;
    m_isEditing = false;
    m_errorMessage.clear();
    m_capturingKey = false;
    m_setFocus = true;
    m_showDeleteConfirm = false;
    m_showVariables = false;
    m_actionEditorOpen = false;
    m_originalName = "";
    m_capturedKeys.clear();
    m_multiKeyMode = false;
}

void KeybindEditorUI::OpenForEdit(const Keybind& keybind) {
    m_currentKeybind = keybind;
    m_originalName = keybind.name;
    m_isOpen = true;
    m_isEditing = true;
    m_errorMessage.clear();
    m_capturingKey = false;
    m_setFocus = false;
    m_showDeleteConfirm = false;
    m_actionEditorOpen = false;
    
    m_capturedKeys = keybind.GetAllKeys();
    m_multiKeyMode = keybind.HasMultiKeys();
}

bool KeybindEditorUI::IsOpen() const {
    return m_isOpen;
}

void KeybindEditorUI::Close() {
    m_isOpen = false;
    m_capturingKey = false;
    m_actionEditorOpen = false;
}

void KeybindEditorUI::OnSave(std::function<void(const Keybind&)> callback) {
    m_onSave = std::move(callback);
}

void KeybindEditorUI::OnCancel(std::function<void()> callback) {
    m_onCancel = std::move(callback);
}

void KeybindEditorUI::OnDelete(std::function<void(const std::string&)> callback) {
    m_onDelete = std::move(callback);
}

void KeybindEditorUI::Render() {
    if (!m_isOpen) return;

    const char* popupName = m_isEditing ? "Tastenbelegung bearbeiten##KeybindEditor" : "Neue Tastenbelegung##KeybindEditor";

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::OpenPopup(popupName);

    ImGui::SetNextWindowSize(ImVec2(Scale(600), Scale(600)), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_PopupBg, Theme().GetColor(ThemeColorRole::SurfaceElevated));
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, Theme().GetColor(ThemeColorRole::Background));

    if (ImGui::BeginPopupModal(popupName, m_capturingKey ? nullptr : &m_isOpen, ImGuiWindowFlags_NoSavedSettings)) {
        RenderMainEditor();
        ImGui::EndPopup();
    } else {
        if (m_isOpen) {
            m_isOpen = false;
            if (m_onCancel) m_onCancel();
        }
    }

    ImGui::PopStyleColor(2);
    
    if (m_actionEditorOpen) {
        RenderActionEditor();
    }
    
    if (m_showDeleteConfirm) {
        RenderDeleteConfirmation();
    }

    if (m_showVariables) {
        RenderVariablesModal();
    }
}

void KeybindEditorUI::RenderAsPanel() {
    RenderMainEditor();

    if (m_actionEditorOpen) {
        RenderActionEditor();
    }

    if (m_showDeleteConfirm) {
        RenderDeleteConfirmation();
    }

    if (m_showVariables) {
        RenderVariablesModal();
    }
}

void KeybindEditorUI::RenderMainEditor() {
    if (!m_errorMessage.empty()) {
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::Danger), "%s", m_errorMessage.c_str());
        ImGui::Separator();
    }

    UIComponents::SectionHeader("Konfiguration");

    if (m_setFocus) {
        ImGui::SetKeyboardFocusHere(1);
        m_setFocus = false;
    }
    UIComponents::InputText("Name", &m_currentKeybind.name, "Name eingeben...");
    UIComponents::InputText("Kategorie", &m_currentKeybind.category, "Z.B. Waffen, Fahrzeuge...");
    ImGui::Spacing();
    UIComponents::Toggle("Aktiviert", &m_currentKeybind.enabled);
    RenderMutedWrappedText("Deaktivierte Keybinds werden nicht ausgelöst.");
    UIComponents::Toggle("Tastendruck durchlassen", &m_currentKeybind.passthrough);
    RenderMutedWrappedText("Wenn aktiviert werden Tastendrücke mit einem Keybind auch an das Spiel gesendet.");

    UIComponents::SectionHeader("Auslösung");

    RenderKeyCaptureUI();

    UIComponents::InputText("Befehl##SlashCommand", &m_currentKeybind.slashCommand, "Z.B. heal");
    RenderMutedWrappedText("Du kannst einen Keybind entweder mit einem Hotkey und/oder einem Befehl auslösen.");

    UIComponents::SectionHeader("Aktionen");
    RenderMutedWrappedText("Füge hier Aktionen wie Befehle, Texte oder Verzögerungen hinzu. z.B kannst du hier /car lock als Befehl hinzufügen");
    if (UIComponents::SecondaryButton("+ Hinzufügen##AddAction")) {
        m_currentAction = Action();
        m_isEditingAction = false;
        m_actionEditorOpen = true;
        m_capturingActionKey = false;
    }
    ImGui::SameLine();
    if (UIComponents::GhostButton("Variablen##ShowVariables")) {
        m_showVariables = true;
    }
    
    RenderActionList();

    if (UIComponents::PrimaryButton("Speichern", ImVec2(Scale(120), 0))) {
        if (m_multiKeyMode) {
            m_currentKeybind.keys = m_capturedKeys;
            m_currentKeybind.key = m_capturedKeys.empty() ? 0 : m_capturedKeys[0];
        } else {
            m_currentKeybind.key = m_capturedKeys.empty() ? 0 : m_capturedKeys[0];
            m_currentKeybind.keys.clear();
            if (m_currentKeybind.key != 0) {
                m_currentKeybind.keys.push_back(m_currentKeybind.key);
            }
        }
        
        if (!m_currentKeybind.slashCommand.empty()) {
            if (m_currentKeybind.slashCommand[0] == '/') {
                m_currentKeybind.slashCommand = m_currentKeybind.slashCommand.substr(1);
            }

            auto trim = [](std::string& s) {
                size_t start = s.find_first_not_of(" \t\n\r");
                if (start == std::string::npos) { s = ""; return; }
                size_t end = s.find_last_not_of(" \t\n\r");
                s = s.substr(start, end - start + 1);
            };
            trim(m_currentKeybind.slashCommand);

            std::transform(m_currentKeybind.slashCommand.begin(), m_currentKeybind.slashCommand.end(), 
                          m_currentKeybind.slashCommand.begin(), ::tolower);
        }

        if (m_currentKeybind.name.empty()) {
            m_errorMessage = "Name darf nicht leer sein.";
        } else if (m_currentKeybind.enabled && m_currentKeybind.GetAllKeys().empty() && m_currentKeybind.slashCommand.empty()) {
            m_errorMessage = "Mindestens eine Taste oder ein Slash-Befehl muss gesetzt sein, wenn die Belegung aktiviert ist.";
        } else if (!m_currentKeybind.slashCommand.empty()) {
            if (m_currentKeybind.slashCommand.find(' ') != std::string::npos) {
                m_errorMessage = "Slash-Befehl darf keine Leerzeichen enthalten (nur der Befehlsname, ohne Argumente).";
            } else if (m_currentKeybind.slashCommand.length() > 32) {
                m_errorMessage = "Slash-Befehl ist zu lang (maximal 32 Zeichen).";
            } else {
                m_isOpen = false;
                if (m_onSave) m_onSave(m_currentKeybind);
            }
        } else {
            m_isOpen = false;
            if (m_onSave) m_onSave(m_currentKeybind);
        }
    }
    
    ImGui::SameLine();
    if (UIComponents::SecondaryButton("Abbrechen", ImVec2(Scale(120), 0))) {
        m_isOpen = false;
        if (m_onCancel) m_onCancel();
    }
    
    if (m_isEditing) {
        ImGui::SameLine();
        if (UIComponents::DangerButton("Löschen", ImVec2(Scale(120), 0))) {
            m_showDeleteConfirm = true;
        }
    }
}

void KeybindEditorUI::RenderActionList() {
    UIComponents::BeginCard("ActionList", 0.0f, false);
    for (size_t i = 0; i < m_currentKeybind.actions.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        
        Action& action = m_currentKeybind.actions[i];
        
        std::string desc;
        switch (action.type) {
            case ActionType::Chat: desc = "Chat: " + action.data; break;
            case ActionType::Command: desc = "Befehl: " + action.data; break;
            case ActionType::Delay: desc = "Verzögerung: " + action.data + " ms"; break;
            case ActionType::KeyPress: 
                desc = "Key: " + GetKeyName(std::atoi(action.data.empty() ? "0" : action.data.c_str())); 
                break;
        }
        
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::TextSecondary), "%s", desc.c_str());
        
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - Scale(250));
        
        if (UIComponents::GhostButton("^")) MoveActionUp(i);
        ImGui::SameLine();
        if (UIComponents::GhostButton("v")) MoveActionDown(i);
        ImGui::SameLine();
        if (UIComponents::SecondaryButton("Bearb.")) {
            m_currentAction = action;
            m_editingActionIndex = i;
            m_isEditingAction = true;
            m_actionEditorOpen = true;
            m_capturingActionKey = false;
        }
        ImGui::SameLine();
        if (UIComponents::DangerButton("Lösch.")) {
            DeleteAction(i);
        }
        
        ImGui::PopID();
    }
    UIComponents::EndCard();
}

void KeybindEditorUI::RenderActionEditor() {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::OpenPopup("Aktion bearbeiten");
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Theme().GetColor(ThemeColorRole::SurfaceElevated));
    if (ImGui::BeginPopupModal("Aktion bearbeiten", &m_actionEditorOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        const char* items[] = { "Chat-Nachricht", "Befehl", "Verzögerung", "Taste" };
        int currentItem = static_cast<int>(m_currentAction.type);
        if (UIComponents::Combo("Aktionstyp", &currentItem, items, IM_ARRAYSIZE(items))) {
            m_currentAction.type = static_cast<ActionType>(currentItem);
            m_currentAction.data = "";
        }
        
        UIComponents::Separator();
        
        switch (m_currentAction.type) {
            case ActionType::Chat:
                UIComponents::InputText("Nachricht", &m_currentAction.data, "Nachricht eingeben...");
                RenderMutedWrappedText("Variablen: {zone}, {hp}, {weste}, {carhp}, {total_kills}, {total_deaths}, {kd}");
                break;
            case ActionType::Command:
                UIComponents::InputText("Befehl", &m_currentAction.data, "Befehl eingeben (ohne /)...");
                RenderMutedWrappedText("Variablen: {zone}, {hp}, {weste}, {carhp}, {total_kills}, {total_deaths}, {kd}");
                break;
            case ActionType::Delay:
                UIComponents::InputText("Millisekunden", &m_currentAction.data, "Z.B. 500");
                break;
            case ActionType::KeyPress: {
                int vk = std::atoi(m_currentAction.data.empty() ? "0" : m_currentAction.data.c_str());
                ImGui::TextColored(Theme().GetColor(ThemeColorRole::TextSecondary), "Taste:");
                ImGui::SameLine();
                if (m_capturingActionKey) {
            UIComponents::PrimaryButton("Taste drücken...", ImVec2(Scale(250), 0));

                    if (ImGui::GetFrameCount() > m_captureActionKeyFrame + 1) {
                        const int new_vk = FindPressedVirtualKey(m_actionCaptureIgnoreKeys);
                        if (new_vk == VK_ESCAPE) {
                            m_capturingActionKey = false;
                        } else if (new_vk != 0) {
                            m_currentAction.data = std::to_string(new_vk);
                            m_capturingActionKey = false;
                        }
                    }
                } else {
                    if (UIComponents::SecondaryButton(GetKeyName(vk).c_str(), ImVec2(Scale(250), 0))) {
                        m_capturingActionKey = true;
                        m_captureActionKeyFrame = ImGui::GetFrameCount();
                        CollectPressedVirtualKeys(m_actionCaptureIgnoreKeys);
                    }
                }
                break;
            }
        }
        
        UIComponents::Separator();
        if (UIComponents::PrimaryButton("Übernehmen", ImVec2(Scale(120), 0))) {
            if (m_isEditingAction) {
                m_currentKeybind.actions[m_editingActionIndex] = m_currentAction;
            } else {
                m_currentKeybind.actions.push_back(m_currentAction);
            }
            m_actionEditorOpen = false;
        }
        ImGui::SameLine();
        if (UIComponents::SecondaryButton("Abbrechen", ImVec2(Scale(120), 0))) {
            m_actionEditorOpen = false;
        }
        
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

void KeybindEditorUI::MoveActionUp(size_t index) {
    if (index > 0 && index < m_currentKeybind.actions.size()) {
        std::swap(m_currentKeybind.actions[index], m_currentKeybind.actions[index - 1]);
    }
}

void KeybindEditorUI::MoveActionDown(size_t index) {
    if (index < m_currentKeybind.actions.size() - 1) {
        std::swap(m_currentKeybind.actions[index], m_currentKeybind.actions[index + 1]);
    }
}

void KeybindEditorUI::DeleteAction(size_t index) {
    if (index < m_currentKeybind.actions.size()) {
        m_currentKeybind.actions.erase(m_currentKeybind.actions.begin() + index);
    }
}

void KeybindEditorUI::RenderDeleteConfirmation() {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::OpenPopup("Löschen bestätigen##DeleteConfirm");
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Theme().GetColor(ThemeColorRole::SurfaceElevated));
    if (ImGui::BeginPopupModal("Löschen bestätigen##DeleteConfirm", &m_showDeleteConfirm, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::TextPrimary), "Soll die Tastenbelegung \"%s\" wirklich gelöscht werden?", m_originalName.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (UIComponents::DangerButton("Ja, löschen", ImVec2(Scale(140), 0))) {
            m_showDeleteConfirm = false;
            m_isOpen = false;
            if (m_onDelete) m_onDelete(m_originalName);
        }

        ImGui::SameLine();
        if (UIComponents::SecondaryButton("Abbrechen", ImVec2(Scale(140), 0))) {
            m_showDeleteConfirm = false;
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

void KeybindEditorUI::RenderVariablesModal() {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(Scale(580), Scale(500)), ImGuiCond_Appearing);
    ImGui::OpenPopup("Verfügbare Variablen##Variables");
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Theme().GetColor(ThemeColorRole::SurfaceElevated));
    if (ImGui::BeginPopupModal("Verfügbare Variablen##Variables", &m_showVariables, ImGuiWindowFlags_NoSavedSettings)) {
        RenderMutedWrappedText("Verwende diese Variablen in Chat-Nachrichten oder Befehlen. Sie werden zur Laufzeit durch die aktuellen Werte ersetzt.");
        ImGui::Spacing();

        UIComponents::BeginCard("VariableList", 0.0f, false);

        auto renderVar = [](const char* tag, const char* description) {
            ImGui::TextColored(Theme().GetColor(ThemeColorRole::Accent), "{%s}", tag);
            ImGui::SameLine(Scale(140));
            ImGui::TextColored(Theme().GetColor(ThemeColorRole::TextSecondary), "%s", description);
        };

        renderVar("zone", "Aktueller Zonenname");
        renderVar("hp", "Aktuelle Lebenspunkte");
        renderVar("weste", "Aktuelle Weste (Rüstung)");
        renderVar("carhp", "Fahrzeug-Lebenspunkte (0 wenn nicht im Fahrzeug)");
        renderVar("carname", "Fahrzeugname (\"Zu Fuß\" wenn nicht im Fahrzeug)");
        renderVar("total_kills", "Gesamtanzahl Kills");
        renderVar("total_deaths", "Gesamtanzahl Tode");
        renderVar("kd", "Kill/Death-Verhältnis");

        UIComponents::EndCard();

        ImGui::Spacing();
        if (UIComponents::PrimaryButton("Schließen", ImVec2(Scale(120), 0))) {
            m_showVariables = false;
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

void KeybindEditorUI::RenderKeyCaptureUI() {
    if (UIComponents::Toggle("Mehrere Tasten", &m_multiKeyMode)) {
        if (!m_multiKeyMode && m_capturedKeys.size() > 1) {
            int firstKey = m_capturedKeys.empty() ? 0 : m_capturedKeys[0];
            m_capturedKeys.clear();
            if (firstKey != 0) {
                m_capturedKeys.push_back(firstKey);
            }
        }
    }
    
    if (m_multiKeyMode) {
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::TextSecondary), "Erfasste Tasten:");

        UIComponents::BeginCard("CapturedKeys", 0.0f, false);
        for (size_t i = 0; i < m_capturedKeys.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            
            std::string keyName = GetKeyName(m_capturedKeys[i]);
            ImGui::TextColored(Theme().GetColor(ThemeColorRole::Accent), "[%s]", keyName.c_str());
            ImGui::SameLine();
            
            if (UIComponents::GhostButton("X##RemoveKey")) {
                RemoveKey(i);
            }
            
            if (i < m_capturedKeys.size() - 1) {
                ImGui::SameLine();
                ImGui::TextColored(Theme().GetColor(ThemeColorRole::TextMuted), "+");
            }
            
            ImGui::PopID();
        }
        UIComponents::EndCard();

        if (m_capturingKey) {
            UIComponents::PrimaryButton("Taste drücken...", ImVec2(Scale(300), 0));

            if (ImGui::GetFrameCount() > m_captureKeyFrame + 1) {
                int vk = FindPressedVirtualKey(m_captureIgnoreKeys);
                if (vk == VK_ESCAPE) {
                    m_capturingKey = false;
                } else if (vk != 0) {
                    bool alreadyExists = false;
                    for (int existingKey : m_capturedKeys) {
                        if (existingKey == vk) {
                            alreadyExists = true;
                            break;
                        }
                    }
                    if (!alreadyExists) {
                        m_capturedKeys.push_back(vk);
                    }
                    m_capturingKey = false;
                }
            }
        } else {
            if (UIComponents::SecondaryButton("+ Taste hinzufügen", ImVec2(Scale(150), 0))) {
                m_capturingKey = true;
                m_captureKeyFrame = ImGui::GetFrameCount();
                CollectPressedVirtualKeys(m_captureIgnoreKeys);
            }
            ImGui::SameLine();
            if (UIComponents::GhostButton("Alle löschen", ImVec2(Scale(120), 0))) {
                ClearAllKeys();
            }
        }

        if (!m_capturedKeys.empty()) {
            ImGui::TextColored(Theme().GetColor(ThemeColorRole::Accent), "Kombination: %s", GetKeyCombinationString().c_str());
        }
    } else {
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::TextSecondary), "Taste:");
        ImGui::SameLine();

        int currentKey = m_capturedKeys.empty() ? 0 : m_capturedKeys[0];
        
        if (m_capturingKey) {
            UIComponents::PrimaryButton("Taste drücken...", ImVec2(Scale(250), 0));

            if (ImGui::GetFrameCount() > m_captureKeyFrame + 1) {
                int vk = FindPressedVirtualKey(m_captureIgnoreKeys);
                if (vk == VK_ESCAPE) {
                    m_capturingKey = false;
                } else if (vk != 0) {
                    m_capturedKeys.clear();
                    m_capturedKeys.push_back(vk);
                    m_capturingKey = false;
                }
            }
        } else {
            std::string keyName = GetKeyName(currentKey);
            if (UIComponents::SecondaryButton(keyName.c_str(), ImVec2(Scale(250), 0))) {
                m_capturingKey = true;
                m_captureKeyFrame = ImGui::GetFrameCount();
                CollectPressedVirtualKeys(m_captureIgnoreKeys);
            }
            ImGui::SameLine();
            if (UIComponents::GhostButton("Zurücksetzen##ResetKey")) {
                m_capturedKeys.clear();
            }
        }
    }
}

void KeybindEditorUI::RemoveKey(size_t index) {
    if (index < m_capturedKeys.size()) {
        m_capturedKeys.erase(m_capturedKeys.begin() + static_cast<std::vector<int>::difference_type>(index));
    }
}

void KeybindEditorUI::ClearAllKeys() {
    m_capturedKeys.clear();
}

std::string KeybindEditorUI::GetKeyCombinationString() const {
    std::string result;
    for (size_t i = 0; i < m_capturedKeys.size(); ++i) {
        if (i > 0) result += " + ";
        result += GetKeyName(m_capturedKeys[i]);
    }
    return result.empty() ? "Nicht gesetzt" : result;
}

} // namespace keybinder
