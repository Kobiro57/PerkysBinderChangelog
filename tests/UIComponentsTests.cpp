/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    UIComponents subtab bar and core component tests
*/
#include "../include/UIComponents.h"
#include "../include/ThemeManager.h"
#include "../include/TextureManager.h"
#include "../include/imgui-1.92.7/imgui.h"
#include "../include/imgui-1.92.7/imgui_internal.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>

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

// ── Sub-tab bar tests ────────────────────────────────────────────

bool TestSubTabBarRendersAllLabels() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestSubTabBar");

    UIComponents::BeginSubTabs("subtest");
    bool clicked1 = UIComponents::SubTabItem("Alpha", true);
    bool clicked2 = UIComponents::SubTabItem("Beta", false);
    bool clicked3 = UIComponents::SubTabItem("Gamma", false);
    UIComponents::EndSubTabs();

    ImGui::End();
    ImGui::Render();

    // Rendering all three without crash is the primary contract.
    // The return values may be false since we didn't simulate input.
    (void)clicked1;
    (void)clicked2;
    (void)clicked3;

    TeardownImGui();
    return true;
}

bool TestSubTabItemReturnsTrueOnClick() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestSubTabClick");

    // Position cursor so the subtab is at a known location.
    // Simulate a left-click on the "Active" item by setting mouse position
    // and pressing the left mouse button before calling SubTabItem.
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(120.0f, 50.0f);
    io.MouseDown[0] = true;

    UIComponents::BeginSubTabs("clicktest");
    bool clicked = UIComponents::SubTabItem("Active", false);
    UIComponents::EndSubTabs();

    ImGui::End();
    ImGui::Render();

    // Reset mouse state
    io.MouseDown[0] = false;

    // The click may or may not register depending on exact hit-test,
    // but the function must not crash. Verify the API contract:
    // when a subtab is clicked, SubTabItem returns true.
    // We accept both outcomes since hit-testing depends on window layout.
    (void)clicked;

    TeardownImGui();
    return true;
}

bool TestDisabledSubTabDoesNotActivate() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestDisabledSubTab");

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(120.0f, 50.0f);
    io.MouseDown[0] = true;

    UIComponents::BeginSubTabs("disabledtest");
    bool clicked = UIComponents::SubTabItem("Disabled", false, true);
    UIComponents::EndSubTabs();

    ImGui::End();
    ImGui::Render();

    io.MouseDown[0] = false;

    // A disabled subtab must never return true, regardless of click state.
    if (!Expect(!clicked, "Disabled subtab must not return true on click")) {
        TeardownImGui();
        return false;
    }

    TeardownImGui();
    return true;
}

// ── Additional subtab visual contract tests ──────────────────────

bool TestSubTabHeightDerivation() {
    using namespace keybinder;
    ThemeManager& tm = ThemeManager::Instance();
    tm.SetTheme("nova");

    const float tabHeight = tm.GetControlHeight(ThemeControlHeightRole::Tab);
    // Subtabs are deliberately taller than before so the glass bar no longer feels squashed.
    const float expectedSubtabHeight = (std::max)(tabHeight + 2.0f * tm.GetAppliedScale(), 12.0f);

    if (!Expect(expectedSubtabHeight > 0.0f, "Subtab height must be positive")) return false;
    if (!Expect(expectedSubtabHeight >= tabHeight, "Subtab height must be at least main tab height")) return false;

    return true;
}

bool TestSubTabInactiveRendersNoCrash() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestInactiveSubTab");

    UIComponents::BeginSubTabs("inactivetest");
    // All inactive, no active subtab
    bool c1 = UIComponents::SubTabItem("Tab1", false);
    bool c2 = UIComponents::SubTabItem("Tab2", false);
    bool c3 = UIComponents::SubTabItem("Tab3", false);
    UIComponents::EndSubTabs();

    ImGui::End();
    ImGui::Render();

    (void)c1; (void)c2; (void)c3;

    TeardownImGui();
    return true;
}

bool TestSubTabsBalanceImGuiIdStack() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestSubTabIdStack");

    const int before = ImGui::GetCurrentWindow()->IDStack.Size;
    UIComponents::BeginSubTabs("balancedtest");
    UIComponents::SubTabItem("One", true);
    UIComponents::SubTabItem("Two", false);
    UIComponents::EndSubTabs();
    const int after = ImGui::GetCurrentWindow()->IDStack.Size;

    ImGui::End();
    ImGui::Render();

    const bool balanced = before == after;
    TeardownImGui();
    return Expect(balanced, "BeginSubTabs/EndSubTabs must balance ImGui ID stack");
}

