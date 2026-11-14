#define NOMINMAX
#include <Windows.h>
#include <setupapi.h>
#include <devguid.h>
#include "kmbox_usb.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>

#pragma comment(lib, "setupapi.lib")

namespace {
    int PortNumber(const std::string& text) {
        const auto pos = text.rfind("COM");
        if (pos == std::string::npos)
            return 0;
        int result = 0;
        for (size_t i = pos + 3; i < text.size(); i++) {
            if (!std::isdigit(static_cast<unsigned char>(text[i])))
                break;
            result = result * 10 + text[i] - '0';
        }
        return result;
    }

    int ControlPortRank(const std::string& text) {
        std::string upper = text;
        std::transform(
            upper.begin(), upper.end(), upper.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::toupper(value));
            });
        if (upper.find("BLUETOOTH") != std::string::npos)
            return 99;
        if (upper.find("KMBOX") != std::string::npos ||
            upper.find("K-BOX") != std::string::npos ||
            upper.find("WINCHIPHEAD") != std::string::npos ||
            upper.find("CH340") != std::string::npos ||
            upper.find("CH341") != std::string::npos)
            return 0;
        if (upper.find("FERRUM") != std::string::npos ||
            upper.find("CP210") != std::string::npos)
            return 2;
        if (upper.find("MAKCU") != std::string::npos ||
            upper.find("CH343") != std::string::npos)
            return 3;
        if (upper.find("USB-SERIAL") != std::string::npos ||
            upper.find("USB SERIAL") != std::string::npos ||
            upper.find("1A86") != std::string::npos)
            return 1;
        return 4;
    }

    const char* DeviceKindLabel(KmboxDeviceKind kind) {
        switch (kind) {
        case KmboxDeviceKind::Ferrum:
            return "Ferrum";
        case KmboxDeviceKind::Makcu:
            return "MAKCU";
        case KmboxDeviceKind::UsbB:
        default:
            return "KMBox USB";
        }
    }

    bool AliveReply(const std::string& reply) {
        // KMBox USB / MAKCU echo km.version and ">>>". Ferrum: kmbox: Ferrum.
        return reply.find("km.") != std::string::npos ||
            reply.find("kmbox") != std::string::npos ||
            reply.find("Ferrum") != std::string::npos ||
            reply.find("ferrum") != std::string::npos ||
            reply.find(">>>") != std::string::npos;
    }

    bool LineIs(const std::string& reply, char state) {
        if (reply.find(std::string(")\r\n") + state) != std::string::npos)
            return true;
        if (reply.find(std::string(")\n") + state) != std::string::npos)
            return true;
        for (size_t i = 0; i < reply.size(); ++i) {
            if (reply[i] != state)
                continue;
            const bool start = i == 0 || reply[i - 1] == '\n' || reply[i - 1] == '\r';
            const bool end = i + 1 >= reply.size() ||
                reply[i + 1] == '\r' || reply[i + 1] == '\n';
            if (start && end)
                return true;
        }
        return false;
    }

    const char* ButtonName(int button) {
        switch (button) {
        case 2: return "right";
        case 3: return "middle";
        case 4: return "ms1";
        case 5: return "ms2";
        default: return "left";
        }
    }

    KmboxAcProfile ProfileFromName(std::string name) {
        std::transform(
            name.begin(), name.end(), name.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
        while (!name.empty() &&
               (name.back() == '\r' || name.back() == '\n' ||
                name.back() == ' '))
            name.pop_back();
        if (name == "be" || name == "battleye")
            return KmboxAcProfile::Be;
        if (name == "ace")
            return KmboxAcProfile::Ace;
        if (name == "eac" || name == "easyanticheat")
            return KmboxAcProfile::Eac;
        if (name == "robrun")
            return KmboxAcProfile::RobRun;
        return KmboxAcProfile::RobRun;
    }

    const char* LockName(int button) {
        switch (button) {
        case 2: return "mr";
        case 3: return "mm";
        case 4: return "ms1";
        case 5: return "ms2";
        default: return "ml";
        }
    }

    uint8_t ButtonBit(int button) {
        return static_cast<uint8_t>(
            button >= 1 && button <= 5 ? 1u << (button - 1) : 0u);
    }
}

