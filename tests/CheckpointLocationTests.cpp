#include "../include/CheckpointLocation.h"
#include <cstdio>
#include <limits>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] CheckpointLocation: %s (line %d)\n", #value, __LINE__); } } while (0)
using Zone = keybinder::CheckpointLocationZone;

void TargetCityAndDistrictComeFromTheSameCoordinates() {
    // Representative nested navigation/map records, deliberately shuffled.
    const std::vector<Zone> nav{
        {-3000, -3000, -1000, 3000, 3000, 1000, 0, 0, "San Andreas"},
        {600, -1800, -100, 1500, -1000, 200, 0, 1, "Market"},
        {1500, 100, -100, 2400, 500, 200, 0, 0, "Palomino Creek"},
        {-100, -500, -100, 3000, 1600, 1000, 0, 0, "Red County"},
        {500, -2500, -100, 3000, -600, 1000, 0, 1, "Los Santos"},
        {900, -1500, -100, 1100, -1200, 200, 2, 1, "Gang territory, not a district"},
        {-2600, -2100, -100, -2100, -1100, 200, 1, 2, "Foster Valley"},
        {2000, 2000, -100, 2600, 2800, 200, 0, 3, "Prickle Pine"}};
    const std::vector<Zone> map{
        {-3000, -3000, -1000, 3000, 3000, 1000, 3, 0, "San Andreas"},
        {-100, -500, -1000, 3000, 1600, 1000, 3, 0, "Red County"},
        {500, -2500, -1000, 3000, -600, 1000, 3, 1, "Los Santos"},
        {-3000, -2600, -1000, -1600, 1800, 1000, 3, 2, "San Fierro"},
        {900, 1700, -1000, 3000, 3000, 1000, 3, 3, "Las Venturas"}};
    CHECK(keybinder::ResolveCheckpointLocation(1000, -1400, 10, nav, map) == "Los Santos Market");
    CHECK(keybinder::ResolveCheckpointLocation(1927.8734f, 168.8767f, 40.1172f, nav, map) == "Red County Palomino Creek");
    CHECK(keybinder::ResolveCheckpointLocation(-2300, -1500, 30, nav, map) == "San Fierro Foster Valley");
    CHECK(keybinder::ResolveCheckpointLocation(2300, 2300, 30, nav, map) == "Las Venturas Prickle Pine");
    // Map targets above/below the ground retain their actual horizontal district.
    for (float z : {-5000.0f, -1000.0f, 201.0f, 1000.0f, 15000.0f})
        CHECK(keybinder::ResolveCheckpointLocation(1000, -1400, z, nav, map) == "Los Santos Market");
    auto reversed = nav;
    std::reverse(reversed.begin(), reversed.end());
    CHECK(keybinder::ResolveCheckpointLocation(1000, -1400, 10, reversed, map) == "Los Santos Market");
    CHECK(keybinder::ResolveCheckpointLocation(1000, -1400, 10, nav, {}) == "Los Santos Market");
    CHECK(keybinder::ResolveCheckpointLocation(1000, -1400, 10, nav, {map.front()}) == "Los Santos Market");
    CHECK(keybinder::ResolveCheckpointLocation(1927, 168, 40, nav, {}) == "Palomino Creek");
}