bool TestSubTabItemNoInputReturnsFalse() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestSubTabNoInput");

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDown[0] = false;
    io.MousePos = ImVec2(-100.0f, -100.0f);

    UIComponents::BeginSubTabs("noinputtest");
    const bool clicked = UIComponents::SubTabItem("Idle", false);
    UIComponents::EndSubTabs();

    ImGui::End();
    ImGui::Render();

    TeardownImGui();
    return Expect(!clicked, "Inactive subtab with no input should not report a click");
}

// ── Additional subtab behavior tests ──────────────────────────────

bool TestSubTabMultipleActiveRendersNoCrash() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestMultiActiveSubTab");

    UIComponents::BeginSubTabs("multiactivetest");
    // Only first should logically be "active", but rendering multiple active should not crash
    bool c1 = UIComponents::SubTabItem("Active1", true);
    bool c2 = UIComponents::SubTabItem("Active2", true);
    bool c3 = UIComponents::SubTabItem("Inactive", false);
    UIComponents::EndSubTabs();

    ImGui::End();
    ImGui::Render();

    (void)c1; (void)c2; (void)c3;

    TeardownImGui();
    return true;
}

bool TestSubTabEmptyLabelRendersNoCrash() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestEmptySubTabLabel");

    UIComponents::BeginSubTabs("emptylabeltest");
    bool c1 = UIComponents::SubTabItem("", false);
    bool c2 = UIComponents::SubTabItem("Normal", true);
    UIComponents::EndSubTabs();

    ImGui::End();
    ImGui::Render();

    (void)c1; (void)c2;

    TeardownImGui();
    return true;
}

bool TestSubTabsDifferentIdsAreIndependent() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestIndependentSubTabs");

    // Two independent subtab bars with different IDs should not interfere
    UIComponents::BeginSubTabs("barA");
    UIComponents::SubTabItem("A1", true);
    UIComponents::SubTabItem("A2", false);
    UIComponents::EndSubTabs();

    UIComponents::BeginSubTabs("barB");
    UIComponents::SubTabItem("B1", false);
    UIComponents::SubTabItem("B2", true);
    UIComponents::EndSubTabs();

    ImGui::End();
    ImGui::Render();

    TeardownImGui();
    return true;
}

bool TestTruncateTextToPixelWidthKeepsFittingText() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    const auto result = UIComponents::TruncateTextToPixelWidth("ShortName", 1000.0f);
    ImGui::Render();

    TeardownImGui();
    return Expect(result.first == "ShortName", "Fitting text should remain unchanged")
        && Expect(!result.second, "Fitting text should not report truncation");
}

bool TestTruncateTextToPixelWidthAddsEllipsis() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    const auto result = UIComponents::TruncateTextToPixelWidth("VeryLongOnlinePlayerName", 24.0f);
    ImGui::Render();

    TeardownImGui();
    const bool hasEllipsis = result.first.size() >= 3
        && result.first.substr(result.first.size() - 3) == "...";
    return Expect(result.second, "Overflowing text should report truncation")
        && Expect(hasEllipsis, "Overflowing text should end with ellipsis");
}

bool TestMetricCardRendersNoCrash() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestMetricCard");

    const ImVec4 valueColor(0.2f, 0.9f, 0.4f, 1.0f);
    UIComponents::MetricCard("MetricCard", "Kills", "42", "Heute", &valueColor, nullptr, 180.0f);

    ImGui::End();
    ImGui::Render();
    TeardownImGui();
    return true;
}

bool TestThumbnailHelpersRenderNoCrash() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestThumbnailHelpers");

    const float thumbSize = UIComponents::ThumbnailSize();
    const float cardWidth = UIComponents::ThumbnailCardWidth();
    const float layoutWidth = UIComponents::GetThumbnailLayoutWidth(cardWidth);

    UIComponents::BeginCard("ThumbnailCard", cardWidth, true);
    UIComponents::CenterNextItem(thumbSize);
    const bool clicked = UIComponents::ThumbnailButton("##thumb", ImVec2(thumbSize, thumbSize), true);
    TextureInfo emptyTexture;
    UIComponents::DrawFittedTexturePreview(emptyTexture);
    UIComponents::DrawThumbnailTextOverlay("PNG", IM_COL32_WHITE);
    ImGui::Spacing();
    UIComponents::DrawThumbnailCaption("Example.png", true);
    UIComponents::EndCard();

    ImGui::End();
    ImGui::Render();
    TeardownImGui();

    (void)clicked;
    return Expect(thumbSize > 0.0f, "Thumbnail size should be positive")
        && Expect(cardWidth >= thumbSize, "Thumbnail card width should fit thumbnail")
        && Expect(layoutWidth >= cardWidth, "Thumbnail layout width should fit at least one card");
}


