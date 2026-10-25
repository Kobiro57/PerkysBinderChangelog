#include "../include/WeaponIconManager.h"
#include "../include/PersistentPaths.h"

#include <cstdio>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include <windows.h>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] WeaponIcons: %s (line %d)\n", #value, __LINE__); return; } } while (0)

struct TestAssets {
    std::vector<std::string> files;
    bool Add(const char* filename, int weapon = 24) {
        const auto* definition = keybinder::FindWeaponIconDefinition(weapon);
        if (!definition) return false;
        const std::string directory = keybinder::PersistentPaths::WeaponIconsDir() + "\\" + definition->folder;
        CreateDirectoryA(directory.c_str(), nullptr);
        const std::string path = directory + "\\" + filename;
        HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        // The manager's injected loader owns the test image-decoder decision.
        const unsigned char pngHeader[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        DWORD written = 0;
        const bool result = WriteFile(file, pngHeader, sizeof(pngHeader), &written, nullptr) && written == sizeof(pngHeader);
        CloseHandle(file);
        files.push_back(path);
        return result;
    }
    ~TestAssets() { for (const auto& file : files) DeleteFileA(file.c_str()); }
};

struct TextureTracker {
    int loads = 0, destroyed = 0;
    std::set<void*> alive;
    keybinder::WeaponIconManager::TestingHooks Hooks() {
        keybinder::WeaponIconManager::TestingHooks hooks;
        hooks.loadTexture = [&](const std::string& path) -> void* {
            ++loads;
            if (path.find("broken.png") != std::string::npos) return nullptr;
            void* texture = reinterpret_cast<void*>(static_cast<uintptr_t>(loads + 100));
            alive.insert(texture);
            return texture;
        };
        hooks.destroyTexture = [&](void* texture) { if (alive.erase(texture) != 1) ++failures; ++destroyed; };
        return hooks;
    }
};

void TestCatalogAndPathsAreWeaponScoped() {
    std::set<int> ids;
    for (const auto& weapon : keybinder::kWeaponIconCatalog) {
        CHECK(ids.insert(weapon.weaponId).second);
        CHECK(keybinder::FindWeaponIconDefinition(weapon.weaponId) == &weapon);
        const std::string path = std::string("Waffenicons\\") + weapon.folder + "\\icon.png";
        CHECK(keybinder::NormalizeWeaponIconPath(weapon.weaponId, path) == path);
    }
    CHECK(ids.size() == 43);
    for (int invalid : {-1, 0, 19, 20, 21, 47, 54, 100}) CHECK(!keybinder::FindWeaponIconDefinition(invalid));
    CHECK(keybinder::NormalizeWeaponIconPath(24, "waffenicons/desert eagle/Custom.PNG") == "Waffenicons\\Desert Eagle\\Custom.PNG");
    for (const char* path : {"C:\\icon.png", "\\server\\icon.png", "../icon.png", "Waffenicons/Desert Eagle/../icon.png",
        "Waffenicons/Desert Eagle/nested/icon.png", "Waffenicons/M4/icon.png", "Waffenicons/Desert Eagle/icon.jpg",
        "Waffenicons/Desert Eagle/icon.png ", "Waffenicons/Desert Eagle/icon.png:stream", "Waffenicons/Desert Eagle/.png"})
        CHECK(keybinder::NormalizeWeaponIconPath(24, path).empty());
}

void TestAutomaticChoiceExplicitSelectionAndOriginalFallback() {
    TestAssets assets;
    TextureTracker textures;
    keybinder::WeaponIconManager manager;
    manager.SetTestingHooks(textures.Hooks());
    manager.Initialize();
    CHECK(!manager.IsEnabled());
    CHECK(!manager.IsAppliedForWeapon(24));
    for (const auto& weapon : keybinder::kWeaponIconCatalog) {
        const auto path = keybinder::PersistentPaths::WeaponIconsDir() + "\\" + weapon.folder;
        CHECK(GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES);
    }
    CHECK(assets.Add("z_test.png"));
    CHECK(assets.Add("A_test.PNG"));
    CHECK(assets.Add("ignored.jpg"));
    CHECK(assets.Add("m4_test.png", 31));
    CHECK(manager.RefreshLibrary());
    const auto files = manager.GetAvailableIcons(24);
    CHECK(files.size() == 2);
    CHECK(files.front() == "Waffenicons\\Desert Eagle\\A_test.PNG");
    CHECK(textures.loads == 0);
    keybinder::Settings settings;
    settings.weapon_icons_enabled = true;
    CHECK(manager.ApplySettings(settings));
    CHECK(manager.IsAppliedForWeapon(24) && manager.IsAppliedForWeapon(31));
    CHECK(!manager.IsAppliedForWeapon(0));
    CHECK(manager.GetConfiguredIcon(24) == files.front());
    CHECK(textures.loads == 2);
    CHECK(manager.ApplySettings(settings));
    CHECK(textures.loads == 2); // unchanged selection reuses the loaded texture
    settings.weapon_icon_selections = {{24, "Waffenicons/Desert Eagle/z_test.png", true}, {31, "", false}};
    CHECK(manager.ApplySettings(settings));
    CHECK(manager.GetConfiguredIcon(24) == "Waffenicons\\Desert Eagle\\z_test.png");
    CHECK(manager.IsAppliedForWeapon(24) && !manager.IsAppliedForWeapon(31));
    CHECK(textures.loads == 3 && textures.destroyed == 2);
    settings.weapon_icon_selections[0].path = "Waffenicons/Desert Eagle/missing.png";
    CHECK(manager.ApplySettings(settings));
    CHECK(manager.GetConfiguredIcon(24).empty() && !manager.IsAppliedForWeapon(24));
    CHECK(textures.alive.empty());
    settings.weapon_icon_selections = {{24, "Waffenicons/M4/m4_test.png", true}, {31, "", false}};
    CHECK(manager.ApplySettings(settings));
    CHECK(!manager.IsAppliedForWeapon(24));
    manager.Shutdown();
    CHECK(textures.alive.empty());
}

void TestDecodeFailureReloadAndDeviceLifetime() {
    TestAssets assets;
    TextureTracker textures;
    keybinder::WeaponIconManager manager;
    manager.SetTestingHooks(textures.Hooks());
    manager.Initialize();
    CHECK(assets.Add("good.png"));
    CHECK(assets.Add("broken.png"));
    CHECK(manager.RefreshLibrary());
    keybinder::Settings settings;
    settings.weapon_icons_enabled = true;
    settings.weapon_icon_selections = {{24, "Waffenicons/Desert Eagle/broken.png", true}};
    CHECK(!manager.ApplySettings(settings));
    CHECK(!manager.IsAppliedForWeapon(24) && textures.alive.empty());
    CHECK(!manager.GetLastError().empty());
    settings.weapon_icon_selections[0].path = "Waffenicons/Desert Eagle/good.png";
    CHECK(manager.ApplySettings(settings));
    CHECK(manager.GetLastError().empty() && manager.IsAppliedForWeapon(24));
    const int loadedBefore = textures.loads;
    manager.OnDeviceLost();
    CHECK(!manager.IsAppliedForWeapon(24) && textures.alive.empty());
    CHECK(!manager.IsDeviceReadyForTesting());
    CHECK(manager.ApplySettings(settings));
    CHECK(textures.loads == loadedBefore);
    manager.OnDeviceReset();
    CHECK(manager.IsAppliedForWeapon(24) && textures.loads == loadedBefore + 1);
    CHECK(manager.RefreshLibrary());
    CHECK(manager.IsAppliedForWeapon(24) && textures.loads == loadedBefore + 2);
    settings.weapon_icons_enabled = false;
    CHECK(manager.ApplySettings(settings));
    CHECK(!manager.IsAppliedForWeapon(24) && textures.alive.empty());
    manager.Shutdown();
    CHECK(textures.destroyed == textures.loads - 1); // only the broken image was never created
}

void TestDeferredSettingsAndMissingFileOnRefresh() {
    TestAssets assets;
    TextureTracker textures;
    keybinder::WeaponIconManager manager;
    manager.SetTestingHooks(textures.Hooks());
    keybinder::Settings settings;
    settings.weapon_icons_enabled = true;
    settings.weapon_icon_selections = {{24, "Waffenicons/Desert Eagle/deferred.png", true}};
    CHECK(manager.ApplySettings(settings));
    CHECK(!manager.IsAppliedForWeapon(24));
    CHECK(!keybinder::PersistentPaths::EnsureLayout().empty());
    CHECK(assets.Add("deferred.png"));
    manager.Initialize();
    CHECK(manager.IsAppliedForWeapon(24));
    CHECK(textures.loads == 1);
    DeleteFileA(assets.files.back().c_str());
    CHECK(manager.RefreshLibrary());
    CHECK(!manager.IsAppliedForWeapon(24) && textures.alive.empty());
    manager.Shutdown();
}
} // namespace

int RunWeaponIconManagerTests() {
    checks = failures = 0;
    TestCatalogAndPathsAreWeaponScoped();
    TestAutomaticChoiceExplicitSelectionAndOriginalFallback();
    TestDecodeFailureReloadAndDeviceLifetime();
    TestDeferredSettingsAndMissingFileOnRefresh();
    std::printf("WeaponIconManagerTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
