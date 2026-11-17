#include "../include/SprayAccess.h"
#include <cstdio>

int RunSprayAccessTests() {
    int checks = 0, failures = 0;
    auto check = [&](bool value) { ++checks; if (!value) { ++failures; std::printf("[FAIL] SprayAccess check %d\n", checks); } };
    for (const std::string name : {"SAPD", "LSPD", "Army", "Fire Department", "Red County Brotherhood", "RCB", "Grove Street", "Ballas", "Meine Gang"}) {
        check(keybinder::CanUseSprayTags(name));
        check(keybinder::CanUseSprayTags(name, name));
        check(!keybinder::CanUseSprayTags(name, std::string("Zivilisten")));
    }
    for (const std::string name : {"", " ", "Zivilist", "Zivilisten", "{FFFFFF}ZiViLiStEn", "Civilian", "none", "keine Fraktion", "-", "0", "Unknown"})
        check(!keybinder::CanUseSprayTags(name));
    check(keybinder::CanUseSprayTags("Zivilist", std::string("Grove Street"))); // Current gang is independent of faction membership.
    check(!keybinder::CanUseSprayTags("", std::string("Grove Street"))); // API alone cannot authorize a different server/account.
    check(!keybinder::CanUseSprayTags("Unknown", std::string("Ballas")));
    check(keybinder::CanUseSprayTags("Army", std::string{}));
    std::printf("SprayAccessTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
