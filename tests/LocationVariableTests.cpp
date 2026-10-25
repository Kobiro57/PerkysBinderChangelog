#include "../include/ActionExecutor.h"
#include "../include/LocationNames.h"
#include "../include/MessageVariables.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#ifdef LOCATION_VARIABLE_TEST_MAIN
#include <atomic>
std::atomic<int> g_shutdown_phase{0};
#endif

namespace {
int checks = 0;
int failures = 0;
#define LOCATION_CHECK(condition) do { ++checks; if (!(condition)) { ++failures; std::printf("[FAIL] LocationVariables line %d: %s\n", __LINE__, #condition); } } while (0)

struct Bridge : keybinder::SAMPBridgeInterface {
    mutable std::vector<std::string> chat;
    mutable std::vector<std::string> commands;
    bool SendChat(const std::string& text) const override { chat.push_back(text); return true; }
    bool SendCommand(const std::string& text) const override { commands.push_back(text); return true; }
};

struct Data : keybinder::GameDataProvider {
    int cityLevel = 1;
    std::string district = "Market";
    float GetPlayerHP() const override { return 67.0f; }
    float GetPlayerArmour() const override { return 12.0f; }
    float GetVehicleHP() const override { return 1000.0f; }
    int GetTotalKills() const override { return 12; }
    int GetTotalDeaths() const override { return 3; }
    std::string GetVehicleName() const override { return "Bobcat"; }
    std::string GetCityName() const override { return keybinder::CityNameFromGtaLevel(cityLevel); }
    std::string GetDistrictName() const override { return district; }
};

void TestCityAndDistrictAreDifferentValues() {
    Bridge bridge;
    Data data;
    keybinder::ActionExecutor executor;
    executor.SetSAMPBridge(&bridge);
    executor.SetGameDataProvider(&data);
    executor.Execute({keybinder::Action(keybinder::ActionType::Chat, "{city}|{bezirk}|{zone}|{hp}|{weste}|{total_kills}|{kd}")});
    executor.Update();
    LOCATION_CHECK(bridge.chat.size() == 1);
    LOCATION_CHECK(bridge.chat.back() == "Los Santos|Market|Market|67|12|12|4.00");
    LOCATION_CHECK(bridge.commands.empty());
    data.cityLevel = 2;
    data.district = "Garcia";
    executor.Execute({keybinder::Action(keybinder::ActionType::Command, "/r {city}: {bezirk}")});
    executor.Update();
    LOCATION_CHECK(bridge.commands.size() == 1);
    LOCATION_CHECK(bridge.commands.back() == "/r San Fierro: Garcia");
    data.cityLevel = 0; // GTA handles the actual boundary, not a copied approximation.
    data.district = "Blueberry";
    executor.Execute({keybinder::Action(keybinder::ActionType::Chat, "{city}|{bezirk}")});
    executor.Update();
    LOCATION_CHECK(bridge.chat.back() == "San Andreas|Blueberry");
    LOCATION_CHECK(std::string(keybinder::CityNameFromGtaLevel(3)) == "Las Venturas");
    LOCATION_CHECK(std::string(keybinder::CityNameFromGtaLevel(-1)) == "Unknown");
    LOCATION_CHECK(std::string(keybinder::CityNameFromGtaLevel(4)) == "Unknown");
}

void TestMissingLocationDoesNotQueryServer() {
    Bridge bridge;
    keybinder::ActionExecutor executor;
    executor.SetSAMPBridge(&bridge);
    executor.Execute({keybinder::Action(keybinder::ActionType::Chat, "{city}|{bezirk}")});
    executor.Update();
    LOCATION_CHECK(bridge.chat.size() == 1);
    LOCATION_CHECK(bridge.chat.back() == "Unknown|Unknown");
    LOCATION_CHECK(bridge.commands.empty());
}

template <std::size_t Count>
void CheckCatalog(const keybinder::MessageVariableInfo (&catalog)[Count]) {
    std::set<std::string> names;
    bool hasCity = false, hasDistrict = false;
    for (const auto& variable : catalog) {
        std::string lower = variable.tag;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        LOCATION_CHECK(lower == variable.tag);
        LOCATION_CHECK(names.insert(lower).second);
        LOCATION_CHECK(lower != "victim" && lower != "killer" && lower != "weapon");
        LOCATION_CHECK(lower != "zone" && lower != "district");
        hasCity = hasCity || lower == "city";
        hasDistrict = hasDistrict || lower == "bezirk";
    }
    LOCATION_CHECK(hasCity);
    LOCATION_CHECK(hasDistrict);
}
} // namespace

int RunLocationVariableTests() {
    checks = 0;
    failures = 0;
    TestCityAndDistrictAreDifferentValues();
    TestMissingLocationDoesNotQueryServer();
    CheckCatalog(keybinder::kCombatMessageVariables);
    CheckCatalog(keybinder::kTextBindingVariables);
    std::printf("LocationVariableTests: %d checks, %d failures\n", checks, failures);
    return failures;
}

#ifdef LOCATION_VARIABLE_TEST_MAIN
int main() { return RunLocationVariableTests(); }
#endif
