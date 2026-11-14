#include "kmbox_identity.h"
#include <algorithm>
#include <cctype>
#include <fstream>

namespace {

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string Strip0x(std::string s) {
    s = Lower(std::move(s));
    if (s.rfind("0x", 0) == 0)
        s.erase(0, 2);
    return s;
}

} // namespace

namespace KmboxIdentity {

Resolved Resolve(const std::string& presetIn,
                 const std::string& vid,
                 const std::string& pid,
                 const std::string& manufacturer,
                 const std::string& product,
                 const std::string& serial,
                 const std::string& bcdDevice) {
    Resolved out;
    out.preset = Lower(presetIn.empty() ? "logitech_lightspeed_c547" : presetIn);
    out.vid = Strip0x(vid);
    out.pid = Strip0x(pid);
    out.manufacturer = manufacturer;
    out.product = product;
    out.serial = serial;
    out.bcdDevice = Strip0x(bcdDevice);

    if (out.preset == "logitech_lightspeed_c547" || out.preset == "logitech_c547") {
        out.preset = "logitech_lightspeed_c547";
        out.vid = "046d";
        out.pid = "c547";
        if (out.manufacturer.empty())
            out.manufacturer = "Logitech";
        if (out.product.empty())
            out.product = "USB Receiver";
        if (out.bcdDevice.empty())
            out.bcdDevice = "0110";
        return out;
    }

    if (out.preset == "stock") {
        out.vid.clear();
        out.pid.clear();
        out.manufacturer.clear();
        out.product.clear();
        out.serial.clear();
        out.bcdDevice.clear();
        return out;
    }

    // custom — keep caller-supplied vid/pid/optional strings
    out.preset = "custom";
    return out;
}

bool WriteIdentityTextFile(const std::filesystem::path& file, const Resolved& id) {
    std::ofstream out(file);
    if (!out)
        return false;
    out << "# USB identity toward the GAME PC (HID port). Serial COM unchanged.\n"
        << "# ROBRUN_IDENTITY_V3 — vendor device.* (not full descriptor clone).\n"
        << "# Empty vid/pid = stock KMBox. Re-flash + power-cycle both USBs after edits.\n"
        << "#\n";
    if (!id.preset.empty() && id.preset != "custom")
        out << "preset=" << id.preset << "\n";
    out << "vid=" << id.vid << "\n"
        << "pid=" << id.pid << "\n";
    if (!id.manufacturer.empty())
        out << "manufacturer=" << id.manufacturer << "\n";
    if (!id.product.empty())
        out << "product=" << id.product << "\n";
    if (!id.serial.empty())
        out << "serial=" << id.serial << "\n";
    if (!id.bcdDevice.empty())
        out << "bcd_device=" << id.bcdDevice << "\n";
    return true;
}

} // namespace KmboxIdentity
