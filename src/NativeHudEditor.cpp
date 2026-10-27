#include "../include/NativeHudEditor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef KEYBINDER_TESTING
#include "../include/plugin-sdk/shared/RenderWare.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CFont.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CMenuManager.h"
#include "../include/plugin-sdk/safetyhook/safetyhook.hpp"
#include <vector>
#endif

namespace keybinder {
namespace {
bool Valid(HudElement element) { return static_cast<std::size_t>(element) < kHudElementCount; }
unsigned Byte(float value) { return static_cast<unsigned>(std::lround((std::max)(0.0f, (std::min)(value, 255.0f)))); }
}

void NativeHudEditor::SetLayout(const HudLayout& layout) {
    // An invisible element can retain the bounds from its last native draw.
    // Invalidate those bounds after movement or a resolution change so its
    // drag handle uses the new position instead of a stale rectangle.
#ifndef KEYBINDER_TESTING
    if (viewport_width_ != RsGlobal.maximumWidth || viewport_height_ != RsGlobal.maximumHeight) {
        for (auto& bounds : bounds_) bounds = {};
        viewport_width_ = RsGlobal.maximumWidth;
        viewport_height_ = RsGlobal.maximumHeight;
    }
#endif
    for (std::size_t i = 0; i < layout_.size(); ++i) {
        const auto next = SanitizeHudElementStyle(layout[i]);
        if (next.offset_x != layout_[i].offset_x || next.offset_y != layout_[i].offset_y) bounds_[i] = {};
        layout_[i] = next;
    }
}

void NativeHudEditor::TransformPoint(HudElement element, float& x, float& y, float width, float height) const {
    if (!Valid(element) || !std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) return;
    const auto& style = layout_[static_cast<std::size_t>(element)];
    x += style.offset_x * width;
    y += style.offset_y * height;
}

std::uint32_t NativeHudEditor::TransformColor(HudElement element, std::uint32_t argb, bool text) const {
    if (!Valid(element)) return argb;
    const auto& style = layout_[static_cast<std::size_t>(element)];
    if (!style.custom_color) return argb;
    const unsigned a = argb >> 24, r = (argb >> 16) & 255, g = (argb >> 8) & 255, b = argb & 255;
    const unsigned alpha = Byte(static_cast<float>(a) * style.color[3]);
    // Bar borders, map outlines and font drop shadows must remain legible.
    if (!text && r == 0 && g == 0 && b == 0) return (alpha << 24) | (argb & 0xffffff);
    float strength = 255.0f;
    if (element == HudElement::Wanted) strength = static_cast<float>((std::max)({r, g, b}));
    if (element == HudElement::Radar) {
        // A map tint retains the map texture's detail and marker luminance.
        return (alpha << 24) | (Byte(r * style.color[0]) << 16) | (Byte(g * style.color[1]) << 8) | Byte(b * style.color[2]);
    }
    return (alpha << 24) | (Byte(strength * style.color[0]) << 16) | (Byte(strength * style.color[1]) << 8) | Byte(strength * style.color[2]);
}

HudElement NativeHudEditor::ClassifyPlayerInfoText(const char* text) {
    if (!text) return HudElement::Count;
    const auto digit = [](char c) { return c >= '0' && c <= '9'; };
    // Only the native DrawPlayerInfo scope invokes this classifier. Similar
    // server textdraws and chat strings are never classified or changed.
    const std::size_t size = std::strlen(text);
    if (size == 5 && digit(text[0]) && digit(text[1]) && text[2] == ':' && digit(text[3]) && digit(text[4])) return HudElement::Clock;
    std::size_t begin = text[0] == '-' ? 1 : 0;
    if (begin < size && text[begin] == '$' && begin + 1 < size) {
        for (std::size_t i = begin + 1; i < size; ++i) if (!digit(text[i])) return HudElement::Count;
        return HudElement::Money;
    }
    return HudElement::Count;
}

void NativeHudEditor::ResetBounds(HudElement element) {
    if (Valid(element)) bounds_[static_cast<std::size_t>(element)] = {};
}

void NativeHudEditor::RecordPoint(HudElement element, float x, float y) {
    if (!Valid(element) || !std::isfinite(x) || !std::isfinite(y)) return;
    auto& bounds = bounds_[static_cast<std::size_t>(element)];
    if (!bounds.valid) { bounds = {true, x, y, x, y}; return; }
    bounds.left = (std::min)(bounds.left, x); bounds.right = (std::max)(bounds.right, x);
    bounds.top = (std::min)(bounds.top, y); bounds.bottom = (std::max)(bounds.bottom, y);
}

bool NativeHudEditor::GetElementBounds(HudElement element, float& x, float& y, float& width, float& height) const {
    if (!Valid(element)) return false;
    const auto& bounds = bounds_[static_cast<std::size_t>(element)];
    if (!bounds.valid || bounds.right <= bounds.left || bounds.bottom <= bounds.top) return false;
    x = bounds.left; y = bounds.top; width = bounds.right - bounds.left; height = bounds.bottom - bounds.top;
    return true;
}

#ifndef KEYBINDER_TESTING
// Signatures/addresses below are supplied by the bundled plugin-sdk CHud.cpp
// and CFont.cpp, verified against the primary gta-reversed Hud.cpp/Font.cpp:
// https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Hud.cpp
// https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Font.cpp
// RW submissions use the device's real callbacks, including direct macro calls;
// detouring only an exported wrapper would miss the game's native submissions.
struct NativeHudHooks {
    static NativeHudEditor* editor;
    static std::vector<safetyhook::InlineHook*> hooks;
    static thread_local HudElement element;
    static thread_local bool player_info;
    static thread_local unsigned font_depth;
    static thread_local unsigned submission_depth;
    static thread_local bool font_rendering;
    static safetyhook::InlineHook *health, *armour, *breath, *ammo, *wanted, *vehicle, *zone, *radar, *player,
        *print, *font_buffer, *primitive, *indexed;

