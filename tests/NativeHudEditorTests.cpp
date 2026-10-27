#include "../include/NativeHudEditor.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace keybinder {
// Feed observations through the same private accumulator used by the native
// callbacks, without reading game memory or loading the game.
struct NativeHudHooks {
    static void Observe(NativeHudEditor& editor, HudElement element, float x, float y) {
        editor.RecordPoint(element, x, y);
    }
};
}

namespace {
int passed = 0, failed = 0;
#define HUD_CHECK(test) do { if (test) ++passed; else { ++failed; std::printf("[FAIL] NativeHud: %s (line %d)\n",#test,__LINE__); } } while(0)
bool Near(float a,float b) { return std::fabs(a-b)<0.001f; }
}

int RunNativeHudEditorTests() {
    std::printf("\n=== NativeHudEditor Tests ===\n\n");
    passed = failed = 0;
    using namespace keybinder;
    NativeHudEditor editor;
    for (const auto& definition : kHudElements) {
        float x=123.5f,y=45.25f;
        editor.TransformPoint(definition.element,x,y,2560,1440);
        HUD_CHECK(Near(x,123.5f) && Near(y,45.25f));
        HUD_CHECK(editor.TransformColor(definition.element,0x7f123456)==0x7f123456);
    }
    HudLayout layout{};
    auto& money=layout[static_cast<std::size_t>(HudElement::Money)];
    money.offset_x=-0.1f; money.offset_y=0.25f;
    money.custom_color=true; money.color={{0.0f,1.0f,0.5f,0.5f}};
    editor.SetLayout(layout);
    float x=300,y=100;
    editor.TransformPoint(HudElement::Money,x,y,1920,1080);
    HUD_CHECK(Near(x,108) && Near(y,370));
    x=300;y=100;
    editor.TransformPoint(HudElement::Money,x,y,1280,720);
    HUD_CHECK(Near(x,172) && Near(y,280));
    x=300;y=100;
    editor.TransformPoint(HudElement::Ammo,x,y,1920,1080);
    HUD_CHECK(Near(x,300) && Near(y,100));
    HUD_CHECK(editor.TransformColor(HudElement::Money,0xc8b4191d)==0x6400ff80);
    HUD_CHECK(editor.TransformColor(HudElement::Money,0xc8000000)==0x64000000);
    HUD_CHECK(editor.TransformColor(HudElement::Ammo,0xc8b4191d)==0xc8b4191d);
    HUD_CHECK(editor.TransformColor(HudElement::Count,0x12345678)==0x12345678);
    auto& radar=layout[static_cast<std::size_t>(HudElement::Radar)];
    radar.custom_color=true; radar.color={{0.5f,1,0,1}};
    editor.SetLayout(layout);
    HUD_CHECK(editor.TransformColor(HudElement::Radar,0xc8a0b0c0)==0xc850b000);
    HUD_CHECK(editor.TransformColor(HudElement::Radar,0xff000000)==0xff000000);
    HUD_CHECK(NativeHudEditor::ClassifyPlayerInfoText("14:38")==HudElement::Count);
    HUD_CHECK(NativeHudEditor::ClassifyPlayerInfoText("00:00")==HudElement::Count);
    HUD_CHECK(NativeHudEditor::ClassifyPlayerInfoText("$00110520")==HudElement::Money);
    HUD_CHECK(NativeHudEditor::ClassifyPlayerInfoText("-$0000050")==HudElement::Money);
    for (auto text : {"", "1:38", "14:380", "INFO: 14:38", "$", "$12x", "12$", "15-200", "Market", "Welcome"})
        HUD_CHECK(NativeHudEditor::ClassifyPlayerInfoText(text)==HudElement::Count);
    HUD_CHECK(NativeHudEditor::ClassifyPlayerInfoText(nullptr)==HudElement::Count);
    // Corrupt persisted floats cannot propagate NaN/Inf into GPU coordinates.
    money.offset_x=std::numeric_limits<float>::quiet_NaN(); money.offset_y=2;
    money.color={{std::numeric_limits<float>::infinity(),-1,2,std::numeric_limits<float>::quiet_NaN()}};
    editor.SetLayout(layout);
    x=5;y=6;editor.TransformPoint(HudElement::Money,x,y,640,448);
    HUD_CHECK(Near(x,5) && Near(y,454));
    HUD_CHECK(editor.TransformColor(HudElement::Money,0xffffffff)==0xffff00ff);
    x=5;y=6;editor.TransformPoint(HudElement::Money,x,y,0,448);
    HUD_CHECK(Near(x,5) && Near(y,6));
    x=5;y=6;editor.TransformPoint(HudElement::Count,x,y,640,448);
    HUD_CHECK(Near(x,5) && Near(y,6));
    float bx=11,by=12,bw=13,bh=14;
    HUD_CHECK(!editor.GetElementBounds(HudElement::Money,bx,by,bw,bh));
    HUD_CHECK(Near(bx,11)&&Near(by,12)&&Near(bw,13)&&Near(bh,14));
    NativeHudHooks::Observe(editor,HudElement::Money,100,200);
    NativeHudHooks::Observe(editor,HudElement::Money,180,210);
    HUD_CHECK(editor.GetElementBounds(HudElement::Money,bx,by,bw,bh));
    HUD_CHECK(Near(bx,100)&&Near(by,200)&&Near(bw,80)&&Near(bh,10));
    editor.SetLayout(layout);
    HUD_CHECK(editor.GetElementBounds(HudElement::Money,bx,by,bw,bh));
    money.offset_x=0.3f;
    editor.SetLayout(layout);
    HUD_CHECK(!editor.GetElementBounds(HudElement::Money,bx,by,bw,bh));
    HUD_CHECK(editor.Initialize());
    HUD_CHECK(editor.Initialize());
    editor.Shutdown();editor.Shutdown();
    HUD_CHECK(!editor.GetElementBounds(HudElement::Money,bx,by,bw,bh));
    editor.SetLayout(HudLayout{});
    HUD_CHECK(editor.TransformColor(HudElement::Money,0xffffffff)==0xffffffff);
    HUD_CHECK(kHudElementCount == 4);
    HUD_CHECK(NativeHudEditor::ResolvePrimitiveElement(HudElement::Count,true,false,false)==HudElement::Weapon);
    // Vital-bar exclusions must beat weapon fallback and any enclosing scope.
    HUD_CHECK(NativeHudEditor::ResolvePrimitiveElement(HudElement::Count,true,true,false)==HudElement::Count);
    HUD_CHECK(NativeHudEditor::ResolvePrimitiveElement(HudElement::Radar,true,true,false)==HudElement::Count);
    HUD_CHECK(NativeHudEditor::ResolvePrimitiveElement(HudElement::Ammo,true,false,false)==HudElement::Ammo);
    HUD_CHECK(NativeHudEditor::ResolvePrimitiveElement(HudElement::Radar,false,false,false)==HudElement::Radar);
    HUD_CHECK(NativeHudEditor::ResolvePrimitiveElement(HudElement::Count,false,false,false)==HudElement::Count);
    HUD_CHECK(NativeHudEditor::ResolvePrimitiveElement(HudElement::Ammo,true,false,true)==HudElement::Count);
    std::printf("\n=== NativeHudEditor Results: %d passed, %d failed ===\n\n",passed,failed);
    return failed;
}
