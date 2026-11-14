from pathlib import Path

p = Path(r"C:\Development\dma\src\game\runtime.cpp")
text = p.read_text(encoding="utf-8")

old_scatter = '''    struct FollowSample {
        float position[3] = {};
    };
    struct BrainSample {
        int frame = 0;
        float fov = 0.f;
        float pose[7] = {};
        float correction[3] = {};
        float orient[4] = { 0.f, 0.f, 0.f, 1.f };
    };
    struct BattleCamSample {
        Vector3 position{};
        float orient[4] = { 0.f, 0.f, 0.f, 1.f };
        Vector3 forward{};
        Vector3 right{};
        Vector3 up{};
        float distance = 0.f;
    };
    std::vector<Sample> samples(actors.size());
    ClientSample clientSample;
    FollowSample followSample;
    BattleCamSample battleCamSample;
    BrainSample brainSample[8];
    uintptr_t clientPos = m_clientPos.load();
    uintptr_t clientLookQuat = m_clientLookQuat.load();
    uintptr_t clientYawAddr = m_clientYaw.load();
    uintptr_t clientPitchAddr = m_clientPitch.load();
    uintptr_t followAddr = m_followPos.load();
    int brainCount = m_brainCount.load();
    if (brainCount < 0) brainCount = 0;
    if (brainCount > 8) brainCount = 8;
    uintptr_t brainAddr[8] = {};
    for (int i = 0; i < brainCount; ++i)
        brainAddr[i] = m_brains[i].load();
    int rigKind = m_rigKind.load();
    uintptr_t rigAddr = m_rig.load();
    float rigRaw[40] = {};
    uintptr_t battleCam = m_battleCamera.load();
'''

new_scatter = '''    struct BattleCamSample {
        Vector3 position{};
        float orient[4] = { 0.f, 0.f, 0.f, 1.f };
        Vector3 forward{};
        Vector3 right{};
        Vector3 up{};
        float distance = 0.f;
    };
    std::vector<Sample> samples(actors.size());
    ClientSample clientSample;
    BattleCamSample battleCamSample;
    uintptr_t clientPos = m_clientPos.load();
    uintptr_t clientLookQuat = m_clientLookQuat.load();
    uintptr_t clientYawAddr = m_clientYaw.load();
    uintptr_t clientPitchAddr = m_clientPitch.load();
    uintptr_t battleCam = m_battleCamera.load();
'''

if old_scatter not in text:
    raise SystemExit("scatter block not found")
text = text.replace(old_scatter, new_scatter, 1)

old_extra = '''        if (followAddr)
            scatter.Prepare(followAddr, followSample.position, sizeof(followSample.position));
        if (IsPtr(battleCam)) {
            scatter.Prepare(battleCam + offsets::battle_camera::Position,
                            &battleCamSample.position, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::Orientation,
                            battleCamSample.orient, sizeof(battleCamSample.orient));
            scatter.Prepare(battleCam + offsets::battle_camera::Forward,
                            &battleCamSample.forward, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::Right,
                            &battleCamSample.right, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::Up,
                            &battleCamSample.up, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::DistanceToFollowTarget,
                            &battleCamSample.distance);
        }
        if (rigAddr) {
            size_t rigBytes = rigKind == 2 ? sizeof(rigRaw) : sizeof(float) * 6;
            scatter.Prepare(rigAddr + offsets::third_person::ShoulderOffset, rigRaw, rigBytes);
        }
        for (int i = 0; i < brainCount; ++i) {
            if (!brainAddr[i]) continue;
            scatter.Prepare(brainAddr[i] + offsets::cinemachine_brain::LastFrame, &brainSample[i].frame);
            scatter.Prepare(brainAddr[i] + offsets::cinemachine_brain::FieldOfView, &brainSample[i].fov);
            scatter.Prepare(brainAddr[i] + offsets::cinemachine_brain::RawPose,
                            brainSample[i].pose, sizeof(brainSample[i].pose));
            scatter.Prepare(brainAddr[i] + offsets::cinemachine_brain::PositionCorrection,
                            brainSample[i].correction, sizeof(brainSample[i].correction));
            scatter.Prepare(brainAddr[i] + offsets::cinemachine_brain::OrientationCorrection,
                            brainSample[i].orient, sizeof(brainSample[i].orient));
        }
        scatter.Execute();
'''

new_extra = '''        if (IsPtr(battleCam)) {
            scatter.Prepare(battleCam + offsets::battle_camera::Position,
                            &battleCamSample.position, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::Orientation,
                            battleCamSample.orient, sizeof(battleCamSample.orient));
            scatter.Prepare(battleCam + offsets::battle_camera::Forward,
                            &battleCamSample.forward, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::Right,
                            &battleCamSample.right, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::Up,
                            &battleCamSample.up, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::DistanceToFollowTarget,
                            &battleCamSample.distance);
        }
        scatter.Execute();
'''

if old_extra not in text:
    raise SystemExit("extra scatter not found")
text = text.replace(old_extra, new_extra, 1)

start = text.find("    CameraData camera;\n    camera.status = status;")
end = text.find("    int enabledPlayers = 0;\n    for (const auto& a : actors) {")
if start < 0 or end < 0 or end <= start:
    raise SystemExit(f"camera compose markers {start} {end}")