    struct Scope {
        HudElement previous;
        explicit Scope(HudElement next) : previous(element) {
            element = next;
            if (editor) editor->ResetBounds(next);
        }
        ~Scope() { element = previous; }
    };
    struct Counter { unsigned& value; explicit Counter(unsigned& v) : value(v) { ++value; } ~Counter() { --value; } };

    static bool Install(safetyhook::InlineHook*& destination, void* target, void* callback) {
        if (!target) return false;
        auto hook = safetyhook::InlineHook::create(target, callback);
        if (!hook) return false;
        destination = new safetyhook::InlineHook(std::move(*hook));
        hooks.push_back(destination);
        return true;
    }
    static void Remove() {
        for (auto it = hooks.rbegin(); it != hooks.rend(); ++it) delete *it;
        hooks.clear();
        health = armour = breath = ammo = wanted = vehicle = zone = radar = player = nullptr;
        print = font_buffer = primitive = indexed = nullptr;
        editor = nullptr;
        element = HudElement::Count; player_info = font_rendering = false;
    }
    static HudElement PrimitiveElement() {
        if (font_rendering || font_depth || !editor) return HudElement::Count;
        if (Valid(element)) return element;
        // DrawPlayerInfo's only unscoped native sprite is the weapon/fist.
        // Health/armour/breath and ammo are nested scopes. No additional detour
        // at DrawWeaponIcon is needed, so weapon texture switching can install
        // and remove its hook independently without breaking this editor.
        return player_info ? HudElement::Weapon : HudElement::Count;
    }
    static void __cdecl Health(int playerId, int x, int y) { Scope scope(HudElement::Health); health->ccall<void>(playerId,x,y); }
    static void __cdecl Armour(int playerId, int x, int y) { Scope scope(HudElement::Armour); armour->ccall<void>(playerId,x,y); }
    static void __cdecl Breath(int playerId, int x, int y) { Scope scope(HudElement::Breath); breath->ccall<void>(playerId,x,y); }
    static void __cdecl Ammo(void* ped, int x, int y, float alpha) { Scope scope(HudElement::Ammo); ammo->ccall<void>(ped,x,y,alpha); }
    static void __cdecl Wanted() { Scope scope(HudElement::Wanted); wanted->ccall<void>(); }
    static void __cdecl Vehicle() { Scope scope(HudElement::VehicleName); vehicle->ccall<void>(); }
    static void __cdecl Zone() { Scope scope(HudElement::ZoneName); zone->ccall<void>(); }
    static void __cdecl Radar() {
        if (FrontEndMenuManager.m_bMenuActive || FrontEndMenuManager.m_bDrawRadarOrMap) { radar->ccall<void>(); return; }
        Scope scope(HudElement::Radar); radar->ccall<void>();
    }
    static void __cdecl Player() {
        const bool previous = player_info; player_info = true;
        if (editor) for (auto item : {HudElement::Weapon,HudElement::Money,HudElement::Clock}) editor->ResetBounds(item);
        player->ccall<void>(); player_info = previous;
    }
    static void __cdecl FontBuffer() {
        const bool previous = font_rendering; font_rendering = true;
        font_buffer->ccall<void>(); font_rendering = previous;
    }
    static void Print(safetyhook::InlineHook* hook, float x, float y, const char* text) {
        HudElement selected = element;
        if (!Valid(selected) && player_info) selected = NativeHudEditor::ClassifyPlayerInfoText(text);
        if (!editor || !Valid(selected) || font_depth) { hook->ccall<void>(x,y,text); return; }
        Counter guard(font_depth);
        editor->TransformPoint(selected,x,y,static_cast<float>(RsGlobal.maximumWidth),static_cast<float>(RsGlobal.maximumHeight));
        // PrintString snapshots current color into its own font buffer. Restore
        // immediately afterwards; chat and subsequent native draws retain it.
        CRGBA saved = *CFont::m_Color;
        const auto original = (static_cast<unsigned>(saved.a)<<24)|(static_cast<unsigned>(saved.r)<<16)|(static_cast<unsigned>(saved.g)<<8)|saved.b;
        const auto changed = editor->TransformColor(selected,original,true);
        CFont::m_Color->r = (changed>>16)&255; CFont::m_Color->g = (changed>>8)&255;
        CFont::m_Color->b = changed&255; CFont::m_Color->a = changed>>24;
        if (text && text[0]) {
            const float width = CFont::GetStringWidth(text,true,false);
            const float height = (std::max)(8.0f,CFont::m_Scale->y*18.0f);
            const float left = x - (CFont::m_bFontRightAlign ? width : CFont::m_bFontCentreAlign ? width*0.5f : 0.0f);
            editor->RecordPoint(selected,left,y); editor->RecordPoint(selected,left+width,y+height);
        }
        hook->ccall<void>(x,y,text);
        *CFont::m_Color = saved;
    }
    static void __cdecl PrintString(float x,float y,const char* text) { Print(print,x,y,text); }