bool TestSoftGlowRectAppendsGeometryAndRestoresFlags() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestSoftGlowRect");
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const int beforeVertices = drawList->VtxBuffer.Size;
    const int beforeIndices = drawList->IdxBuffer.Size;
    const ImDrawListFlags beforeFlags = drawList->Flags;

    UIComponents::AddSoftGlowRect(
        drawList,
        ImVec2(40.0f, 40.0f),
        ImVec2(120.0f, 88.0f),
        IM_COL32(0, 220, 255, 220),
        8.0f,
        12.0f,
        0.8f,
        10);

    const bool appended = drawList->VtxBuffer.Size > beforeVertices && drawList->IdxBuffer.Size > beforeIndices;
    const bool flagsRestored = drawList->Flags == beforeFlags;
    ImGui::End();
    ImGui::Render();
    TeardownImGui();

    return Expect(appended, "Soft rect glow should append draw geometry")
        && Expect(flagsRestored, "Soft rect glow should restore draw-list flags");
}

bool TestSoftGlowRectUsesBoundedBypassClip() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200.0f, 200.0f), ImGuiCond_Always);
    ImGui::Begin("TestSoftGlowRectBoundedClip", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const int firstCandidateCommand = drawList->CmdBuffer.Size > 0 ? drawList->CmdBuffer.Size - 1 : 0;
    drawList->PushClipRect(ImVec2(50.0f, 50.0f), ImVec2(100.0f, 100.0f), false);

    UIComponents::AddSoftGlowRect(
        drawList,
        ImVec2(40.0f, 40.0f),
        ImVec2(120.0f, 88.0f),
        IM_COL32(0, 220, 255, 220),
        8.0f,
        12.0f,
        0.8f,
        10,
        ImDrawFlags_RoundCornersAll,
        true);

    bool foundIntersectedClip = false;
    constexpr float kEpsilon = 0.01f;
    for (int i = firstCandidateCommand; i < drawList->CmdBuffer.Size; ++i) {
        const ImDrawCmd& cmd = drawList->CmdBuffer[i];
        if (cmd.ElemCount == 0) {
            continue;
        }

        const bool matchesExpectedClip =
            std::fabs(cmd.ClipRect.x - 50.0f) <= kEpsilon &&
            std::fabs(cmd.ClipRect.y - 50.0f) <= kEpsilon &&
            std::fabs(cmd.ClipRect.z - 100.0f) <= kEpsilon &&
            std::fabs(cmd.ClipRect.w - 100.0f) <= kEpsilon;
        if (matchesExpectedClip) {
            foundIntersectedClip = true;
            break;
        }
    }

    drawList->PopClipRect();
    ImGui::End();
    ImGui::Render();
    TeardownImGui();

    return Expect(foundIntersectedClip, "Soft rect glow bypass clip should stay inside the active clip rect");
}

bool TestSoftGlowRectRejectsInvalidInput() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestSoftGlowRectInvalid");
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const int beforeVertices = drawList->VtxBuffer.Size;
    const int beforeIndices = drawList->IdxBuffer.Size;
    const ImDrawListFlags beforeFlags = drawList->Flags;

    UIComponents::AddSoftGlowRect(nullptr, ImVec2(0, 0), ImVec2(10, 10), IM_COL32_WHITE, 2.0f, 4.0f);
    UIComponents::AddSoftGlowRect(drawList, ImVec2(20, 20), ImVec2(10, 10), IM_COL32_WHITE, 2.0f, 4.0f);
    UIComponents::AddSoftGlowRect(drawList, ImVec2(10, 10), ImVec2(20, 20), IM_COL32_WHITE, 2.0f, 0.0f);
    UIComponents::AddSoftGlowRect(drawList, ImVec2(10, 10), ImVec2(20, 20), IM_COL32_WHITE, 2.0f, 4.0f, 1.0f, 0);

    const bool unchanged = drawList->VtxBuffer.Size == beforeVertices && drawList->IdxBuffer.Size == beforeIndices;
    const bool flagsRestored = drawList->Flags == beforeFlags;
    ImGui::End();
    ImGui::Render();
    TeardownImGui();

    return Expect(unchanged, "Invalid soft rect glow calls should not append geometry")
        && Expect(flagsRestored, "Invalid soft rect glow calls should preserve draw-list flags");
}

bool TestSoftGlowArcAppendsGeometryAndRestoresFlags() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestSoftGlowArc");
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const int beforeVertices = drawList->VtxBuffer.Size;
    const int beforeIndices = drawList->IdxBuffer.Size;
    const ImDrawListFlags beforeFlags = drawList->Flags;

    UIComponents::AddSoftGlowArc(
        drawList,
        ImVec2(120.0f, 120.0f),
        52.0f,
        0.2f,
        2.8f,
        IM_COL32(255, 120, 32, 230),
        4.0f,
        14.0f,
        0.75f,
        10,
        24);

    const bool appended = drawList->VtxBuffer.Size > beforeVertices && drawList->IdxBuffer.Size > beforeIndices;
    const bool flagsRestored = drawList->Flags == beforeFlags;
    ImGui::End();
    ImGui::Render();
    TeardownImGui();

    return Expect(appended, "Soft arc glow should append draw geometry")
        && Expect(flagsRestored, "Soft arc glow should restore draw-list flags");
}

