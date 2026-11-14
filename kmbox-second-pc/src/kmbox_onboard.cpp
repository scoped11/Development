#define NOMINMAX
#include <Windows.h>
#include <restartmanager.h>
#include <setupapi.h>
#include <devguid.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "Rstrtmgr.lib")

namespace {

constexpr const char* kFactoryBoot =
    "# ROBRUN_HARD_RESET — empty boot. Do not import device (USB reconnect loop).\n"
    "pass\n";

constexpr const char* kFactoryIdentity =
    "# ROBRUN_FACTORY_RESET\n"
    "preset=stock\n"
    "vid=\n"
    "pid=\n";

constexpr const char* kFactoryMain = "pass\n";

void Log(const std::string& line);

std::string ExeDir() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string s = path;
    const auto slash = s.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : s.substr(0, slash);
}

int PortNumber(const std::string& text) {
    const auto pos = text.rfind("COM");
    if (pos == std::string::npos)
        return 0;
    int result = 0;
    for (size_t i = pos + 3; i < text.size(); i++) {
        if (!std::isdigit(static_cast<unsigned char>(text[i])))
            break;
        result = result * 10 + (text[i] - '0');
    }
    return result;
}

int ControlRank(const std::string& text) {
    std::string upper = text;
    std::transform(upper.begin(), upper.end(), upper.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (upper.find("PNP0501") != std::string::npos ||
        upper.find("BLUETOOTH") != std::string::npos)
        return 99;
    if (upper.find("KMBOX") != std::string::npos ||
        upper.find("CH340") != std::string::npos ||
        upper.find("CH341") != std::string::npos)
        return 0;
    if (upper.find("USB-SERIAL") != std::string::npos ||
        upper.find("USB SERIAL") != std::string::npos ||
        upper.find("1A86") != std::string::npos)
        return 1;
    return 4;
}

struct Port {
    int number = 0;
    std::string description;
};

std::vector<Port> ListPorts() {
    std::vector<Port> result;
    HDEVINFO devices = SetupDiGetClassDevsA(
        &GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE)
        return result;
    SP_DEVINFO_DATA device{};
    device.cbSize = sizeof(device);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(devices, i, &device); i++) {
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
    std::sort(result.begin(), result.end(), [](const Port& a, const Port& b) {
        const int ra = ControlRank(a.description);
        const int rb = ControlRank(b.description);
        if (ra != rb)
            return ra < rb;
        return a.number < b.number;
    });
    return result;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty())
        return {};
    const int bytes = WideCharToMultiByte(
        CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    if (bytes <= 0)
        return {};
    std::string out(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
        out.data(), bytes, nullptr, nullptr);
    return out;
}

std::wstring ImagePath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};
    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    std::wstring result;
    if (QueryFullProcessImageNameW(process, 0, path, &size))
        result.assign(path, size);
    CloseHandle(process);
    return result;
}

std::wstring LowerWide(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), ::towlower);
    return text;
}

bool ProtectedProcess(const std::wstring& path) {
    const std::wstring lower = LowerWide(path);
    const wchar_t* skip[] = {
        L"\\csrss.exe", L"\\lsass.exe", L"\\services.exe", L"\\svchost.exe",
        L"\\wininit.exe", L"\\winlogon.exe", L"\\smss.exe", L"\\dwm.exe"
    };
    for (const wchar_t* name : skip) {
        if (lower.find(name) != std::wstring::npos)
            return true;
    }
    return false;
}

void StopProcess(DWORD pid, const std::wstring& path) {
    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!process) {
        Log("[!] Could not close " + WideToUtf8(path) + " pid " + std::to_string(pid));
        return;
    }
    TerminateProcess(process, 1);
    WaitForSingleObject(process, 2000);
    CloseHandle(process);
    Log("[*] Closed " + WideToUtf8(path) + " pid " + std::to_string(pid));
}

void ReleaseComUsers(int port) {
    const DWORD self = GetCurrentProcessId();
    wchar_t device[32]{};
    swprintf_s(device, L"\\\\.\\COM%d", port);
    DWORD session = 0;
    wchar_t key[CCH_RM_SESSION_KEY + 1]{};
    if (RmStartSession(&session, 0, key) == ERROR_SUCCESS) {
        LPCWSTR resources[] = { device };
        if (RmRegisterResources(session, 1, resources, 0, nullptr, 0, nullptr) == ERROR_SUCCESS) {
            UINT needed = 0;
            UINT count = 0;
            DWORD reason = 0;
            DWORD listed = RmGetList(session, &needed, &count, nullptr, &reason);
            if (listed == ERROR_MORE_DATA && needed > 0) {
                std::vector<RM_PROCESS_INFO> info(needed);
                count = needed;
                listed = RmGetList(session, &needed, &count, info.data(), &reason);
                if (listed == ERROR_SUCCESS) {
                    for (UINT i = 0; i < count; ++i) {
                        const DWORD pid = info[i].Process.dwProcessId;
                        if (pid == 0 || pid == self)
                            continue;
                        if (info[i].strServiceShortName[0] != L'\0') {
                            Log("[!] Windows service holds COM" + std::to_string(port) +
                                ": " + WideToUtf8(info[i].strServiceShortName));
                            continue;
                        }
                        const std::wstring path = ImagePath(pid);
                        const std::wstring shown = path.empty()
                            ? std::wstring(info[i].strAppName) : path;
                        if (ProtectedProcess(shown)) {
                            Log("[!] Left system process on COM" + std::to_string(port) +
                                ": " + WideToUtf8(shown));
                            continue;
                        }
                        Log("[*] COM" + std::to_string(port) + " is open in another program");
                        StopProcess(pid, shown);
                    }
                }
            }
        }
        RmEndSession(session);
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID == self)
                continue;
            const std::wstring name = LowerWide(entry.szExeFile);
            const std::wstring path = ImagePath(entry.th32ProcessID);
            const std::wstring lower = LowerWide(path);
            const bool ours =
                name == L"kmboxonboard.exe" ||
                name == L"putty.exe" || name == L"plink.exe" ||
                lower.find(L"\\kmbox-second-pc\\tools\\python\\") != std::wstring::npos;
            if (!ours)
                continue;
            Log("[*] Stopping leftover " + WideToUtf8(path.empty() ? name : path));
            StopProcess(entry.th32ProcessID, path.empty() ? name : path);
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    Sleep(300);
}

