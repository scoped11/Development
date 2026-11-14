#pragma once
#include "kmbox_ac.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

enum class KmboxDeviceKind {
    UsbB = 0, // official KMBox USB B / B+
    Makcu,
    Ferrum,
};

struct KmboxComPort {
    int number = 0;
    std::string description;
};

class KmboxUsb {
public:
    static KmboxUsb& Get() {
        static KmboxUsb instance;
        return instance;
    }

    static std::vector<KmboxComPort> ListPorts();
    bool Connect(int comPort = 0);
    void Disconnect();
    bool IsConnected() const { return m_connected.load(); }
    int GetComPort() const { return m_comPort; }
    int GetBaud() const { return m_baud; }
    KmboxDeviceKind DeviceKind() const { return m_deviceKind; }
    std::string LastError() const;
    std::string Version() const;
    bool Move(int dx, int dy); // Queued generic/aim movement.
    // Sends a short move pattern on the serial link (KMBox USB B/B+ (optional MAKCU/Ferrum)).
    // Watch the game PC cursor — if it jumps, the box path works.
    bool TestMouseMove();
    void QueueAimMove(int dx, int dy);
    void QueueRecoilMove(int dx, int dy);
    void ClearAimMove();
    void ClearRecoilMove();
    void ClearMoves();
    bool Click(int button = 1);
    bool IsButtonDown(int button) const;
    void ConfigurePollButtons(int aimButton, int recoilButton);
    void SetAcProfile(KmboxAcProfile profile);
    void SetAcProfileName(const std::string& name);
    KmboxAcProfile GetAcProfile() const { return m_acProfile; }
    void ServiceOnce();
    void LockButton(int button, bool locked);
    int LastMoveX() const { return m_lastMoveX.load(); }
    int LastMoveY() const { return m_lastMoveY.load(); }

private:
    KmboxUsb() = default;
    ~KmboxUsb() { Disconnect(); }
    KmboxUsb(const KmboxUsb&) = delete;
    KmboxUsb& operator=(const KmboxUsb&) = delete;

    bool OpenPort(int comPort, int baud);
    bool Handshake();
    void ApplyAcProfileFromFile();
    bool Send(const std::string& inner);
    std::string ReadReply(int timeoutMs);
    bool QueryButton(int button);
    void DrainInput();
    void ParseInput(const std::string& bytes);
    void ClosePort();

    void* m_handle = nullptr;
    int m_comPort = 0;
    int m_baud = 115200;
    int m_txCount = 0;
    bool m_kmPrefix = true;
    KmboxDeviceKind m_deviceKind = KmboxDeviceKind::UsbB;
    std::atomic<bool> m_connected{false};
    std::atomic<uint8_t> m_buttonMask{0};
    std::atomic<uint32_t> m_lastButtonMs{0};
    std::atomic<int> m_pendingClick{0};
    int m_clickReleaseButton = 0;
    uint32_t m_clickReleaseAt = 0;
    std::atomic<int> m_aimMoveX{0};
    std::atomic<int> m_aimMoveY{0};
    std::atomic<int> m_recoilMoveX{0};
    std::atomic<int> m_recoilMoveY{0};
    std::atomic<int> m_pendingLockButton{0};
    std::atomic<bool> m_pendingLockState{false};
    std::atomic<uint32_t> m_lockVersion{0};
    uint32_t m_appliedLockVersion = 0;
    int m_appliedLockButton = 0;
    std::atomic<int> m_lastMoveX{0};
    std::atomic<int> m_lastMoveY{0};
    std::atomic<int> m_pollAimButton{4};
    std::atomic<int> m_pollRecoilButton{1};
    mutable std::mutex m_mutex;
    std::string m_error;
    std::string m_version;
    std::string m_rxBuffer;
    KmboxAcProfile m_acProfile = KmboxAcProfile::RobRun;
    KmboxAcTuning m_ac = KmboxAcTuningFor(KmboxAcProfile::RobRun);
    uint32_t m_lastMoveSendMs = 0;
};