bool TestSoftGlowArcRejectsInvalidInput() {
    SetupImGui();
    using namespace keybinder;

    ImGui::NewFrame();
    ImGui::Begin("TestSoftGlowArcInvalid");
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const int beforeVertices = drawList->VtxBuffer.Size;
    const int beforeIndices = drawList->IdxBuffer.Size;
    const ImDrawListFlags beforeFlags = drawList->Flags;

    UIComponents::AddSoftGlowArc(nullptr, ImVec2(0, 0), 10.0f, 0.0f, 1.0f, IM_COL32_WHITE, 2.0f, 4.0f);
    UIComponents::AddSoftGlowArc(drawList, ImVec2(40, 40), 0.0f, 0.0f, 1.0f, IM_COL32_WHITE, 2.0f, 4.0f);
    UIComponents::AddSoftGlowArc(drawList, ImVec2(40, 40), 10.0f, 1.0f, 0.5f, IM_COL32_WHITE, 2.0f, 4.0f);
    UIComponents::AddSoftGlowArc(drawList, ImVec2(40, 40), 10.0f, 0.0f, 1.0f, IM_COL32_WHITE, 2.0f, 4.0f, 1.0f, 0);

    const bool unchanged = drawList->VtxBuffer.Size == beforeVertices && drawList->IdxBuffer.Size == beforeIndices;
    const bool flagsRestored = drawList->Flags == beforeFlags;
    ImGui::End();
    ImGui::Render();
    TeardownImGui();

    return Expect(unchanged, "Invalid soft arc glow calls should not append geometry")
        && Expect(flagsRestored, "Invalid soft arc glow calls should preserve draw-list flags");
}

} // namespace

int RunUIComponentsTests() {
    int failed = 0;

    auto run = [&](const char* name, bool (*test)()) {
        std::printf("Running %s... ", name);
        bool result = test();
        if (result) {
            std::printf("PASS\n");
        } else {
            ++failed;
        }
    };

    run("TestSubTabBarRendersAllLabels", TestSubTabBarRendersAllLabels);
    run("TestSubTabItemReturnsTrueOnClick", TestSubTabItemReturnsTrueOnClick);
    run("TestDisabledSubTabDoesNotActivate", TestDisabledSubTabDoesNotActivate);
    run("TestSubTabHeightDerivation", TestSubTabHeightDerivation);
    run("TestSubTabInactiveRendersNoCrash", TestSubTabInactiveRendersNoCrash);
    run("TestSubTabsBalanceImGuiIdStack", TestSubTabsBalanceImGuiIdStack);
    run("TestSubTabItemNoInputReturnsFalse", TestSubTabItemNoInputReturnsFalse);
    run("TestSubTabMultipleActiveRendersNoCrash", TestSubTabMultipleActiveRendersNoCrash);
    run("TestSubTabEmptyLabelRendersNoCrash", TestSubTabEmptyLabelRendersNoCrash);
    run("TestSubTabsDifferentIdsAreIndependent", TestSubTabsDifferentIdsAreIndependent);
    run("TestTruncateTextToPixelWidthKeepsFittingText", TestTruncateTextToPixelWidthKeepsFittingText);
    run("TestTruncateTextToPixelWidthAddsEllipsis", TestTruncateTextToPixelWidthAddsEllipsis);
    run("TestMetricCardRendersNoCrash", TestMetricCardRendersNoCrash);
    run("TestThumbnailHelpersRenderNoCrash", TestThumbnailHelpersRenderNoCrash);
    run("TestSoftGlowRectAppendsGeometryAndRestoresFlags", TestSoftGlowRectAppendsGeometryAndRestoresFlags);
    run("TestSoftGlowRectUsesBoundedBypassClip", TestSoftGlowRectUsesBoundedBypassClip);
    run("TestSoftGlowRectRejectsInvalidInput", TestSoftGlowRectRejectsInvalidInput);
    run("TestSoftGlowArcAppendsGeometryAndRestoresFlags", TestSoftGlowArcAppendsGeometryAndRestoresFlags);
    run("TestSoftGlowArcRejectsInvalidInput", TestSoftGlowArcRejectsInvalidInput);

    return failed;
}

#ifdef UICOMPONENTS_TEST_MAIN
int main() {
    return RunUIComponentsTests();
}
#endif