HANDLE OpenCom(int port, int baud) {
    ReleaseComUsers(port);
    char path[32]{};
    std::snprintf(path, sizeof(path), "\\\\.\\COM%d", port);
    HANDLE handle = CreateFileA(
        path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;
    DCB state{};
    state.DCBlength = sizeof(state);
    if (!GetCommState(handle, &state)) {
        CloseHandle(handle);
        return INVALID_HANDLE_VALUE;
    }
    state.BaudRate = static_cast<DWORD>(baud);
    state.ByteSize = 8;
    state.Parity = NOPARITY;
    state.StopBits = ONESTOPBIT;
    state.fBinary = TRUE;
    // Never pulse DTR/RTS — that resets the ESP and the HID USB re-enumerates
    // (Windows connect/disconnect sound) on every COM open.
    state.fDtrControl = DTR_CONTROL_DISABLE;
    state.fRtsControl = RTS_CONTROL_DISABLE;
    state.fOutxCtsFlow = FALSE;
    state.fOutxDsrFlow = FALSE;
    if (!SetCommState(handle, &state)) {
        CloseHandle(handle);
        return INVALID_HANDLE_VALUE;
    }
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutConstant = 80;
    timeouts.WriteTotalTimeoutConstant = 200;
    SetCommTimeouts(handle, &timeouts);
    EscapeCommFunction(handle, CLRDTR);
    EscapeCommFunction(handle, CLRRTS);
    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
    return handle;
}

std::ofstream g_log;

void Log(const std::string& line) {
    std::cout << line << std::endl;
    std::cout.flush();
    if (g_log) {
        g_log << line << std::endl;
        g_log.flush();
    }
}

void WaitForEnter() {
    std::cout.flush();
    std::cin.clear();
    std::string dummy;
    std::getline(std::cin, dummy);
}

void WaitUnplugReplug(const char* title, const char* body) {
    Log("");
    Log("============================================================");
    Log(std::string("  ") + title);
    Log("============================================================");
    Log(body);
    Log("");
    Log(">>> Unplug BOTH USB cables on the KMBox now.");
    Log(">>> Wait 2 seconds.");
    Log(">>> Plug them back as instructed above.");
    Log(">>> Then press Enter here. Do not close this window.");
    Log("============================================================");
    WaitForEnter();
    Log("[*] Continuing...");
}

bool WriteAll(HANDLE handle, const void* data, DWORD size) {
    DWORD written = 0;
    return WriteFile(handle, data, size, &written, nullptr) && written == size;
}

std::string ReadFor(HANDLE handle, int timeoutMs, const char* stop = nullptr) {
    const DWORD started = GetTickCount();
    std::string reply;
    char bytes[256]{};
    while (GetTickCount() - started < static_cast<DWORD>((std::max)(40, timeoutMs))) {
        DWORD read = 0;
        if (!ReadFile(handle, bytes, sizeof(bytes), &read, nullptr) || read == 0) {
            if (!reply.empty() && !stop)
                break;
            Sleep(15);
            continue;
        }
        reply.append(bytes, read);
        if (stop && reply.find(stop) != std::string::npos)
            break;
    }
    return reply;
}

// Reads until overallMs, or until quietMs with no bytes after some data.
// Does not return on the first gap during a short bootloader burst unless
// that gap lasts quietMs.
std::string ReadUntilQuiet(HANDLE handle, int overallMs, int quietMs) {
    std::string reply;
    const DWORD started = GetTickCount();
    DWORD lastData = started;
    bool saw = false;
    char bytes[256]{};
    while (GetTickCount() - started < static_cast<DWORD>(overallMs)) {
        DWORD read = 0;
        if (ReadFile(handle, bytes, sizeof(bytes), &read, nullptr) && read > 0) {
            reply.append(bytes, read);
            lastData = GetTickCount();
            saw = true;
            if (reply.find(">>>") != std::string::npos)
                break;
            continue;
        }
        if (saw && GetTickCount() - lastData >= static_cast<DWORD>(quietMs))
            break;
        Sleep(15);
    }
    return reply;
}

std::string Printable(const std::string& text);

void LogBlob(const char* tag, const std::string& text) {
    if (text.empty()) {
        Log(std::string(tag) + " (silent)");
        return;
    }
    std::string flat = Printable(text);
    constexpr size_t kStep = 180;
    for (size_t i = 0; i < flat.size(); i += kStep)
        Log(std::string(tag) + " " + flat.substr(i, kStep));
}

std::string Printable(const std::string& text) {
    std::string out;
    for (unsigned char c : text) {
        if (c >= 32 && c < 127)
            out += static_cast<char>(c);
        else if (c == '\n' || c == '\r' || c == '\t')
            out += ' ';
        else
            out += '.';
        if (out.size() > 700)
            break;
    }
    return out;
}

bool HasPrompt(const std::string& reply) {
    return reply.find(">>>") != std::string::npos ||
        reply.find("raw REPL") != std::string::npos;
}

bool Alive(const std::string& reply) {
    return HasPrompt(reply) ||
        reply.find("km.") != std::string::npos ||
        reply.find("kmbox") != std::string::npos ||
        reply.find("MicroPython") != std::string::npos;
}

void PulseAppReset(HANDLE handle) {
    // One EN pulse. GPIO0 stays high so we run firmware, not download mode.
    Log("[*] One MCU reset (ONE USB click is OK — it must not keep repeating)");
    EscapeCommFunction(handle, CLRDTR);
    EscapeCommFunction(handle, SETRTS);
    Sleep(120);
    EscapeCommFunction(handle, CLRRTS);
    Sleep(40);
}

bool ExitRawIfNeeded(HANDLE handle, const std::string& seen) {
    if (seen.find("raw REPL") == std::string::npos &&
        seen.find("CTRL-B") == std::string::npos)
        return false;
    const char ctrlB = '\x02';
    WriteAll(handle, &ctrlB, 1);
    Sleep(80);
    return ReadFor(handle, 600, ">>>").find(">>>") != std::string::npos;
}

bool CatchRepl(HANDLE handle) {
    const char ctrlB = '\x02';
    const char ctrlC = '\x03';
    const char nl[] = "\r\n";
    WriteAll(handle, &ctrlB, 1);
    std::string acc;
    const DWORD deadline = GetTickCount() + 6000;
    while (GetTickCount() < deadline) {
        WriteAll(handle, &ctrlC, 1);
        WriteAll(handle, &ctrlC, 1);
        WriteAll(handle, nl, 2);
        acc += ReadFor(handle, 70, ">>>");
        if (acc.find(">>>") != std::string::npos) {
            Log("[+] Caught friendly REPL >>>");
            return true;
        }
        if (ExitRawIfNeeded(handle, acc)) {
            Log("[+] Left raw REPL, have >>>");
            return true;
        }
    }
    Log(std::string("[*] Catch buffer: ") + Printable(acc));
    return acc.find(">>>") != std::string::npos;
}

bool BreakToRepl(HANDLE handle) {
    return CatchRepl(handle);
}

bool Probe(HANDLE handle) {
    PulseAppReset(handle);
    if (CatchRepl(handle))
        return true;
    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    const char* cmd = "km.version()\r\n";
    WriteAll(handle, cmd, static_cast<DWORD>(strlen(cmd)));
    const std::string reply = ReadFor(handle, 800, ">>>");
    Log(std::string("[*] km.version: ") + Printable(reply));
    return Alive(reply);
}

bool ToFriendlyRepl(HANDLE handle) {
    return CatchRepl(handle);
}

bool RawExec(HANDLE handle, const std::string& code) {
    const char ctrlA = '\x01';
    const char ctrlD = '\x04';
    const char ctrlB = '\x02';
    WriteAll(handle, &ctrlB, 1);
    Sleep(50);
    WriteAll(handle, &ctrlA, 1);
    ReadFor(handle, 400);
    if (!WriteAll(handle, code.data(), static_cast<DWORD>(code.size())))
        return false;
    WriteAll(handle, &ctrlD, 1);
    const std::string reply = ReadFor(handle, 2500);
    WriteAll(handle, &ctrlB, 1);
    ReadFor(handle, 400, ">>>");
    Log(std::string("[*] raw exec: ") + Printable(reply));
    return reply.find("OK") != std::string::npos ||
        reply.find(">>>") != std::string::npos;
}

bool ReplExec(HANDLE handle, const std::string& line) {
    const std::string cmd = line + "\r\n";
    if (!WriteAll(handle, cmd.data(), static_cast<DWORD>(cmd.size())))
        return false;
    const std::string reply = ReadFor(handle, 2500, ">>>");
    if (reply.find("Traceback") != std::string::npos) {
        Log(std::string("[!] REPL: ") +
            reply.substr(0, (std::min)(reply.size(), size_t{160})));
        return false;
    }
    return reply.find(">>>") != std::string::npos;
}

std::string PySingleQuoted(const std::string& text) {
    std::string out = "'";
    for (unsigned char c : text) {
        if (c == '\\')
            out += "\\\\";
        else if (c == '\'')
            out += "\\'";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            continue;
        else if (c < 32 || c > 126)
            out += "\\x" + std::string(1, "0123456789abcdef"[c >> 4]) +
                std::string(1, "0123456789abcdef"[c & 15]);
        else
            out += static_cast<char>(c);
    }
    out += "'";
    return out;
}

bool WriteBoardFile(HANDLE handle, const std::string& name, const std::string& text) {
    if (!ReplExec(handle, "f=open('" + name + "','w')"))
        return false;
    constexpr size_t kChunk = 40;
    if (text.empty())
        return ReplExec(handle, "f.close()");
    for (size_t i = 0; i < text.size(); i += kChunk) {
        const std::string piece = text.substr(i, kChunk);
        if (!ReplExec(handle, "f.write(" + PySingleQuoted(piece) + ")"))
            return false;
    }
    return ReplExec(handle, "f.close()");
}

std::string ReadFileText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool WriteFileText(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out << text;
    return true;
}

void CloseCom(HANDLE& handle) {
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

std::vector<int> RankedPorts(int preferred) {
    std::vector<int> tryPorts;
    if (preferred > 0)
        tryPorts.push_back(preferred);
    for (const auto& p : ListPorts()) {
        if (ControlRank(p.description) >= 99)
            continue;
        if (std::find(tryPorts.begin(), tryPorts.end(), p.number) == tryPorts.end())
            tryPorts.push_back(p.number);
    }
    return tryPorts;
}

bool SetBaud(HANDLE handle, int baud) {
    DCB state{};
    state.DCBlength = sizeof(state);
    if (!GetCommState(handle, &state))
        return false;
    state.BaudRate = static_cast<DWORD>(baud);
    state.fDtrControl = DTR_CONTROL_DISABLE;
    state.fRtsControl = RTS_CONTROL_DISABLE;
    return SetCommState(handle, &state) != 0;
}

HANDLE OpenAndProbe(int preferred, int& usedPort, int& usedBaud) {
    auto ports = RankedPorts(preferred);
    if (ports.empty()) {
        Log("[!] No KMBox COM. Plug SERIAL into THIS PC, then press Enter once.");
        WaitForEnter();
        ports = RankedPorts(preferred);
        if (ports.empty())
            return INVALID_HANDLE_VALUE;
    }

    const int port = ports.front();
    Log(std::string("[*] Opening COM") + std::to_string(port) +
        " once at 115200");
    HANDLE h = OpenCom(port, 115200);
    if (h == INVALID_HANDLE_VALUE) {
        Log("[!] Could not open COM" + std::to_string(port) +
            ". Close the old flash window and anything else using that port.");
        return INVALID_HANDLE_VALUE;
    }
    if (Probe(h)) {
        usedPort = port;
        usedBaud = 115200;
        return h;
    }
    Log("[!] No >>> at 115200. Not switching baud. KMBox manual is 115200 only.");
    CloseHandle(h);
    return INVALID_HANDLE_VALUE;
}

bool FactoryRestore(HANDLE handle) {
    Log("[*] Factory step: wipe onboard scripts. Not toggling USB gadget.");
    ReplExec(handle, "exec(\"try:\\n import os\\n ns=('boot.py','main.py','ac_profile.txt','kmbox_identity.txt')\\n [os.remove(n) for n in ns if n in os.listdir()]\\nexcept Exception:\\n pass\")");
    if (!WriteBoardFile(handle, "boot.py", kFactoryBoot)) {
        Log("[!] Factory boot.py write failed");
        return false;
    }
    Log("[+] Factory boot.py written (no device.enable — avoids HID reconnect loop)");
    if (!WriteBoardFile(handle, "main.py", kFactoryMain)) {
        Log("[!] Factory main.py write failed");
        return false;
    }
    if (!WriteBoardFile(handle, "kmbox_identity.txt", kFactoryIdentity)) {
        Log("[!] Factory identity write failed");
        return false;
    }
    Log("[+] Factory identity written (stock / empty vid pid)");
    return true;
}

bool HardResetBox(HANDLE handle) {
    // Never use raw REPL (Ctrl-A). KMBox parses those bytes as km.baud()
    // and Baud=0 panics Core 1 (IntegerDivideByZero) — that is the click loop.
    Log("[*] Wipe boot.py with friendly REPL only (Ctrl-C, no raw mode).");
    const char nl[] = "\r\n";
    WriteAll(handle, nl, 2);
    if (ReadFor(handle, 400, ">>>").find(">>>") == std::string::npos) {
        Log("[!] No >>> yet. Not sending any other bytes.");
        return false;
    }
    if (!FactoryRestore(handle)) {
        Log("[!] Friendly wipe failed. Not falling back to raw REPL.");
        return false;
    }
    Log("[+] Empty boot.py written.");
    return true;
}

bool PushFirmware(HANDLE handle, const std::string& boot, const std::string& mainPy,
                  const std::string& ac, const std::string& identity) {
    Log("[*] Writing new firmware boot.py ...");
    if (!WriteBoardFile(handle, "boot.py", boot)) {
        Log("[!] boot.py write failed");
        return false;
    }
    Log("[+] boot.py written");
    Log("[*] Writing main.py ...");
    if (!WriteBoardFile(handle, "main.py", mainPy)) {
        Log("[!] main.py write failed");
        return false;
    }
    Log("[+] main.py written");
    Log("[*] Writing ac_profile.txt ...");
    if (!WriteBoardFile(handle, "ac_profile.txt", ac + "\n")) {
        Log("[!] ac_profile.txt write failed");
        return false;
    }
    Log("[+] ac_profile.txt written");
    Log("[*] Writing kmbox_identity.txt ...");
    if (identity.empty() || !WriteBoardFile(handle, "kmbox_identity.txt", identity)) {
        Log("[!] kmbox_identity.txt write failed");
        return false;
    }
    Log("[+] kmbox_identity.txt written");
    return true;
}

int RunLogged(const std::string& cmd) {
    Log(std::string("[*] ") + cmd);
    SECURITY_ATTRIBUTES inherit{};
    inherit.nLength = sizeof(inherit);
    inherit.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &inherit, 0))
        return -1;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA start{};
    start.cb = sizeof(start);
    start.dwFlags = STARTF_USESTDHANDLES;
    start.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    start.hStdOutput = writePipe;
    start.hStdError = writePipe;
    PROCESS_INFORMATION process{};
    // /S keeps the inner quotes. A bare _popen strips them and cmd then
    // reports "filename syntax is incorrect" for python.exe.
    std::string line = "cmd.exe /d /s /c \"" + cmd + "\"";
    std::vector<char> mutableLine(line.begin(), line.end());
    mutableLine.push_back('\0');
    const BOOL started = CreateProcessA(
        nullptr, mutableLine.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &start, &process);
    CloseHandle(writePipe);
    if (!started) {
        CloseHandle(readPipe);
        Log("[!] Could not start command");
        return -1;
    }

    std::string pending;
    char buf[512]{};
    DWORD read = 0;
    while (ReadFile(readPipe, buf, sizeof(buf), &read, nullptr) && read > 0) {
        pending.append(buf, read);
        for (;;) {
            const auto cut = pending.find('\n');
            if (cut == std::string::npos)
                break;
            std::string row = pending.substr(0, cut);
            pending.erase(0, cut + 1);
            while (!row.empty() && (row.back() == '\r' || row.back() == '\n'))
                row.pop_back();
            if (!row.empty())
                Log(std::string("    ") + row);
        }
    }
    if (!pending.empty())
        Log(std::string("    ") + pending);
    CloseHandle(readPipe);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return static_cast<int>(code);
}

