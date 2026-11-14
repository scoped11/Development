// Driver2 external ESP for Rob Run (3rd-person shoulder camera).
// Requires mapped Driver2 session (--mode session or ranked-safe map).
//
//   robrun_esp_external.exe              # auto PID, console dots
//   robrun_esp_external.exe --overlay    # transparent GDI overlay

#include "robrun_attach.hpp"
#include "robrun_camera.hpp"
#include "robrun_mem.hpp"
#include "robrun_overlay.hpp"
#include "robrun_process.hpp"
#include "pf_watermark.hpp"
#include "../../../dma/src/engine/offsets.h"
#include "../../../dma/src/math/vector.h"
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

static void CollectPlayers(RobRunMem& mem, uintptr_t battle, std::vector<Vector3>& out)
{
    out.clear();
    uintptr_t world = mem.Read<uintptr_t>(battle + offsets::battle_entities::WorldEntities);
    if (!IsPtr(world))
        return;
    uintptr_t dict = mem.Read<uintptr_t>(world + offsets::world::BattleCharacters);
    std::vector<DictItem> items;
    if (!ReadIntDictionary(mem, dict, items, 300))
        return;
    uintptr_t localChar = 0;
    uintptr_t playerEnt = mem.Read<uintptr_t>(battle + offsets::battle_entities::Player);
    if (IsPtr(playerEnt))
        localChar = mem.Read<uintptr_t>(playerEnt + offsets::player_entity::ServerModel);

    for (const auto& item : items) {
        uintptr_t character = item.value;
        if (character == localChar)
            continue;
        Vector3 pos{};
        if (ReadCharacterPosition(mem, character, pos))
            out.push_back(pos);
    }
}

int main(int argc, char** argv)
{
    bool overlay = false;
    uint32_t pidArg = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--overlay")
            overlay = true;
        else
            pidArg = static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 0));
    }

    uint32_t pid = pidArg ? pidArg : FindRobRunPid();
    if (!pid) {
        printf("[!] ROB RUN.exe not found\n");
        return 1;
    }

    RobRunMem mem(pid);
    if (!mem.Connect()) {
        printf("[!] Driver2 session unavailable — map driver first (DriverLoader / ranked-safe)\n");
        return 2;
    }

    size_t gaSize = 0;
    uintptr_t gameAssembly = ModuleBase(pid, L"GameAssembly.dll", &gaSize);
    if (!gameAssembly) {
        printf("[!] GameAssembly.dll not loaded (pid=%u)\n", pid);
        return 3;
    }
    if (!offsets::IsPlausibleGameAssemblySize(gaSize)) {
        printf("[*] GameAssembly size 0x%zx — refresh dump if attach fails\n", gaSize);
    }

    printf("[+] pid=%u GameAssembly=0x%llx size=0x%zx\n",
        pid, (unsigned long long)gameAssembly, gaSize);

    PfWatermark brandMark;
    {
        std::wstring brand = PfBrandImagePathRobRun();
        if (!brand.empty()) {
            if (!brandMark.Create(brand, 132))
                printf("[*] brand watermark skipped\n");
        }
    }

    RobRunOverlay layer;
    if (overlay && !layer.Create()) {
        printf("[!] overlay create failed — continuing console-only\n");
        overlay = false;
    }

    RobRunWorld world{};
    EspCamera cam{};
    cam.screenW = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    cam.screenH = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));

    for (;;) {
        if (!world.battleEntities) {
            world = AttachBattleWorld(mem, pid, gameAssembly, gaSize);
            if (world.battleEntities) {
                printf("[+] BattleEntities @ 0x%llx camera=0x%llx\n",
                    (unsigned long long)world.battleEntities,
                    (unsigned long long)world.battleCamera);
            } else {
                printf("[*] waiting for match world...\n");
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
        } else if (!ValidateBattleEntities(mem, world.battleEntities)) {
            world = {};
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        cam.valid = false;
        CameraFromBattleModel(mem, world.battleCamera, cam);
        EnrichCameraFromLocalPlayer(mem, world.battleEntities, cam);
        if (!cam.valid) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        std::vector<Vector3> players;
        CollectPlayers(mem, world.battleEntities, players);

        std::vector<EspDot> dots;
        dots.reserve(players.size());
        int onScreen = 0;
        for (const Vector3& p : players) {
            Vector2 scr{};
            EspDot d{};
            d.onScreen = WorldToScreenUnity(p, cam.view,
                static_cast<int>(cam.screenW), static_cast<int>(cam.screenH), scr);
            d.screen = scr;
            if (d.onScreen)
                ++onScreen;
            dots.push_back(d);
        }

        if (overlay)
            layer.Draw(dots);
        else
            printf("\r players=%zu onScreen=%d cam=(%.1f,%.1f,%.1f) shoulder-axes=%s   ",
                players.size(), onScreen,
                cam.view.location.x, cam.view.location.y, cam.view.location.z,
                cam.view.hasAxes ? "yes" : "no");

        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
}
