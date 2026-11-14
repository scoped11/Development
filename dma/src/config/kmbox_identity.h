#pragma once
#include <filesystem>
#include <string>

namespace KmboxIdentity {

struct Resolved {
    std::string preset; // stock | custom | logitech_lightspeed_c547 | ...
    std::string vid;
    std::string pid;
    std::string manufacturer;
    std::string product;
    std::string serial;
    std::string bcdDevice;
    bool stock() const { return vid.empty() && pid.empty(); }
};

// Maps assist.hidPreset + custom fields into values for kmbox_identity.txt.
Resolved Resolve(const std::string& preset,
                 const std::string& vid,
                 const std::string& pid,
                 const std::string& manufacturer,
                 const std::string& product,
                 const std::string& serial,
                 const std::string& bcdDevice);

bool WriteIdentityTextFile(const std::filesystem::path& file, const Resolved& id);

} // namespace KmboxIdentity
