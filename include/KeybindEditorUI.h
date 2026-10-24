/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Keybind Editor UI header
*/
#pragma once

#include "Keybind.h"
#include "imgui-1.92.7/imgui.h"
#include <functional>
#include <string>

namespace keybinder {

class KeybindEditorUI {
public:
    KeybindEditorUI();
    ~KeybindEditorUI() = default;

    // Renders the editor as a popup modal
    void Render();

    // Renders the editor as an inline panel (no popup)
    void RenderAsPanel();

    // Open the editor for creating a new keybind
    void OpenForCreate();
    
    // Open the editor for editing an existing keybind
    void OpenForEdit(const Keybind& keybind);
    
    // Check if the editor is currently open
    bool IsOpen() const;
    
    // Close the editor without saving
    void Close();
    
    // Check if the editor is actively capturing a key press (main key or action key)
    bool IsCapturingKey() const { return m_capturingKey || m_capturingActionKey; }

    // Callbacks
    void OnSave(std::function<void(const Keybind&)> callback);
    void OnCancel(std::function<void()> callback);
    void OnDelete(std::function<void(const std::string&)> callback);

private:
    bool m_isOpen;
    bool m_isEditing; // true if editing existing, false if creating new
    std::string m_originalName; // to know which keybind is being edited/deleted
    
    // Current state of the keybind being edited
    Keybind m_currentKeybind;
    
    // Multi-key capture state
    std::vector<int> m_capturedKeys;  // Keys captured during multi-key capture mode
    bool m_multiKeyMode;              // true = capture multiple keys, false = legacy single key
    
    // Callbacks
    std::function<void(const Keybind&)> m_onSave;
    std::function<void()> m_onCancel;
    std::function<void(const std::string&)> m_onDelete;
    
    // Error state for validation
    std::string m_errorMessage;
    
    // Main editor UI state
    bool m_capturingKey;
    int m_captureKeyFrame;  // Frame when m_capturingKey was activated (to skip stale key presses)
    std::vector<int> m_captureIgnoreKeys;  // Virtual keys already held when capture started (ignored)
    bool m_setFocus;  // Flag to auto-focus name input on next render

    // Delete confirmation state
    bool m_showDeleteConfirm;
    
    // Variables modal state
    bool m_showVariables;
    
    // Action editor UI state
    bool m_actionEditorOpen;
    bool m_isEditingAction;
    size_t m_editingActionIndex;
    Action m_currentAction;
    bool m_capturingActionKey;
    int m_captureActionKeyFrame;  // Frame when m_capturingActionKey was activated
    std::vector<int> m_actionCaptureIgnoreKeys;  // Virtual keys already held when action capture started
    
    // Internal render helpers
    void RenderMainEditor();
    void RenderActionList();
    void RenderActionEditor();
    void RenderDeleteConfirmation();
    void RenderKeyCaptureUI();  // New helper for multi-key capture UI
    void RenderVariablesModal();
    
    // Action operations
    void MoveActionUp(size_t index);
    void MoveActionDown(size_t index);
    void DeleteAction(size_t index);
    
    // Key operations for multi-key mode
    void RemoveKey(size_t index);
    void ClearAllKeys();
    std::string GetKeyCombinationString() const;
    
    // Key names helper
    std::string GetKeyName(int virtualKey) const;
};

} // namespace keybinder