std::vector<KmboxComPort> KmboxUsb::ListPorts() {
    std::vector<KmboxComPort> result;
    HDEVINFO devices = SetupDiGetClassDevsA(
        &GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE)
        return result;

    SP_DEVINFO_DATA device{};
    device.cbSize = sizeof(device);
    for (DWORD index = 0;
         SetupDiEnumDeviceInfo(devices, index, &device); index++) {
        char friendly[512]{};
        char hardwareId[512]{};
        SetupDiGetDeviceRegistryPropertyA(
            devices, &device, SPDRP_FRIENDLYNAME, nullptr,
            reinterpret_cast<PBYTE>(friendly), sizeof(friendly), nullptr);
        SetupDiGetDeviceRegistryPropertyA(
            devices, &device, SPDRP_HARDWAREID, nullptr,
            reinterpret_cast<PBYTE>(hardwareId), sizeof(hardwareId), nullptr);
        std::string description = friendly[0] ? friendly : hardwareId;
        const int port = PortNumber(description);
        if (port <= 0)
            continue;
        if (hardwareId[0])
            description += "  [" + std::string(hardwareId) + "]";
        result.push_back({port, std::move(description)});
    }
    SetupDiDestroyDeviceInfoList(devices);
    std::sort(
        result.begin(), result.end(),
        [](const KmboxComPort& left, const KmboxComPort& right) {
            const int leftRank = ControlPortRank(left.description);
            const int rightRank = ControlPortRank(right.description);
            if (leftRank != rightRank)
                return leftRank < rightRank;
            return left.number < right.number;
        });
    return result;
}

std::string KmboxUsb::LastError() const {
    std::lock_guard lock(m_mutex);
    return m_error;
}

std::string KmboxUsb::Version() const {
    std::lock_guard lock(m_mutex);
    return m_version;
}

