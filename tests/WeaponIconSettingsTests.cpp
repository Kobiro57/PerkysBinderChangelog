#include "../include/SettingsManager.h"
#include "../include/WeaponIconCatalog.h"
#include <cstdio>
#include <string>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] WeaponIconSettings line %d: %s\n", __LINE__, #value); } } while (0)

void TestDefaultsAndRoundTripPreservePerWeaponChoices() {
    keybinder::SettingsManager manager;
    CHECK(!manager.Get().weapon_icons_enabled);
    CHECK(manager.Get().weapon_icon_selections.empty());
    CHECK(manager.LoadFromJson("{}"));
    CHECK(!manager.Get().weapon_icons_enabled);
    auto& settings = manager.GetMutable();
    settings.weapon_icons_enabled = true;
    settings.weapon_icon_selections = {
        {24, "Waffenicons/Desert Eagle/mein icon.PNG", true},
        {31, "Waffenicons\\M4\\rot.png", false},
        {22, "", false},
        {30, "", true}
    };
    keybinder::SettingsManager restored;
    CHECK(restored.LoadFromJson(manager.GetSettingsJson()));
    CHECK(restored.Get().weapon_icons_enabled);
    const auto& selections = restored.Get().weapon_icon_selections;
    CHECK(selections.size() == 4);
    if (selections.size() != 4) return;
    CHECK(selections[0].weaponId == 24 && selections[0].enabled);
    CHECK(selections[0].path == "Waffenicons\\Desert Eagle\\mein icon.PNG");
    CHECK(selections[1].weaponId == 31 && !selections[1].enabled);
    CHECK(selections[1].path == "Waffenicons\\M4\\rot.png");
    CHECK(selections[2].weaponId == 22 && !selections[2].enabled && selections[2].path.empty());
    CHECK(selections[3].weaponId == 30 && selections[3].enabled && selections[3].path.empty());
}

void TestInvalidIdsDuplicateChoicesAndUnsafePathsNeverPersist() {
    keybinder::SettingsManager manager;
    CHECK(manager.LoadFromJson(R"({"weapon_icons_enabled":true,"weapon_icon_selections":[
      {"weapon_id":24,"path":"Waffenicons/Desert Eagle/first.png","enabled":false},
      {"weapon_id":24,"path":"Waffenicons/Desert Eagle/second.png","enabled":true},
      {"weapon_id":0,"path":"Waffenicons/Fist/icon.png"},
      {"weapon_id":19,"path":"Waffenicons/Rocket/icon.png"},
      {"weapon_id":999,"path":"Waffenicons/Unknown/icon.png"},
      {"weapon_id":31,"path":"Waffenicons/M4/../../outside.png"},
      {"weapon_id":30,"path":"C:/outside.png"},
      {"weapon_id":22,"path":"Waffenicons/Desert Eagle/wrong weapon.png"},
      {"weapon_id":23,"path":"Waffenicons/Silenced Pistol/not an image.exe"}
    ]})"));
    CHECK(manager.Get().weapon_icons_enabled);
    const auto& selections = manager.Get().weapon_icon_selections;
    CHECK(selections.size() == 5);
    if (!selections.empty()) CHECK(selections[0].weaponId == 24 && !selections[0].enabled && selections[0].path == "Waffenicons\\Desert Eagle\\first.png");
    for (std::size_t index = 1; index < selections.size(); ++index) CHECK(!selections[index].enabled && selections[index].path.empty());
    const auto json = manager.GetSettingsJson();
    CHECK(json.find("outside.png") == std::string::npos);
    CHECK(json.find("not an image.exe") == std::string::npos);
    CHECK(json.find("second.png") == std::string::npos);
    CHECK(json.find("wrong weapon.png") == std::string::npos);
}

void TestRuntimeMutationsAreFilteredBeforeSaving() {
    keybinder::SettingsManager manager;
    manager.GetMutable().weapon_icon_selections = {
        {24, "Waffenicons\\Desert Eagle\\valid.png", true},
        {24, "Waffenicons\\Desert Eagle\\duplicate.png", false},
        {31, "..\\outside.png", true},
        {-1, "", true}
    };
    keybinder::SettingsManager restored;
    CHECK(restored.LoadFromJson(manager.GetSettingsJson()));
    CHECK(restored.Get().weapon_icon_selections.size() == 2);
    if (!restored.Get().weapon_icon_selections.empty()) CHECK(restored.Get().weapon_icon_selections[0].path == "Waffenicons\\Desert Eagle\\valid.png");
    if (restored.Get().weapon_icon_selections.size() == 2) CHECK(restored.Get().weapon_icon_selections[1].weaponId == 31
        && !restored.Get().weapon_icon_selections[1].enabled && restored.Get().weapon_icon_selections[1].path.empty());
}
} // namespace

int RunWeaponIconSettingsTests() {
    checks = failures = 0;
    TestDefaultsAndRoundTripPreservePerWeaponChoices();
    TestInvalidIdsDuplicateChoicesAndUnsafePathsNeverPersist();
    TestRuntimeMutationsAreFilteredBeforeSaving();
    std::printf("WeaponIconSettingsTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