void InvalidOrMissingDataNeverInventsATargetLocation() {
    const Zone valid{600, -1800, -100, 1500, -1000, 200, 0, 1, "Market"};
    CHECK(keybinder::ResolveCheckpointLocation(1000, -1400, 10, {}, {}).empty());
    CHECK(keybinder::ResolveCheckpointLocation(1900, 100, 10, {valid}, {}).empty());
    for (float bad : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 20001.0f}) {
        CHECK(keybinder::ResolveCheckpointLocation(bad, -1400, 10, {valid}, {}).empty());
        CHECK(keybinder::ResolveCheckpointLocation(1000, bad, 10, {valid}, {}).empty());
        CHECK(keybinder::ResolveCheckpointLocation(1000, -1400, bad, {valid}, {}).empty());
    }
    for (auto bad : {Zone{1500, -1800, -100, 600, -1000, 200, 0, 1, "Market"},
        Zone{600, -1000, -100, 1500, -1800, 200, 0, 1, "Market"},
        Zone{600, -1800, 200, 1500, -1000, -100, 0, 1, "Market"},
        Zone{600, -1800, -100, 1500, -1000, 200, 3, 1, "Market"},
        Zone{600, -1800, -100, 1500, -1000, 200, 2, 1, "Market"}})
        CHECK(keybinder::ResolveCheckpointLocation(1000, -1400, 10, {bad}, {}).empty());
    for (int field = 0; field < 6; ++field) {
        auto bad = valid;
        switch (field) {
        case 0: bad.x1 = std::numeric_limits<float>::quiet_NaN(); break;
        case 1: bad.y1 = std::numeric_limits<float>::quiet_NaN(); break;
        case 2: bad.z1 = std::numeric_limits<float>::quiet_NaN(); break;
        case 3: bad.x2 = std::numeric_limits<float>::infinity(); break;
        case 4: bad.y2 = std::numeric_limits<float>::infinity(); break;
        case 5: bad.z2 = std::numeric_limits<float>::infinity(); break;
        }
        CHECK(keybinder::ResolveCheckpointLocation(1000, -1400, 10, {bad}, {}).empty());
    }
    for (auto point : {std::pair<float, float>{600, -1800}, {1500, -1000}, {600, -1000}, {1500, -1800}})
        CHECK(keybinder::ResolveCheckpointLocation(point.first, point.second, 10, {valid}, {}) == "Los Santos Market");
    for (auto text : {"", "Unknown", "Market\n/r injected", "{FFFFFF}Market", "[Market]"}) {
        auto bad = valid; bad.name = text;
        CHECK(keybinder::ResolveCheckpointLocation(1000, -1400, 10, {bad}, {}).empty());
    }
    CHECK(keybinder::FormatCheckpointLocation(" Los Santos ", " Market ") == "Los Santos Market");
    CHECK(keybinder::FormatCheckpointLocation("Los Santos", "los santos") == "Los Santos");
    CHECK(keybinder::FormatCheckpointLocation("Los Santos", "").empty());
    CHECK(keybinder::FormatCheckpointLocation("Unknown", "Market") == "Market");
}

void NativeZoneRecordUsesGameLayoutRatherThanLegacySdkStride() {
    unsigned char bytes[64]{}; // Two native 0x20-byte records.
    std::memcpy(bytes + 8, "MARKET", 6);
    const std::int16_t minimumX = 600, maximumX = 1500;
    std::memcpy(bytes + 0x10, &minimumX, sizeof(minimumX));
    std::memcpy(bytes + 0x16, &maximumX, sizeof(maximumX));
    bytes[0x1e] = 1; bytes[0x1f] = 1;
    bytes[0x20 + 0x1e] = 3; bytes[0x20 + 0x1f] = 2;
    keybinder::NativeCheckpointZoneRecord first{}, second{};
    std::memcpy(&first, bytes, sizeof(first));
    std::memcpy(&second, bytes + sizeof(first), sizeof(second));
    CHECK(first.x1 == 600 && first.x2 == 1500);
    CHECK(first.type == 1 && first.level == 1);
    CHECK(second.type == 3 && second.level == 2);
    CHECK(std::string(first.textLabel, 6) == "MARKET");
}
} // namespace

int RunCheckpointLocationTests() {
    checks = failures = 0;
    TargetCityAndDistrictComeFromTheSameCoordinates();
    InvalidOrMissingDataNeverInventsATargetLocation();
    NativeZoneRecordUsesGameLayoutRatherThanLegacySdkStride();
    std::printf("CheckpointLocationTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