bool KmboxUsb::OpenPort(int comPort, int baud) {
    ClosePort();
    char path[32]{};
    std::snprintf(path, sizeof(path), "\\\\.\\COM%d", comPort);
    HANDLE handle = CreateFileA(
        path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        m_error = "Could not open COM" + std::to_string(comPort);
        return false;
    }

    DCB state{};
    state.DCBlength = sizeof(state);
    if (!GetCommState(handle, &state)) {
        CloseHandle(handle);
        m_error = "Could not read serial settings";
        return false;
    }
    state.BaudRate = static_cast<DWORD>(baud);
    state.ByteSize = 8;
    state.Parity = NOPARITY;
    state.StopBits = ONESTOPBIT;
    state.fBinary = TRUE;
    state.fDtrControl = DTR_CONTROL_DISABLE;
    state.fRtsControl = RTS_CONTROL_DISABLE;
    state.fOutxCtsFlow = FALSE;
    state.fOutxDsrFlow = FALSE;
    if (!SetCommState(handle, &state)) {
        CloseHandle(handle);
        m_error = "Could not configure COM" + std::to_string(comPort);
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutConstant = 80;
    timeouts.WriteTotalTimeoutConstant = 80;
    SetCommTimeouts(handle, &timeouts);
    EscapeCommFunction(handle, CLRDTR);
    EscapeCommFunction(handle, CLRRTS);
    PurgeComm(
        handle, PURGE_RXCLEAR | PURGE_TXCLEAR |
        PURGE_RXABORT | PURGE_TXABORT);
    m_handle = handle;
    m_comPort = comPort;
    m_baud = baud;
    return true;
}

void KmboxUsb::ClosePort() {
    if (m_handle) {
        CloseHandle(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }
}

bool KmboxUsb::Send(const std::string& inner) {
    if (!m_handle)
        return false;
    std::string command = m_kmPrefix ? "km." : ".";
    command += inner;
    command += "\r\n";
    DWORD written = 0;
    return WriteFile(
        static_cast<HANDLE>(m_handle), command.data(),
        static_cast<DWORD>(command.size()), &written, nullptr) &&
        written == command.size();
}

std::string KmboxUsb::ReadReply(int timeoutMs) {
    if (!m_handle)
        return {};
    HANDLE handle = static_cast<HANDLE>(m_handle);
    COMMTIMEOUTS previous{};
    GetCommTimeouts(handle, &previous);
    COMMTIMEOUTS current = previous;
    current.ReadIntervalTimeout = 15;
    current.ReadTotalTimeoutConstant =
        static_cast<DWORD>((std::max)(20, timeoutMs));
    current.ReadTotalTimeoutMultiplier = 0;
    SetCommTimeouts(handle, &current);

    std::string reply;
    const DWORD started = GetTickCount();
    char bytes[256]{};
    while (GetTickCount() - started <
           static_cast<DWORD>((std::max)(40, timeoutMs))) {
        DWORD read = 0;
        if (!ReadFile(handle, bytes, sizeof(bytes), &read, nullptr) ||
            read == 0) {
            if (!reply.empty())
                break;
            continue;
        }
        reply.append(bytes, read);
        if (reply.find(">>>") != std::string::npos)
            break;
    }
    SetCommTimeouts(handle, &previous);
    return reply;
}

bool KmboxUsb::Handshake() {
    HANDLE handle = static_cast<HANDLE>(m_handle);
    std::string lastReply;
    // km. first. Ferrum rejects a bare ".version()" and sends nothing back.
    for (const bool kmPrefix : {true, false}) {
        PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
        Sleep(40);
        m_kmPrefix = kmPrefix;
        if (!Send("version()"))
            continue;
        const std::string reply = ReadReply(250);
        if (!reply.empty())
            lastReply = reply;
        if (!AliveReply(reply))
            continue;
        m_version = reply;
        const bool ferrum = reply.find("Ferrum") != std::string::npos ||
            reply.find("ferrum") != std::string::npos;
        const bool makcu = reply.find("MAKCU") != std::string::npos ||
            reply.find("makcu") != std::string::npos;
        if (ferrum)
            m_deviceKind = KmboxDeviceKind::Ferrum;
        else if (makcu)
            m_deviceKind = KmboxDeviceKind::Makcu;
        else
            m_deviceKind = KmboxDeviceKind::UsbB;
        Send("echo(1)");
        ReadReply(100);
        if (ferrum || m_deviceKind == KmboxDeviceKind::UsbB)
            Send("buttons(1)");
        else
            Send("buttons(1,10)");
        ReadReply(80);
        return true;
    }
    m_kmPrefix = true;
    m_error = lastReply.empty()
        ? "No km.version reply (wrong COM port, baud, or device)"
        : "Device replied but it was not a KM box";
    return false;
}

void KmboxUsb::SetAcProfile(KmboxAcProfile profile) {
    m_acProfile = profile;
    KmboxAcProfile tuning = profile;
    if (tuning == KmboxAcProfile::RobRun)
        tuning = KmboxAcProfile::Eac;
    m_ac = KmboxAcTuningFor(tuning);
}

void KmboxUsb::SetAcProfileName(const std::string& name) {
    SetAcProfile(ProfileFromName(name));
}

void KmboxUsb::ApplyAcProfileFromFile() {
    const char* paths[] = {
        "kmbox_ac_profile.txt",
        "debug\\kmbox_ac_profile.txt",
    };
    for (const char* path : paths) {
        std::ifstream file(path);
        if (!file)
            continue;
        std::string line;
        if (!std::getline(file, line))
            continue;
        SetAcProfile(ProfileFromName(line));
        return;
    }
}

bool KmboxUsb::Connect(int comPort) {
    std::lock_guard lock(m_mutex);
    if (m_connected)
        return true;
    ApplyAcProfileFromFile();
    m_error.clear();
    m_version.clear();

    std::vector<int> ports;
    if (comPort > 0)
        ports.push_back(comPort);
    for (const auto& port : ListPorts()) {
        if (std::find(ports.begin(), ports.end(), port.number) ==
            ports.end())
            ports.push_back(port.number);
    }
    if (ports.empty()) {
        m_error = "No serial COM ports found";
        return false;
    }

    constexpr int kboxBaudRates[] = {115200, 921600};
    constexpr int altBaudRates[] = {115200, 4000000, 921600, 2000000};
    for (const int port : ports) {
        for (const int baud : kboxBaudRates) {
            std::cout << "[*] KMBox USB trying COM" << port
                      << " @ " << baud << "\n";
            if (OpenPort(port, baud) && Handshake()) {
                m_connected = true;
                m_error.clear();
                std::cout << "[+] " << DeviceKindLabel(m_deviceKind)
                          << " on COM" << port << " @ " << baud
                          << " (AC profile "
                          << static_cast<int>(m_acProfile) << ")\n";
                return true;
            }
            ClosePort();
        }
        if (m_connected)
            break;
        for (const int baud : altBaudRates) {
            if (baud == 115200 || baud == 921600)
                continue;
            std::cout << "[*] alt baud COM" << port << " @ " << baud << "\n";
            if (OpenPort(port, baud) && Handshake()) {
                m_connected = true;
                m_error.clear();
                std::cout << "[+] " << DeviceKindLabel(m_deviceKind)
                          << " on COM" << port << " @ " << baud << "\n";
                return true;
            }
            ClosePort();
        }
    }
    if (m_error.empty())
        m_error = "No KMBox USB reply on available COM ports";
    return false;
}

void KmboxUsb::Disconnect() {
    std::lock_guard lock(m_mutex);
    if (m_handle && m_connected) {
        Send("buttons(0)");
        Send("lock_ms1(0)");
        Send("lock_ms2(0)");
    }
    ClosePort();
    m_connected = false;
    m_comPort = 0;
    m_buttonMask = 0;
    m_pendingClick = 0;
    m_clickReleaseButton = 0;
    m_clickReleaseAt = 0;
    ClearMoves();
    m_pendingLockButton = 0;
    m_pendingLockState = false;
    m_lockVersion++;
    m_appliedLockButton = 0;
    m_rxBuffer.clear();
}

void KmboxUsb::ParseInput(const std::string& bytes) {
    m_rxBuffer += bytes;
    constexpr char tag[] = "km.buttons";
    constexpr size_t tagLength = sizeof(tag) - 1;
    for (;;) {
        const auto position = m_rxBuffer.find(tag);
        if (position == std::string::npos) {
            if (m_rxBuffer.size() > 256)
                m_rxBuffer.erase(0, m_rxBuffer.size() - 64);
            return;
        }
        if (position + tagLength >= m_rxBuffer.size())
            return;
        m_buttonMask = static_cast<uint8_t>(
            m_rxBuffer[position + tagLength]);
        m_lastButtonMs = GetTickCount();
        m_rxBuffer.erase(0, position + tagLength + 1);
    }
}

void KmboxUsb::DrainInput() {
    if (!m_handle)
        return;
    HANDLE handle = static_cast<HANDLE>(m_handle);
    COMMTIMEOUTS previous{};
    GetCommTimeouts(handle, &previous);
    COMMTIMEOUTS immediate = previous;
    immediate.ReadIntervalTimeout = MAXDWORD;
    immediate.ReadTotalTimeoutMultiplier = 0;
    immediate.ReadTotalTimeoutConstant = 0;
    SetCommTimeouts(handle, &immediate);
    char bytes[256]{};
    DWORD read = 0;
    while (ReadFile(handle, bytes, sizeof(bytes), &read, nullptr) &&
           read > 0)
        ParseInput(std::string(bytes, read));
    SetCommTimeouts(handle, &previous);
}

bool KmboxUsb::QueryButton(int button) {
    if (button < 1 || button > 5)
        return false;
    char command[24]{};
    std::snprintf(
        command, sizeof(command), "%s()", ButtonName(button));
    if (!Send(command))
        return false;
    const std::string reply = ReadReply(40);
    ParseInput(reply);
    const bool down = LineIs(reply, '1');
    const bool up = LineIs(reply, '0');
    if (!down && !up)
        return false;
    uint8_t mask = m_buttonMask.load();
    const uint8_t bit = ButtonBit(button);
    m_buttonMask = down
        ? static_cast<uint8_t>(mask | bit)
        : static_cast<uint8_t>(mask & ~bit);
    m_lastButtonMs = GetTickCount();
    return true;
}

bool KmboxUsb::IsButtonDown(int button) const {
    return (m_buttonMask.load() & ButtonBit(button)) != 0;
}

void KmboxUsb::ConfigurePollButtons(int aimButton, int recoilButton) {
    m_pollAimButton = std::clamp(aimButton, 0, 5);
    m_pollRecoilButton = std::clamp(recoilButton, 0, 5);
}

void KmboxUsb::ServiceOnce() {
    std::lock_guard lock(m_mutex);
    if (!m_connected || !m_handle)
        return;
    const int aimButton = m_pollAimButton.load();
    const int recoilButton = m_pollRecoilButton.load();

    const uint32_t lockVersion = m_lockVersion.load();
    if (lockVersion != m_appliedLockVersion) {
        const int requestedButton = m_pendingLockButton.load();
        const bool requestedState = m_pendingLockState.load();
        char command[32]{};
        if (m_appliedLockButton &&
            (m_appliedLockButton != requestedButton ||
             !requestedState)) {
            std::snprintf(
                command, sizeof(command), "lock_%s(0)",
                LockName(m_appliedLockButton));
            Send(command);
            m_appliedLockButton = 0;
        }
        if (requestedState && requestedButton >= 1 &&
            requestedButton <= 5 &&
            m_appliedLockButton != requestedButton) {
            std::snprintf(
                command, sizeof(command), "lock_%s(1)",
                LockName(requestedButton));
            Send(command);
            m_appliedLockButton = requestedButton;
        }
        m_appliedLockVersion = lockVersion;
    }

    const uint32_t nowMs = GetTickCount();
    if (m_clickReleaseButton &&
        static_cast<int32_t>(nowMs - m_clickReleaseAt) >= 0) {
        char command[32]{};
        std::snprintf(
            command, sizeof(command), "%s(0)",
            ButtonName(m_clickReleaseButton));
        Send(command);
        m_clickReleaseButton = 0;
    }
    const int click = m_clickReleaseButton
        ? 0 : m_pendingClick.exchange(0);
    if (click >= 1 && click <= 5) {
        char command[32]{};
        std::snprintf(
            command, sizeof(command), "%s(1)", ButtonName(click));
        Send(command);
        m_clickReleaseButton = click;
        m_clickReleaseAt = nowMs + 12;
    }
    DrainInput();
    if (GetTickCount() - m_lastButtonMs.load() >
        static_cast<uint32_t>(m_ac.buttonPollStaleMs)) {
        QueryButton(1);
        if (aimButton >= 2 && aimButton <= 5)
            QueryButton(aimButton);
        if (recoilButton >= 2 && recoilButton <= 5 &&
            recoilButton != aimButton)
            QueryButton(recoilButton);
    }

    int moveX = m_aimMoveX.exchange(0) +
        m_recoilMoveX.exchange(0);
    int moveY = m_aimMoveY.exchange(0) +
        m_recoilMoveY.exchange(0);
    moveX = std::clamp(moveX, -m_ac.maxClampPerAxis, m_ac.maxClampPerAxis);
    moveY = std::clamp(moveY, -m_ac.maxClampPerAxis, m_ac.maxClampPerAxis);
    m_lastMoveX = moveX;
    m_lastMoveY = moveY;
    if (moveX || moveY) {
        if (static_cast<int32_t>(nowMs - m_lastMoveSendMs) <
            m_ac.minMsBetweenMove) {
            m_aimMoveX.fetch_add(moveX);
            m_aimMoveY.fetch_add(moveY);
            return;
        }
        const int chunkX = std::clamp(
            moveX, -m_ac.maxDeltaPerMove, m_ac.maxDeltaPerMove);
        const int chunkY = std::clamp(
            moveY, -m_ac.maxDeltaPerMove, m_ac.maxDeltaPerMove);
        const int remX = moveX - chunkX;
        const int remY = moveY - chunkY;
        if (remX || remY) {
            m_aimMoveX.fetch_add(remX);
            m_aimMoveY.fetch_add(remY);
        }
        char command[64]{};
        if (m_ac.segmentedMove && m_ac.moveCurveSteps > 0 &&
            (std::abs(chunkX) > m_ac.segmentAbove ||
             std::abs(chunkY) > m_ac.segmentAbove)) {
            std::snprintf(
                command, sizeof(command), "move(%d,%d,%d)", chunkX,
                chunkY, m_ac.moveCurveSteps);
        } else {
            std::snprintf(
                command, sizeof(command), "move(%d,%d)", chunkX, chunkY);
        }
        Send(command);
        m_lastMoveSendMs = nowMs;
        if (++m_txCount >= 16) {
            DrainInput();
            m_txCount = 0;
        }
    }
}

void KmboxUsb::LockButton(int button, bool locked) {
    if (button < 1 || button > 5)
        return;
    m_pendingLockButton = button;
    m_pendingLockState = locked;
    m_lockVersion++;
}

bool KmboxUsb::Move(int dx, int dy) {
    if (!m_connected.load())
        return false;
    QueueAimMove(dx, dy);
    return true;
}

bool KmboxUsb::TestMouseMove() {
    std::lock_guard lock(m_mutex);
    if (!m_connected || !m_handle) {
        m_error = "Device not connected";
        return false;
    }
    static const int kPattern[][2] = {
        { 36, 0 }, { 0, 36 }, { -36, 0 }, { 0, -36 },
    };
    int sentX = 0;
    int sentY = 0;
    for (const auto& step : kPattern) {
        char command[48]{};
        std::snprintf(
            command, sizeof(command), "move(%d,%d)", step[0], step[1]);
        if (!Send(command)) {
            m_error = "Serial write failed during test move";
            return false;
        }
        sentX += step[0];
        sentY += step[1];
        Sleep(12);
        DrainInput();
    }
    m_lastMoveX = sentX;
    m_lastMoveY = sentY;
    m_error.clear();
    return true;
}

void KmboxUsb::QueueAimMove(int dx, int dy) {
    m_aimMoveX.store(dx);
    m_aimMoveY.store(dy);
}

void KmboxUsb::QueueRecoilMove(int dx, int dy) {
    m_recoilMoveX.fetch_add(dx);
    m_recoilMoveY.fetch_add(dy);
}

void KmboxUsb::ClearAimMove() {
    m_aimMoveX = 0;
    m_aimMoveY = 0;
}

void KmboxUsb::ClearRecoilMove() {
    m_recoilMoveX = 0;
    m_recoilMoveY = 0;
}

void KmboxUsb::ClearMoves() {
    ClearAimMove();
    ClearRecoilMove();
    m_lastMoveX = 0;
    m_lastMoveY = 0;
}

bool KmboxUsb::Click(int button) {
    if (!m_connected.load() || button < 1 || button > 5)
        return false;
    m_pendingClick = button;
    return true;
}