struct FlashPart {
    uint8_t type = 0;
    uint8_t subtype = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
    std::string label;
};

uint32_t ReadLe32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) |
        (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) |
        (static_cast<uint32_t>(p[3]) << 24);
}

bool LoadPartitions(const std::string& path, std::vector<FlashPart>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::vector<unsigned char> buf(
        (std::istreambuf_iterator<char>(in)), {});
    for (size_t i = 0; i + 32 <= buf.size(); i += 32) {
        const uint16_t magic = static_cast<uint16_t>(buf[i] | (buf[i + 1] << 8));
        if (magic != 0x50AA) {
            if (out.empty())
                return false;
            break;
        }
        FlashPart part;
        part.type = buf[i + 2];
        part.subtype = buf[i + 3];
        part.offset = ReadLe32(&buf[i + 4]);
        part.size = ReadLe32(&buf[i + 8]);
        part.label.assign(reinterpret_cast<const char*>(&buf[i + 12]), 16);
        part.label.resize(strnlen(part.label.c_str(), 16));
        out.push_back(part);
    }
    return !out.empty();
}

bool EraseSavedScript(const FlashPart& part) {
    if (part.type != 0x01)
        return false;
    if (part.offset < 0x9000 || part.size == 0)
        return false;
    if ((part.offset % 0x1000) != 0 || (part.size % 0x1000) != 0)
        return false;
    std::string label = part.label;
    std::transform(label.begin(), label.end(), label.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (label.find("phy") != std::string::npos ||
        label.find("ota") != std::string::npos ||
        label.find("factory") != std::string::npos ||
        label.find("app") != std::string::npos)
        return false;
    if (label == "nvs" || label.find("vfs") != std::string::npos ||
        label.find("fat") != std::string::npos ||
        label.find("spiffs") != std::string::npos ||
        label.find("storage") != std::string::npos ||
        part.subtype == 0x02 || part.subtype == 0x81 || part.subtype == 0x82)
        return true;
    return false;
}

std::string g_python;

std::string Quote(const std::string& text) {
    return "\"" + text + "\"";
}

bool PyHasEsptool(const std::string& python) {
    if (python.empty())
        return false;
    return RunLogged(Quote(python) + " -m esptool version") == 0;
}

std::string FindOnPath(const char* name) {
    char buf[MAX_PATH]{};
    DWORD n = SearchPathA(nullptr, name, ".exe", MAX_PATH, buf, nullptr);
    if (n == 0 || n >= MAX_PATH)
        return {};
    std::string path = buf;
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.find("windowsapps") != std::string::npos)
        return {};
    return path;
}

