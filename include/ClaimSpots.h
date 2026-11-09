#pragma once

#include <array>

namespace keybinder {

struct ClaimSpot {
    const char* name;
    float x;
    float y;
    float z;
};

// Fixed RCB flag positions supplied as AddPlayerClass saves. The map uses X/Y;
// height is retained for source review. Ped model, heading and weapons do not
// describe the map location and are deliberately not used as game actions.
inline constexpr std::array<ClaimSpot, 28> kRcbClaimSpots{{
    {"Montgomery", 1583.9396f, 33.5867f, 26.9868f},
    {"Palomino Creek", 1927.8734f, 168.8767f, 40.1172f},
    {"Palomino Creek Strand", 2160.4473f, -107.1974f, 2.7599f},
    {"Palomino Creek Friedhof", 2239.8594f, -86.8115f, 27.8548f},
    {"Palomino Creek Dach 1", 2311.3462f, -17.4462f, 32.5313f},
    {"Palomino Creek Dach 2", 2330.9189f, 80.9126f, 32.0073f},
    {"Palomino Creek Dach 3", 2547.1592f, 93.7197f, 29.8577f},
    {"Montgomery Schrottplatz", 1297.6686f, 182.1735f, 20.4670f},
    {"Montgomery Brauerrei 1", 1297.8884f, 308.8936f, 27.5555f},
    {"Montgomery Brauerrei 2", 1287.1136f, 270.3256f, 23.7625f},
    {"Montgomery Brauerrei 3", 1247.1160f, 367.3232f, 22.7848f},
    {"Montgomery Brauerrei 4", 1488.7997f, 361.4571f, 21.9852f},
    {"Montgomery Tanke", 1371.5627f, 471.9797f, 23.1484f},
    {"Blueberry Wohnwagen 1", 760.6830f, 381.3502f, 26.0898f},
    {"Blueberry Wohnwagen 2", 752.6452f, 259.9708f, 29.7391f},
    {"Fern Ridge J\xC3\xA4" "ger", 863.5613f, -23.6717f, 66.2610f},
    {"Dillimore", 645.8249f, -519.8103f, 21.1673f},
    {"Dillimore Seppli Haus", 743.5359f, -588.8473f, 20.5990f},
    {"Dillimore Dach 2", 803.7162f, -564.1726f, 21.3363f},
    {"Dillimore Hiltop", 1072.5286f, -291.1361f, 76.8663f},
    {"Blueberry Wohnwagen West", 252.7985f, -292.9166f, 1.5836f},
    {"Blueberry Papierrei", 193.9769f, -229.6732f, 7.0782f},
    {"Blueberry Pizzastack", 206.3966f, -179.7932f, 5.0786f},
    {"Blueberry Sprunk", 172.4167f, -161.3612f, 6.7786f},
    {"Blueberry Hinterhof", 183.5547f, -108.0306f, 2.0234f},
    {"Blueberry Gasse", 355.0618f, -76.3290f, 1.3705f},
    {"Blueberry Holzf\xC3\xA4" "ller 1", -437.4864f, -59.2712f, 62.0269f},
    {"Blueberry Holzf\xC3\xA4" "ller 2", -549.7010f, -202.3125f, 80.4654f},
}};

} // namespace keybinder
