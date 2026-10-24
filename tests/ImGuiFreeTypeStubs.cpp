#define IMGUI_DEFINE_MATH_OPERATORS
#include "../include/imgui-1.92.7/imgui.h"
#include "../include/imgui-1.92.7/imgui_internal.h"
#include "../include/imgui-1.92.7/misc/freetype/imgui_freetype.h"

// Stub font loader for test builds: provides a valid ImFontLoader with no-op
// callbacks so io.Fonts->Build() succeeds without real FreeType.
static bool StubFontSrcInit(ImFontAtlas*, ImFontConfig*) { return true; }
static void StubFontSrcDestroy(ImFontAtlas*, ImFontConfig*) {}
static bool StubFontSrcContainsGlyph(ImFontAtlas*, ImFontConfig*, ImWchar) { return false; }
static bool StubFontBakedLoadGlyph(ImFontAtlas*, ImFontConfig*, ImFontBaked*, void*, ImWchar, ImFontGlyph*, float*) { return false; }

static ImFontLoader g_stub_font_loader = []() {
    ImFontLoader loader;  // memsets to zero via default ctor
    loader.Name = "StubFreeType";
    loader.FontSrcInit = StubFontSrcInit;
    loader.FontSrcDestroy = StubFontSrcDestroy;
    loader.FontSrcContainsGlyph = StubFontSrcContainsGlyph;
    loader.FontBakedLoadGlyph = StubFontBakedLoadGlyph;
    // LoaderInit, LoaderShutdown, FontBakedInit, FontBakedDestroy left null (guarded in imgui)
    // FontBakedSrcLoaderDataSize left 0 (no per-baked-source data)
    return loader;
}();

namespace ImGuiFreeType {
    IMGUI_API const ImFontLoader* GetFontLoader() { return &g_stub_font_loader; }
    IMGUI_API bool DebugEditFontLoaderFlags(unsigned int*) { return false; }
}