bool FileExists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool InstallPortablePython() {
    const std::string tools = ExeDir() + "\\tools";
    const std::string dest = tools + "\\python";
    const std::string zip = tools + "\\python-embed.zip";
    const std::string python = dest + "\\python.exe";
    CreateDirectoryA(tools.c_str(), nullptr);
    CreateDirectoryA(dest.c_str(), nullptr);
    if (!FileExists(python)) {
        Log("[*] No real Python on this PC. Downloading portable Python 3.12.");
        Log("    This can take a minute. Leave the window open.");
        const std::string download =
            "curl.exe -L --fail --retry 3 -o " + Quote(zip) +
            " https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip";
        if (RunLogged(download) != 0)
            return false;
        const std::string expand =
            "tar.exe -xf " + Quote(zip) + " -C " + Quote(dest);
        if (RunLogged(expand) != 0)
            return false;
    } else {
        Log("[*] Portable Python is already extracted.");
    }
    if (!FileExists(python)) {
        Log("[!] Download finished but python.exe is missing.");
        return false;
    }
    {
        std::ofstream pth(dest + "\\python312._pth", std::ios::trunc);
        if (!pth)
            return false;
        pth << "python312.zip\n.\nLib\\site-packages\nimport site\n";
    }
    if (!PyHasEsptool(python)) {
        const std::string getPip = dest + "\\get-pip.py";
        if (!FileExists(getPip)) {
            const std::string pipDl =
                "curl.exe -L --fail --retry 3 -o " + Quote(getPip) +
                " https://bootstrap.pypa.io/get-pip.py";
            if (RunLogged(pipDl) != 0)
                return false;
        }
        Log("[*] Installing esptool into the portable Python. Leave the window open.");
        if (RunLogged(Quote(python) + " " + Quote(getPip)) != 0)
            return false;
        if (RunLogged(Quote(python) + " -m pip install setuptools wheel") != 0)
            return false;
        // The embeddable python312._pth file hides pip's isolated build folder,
        // so setuptools is installed and then not importable. Build in place.
        if (RunLogged(Quote(python) + " -m pip install --no-build-isolation esptool") != 0)
            return false;
    }
    g_python = python;
    return PyHasEsptool(g_python);
}

