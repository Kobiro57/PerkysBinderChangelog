#pragma once

#include "SettingsManager.h"
#include "ResourceIds.h"
#include "plugin-sdk/plugin_sa/game_sa/CRadar.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

namespace keybinder {

enum class RadarSpritePack {
    Radar1,
    Radar2,
};

struct RadarSpriteResource {
    int radarSprite;
    int radar1ResourceId;
    int radar2ResourceId;
    const char* nativeName;
    const char* name;
    bool allowTextureNameMatch = true;
};

inline constexpr RadarSpriteResource kRadarSpriteResources[] = {
    {RADAR_SPRITE_CENTRE, IDR_RADAR_CENTRE, IDR_RADAR_CENTRE, "radar_centre", "radar centre"},
    {RADAR_SPRITE_MAP_HERE, IDR_RADAR_MAP_HERE, IDR_RADAR2_MAP_HERE, "arrow", "radar map-here arrow", false},
    {RADAR_SPRITE_NORTH, IDR_RADAR_NORTH, IDR_RADAR2_NORTH, "radar_north", "radar north"},
    {RADAR_SPRITE_AIRYARD, IDR_RADAR_AIRYARD, IDR_RADAR2_AIRYARD, "radar_airYard", "radar airyard"},
    {RADAR_SPRITE_AMMUGUN, IDR_RADAR_AMMUGUN, IDR_RADAR2_AMMUGUN, "radar_ammugun", "radar ammunation"},
    {RADAR_SPRITE_BARBERS, IDR_RADAR_BARBERS, IDR_RADAR2_BARBERS, "radar_barbers", "radar barbers"},
    {RADAR_SPRITE_BIGSMOKE, IDR_RADAR_BIGSMOKE, IDR_RADAR2_BIGSMOKE, "radar_BIGSMOKE", "radar Big Smoke"},
    {RADAR_SPRITE_BOATYARD, IDR_RADAR_BOATYARD, IDR_RADAR2_BOATYARD, "radar_boatyard", "radar boatyard"},
    {RADAR_SPRITE_BURGERSHOT, IDR_RADAR_BURGERSHOT, IDR_RADAR2_BURGERSHOT, "radar_burgerShot", "radar Burger Shot"},
    {RADAR_SPRITE_BULLDOZER, IDR_RADAR_BULLDOZER, IDR_RADAR2_BULLDOZER, "radar_bulldozer", "radar bulldozer"},
    {RADAR_SPRITE_CATALINAPINK, IDR_RADAR_CATALINAPINK, IDR_RADAR2_CATALINAPINK, "radar_CATALINAPINK", "radar Catalina"},
    {RADAR_SPRITE_CESARVIAPANDO, IDR_RADAR_CESARVIAPANDO, IDR_RADAR2_CESARVIAPANDO, "radar_CESARVIAPANDO", "radar Cesar Vialpando"},
    {RADAR_SPRITE_CHICKEN, IDR_RADAR_CHICKEN, IDR_RADAR2_CHICKEN, "radar_chicken", "radar chicken"},
    {RADAR_SPRITE_CJ, IDR_RADAR_CJ, IDR_RADAR2_CJ, "radar_CJ", "radar CJ"},
    {RADAR_SPRITE_CRASH1, IDR_RADAR_CRASH1, IDR_RADAR2_CRASH1, "radar_CRASH1", "radar CRASH"},
    {RADAR_SPRITE_DINER, IDR_RADAR_DINER, IDR_RADAR2_DINER, "radar_diner", "radar diner"},
    {RADAR_SPRITE_EMMETGUN, IDR_RADAR_EMMETGUN, IDR_RADAR2_EMMETGUN, "radar_emmetGun", "radar Emmet gun"},
    {RADAR_SPRITE_ENEMYATTACK, IDR_RADAR_ENEMYATTACK, IDR_RADAR2_ENEMYATTACK, "radar_enemyAttack", "radar enemy attack"},
    {RADAR_SPRITE_FIRE, IDR_RADAR_FIRE, IDR_RADAR2_FIRE, "radar_fire", "radar fire"},
    {RADAR_SPRITE_GIRLFRIEND, IDR_RADAR_GIRLFRIEND, IDR_RADAR2_GIRLFRIEND, "radar_girlfriend", "radar girlfriend"},
    {RADAR_SPRITE_HOSTPITAL, IDR_RADAR_HOSTPITAL, IDR_RADAR2_HOSTPITAL, "radar_hostpitaL", "radar hospital"},
    {RADAR_SPRITE_LOGOSYNDICATE, IDR_RADAR_LOCOSYNDICATE, IDR_RADAR2_LOCOSYNDICATE, "radar_LocoSyndicate", "radar Loco Syndicate"},
    {RADAR_SPRITE_MADDOG, IDR_RADAR_MADDOG, IDR_RADAR2_MADDOG, "radar_MADDOG", "radar Madd Dogg"},
    {RADAR_SPRITE_MAFIACASINO, IDR_RADAR_MAFIACASINO, IDR_RADAR2_MAFIACASINO, "radar_mafiaCasino", "radar mafia casino"},
    {RADAR_SPRITE_MCSTRAP, IDR_RADAR_MCSTRAP, IDR_RADAR2_MCSTRAP, "radar_MCSTRAP", "radar MC Strap"},
    {RADAR_SPRITE_MODGARAGE, IDR_RADAR_MODGARAGE, IDR_RADAR2_MODGARAGE, "radar_modGarage", "radar mod garage"},
    {RADAR_SPRITE_OGLOC, IDR_RADAR_OGLOC, IDR_RADAR2_OGLOC, "radar_OGLOC", "radar OG Loc"},
    {RADAR_SPRITE_PIZZA, IDR_RADAR_PIZZA, IDR_RADAR2_PIZZA, "radar_pizza", "radar pizza"},
    {RADAR_SPRITE_POLICE, IDR_RADAR_POLICE, IDR_RADAR2_POLICE, "radar_police", "radar police"},
    {RADAR_SPRITE_PROPERTYG, IDR_RADAR_PROPERTYG, IDR_RADAR2_PROPERTYG, "radar_propertyG", "radar property green"},
    {RADAR_SPRITE_PROPERTYR, IDR_RADAR_PROPERTYR, IDR_RADAR2_PROPERTYR, "radar_propertyR", "radar property red"},
    {RADAR_SPRITE_RACE, IDR_RADAR_RACE, IDR_RADAR2_RACE, "radar_race", "radar race"},
    {RADAR_SPRITE_RYDER, IDR_RADAR_RYDER, IDR_RADAR2_RYDER, "radar_RYDER", "radar Ryder"},
    {RADAR_SPRITE_SAVEGAME, IDR_RADAR_SAVEGAME, IDR_RADAR2_SAVEGAME, "radar_saveGame", "radar save game"},
    {RADAR_SPRITE_SCHOOL, IDR_RADAR_SCHOOL, IDR_RADAR2_SCHOOL, "radar_school", "radar school"},
    {RADAR_SPRITE_QMARK, IDR_RADAR_QMARK, IDR_RADAR2_QMARK, "radar_qmark", "radar question mark"},
    {RADAR_SPRITE_SWEET, IDR_RADAR_SWEET, IDR_RADAR2_SWEET, "radar_SWEET", "radar Sweet"},
    {RADAR_SPRITE_TATTOO, IDR_RADAR_TATTOO, IDR_RADAR2_TATTOO, "radar_tattoo", "radar tattoo"},
    {RADAR_SPRITE_THETRUTH, IDR_RADAR_THETRUTH, IDR_RADAR2_THETRUTH, "radar_THETRUTH", "radar The Truth"},
    {RADAR_SPRITE_WAYPOINT, IDR_RADAR_WAYPOINT, IDR_RADAR2_WAYPOINT, "radar_waypoint", "radar waypoint"},
    {RADAR_SPRITE_TORENORANCH, IDR_RADAR_TORENORANCH, IDR_RADAR2_TORENORANCH, "radar_TorenoRanch", "radar Toreno Ranch"},
    {RADAR_SPRITE_TRIADS, IDR_RADAR_TRIADS, IDR_RADAR2_TRIADS, "radar_triads", "radar triads"},
    {RADAR_SPRITE_TRIADSCASINO, IDR_RADAR_TRIADSCASINO, IDR_RADAR2_TRIADSCASINO, "radar_triadsCasino", "radar triads casino"},
    {RADAR_SPRITE_TSHIRT, IDR_RADAR_TSHIRT, IDR_RADAR2_TSHIRT, "radar_tshirt", "radar t-shirt"},
    {RADAR_SPRITE_WOOZIE, IDR_RADAR_WOOZIE, IDR_RADAR2_WOOZIE, "radar_WOOZIE", "radar Woozie"},
    {RADAR_SPRITE_ZERO, IDR_RADAR_ZERO, IDR_RADAR2_ZERO, "radar_ZERO", "radar Zero"},
    {RADAR_SPRITE_DATEDISCO, IDR_RADAR_DATEDISCO, IDR_RADAR2_DATEDISCO, "radar_dateDisco", "radar date disco"},
    {RADAR_SPRITE_DATEDRINK, IDR_RADAR_DATEDRINK, IDR_RADAR2_DATEDRINK, "radar_dateDrink", "radar date drink"},
    {RADAR_SPRITE_DATEFOOD, IDR_RADAR_DATEFOOD, IDR_RADAR2_DATEFOOD, "radar_dateFood", "radar date food"},
    {RADAR_SPRITE_TRUCK, IDR_RADAR_TRUCK, IDR_RADAR2_TRUCK, "radar_truck", "radar truck"},
    {RADAR_SPRITE_CASH, IDR_RADAR_CASH, IDR_RADAR2_CASH, "radar_cash", "radar cash"},
    {RADAR_SPRITE_FLAG, IDR_RADAR_FLAG, IDR_RADAR2_FLAG, "radar_flag", "radar flag"},
    {RADAR_SPRITE_GYM, IDR_RADAR_GYM, IDR_RADAR2_GYM, "radar_gym", "radar gym"},
    {RADAR_SPRITE_IMPOUND, IDR_RADAR_IMPOUND, IDR_RADAR2_IMPOUND, "radar_impound", "radar impound"},
    {RADAR_SPRITE_LIGHT, IDR_RADAR_LIGHT, IDR_RADAR2_LIGHT, "radar_light", "radar light"},
    {RADAR_SPRITE_RUNWAY, IDR_RADAR_RUNWAY, IDR_RADAR2_RUNWAY, "radar_runway", "radar runway"},
    {RADAR_SPRITE_GANGB, IDR_RADAR_GANGB, IDR_RADAR2_GANGB, "radar_gangB", "radar gang B"},
    {RADAR_SPRITE_GANGP, IDR_RADAR_GANGP, IDR_RADAR2_GANGP, "radar_gangP", "radar gang P"},
    {RADAR_SPRITE_GANGY, IDR_RADAR_GANGY, IDR_RADAR2_GANGY, "radar_gangY", "radar gang Y"},
    {RADAR_SPRITE_GANGN, IDR_RADAR_GANGN, IDR_RADAR2_GANGN, "radar_gangN", "radar gang N"},
    {RADAR_SPRITE_GANGG, IDR_RADAR_GANGG, IDR_RADAR2_GANGG, "radar_gangG", "radar gang G"},
    {RADAR_SPRITE_SPRAY, IDR_RADAR_SPRAY, IDR_RADAR2_SPRAY, "radar_spray", "radar spray"},
};

inline constexpr std::size_t kRadarSpriteResourceCount = sizeof(kRadarSpriteResources) / sizeof(kRadarSpriteResources[0]);

inline std::string NormalizeRadarTextureName(std::string name) {
    const std::size_t colon = name.find(':');
    if (colon != std::string::npos) {
        name.erase(0, colon + 1);
    }

    std::replace(name.begin(), name.end(), '/', '\\');
    const std::size_t slash = name.find_last_of('\\');
    if (slash != std::string::npos) {
        name.erase(0, slash + 1);
    }

    constexpr const char* pngSuffix = ".png";
    if (name.size() > 4) {
        std::string suffix = name.substr(name.size() - 4);
        for (char& ch : suffix) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (suffix == pngSuffix) {
            name.resize(name.size() - 4);
        }
    }

    for (char& ch : name) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return name;
}

inline RadarSpritePack NormalizeRadarSpritePack(std::string pack) {
    for (char& ch : pack) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return pack == "radar2" ? RadarSpritePack::Radar2 : RadarSpritePack::Radar1;
}

inline RadarSpritePack ResolveActiveRadarSpritePack(const Settings* settings) {
    return NormalizeRadarSpritePack(settings != nullptr ? settings->quickmap_radar_pack : std::string());
}

inline const RadarSpriteResource* FindRadarSpriteResource(int radarSprite) {
    for (const RadarSpriteResource& resource : kRadarSpriteResources) {
        if (resource.radarSprite == radarSprite) {
            return &resource;
        }
    }
    return nullptr;
}

inline const RadarSpriteResource* FindRadarSpriteResourceByTextureName(const std::string& textureName) {
    const std::string normalized = NormalizeRadarTextureName(textureName);
    if (normalized.empty()) {
        return nullptr;
    }

    for (const RadarSpriteResource& resource : kRadarSpriteResources) {
        if (resource.allowTextureNameMatch && NormalizeRadarTextureName(resource.nativeName) == normalized) {
            return &resource;
        }
    }
    return nullptr;
}

inline int ResolveRadarSpriteResourceId(const RadarSpriteResource& resource, RadarSpritePack pack) {
    return pack == RadarSpritePack::Radar2 ? resource.radar2ResourceId : resource.radar1ResourceId;
}


} // namespace keybinder