    static RwIm2DVertex* Prepare(RwIm2DVertex* vertices, int count) {
        const HudElement selected = PrimitiveElement();
        if (!Valid(selected) || submission_depth || !vertices || count <= 0 || count > 65536) return vertices;
        // Retain the game's vertex array. Radar mask, map, ring and blips are
        // transformed together in the CHud::DrawRadar scope. The pause-menu
        // map is outside that scope and remains untouched.
        static thread_local std::vector<RwIm2DVertex> transformed;
        try { transformed.assign(vertices,vertices+count); }
        catch (const std::bad_alloc&) { return vertices; }
        for (auto& vertex : transformed) {
            editor->TransformPoint(selected,vertex.x,vertex.y,static_cast<float>(RsGlobal.maximumWidth),static_cast<float>(RsGlobal.maximumHeight));
            editor->RecordPoint(selected,vertex.x,vertex.y);
            vertex.emissiveColor = editor->TransformColor(selected,vertex.emissiveColor);
        }
        return transformed.data();
    }
    static RwBool __cdecl Primitive(RwPrimitiveType type,RwIm2DVertex* vertices,RwInt32 count) {
        auto* transformed = Prepare(vertices,count); Counter guard(submission_depth);
        return primitive->ccall<RwBool>(type,transformed,count);
    }
    static RwBool __cdecl Indexed(RwPrimitiveType type,RwIm2DVertex* vertices,RwInt32 count,RwImVertexIndex* indices,RwInt32 index_count) {
        auto* transformed = Prepare(vertices,count); Counter guard(submission_depth);
        return indexed->ccall<RwBool>(type,transformed,count,indices,index_count);
    }
};

NativeHudEditor* NativeHudHooks::editor = nullptr;
std::vector<safetyhook::InlineHook*> NativeHudHooks::hooks;
thread_local HudElement NativeHudHooks::element = HudElement::Count;
thread_local bool NativeHudHooks::player_info = false;
thread_local unsigned NativeHudHooks::font_depth = 0;
thread_local unsigned NativeHudHooks::submission_depth = 0;
thread_local bool NativeHudHooks::font_rendering = false;
safetyhook::InlineHook *NativeHudHooks::health = nullptr, *NativeHudHooks::armour = nullptr, *NativeHudHooks::breath = nullptr,
    *NativeHudHooks::ammo = nullptr, *NativeHudHooks::wanted = nullptr, *NativeHudHooks::vehicle = nullptr,
    *NativeHudHooks::zone = nullptr, *NativeHudHooks::radar = nullptr, *NativeHudHooks::player = nullptr,
    *NativeHudHooks::print = nullptr, *NativeHudHooks::font_buffer = nullptr,
    *NativeHudHooks::primitive = nullptr, *NativeHudHooks::indexed = nullptr;
#endif

bool NativeHudEditor::Initialize() {
    if (initialized_) return true;
    last_error_.clear();
#ifndef KEYBINDER_TESTING
    if (NativeHudHooks::editor && NativeHudHooks::editor != this) { last_error_ = "Ein Interface-Editor ist bereits aktiv."; return false; }
    if (!RwEngineInstance) { last_error_ = "Die HUD-Grafik ist noch nicht bereit."; return false; }
    NativeHudHooks::editor = this;
    const bool installed =
        NativeHudHooks::Install(NativeHudHooks::primitive,reinterpret_cast<void*>(RWSRCGLOBAL(dOpenDevice).fpIm2DRenderPrimitive),reinterpret_cast<void*>(&NativeHudHooks::Primitive)) &&
        NativeHudHooks::Install(NativeHudHooks::indexed,reinterpret_cast<void*>(RWSRCGLOBAL(dOpenDevice).fpIm2DRenderIndexedPrimitive),reinterpret_cast<void*>(&NativeHudHooks::Indexed)) &&
        NativeHudHooks::Install(NativeHudHooks::font_buffer,reinterpret_cast<void*>(0x719840),reinterpret_cast<void*>(&NativeHudHooks::FontBuffer)) &&
        NativeHudHooks::Install(NativeHudHooks::print,reinterpret_cast<void*>(0x71A700),reinterpret_cast<void*>(&NativeHudHooks::PrintString)) &&
        NativeHudHooks::Install(NativeHudHooks::health,reinterpret_cast<void*>(0x589270),reinterpret_cast<void*>(&NativeHudHooks::Health)) &&
        NativeHudHooks::Install(NativeHudHooks::armour,reinterpret_cast<void*>(0x5890A0),reinterpret_cast<void*>(&NativeHudHooks::Armour)) &&
        NativeHudHooks::Install(NativeHudHooks::breath,reinterpret_cast<void*>(0x589190),reinterpret_cast<void*>(&NativeHudHooks::Breath)) &&
        NativeHudHooks::Install(NativeHudHooks::ammo,reinterpret_cast<void*>(0x5893B0),reinterpret_cast<void*>(&NativeHudHooks::Ammo)) &&
        NativeHudHooks::Install(NativeHudHooks::wanted,reinterpret_cast<void*>(0x58D9A0),reinterpret_cast<void*>(&NativeHudHooks::Wanted)) &&
        NativeHudHooks::Install(NativeHudHooks::vehicle,reinterpret_cast<void*>(0x58AEA0),reinterpret_cast<void*>(&NativeHudHooks::Vehicle)) &&
        NativeHudHooks::Install(NativeHudHooks::zone,reinterpret_cast<void*>(0x58AA50),reinterpret_cast<void*>(&NativeHudHooks::Zone)) &&
        NativeHudHooks::Install(NativeHudHooks::radar,reinterpret_cast<void*>(0x58A330),reinterpret_cast<void*>(&NativeHudHooks::Radar)) &&
        NativeHudHooks::Install(NativeHudHooks::player,reinterpret_cast<void*>(0x58EAF0),reinterpret_cast<void*>(&NativeHudHooks::Player));
    if (!installed) {
        NativeHudHooks::Remove();
        last_error_ = "Die native HUD-Anbindung konnte nicht vollständig eingerichtet werden.";
        return false;
    }
#endif
    initialized_ = true;
    return true;
}

void NativeHudEditor::Shutdown() {
#ifndef KEYBINDER_TESTING
    if (NativeHudHooks::editor == this) NativeHudHooks::Remove();
#endif
    initialized_ = false;
    for (auto& bounds : bounds_) bounds = {};
}

} // namespace keybinder