bool EnsureEsptool() {
    const std::string local = ExeDir() + "\\tools\\python\\python.exe";
    if (FileExists(local) && PyHasEsptool(local)) {
        g_python = local;
        return true;
    }
    for (const char* name : {"python", "python3"}) {
        const std::string found = FindOnPath(name);
        if (!found.empty() && PyHasEsptool(found)) {
            g_python = found;
            Log(std::string("[+] Using ") + found);
            return true;
        }
    }
    if (PyHasEsptool("py")) {
        g_python = "py";
        return true;
    }
    return InstallPortablePython();
}

std::string EspTool(int port, const std::string& beforeAfter, const std::string& args) {
    return Quote(g_python) + " -m esptool --chip esp32 --port COM" + std::to_string(port) +
        " --baud 115200 " + beforeAfter + " " + args;
}

bool RecoverFromPanic(int port) {
    Log("[*] Screen stays blank because boot.py crashes the chip before the UI starts.");
    if (!EnsureEsptool()) {
        Log("[!] Could not install esptool. This PC needs internet for the one-time download.");
        return false;
    }
    Log("[*] Entering the ROM bootloader and erasing script storage only.");
    Log("    The KMBox program partition is left in place.");
    ReleaseComUsers(port);
    const char* sequences[] = {
        "D0|R1|W0.4|D1|R0|W0.4|D0",
        "R0|D1|W0.4|R1|D0|W0.4|R0",
        "D1|R1|W0.3|D1|R0|W0.3|D0",
        "D1|R0|W0.4|D0|R1|W0.4|D1",
    };
    const std::string cfgPath = ExeDir() + "\\esptool.cfg";
    bool connected = false;
    for (const char* seq : sequences) {
        {
            std::ofstream cfg(cfgPath, std::ios::trunc);
            if (!cfg)
                return false;
            cfg << "[esptool]\ncustom_reset_sequence = " << seq
                << "\nconnect_attempts = 1\n";
        }
        SetEnvironmentVariableA("ESPTOOL_CFGFILE", cfgPath.c_str());
        Log(std::string("[*] Download-mode reset: ") + seq);
        if (RunLogged(EspTool(port, "--before default-reset --after no-reset", "flash-id")) == 0) {
            connected = true;
            break;
        }
    }
    SetEnvironmentVariableA("ESPTOOL_CFGFILE", nullptr);
    if (!connected) {
        Log("[!] CH340 reset did not enter download mode (boot 0x17 is normal flash boot).");
        Log("    GPIO0 is not being held low. Roll the mouse wheel on the KMBox mouse port,");
        Log("    or reflash the official B+ firmware.");
        return false;
    }
    const std::string partPath = ExeDir() + "\\kmbox-parttable.bin";
    if (RunLogged(EspTool(port, "--before no-reset --after no-reset",
            "read-flash 0x8000 0xC00 \"" + partPath + "\"")) != 0) {
        Log("[!] Could not read the partition table.");
        return false;
    }
    std::vector<FlashPart> parts;
    if (!LoadPartitions(partPath, parts)) {
        Log("[!] Partition table was not readable. Nothing was erased.");
        return false;
    }
    int erased = 0;
    for (const auto& part : parts) {
        const bool wipe = EraseSavedScript(part);
        Log(std::string(wipe ? "[*] erase " : "[*] keep  ") + part.label +
            " type=" + std::to_string(part.type) +
            " off=0x" + std::to_string(part.offset) +
            " size=0x" + std::to_string(part.size));
        if (!wipe)
            continue;
        char addr[32]{};
        char size[32]{};
        std::snprintf(addr, sizeof(addr), "0x%X", part.offset);
        std::snprintf(size, sizeof(size), "0x%X", part.size);
        if (RunLogged(EspTool(port, "--before no-reset --after no-reset",
                std::string("erase-region ") + addr + " " + size)) != 0) {
            Log(std::string("[!] Erase failed for ") + part.label);
            return false;
        }
        ++erased;
    }
    if (erased == 0) {
        Log("[!] No script/NVS partition was safe to erase. Nothing was wiped.");
        return false;
    }
    RunLogged(EspTool(port, "--before no-reset --after hard-reset", "run"));
    Log("[+] Script storage erased. The screen stays blank until the app boots.");
    Sleep(2000);
    HANDLE again = OpenCom(port, 115200);
    if (again == INVALID_HANDLE_VALUE) {
        Log("[*] COM did not reopen. Unplug serial, wait 5 seconds, plug it back,");
        Log("    then run KmboxOnboard.exe once. Do not send any keys yourself.");
        return true;
    }
    const std::string boot = ReadUntilQuiet(again, 10000, 800);
    LogBlob("[*] after erase:", boot);
    const bool stillBad = boot.find("Guru") != std::string::npos ||
        boot.find("Baud =0") != std::string::npos;
    if (boot.find(">>>") != std::string::npos)
        HardResetBox(again);
    CloseCom(again);
    if (stillBad) {
        Log("[!] Still panicking after the erase. Official B+ firmware is required.");
        return false;
    }
    Log("[+] Boot no longer shows Baud =0.");
    return true;
}

