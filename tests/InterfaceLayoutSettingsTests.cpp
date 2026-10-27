#include "../include/SettingsManager.h"
#include "../include/InterfaceLayout.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace {
int checks = 0, failures = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { ++failures; std::printf("[FAIL] InterfaceLayoutSettings line %d: %s\n", __LINE__, #condition); } } while (0)
void DefaultsAndRoundTrip() {
    keybinder::SettingsManager source;
    CHECK(source.LoadFromJson("{}"));
    for (const auto& preset : source.Get().interface_editor_saved_presets) {
        for (const auto& style : preset.layout) {
            CHECK(style.offset_x == 0.0f && style.offset_y == 0.0f && !style.custom_color);
        }
    }
    auto& presets = source.GetMutable().interface_editor_saved_presets;
    for (std::size_t slot = 0; slot < presets.size(); ++slot) {
        presets[slot].name = "Interface " + std::to_string(slot);
        for (std::size_t element = 0; element < keybinder::kHudElementCount; ++element) {
            auto& style = presets[slot].layout[element];
            style.offset_x = static_cast<float>(element) / 20.0f;
            style.offset_y = -static_cast<float>(slot) / 10.0f;
            style.custom_color = (element % 2) != 0;
            style.color = {0.1f, static_cast<float>(slot) / 10.0f, 0.3f, 0.4f};
        }
    }
    keybinder::SettingsManager restored;
    CHECK(restored.LoadFromJson(source.GetSettingsJson()));
    CHECK(restored.Get().interface_editor_saved_presets.size() == presets.size());
    for (std::size_t slot = 0; slot < presets.size(); ++slot) {
        const auto& actual = restored.Get().interface_editor_saved_presets[slot];
        CHECK(actual.name == presets[slot].name);
        for (std::size_t element = 0; element < keybinder::kHudElementCount; ++element) {
            const auto& expected = presets[slot].layout[element];
            const auto& style = actual.layout[element];
            CHECK(std::fabs(style.offset_x - expected.offset_x) < 0.00001f);
            CHECK(std::fabs(style.offset_y - expected.offset_y) < 0.00001f);
            CHECK(style.custom_color == expected.custom_color && style.color == expected.color);
        }
    }
}
void LegacyColorsAndExplicitOriginal() {
    keybinder::SettingsManager manager;
    CHECK(manager.LoadFromJson(R"({"interface_editor_saved_presets":[{"hp_color_r":0.2,"hp_color_g":0.3,"armor_color_b":0.1,"money_color_a":0.5}]})"));
    const auto& layout = manager.Get().interface_editor_saved_presets[0].layout;
    CHECK(layout[0].custom_color && layout[0].color[0] == 0.2f && layout[0].color[1] == 0.3f);
    CHECK(layout[1].custom_color && layout[1].color[2] == 0.1f);
    CHECK(layout[3].custom_color && layout[3].color[3] == 0.5f);
    CHECK(!layout[4].custom_color);
    CHECK(manager.LoadFromJson(R"({"interface_editor_saved_presets":[{"hp_color_r":0.2,"hud_health_custom_color":false,"hud_health_color_r":0.7}]})"));
    CHECK(!manager.Get().interface_editor_saved_presets[0].layout[0].custom_color);
    CHECK(manager.Get().interface_editor_saved_presets[0].layout[0].color[0] == 0.7f);
}
void InvalidRuntimeAndLoadedValues() {
    keybinder::SettingsManager manager;
    CHECK(manager.LoadFromJson(R"({"interface_editor_saved_presets":[{"hud_health_offset_x":9,"hud_health_offset_y":-9,"hud_health_custom_color":true,"hud_health_color_r":-1,"hud_health_color_g":8,"hud_radar_offset_x":0.3}]})"));
    const auto& layout = manager.Get().interface_editor_saved_presets[0].layout;
    CHECK(layout[0].offset_x == 1.0f && layout[0].offset_y == -1.0f);
    CHECK(layout[0].color[0] == 0.0f && layout[0].color[1] == 1.0f);
    CHECK(layout[10].offset_x == 0.3f && layout[10].offset_y == 0.0f);
    auto& style = manager.GetMutable().interface_editor_saved_presets[0].layout[0];
    style.offset_x = std::numeric_limits<float>::quiet_NaN();
    style.offset_y = std::numeric_limits<float>::infinity();
    style.color = {std::numeric_limits<float>::quiet_NaN(), -1.0f, 3.0f, 0.5f};
    keybinder::SettingsManager restored;
    CHECK(restored.LoadFromJson(manager.GetSettingsJson()));
    const auto& result = restored.Get().interface_editor_saved_presets[0].layout[0];
    CHECK(result.offset_x == 0.0f && result.offset_y == 0.0f);
    CHECK((result.color == std::array<float,4>{1.0f,0.0f,1.0f,0.5f}));
    CHECK(manager.LoadFromJson(R"({"interface_editor_saved_presets":[{"hud_health_offset_x":null,"hud_health_offset_y":2.5e-5,"hud_health_color_r":"invalid","hud_health_color_g":0.6}]})"));
    const auto& parsed = manager.Get().interface_editor_saved_presets[0].layout[0];
    CHECK(parsed.offset_x == 0.0f && parsed.offset_y == 2.5e-5f);
    CHECK(parsed.color[0] == 1.0f && parsed.color[1] == 0.6f);
    keybinder::SettingsManager tinyRoundTrip;
    CHECK(tinyRoundTrip.LoadFromJson(manager.GetSettingsJson()));
    CHECK(tinyRoundTrip.Get().interface_editor_saved_presets[0].layout[0].offset_y == 2.5e-5f);
}
}
int RunInterfaceLayoutSettingsTests() {
    checks = failures = 0;
    DefaultsAndRoundTrip(); LegacyColorsAndExplicitOriginal(); InvalidRuntimeAndLoadedValues();
    std::printf("InterfaceLayoutSettingsTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
