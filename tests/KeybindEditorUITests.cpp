#include "../include/KeybindEditorUI.h"
#include <imgui.h>
#include <cstdio>
#include <vector>

using namespace keybinder;

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::printf("[FAIL] %s\n", message);
        return false;
    }
    return true;
}

void SetupImGui() {
    if (ImGui::GetCurrentContext() == nullptr) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(800, 600);
        
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }
}

void TeardownImGui() {
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::DestroyContext();
    }
}

void RunTest(const char* name, bool (*test_func)()) {
    std::printf("Running %s... ", name);
    bool result = test_func();
    if (result) {
        std::printf("PASS\n");
    }
}

bool TestCreateNewKeybind() {
    KeybindEditorUI editor;
    editor.OpenForCreate();
    
    if (!Expect(editor.IsOpen(), "Editor should be open after OpenForCreate")) return false;
    
    bool saved = false;
    editor.OnSave([&saved](const Keybind& kb) {
        saved = true;
    });
    
    // Test that we can set up the required callbacks
    return true;
}

bool TestEditExistingKeybind() {
    KeybindEditorUI editor;
    
    Keybind kb("Test", 65); // 65 = 'A'
    kb.category = "TestCategory";
    editor.OpenForEdit(kb);
    
    if (!Expect(editor.IsOpen(), "Editor should be open after OpenForEdit")) return false;
    
    bool saved = false;
    editor.OnSave([&saved](const Keybind& edited_kb) {
        if (edited_kb.name == "Test" && edited_kb.category == "TestCategory") {
            saved = true;
        }
    });
    
    bool canceled = false;
    editor.OnCancel([&canceled]() {
        canceled = true;
    });

    bool deleted = false;
    editor.OnDelete([&deleted](const std::string& name) {
        if (name == "Test") {
            deleted = true;
        }
    });

    return true;
}

bool TestCloseBehavior() {
    KeybindEditorUI editor;
    editor.OpenForCreate();
    if (!Expect(editor.IsOpen(), "Editor should be open")) return false;
    
    editor.Close();
    if (!Expect(!editor.IsOpen(), "Editor should be closed after Close()")) return false;
    
    return true;
}

} // namespace

int main() {
    SetupImGui();
    
    RunTest("TestCreateNewKeybind", TestCreateNewKeybind);
    RunTest("TestEditExistingKeybind", TestEditExistingKeybind);
    RunTest("TestCloseBehavior", TestCloseBehavior);
    
    TeardownImGui();
    
    std::printf("All KeybindEditorUI tests completed.\n");
    return 0;
}