bool DownloadStockBin(std::string& outPath) {
    const std::string dir = ExeDir() + "\\firmware\\stock";
    CreateDirectoryA((ExeDir() + "\\firmware").c_str(), nullptr);
    CreateDirectoryA(dir.c_str(), nullptr);
    outPath = dir + "\\kmbox-bplus-20240516.bin";
    if (FileExists(outPath))
        return true;
    Log("[*] Downloading official KMBox B+ firmware 2024-05-16.");
    const std::string cmd =
        "curl.exe -fL --retry 3 -o " + Quote(outPath) +
        " https://www.kmbox.top/wiki_doc/firmware/kmboxB/history/kmboxB+%E5%9B%BA%E4%BB%B620240516.bin";
    return RunLogged(cmd) == 0 && FileExists(outPath);
}

bool FlashStockFirmware(int port) {
    Log("[*] Stock firmware cannot be written while the chip is in normal boot (0x17).");
    Log("[*] Hold the BOOT / IO0 button if the box has one.");
    Log("[*] Unplug the COM cable, plug it back, keep holding BOOT, then press Enter.");
    Log("[*] If there is no button, press Enter and this stops without erasing anything.");
    WaitForEnter();
    if (!EnsureEsptool())
        return false;
    std::string bin;
    if (!DownloadStockBin(bin)) {
        Log("[!] Could not download the official firmware.");
        return false;
    }
    ReleaseComUsers(port);
    const std::string id = EspTool(port, "--before no-reset --after no-reset", "flash-id");
    if (RunLogged(id) != 0) {
        Log("[!] Still not in download mode. Nothing was erased.");
        return false;
    }
    Log("[*] Download mode confirmed. Erasing flash and writing the official image at 0x1000.");
    if (RunLogged(EspTool(port, "--before no-reset --after no-reset", "erase-flash")) != 0)
        return false;
    const std::string write = EspTool(
        port, "--before no-reset --after hard-reset",
        "write-flash --flash-size 4MB 0x1000 " + Quote(bin));
    if (RunLogged(write) != 0)
        return false;
    Log("[+] Official May 2024 firmware written. Unplug both cables, wait 5 seconds, plug them back.");
    return true;
}