new_cam = r'''    CameraData camera;
    camera.status = status;
    camera.viewInfo.fov = 60.f;

    Vector3 viewPos{
        clientSample.position[0], clientSample.position[1], clientSample.position[2]
    };
    if (FinitePosition(viewPos) && !haveLocal) {
        local.position = viewPos;
        local.isLocalPlayer = true;
        local.isEnabled = true;
        haveLocal = true;
    }

    // W2S uses the local third-person lens, not the pawn look vector.
    // Game chain: Game+0xD0 -> GameModel+0x10 -> BattleModel+0x358 BattleCameraModel.
    // Position / Forward / Right / Up are the shoulder camera in world space.
    if (FinitePosition(battleCamSample.position)) {
        camera.viewInfo.location = battleCamSample.position;
        if (!ApplyCameraAxes(camera.viewInfo, battleCamSample.forward,
                             battleCamSample.right, battleCamSample.up) &&
            UnitQuat(battleCamSample.orient[0], battleCamSample.orient[1],
                     battleCamSample.orient[2], battleCamSample.orient[3])) {
            SetCameraBasis(camera.viewInfo, battleCamSample.orient[0],
                           battleCamSample.orient[1], battleCamSample.orient[2],
                           battleCamSample.orient[3]);
        }
        camera.valid = camera.viewInfo.location.IsValid() && camera.viewInfo.hasAxes;
    }

    {
        Vector3 clientBody{
            clientSample.position[0], clientSample.position[1], clientSample.position[2]
        };
        if (!haveLocal && FinitePosition(clientBody)) {
            for (auto& e : actors) {
                if (e.type != EntityType::Player || !e.isEnabled)
                    continue;
                if (e.position.Distance(clientBody) < 1.7f) {
                    e.isLocalPlayer = true;
                    local = e;
                    haveLocal = true;
                    break;
                }
            }
        }
        if (haveLocal) {
            for (auto& e : actors) {
                if (e.type != EntityType::Player || e.isLocalPlayer || !e.isEnabled)
                    continue;
                if (FinitePosition(local.position) && e.position.Distance(local.position) < 1.25f)
                    e.isLocalPlayer = true;
            }
        }
    }

    static bool loggedCam = false;
    if (!loggedCam) {
        loggedCam = true;
        std::cout << "[+] Shoulder camera " << (camera.valid ? "valid" : "waiting")
                  << " fov=" << camera.viewInfo.fov
                  << " dist=" << battleCamSample.distance
                  << " cam=(" << camera.viewInfo.location.x << ","
                  << camera.viewInfo.location.y << ","
                  << camera.viewInfo.location.z << ")\n" << std::flush;
    }

'''

text = text[:start] + new_cam + text[end:]

old_refresh = '''void RobRunSession::RefreshCamera(CameraData& camera) {
    if (!camera.valid || !m_inGame.load()) return;
    int brainCount = m_brainCount.load();
'''
# replace whole function by finding SetMemoryAimTarget after RefreshCamera
i = text.find("void RobRunSession::RefreshCamera(CameraData& camera) {")
j = text.find("void RobRunSession::SetMemoryAimTarget", i)
if i < 0 or j < 0:
    raise SystemExit("refresh not found")

text = text[:i] + r'''void RobRunSession::RefreshCamera(CameraData& camera) {
    if (!m_inGame.load()) return;
    uintptr_t battleCam = m_battleCamera.load();
    if (!IsPtr(battleCam) && IsPtr(m_battleModel.load())) {
        uintptr_t fromModel = 0;
        if (Memory::Get().ReadOk(m_battleModel.load() + offsets::battle_model::BattleCamera, fromModel) &&
            IsPtr(fromModel)) {
            m_battleCamera.store(fromModel);
            battleCam = fromModel;
        }
    }
    if (!IsPtr(battleCam)) {
        camera.valid = false;
        return;
    }

    struct BattleCamSample {
        Vector3 position{};
        float orient[4] = { 0.f, 0.f, 0.f, 1.f };
        Vector3 forward{};
        Vector3 right{};
        Vector3 up{};
    } battleCamSample;

    auto& scatter = Memory::Get().BorrowScatter();
    if (!scatter.IsValid()) return;
    scatter.Prepare(battleCam + offsets::battle_camera::Position,
                    &battleCamSample.position, sizeof(Vector3));
    scatter.Prepare(battleCam + offsets::battle_camera::Orientation,
                    battleCamSample.orient, sizeof(battleCamSample.orient));
    scatter.Prepare(battleCam + offsets::battle_camera::Forward,
                    &battleCamSample.forward, sizeof(Vector3));
    scatter.Prepare(battleCam + offsets::battle_camera::Right,
                    &battleCamSample.right, sizeof(Vector3));
    scatter.Prepare(battleCam + offsets::battle_camera::Up,
                    &battleCamSample.up, sizeof(Vector3));
    if (!scatter.Execute()) return;

    if (!FinitePosition(battleCamSample.position)) {
        camera.valid = false;
        return;
    }
    camera.viewInfo.location = battleCamSample.position;
    camera.viewInfo.fov = 60.f;
    if (!ApplyCameraAxes(camera.viewInfo, battleCamSample.forward,
                         battleCamSample.right, battleCamSample.up) &&
        UnitQuat(battleCamSample.orient[0], battleCamSample.orient[1],
                 battleCamSample.orient[2], battleCamSample.orient[3])) {
        SetCameraBasis(camera.viewInfo, battleCamSample.orient[0],
                       battleCamSample.orient[1], battleCamSample.orient[2],
                       battleCamSample.orient[3]);
    }
    camera.valid = camera.viewInfo.location.IsValid() && camera.viewInfo.hasAxes;
}

''' + text[j:]

p.write_text(text, encoding="utf-8")
print("patched", len(text.splitlines()), "lines")
