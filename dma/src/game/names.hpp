#pragma once
#include "config/config.h"
#include <cctype>
#include <string>

namespace Names {

inline bool IsNumericId(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

inline std::string Lower(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline const char* AliasRaw(const std::string& id) {
    struct Pair { const char* raw; const char* name; };
    static const Pair kExact[] = {
        {"Jp7M1", "JP-7"}, {"Jp7M2", "JP-7"}, {"Jp7M3", "JP-7"},
        {"Dp9M1", "DP-9"}, {"Dp9M2", "DP-9"},
        {"Kl545M1", "KL-545"}, {"Kl545M2", "KL-545"}, {"Kl545M3", "KL-545"},
        {"Cb45M1", "CB-45"}, {"Cb45M2", "CB-45"}, {"Cb45M3", "CB-45"},
        {"LmgM2", "LMG"},
        {"Harris500M1", "Harris 500"}, {"Harris500M2", "Harris 500"}, {"Harris500M3", "Harris 500"},
        {"Drugs1", "Weed"}, {"Drugs2", "Weed"}, {"Drugs3", "Weed"},
        {"Drugs4", "Weed"}, {"Drugs5", "Weed"}, {"Drugs6", "Weed"},
        {"DrugsBricket1", "Drug brick"},
        {"PictureFake1", "Fake painting"},
        {"PictureTube1", "Painting"}, {"PictureTube2", "Painting"}, {"PictureTube3", "Painting"},
        {"Decoder1", "Decoder"}, {"Decoder2", "Decoder"}, {"Decoder3", "Decoder"},
        {"Cash1", "Cash"},
        {"Lockpick1", "Lockpick"},
        {"PoliceRadio1", "Police radio"},
    };
    for (const auto& p : kExact) {
        if (id == p.raw)
            return p.name;
    }
    static const Pair kPref[] = {
        {"Jp7", "JP-7"}, {"Dp9", "DP-9"}, {"Kl545", "KL-545"}, {"Cb45", "CB-45"},
        {"Lmg", "LMG"}, {"Harris", "Harris 500"},
        {"Drugs", "Weed"}, {"Picture", "Painting"}, {"Decoder", "Decoder"},
        {"RifleAmmo", "Rifle ammo"}, {"MpAmmo", "SMG ammo"}, {"ShotgunAmmo", "Shotgun ammo"},
        {"PistolAmmo", "Pistol ammo"}, {"ArmorPlate", "Armor plate"},
        {"Backpack", "Backpack"}, {"Vest", "Vest"}, {"Belt", "Belt"},
        {"Injector", "Injector"}, {"SafeKey", "Safe key"}, {"RoomKey", "Room key"},
        {"VipKey", "VIP key"}, {"GraphicsCard", "Graphics card"}, {"VideoCard", "Video card"},
        {"Alcohol", "Alcohol"}, {"Comics", "Comics"}, {"Journal", "Journal"},
        {"Laptop", "Laptop"}, {"Console", "Console"}, {"Phone", "Phone"},
        {"Watch", "Watch"}, {"Ring", "Ring"}, {"Necklace", "Necklace"},
        {"Earrings", "Earrings"}, {"Headphones", "Headphones"}, {"Camera", "Camera"},
        {"Pharma", "Pharma tools"}, {"Jewelry", "Jewelry pouch"}, {"Antique", "Antique set"},
        {"Frag", "Frag"}, {"Flash", "Flashbang"}, {"Smoke", "Smoke"},
        {"Toaster", "Toaster"}, {"Hairdryer", "Hairdryer"}, {"Cash", "Cash"},
    };
    for (const auto& p : kPref) {
        if (id.rfind(p.raw, 0) == 0)
            return p.name;
    }
    return nullptr;
}

inline std::string Pretty(std::string id) {
    if (id.empty() || IsNumericId(id))
        return {};
    if (const char* alias = AliasRaw(id))
        return alias;
    const auto slash = id.find_last_of("/.\\:");
    if (slash != std::string::npos)
        id.erase(0, slash + 1);
    static const char* kJunk[] = {
        "Location", "InventoryItems", "InventoryItem", "LootObject", "Description",
        "ContainerType", "Container", "Item"
    };
    for (const char* junk : kJunk) {
        if (id.rfind(junk, 0) == 0)
            id.erase(0, std::char_traits<char>::length(junk));
    }
    std::string out;
    out.reserve(id.size() + 8);
    for (size_t i = 0; i < id.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(id[i]);
        if (c == '_' || c == '-' || c == '.') {
            if (!out.empty() && out.back() != ' ')
                out.push_back(' ');
            continue;
        }
        if (i > 0 && std::isupper(c) && std::islower(static_cast<unsigned char>(id[i - 1])))
            out.push_back(' ');
        out.push_back(static_cast<char>(c));
    }
    while (!out.empty() && out.front() == ' ')
        out.erase(out.begin());
    if (!out.empty())
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

inline const char* ItemTypeLabel(int type) {
    switch (type) {
    case 0: return "Ammo";
    case 1: return "Armor";
    case 2: return "Armor plate";
    case 3: return "Assault rifle";
    case 4: return "Backpack";
    case 5: return "Belt";
    case 6: return "Pistol";
    case 7: return "Container key";
    case 8: return "Warehouse key";
    case 9: return "Lockpick";
    case 10: return "Police radio";
    case 11: return "Meds";
    case 12: return "Sell item";
    case 13: return "Shotgun";
    case 14: return "SMG";
    case 15: return "Cash";
    case 16: return "Grenade";
    case 17: return "Car";
    case 18: return "Helicopter";
    case 22: return "Furniture";
    case 29: return "Collectable";
    case 32: return "Decoder";
    case 33: return "Bill of sale";
    case 34: return "Police badge";
    case 35: return "Video card";
    case 38: return "Chip";
    case 39: return "Bolt cutter";
    case 40: return "Consumable";
    default: return nullptr;
    }
}

inline bool IsGunType(int type) {
    return type == 3 || type == 6 || type == 13 || type == 14;
}

inline int LootCategory(int type) {
    if (IsGunType(type) || type == 16) return 0; // weapons / grenades
    if (type == 11 || type == 40) return 1;      // meds
    if (type == 0) return 2;                     // ammo
    if (type == 12 || type == 15 || type == 29 || type == 32 || type == 35 || type == 38)
        return 3;                                // valuables
    return 4;
}

inline bool PassesLootFilter(int type, const ESPConfig& config) {
    switch (LootCategory(type)) {
    case 0: return config.lootWeapons;
    case 1: return config.lootMeds;
    case 2: return config.lootAmmo;
    case 3: return config.lootValuables;
    default: return config.lootOther;
    }
}

inline const char* FactionLabel(const std::string& role, bool teammate) {
    if (teammate) return "Teammate";
    const std::string r = Lower(role);
    if (r.find("cop") != std::string::npos && r.find("hub") == std::string::npos)
        return "Cop";
    if (r.find("robber") != std::string::npos)
        return "Robber";
    if (r.find("settler") != std::string::npos)
        return "Settler";
    if (r.find("prisoner") != std::string::npos)
        return "Prisoner";
    if (r.find("citizen") != std::string::npos)
        return "Civilian";
    if (r.find("hub") != std::string::npos)
        return "Hub";
    return role.empty() ? "Player" : role.c_str();
}

inline Colour RarityColour(int tier, const Colour& fallback) {
    switch (tier) {
    case 0: return Colour(0.62f, 0.62f, 0.62f, 1.f); // NoRare
    case 1: return Colour(0.86f, 0.86f, 0.88f, 1.f); // Common
    case 2: return Colour(0.22f, 0.82f, 0.34f, 1.f); // Uncommon green
    case 3: return Colour(0.28f, 0.52f, 0.98f, 1.f); // Rare blue
    case 4: return Colour(0.72f, 0.32f, 0.96f, 1.f); // Epic purple
    case 5: return Colour(0.98f, 0.78f, 0.16f, 1.f); // Legendary yellow
    case 6: return Colour(0.98f, 0.28f, 0.32f, 1.f); // Exotic
    default: return fallback;
    }
}

inline Colour RoleColour(const std::string& role, const ESPConfig& config) {
    const std::string r = Lower(role);
    if (r.find("cop") != std::string::npos && r.find("hub") == std::string::npos)
        return config.copColour;
    if (r.find("robber") != std::string::npos)
        return config.robberColour;
    if (r.find("settler") != std::string::npos)
        return config.settlerColour;
    if (r.find("prisoner") != std::string::npos)
        return config.prisonerColour;
    if (r.find("citizen") != std::string::npos)
        return config.civilianColour;
    return config.playerColour;
}

inline bool SameRole(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return false;
    return Lower(a) == Lower(b);
}

inline bool LooksLikeWeed(const std::string& name) {
    const std::string n = Lower(name);
    return n.find("weed") != std::string::npos || n.find("cannabis") != std::string::npos ||
           n.find("joint") != std::string::npos || n.find("baggie") != std::string::npos;
}

inline bool LooksLikePainting(const std::string& name) {
    const std::string n = Lower(name);
    return n.find("paint") != std::string::npos || n.find("canvas") != std::string::npos ||
           n.find("picture") != std::string::npos || n.find("art") != std::string::npos;
}

} // namespace Names