bool CatchNextBoot(int port) {
    Log("[*] Plug in ONLY the COM cable when it says so.");
    Log("[*] Ctrl-C is sent at the first boot line, before the script sets baud 0.");
    for (int cycle = 1; cycle <= 3; ++cycle) {
        Log(std::string("[*] Boot catch ") + std::to_string(cycle) + "/3. Waiting for COM" +
            std::to_string(port) + ".");
        HANDLE handle = INVALID_HANDLE_VALUE;
        for (int i = 0; i < 40 && handle == INVALID_HANDLE_VALUE; ++i) {
            handle = OpenCom(port, 115200);
            if (handle == INVALID_HANDLE_VALUE)
                Sleep(250);
        }
        if (handle == INVALID_HANDLE_VALUE) {
            Log("[!] COM did not appear. Plug the COM cable into this PC.");
            continue;
        }
        const DWORD deadline = GetTickCount() + 12000;
        std::string acc;
        bool sent = false;
        bool wiped = false;
        char buf[256]{};
        while (GetTickCount() < deadline && !wiped) {
            DWORD read = 0;
            if (!ReadFile(handle, buf, sizeof(buf), &read, nullptr) || read == 0) {
                Sleep(10);
                continue;
            }
            acc.append(buf, read);
            const bool stale =
                !sent &&
                (acc.find("Guru") != std::string::npos ||
                 acc.find("Baud =0") != std::string::npos) &&
                acc.find("ets Jun") == std::string::npos;
            if (stale) {
                Log("[*] Ignoring the old crash text.");
                acc.clear();
                continue;
            }
            if (!sent &&
                (acc.find("ets Jun") != std::string::npos ||
                 acc.find("2nd stage") != std::string::npos)) {
                LogBlob("[*] boot:", acc);
                Log("[*] Boot started. Sending Ctrl-C before boot.py runs.");
                const char ctrl[] = "\x03\x03\x03";
                WriteAll(handle, ctrl, 3);
                sent = true;
                acc.clear();
                continue;
            }
            if (sent && acc.find(">>>") != std::string::npos) {
                Log("[*] REPL is up. Writing an empty boot.py.");
                wiped = HardResetBox(handle);
                break;
            }
            if (sent &&
                (acc.find("Guru") != std::string::npos ||
                 acc.find("Baud =0") != std::string::npos)) {
                LogBlob("[*] still crashed:", acc);
                break;
            }
            if (acc.size() > 2500)
                acc.erase(0, acc.size() - 1200);
        }
        CloseCom(handle);
        if (wiped)
            return true;
        Log("[*] Unplug the COM cable, wait 2 seconds, plug it back in. Leave this window open.");
    }
    return false;
}

}  // namespace

bool WriteIdentityConfig(const std::string& path, const std::string& vid,
                         const std::string& pid) {
    std::ostringstream body;
    body << "# USB identity toward the GAME PC (HID port).\r\n"
         << "# ROBRUN_IDENTITY_V3 — vendor device.* (not full descriptor clone).\r\n";
    if (vid.empty() && pid.empty()) {
        body << "preset=stock\r\nvid=\r\npid=\r\n";
    } else {
        body << "preset=custom\r\nvid=" << vid << "\r\npid=" << pid << "\r\n";
    }
    return WriteFileText(path, body.str());
}

int main(int argc, char** argv) {
    int com = 0;
    std::string ac = "robrun";
    std::string vidArg;
    std::string pidArg;
    bool stockIdentity = false;
    bool listOnly = false;
    bool probeOnly = false;
    bool skipFactory = false;
    bool resetOnly = true;
    bool allowFlash = false;
    const std::string rootEarly = [] {
        char path[MAX_PATH]{};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string s = path;
        const auto slash = s.find_last_of("\\/");
        return slash == std::string::npos ? std::string(".") : s.substr(0, slash);
    }();
    g_log.open(rootEarly + "\\last-flash.log", std::ios::trunc);
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--list")
            listOnly = true;
        else if (a == "--probe")
            probeOnly = true;
        else if (a == "--skip-factory")
            skipFactory = true;
        else if (a == "--reset")
            resetOnly = true;
        else if (a == "--flash")
            allowFlash = true;
        else if ((a == "--com" || a == "-c") && i + 1 < argc) {
            std::string v = argv[++i];
            if (v.size() >= 3)
                com = PortNumber(v);
            else
                com = atoi(v.c_str());
        } else if ((a == "--ac") && i + 1 < argc) {
            ac = argv[++i];
            std::transform(ac.begin(), ac.end(), ac.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        } else if ((a == "--vid") && i + 1 < argc) {
            vidArg = argv[++i];
        } else if ((a == "--pid") && i + 1 < argc) {
            pidArg = argv[++i];
        } else if (a == "--stock") {
            stockIdentity = true;
        } else if (a == "--help" || a == "-h") {
            std::cout
                << "KmboxOnboard — KMBox USB B/B+ MicroPython push (not vendor .bin)\n"
                << "  Default: HARD RESET listen at 115200. Does not reset the chip.\n"
                << "  --flash         Old path: push firmware. Do not use while Baud=0.\n"
                << "  --list          List COM ports\n"
                << "  --probe         Probe km.version only\n"
                << "  --com COM5      Use this port\n"
                << "  --ac robrun     eac | be | ace | robrun (with --flash)\n"
                << "  --reset         Same as the default\n"
                << "  --skip-factory  Skip factory restore (push new firmware only)\n"
                << "Run on the SECOND PC with the SERIAL/control USB plugged in.\n"
                << "This does not bypass EAC/BE/ACE.\n";
            WaitForEnter();
            return 0;
        }
    }

    if (allowFlash)
        resetOnly = false;

    Log(resetOnly
            ? "KmboxOnboard  HARD RESET (115200, no chip reset, no 921600)"
            : "KmboxOnboard  (firmware push; not AC bypass)");
    Log("[*] HID cable ALSO powers the box. Unplug HID or the click loop keeps going.");

    const auto ports = ListPorts();
    if (ports.empty()) {
        Log("[!] No COM ports. Plug the box SERIAL cable into THIS PC.");
        WaitForEnter();
        return 1;
    }
    Log("[*] COM ports:");
    for (const auto& p : ports)
        Log(std::string("    COM") + std::to_string(p.number) + "  " + p.description);
    if (listOnly) {
        WaitForEnter();
        return 0;
    }

    int usedPort = 0;
    int usedBaud = 0;
    HANDLE handle = INVALID_HANDLE_VALUE;

    if (resetOnly) {
        usedPort = com > 0 ? com : RankedPorts(0).empty() ? 0 : RankedPorts(0).front();
        if (usedPort <= 0) {
            Log("[!] No COM to reset.");
            WaitForEnter();
            return 1;
        }
        Log(std::string("[*] COM") + std::to_string(usedPort) +
            ". No button, so this writes a blank boot.py during the boot, before the crash.");
        Log("[*] Unplug BOTH USB cables. Press Enter when they are out.");
        WaitForEnter();
        Log("[*] Now plug in ONLY the COM cable.");
        const bool wiped = CatchNextBoot(usedPort);
        if (wiped) {
            Log("[+] Blank boot.py is on the box. Unplug COM, wait 5 seconds, plug COM and PC back in.");
            WaitForEnter();
            return 0;
        }
        Log("[!] Did not get a prompt before the script crashed.");
        Log("    Run this again. Unplug when it says, then plug only COM when it says.");
        WaitForEnter();
        return 1;
    }

    handle = OpenAndProbe(com, usedPort, usedBaud);
    if (handle == INVALID_HANDLE_VALUE) {
        Log("[!] No km.version(). Serial cable, CH340 driver, 115200.");
        WaitForEnter();
        return 1;
    }
    Log(std::string("[+] KMBox USB on COM") + std::to_string(usedPort) + " @ " +
        std::to_string(usedBaud));
    if (probeOnly) {
        CloseCom(handle);
        WaitForEnter();
        return 0;
    }

    const std::string root = ExeDir();
    const std::string fw = root + "\\firmware";
    const std::string identityPath = fw + "\\kmbox_identity.txt";
    if (stockIdentity)
        WriteIdentityConfig(identityPath, "", "");
    else if (!vidArg.empty() && !pidArg.empty())
        WriteIdentityConfig(identityPath, vidArg, pidArg);
    const std::string identity = ReadFileText(identityPath);
    const std::string boot = ReadFileText(fw + "\\boot.py");
    const std::string mainPy = ReadFileText(fw + "\\main.py");
    if (boot.empty() || mainPy.empty()) {
        Log("[!] Missing " + fw + "\\boot.py or main.py");
        CloseCom(handle);
        WaitForEnter();
        return 1;
    }
    if (identity.empty()) {
        Log("[!] Missing firmware\\kmbox_identity.txt");
        CloseCom(handle);
        WaitForEnter();
        return 1;
    }
    if (ac != "eac" && ac != "be" && ac != "ace" && ac != "robrun")
        ac = "robrun";

    if (!ToFriendlyRepl(handle)) {
        Log("[!] No >>> prompt. Unplug BOTH cables (HID powers the MCU), wait 10s,");
        Log("    plug SERIAL only, retry Reset-KMBox.bat.");
        CloseCom(handle);
        WaitForEnter();
        return 1;
    }

    if (!skipFactory) {
        if (!FactoryRestore(handle)) {
            CloseCom(handle);
            WaitForEnter();
            return 1;
        }
        CloseCom(handle);
        WaitUnplugReplug(
            "STEP 1 / 2 — FACTORY is on the box. Power-cycle now.",
            "1. Unplug BOTH USB cables on the KMBox (serial AND HID).\n"
            "2. Wait 2 seconds so Windows drops the old USB identity.\n"
            "3. Plug the SERIAL / COM cable back into THIS PC only.\n"
            "   Leave the HID / game-PC cable unplugged until the next step.\n"
            "4. Wait until Device Manager shows the CH340 COM port again.");
        handle = OpenAndProbe(com > 0 ? com : usedPort, usedPort, usedBaud);
        if (handle == INVALID_HANDLE_VALUE) {
            Log("[!] Could not reopen serial after factory cycle.");
            Log("    Plug SERIAL into this PC, then run Flash-KMBox.bat again.");
            WaitForEnter();
            return 1;
        }
        Log(std::string("[+] Reconnected COM") + std::to_string(usedPort) + " @ " +
            std::to_string(usedBaud));
        if (!ToFriendlyRepl(handle)) {
            Log("[!] No >>> after replug. Retry Flash-KMBox.bat.");
            CloseCom(handle);
            WaitForEnter();
            return 1;
        }
    }

    Log("[*] Applying new firmware (identity-v3) ac=" + ac);
    if (!PushFirmware(handle, boot, mainPy, ac, identity)) {
        CloseCom(handle);
        Log("[!] Push failed partway. Retry. See last-flash.log.");
        WaitForEnter();
        return 1;
    }
    CloseCom(handle);
    WriteFileText(root + "\\kmbox_ac_profile.txt", ac + "\n");
    WriteFileText(fw + "\\ac_profile.txt", ac + "\n");
    Log("[+] Pushed boot.py, main.py, ac_profile.txt=" + ac +
        ", kmbox_identity.txt");

    auto idValue = [](const std::string& text, const char* key) {
        const std::string tag = std::string(key) + "=";
        const auto pos = text.find(tag);
        if (pos == std::string::npos)
            return std::string();
        auto start = pos + tag.size();
        auto end = text.find_first_of("\r\n", start);
        if (end == std::string::npos)
            end = text.size();
        return text.substr(start, end - start);
    };
    const std::string vidVal = idValue(identity, "vid");
    const std::string pidVal = idValue(identity, "pid");
    if (!vidVal.empty() && !pidVal.empty())
        Log("[*] HID vid/pid for game PC: " + vidVal + " / " + pidVal);
    else
        Log("[*] HID identity: stock KMBox (empty vid/pid in file)");

    WaitUnplugReplug(
        "STEP 2 / 2 — NEW firmware is on the box. Power-cycle now.",
        "1. Unplug BOTH USB cables on the KMBox again.\n"
        "2. Wait 2 seconds.\n"
        "3. Plug SERIAL / COM into THIS (second) PC.\n"
        "4. Plug HID / \"controlled PC\" into the GAME PC.\n"
        "5. On the game PC, Device Manager should show the new HID IDs\n"
        "   (default Logitech 046D:C547) after this cycle.");
    Log("[*] Copy kmbox_ac_profile.txt next to RobRunDMA.exe on this PC.");
    Log("[!] Onboard scripts are HID/serial tuning, not an EAC/BE/ACE bypass.");
    Log("[+] Done. You can close this window.");
    WaitForEnter();
    return 0;
}
