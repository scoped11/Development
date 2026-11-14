// Direct usermode control of mapped Driver2.sys (process + kernel R/W).
// Build: driver2\build.ps1 → Driver2Client.exe
//
// Usage (after Driver2Runner / everything map):
//   Driver2Client.exe probe
//   Driver2Client.exe kread 0xFFFFF78000000000 8
//   Driver2Client.exe pread <pid> <hex_va> <size>
//   Driver2Client.exe pwrite <pid> <hex_va> <hex_bytes...>

#include "sdk/driver2_api.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint64_t ParseU64(const char* s)
{
    return (uint64_t)_strtoui64(s, nullptr, 0);
}

static void Usage()
{
    printf("Driver2Client — direct IOCTL to mapped Driver2.sys\n\n");
    printf("  Driver2Client.exe [--device \\\\.\\WCIFS] probe\n");
    printf("  Driver2Client.exe ace-strengthen [hex_features=3f]\n");
    printf("  Driver2Client.exe ace-delta [hex_features=1f]\n");
    printf("  Driver2Client.exe eac-improve [hex_features=7f]\n");
    printf("  Driver2Client.exe eac-encptr [hex_features=f]\n");
    printf("  Driver2Client.exe be-improve [hex_features=1f]\n");
    printf("  Driver2Client.exe kread <kernel_va> [size<=256]\n");
    printf("  Driver2Client.exe pread <pid> <va> <size>\n");
    printf("  Driver2Client.exe pwrite <pid> <va> <hexbyte> [hexbyte...]\n\n");
    printf("Opens \\\\.\\MsFlt* / dynamic device; session auth required.\n");
}

int main(int argc, char** argv)
{
    int base = 1;
    if (argc >= 3 && !strcmp(argv[1], "--device")) {
        Driver2SetDeviceName(nullptr);
        BetestSetDeviceOverride(argv[2]);
        base = 3;
    }
    if (argc <= base) { Usage(); return 1; }

    HANDLE h = Driver2Open();
    if (h == INVALID_HANDLE_VALUE) {
        printf("[!] device not open — map Driver2 first (Driver2Runner / everything.ps1)\n");
        return 2;
    }
    if (!Driver2Auth(h)) {
        printf("[!] session auth failed err=%lu\n", GetLastError());
        CloseHandle(h);
        return 3;
    }

    const char* cmd = argv[base];
    if (!strcmp(cmd, "ace-strengthen")) {
        uint32_t feat = ACE_STR_ALL;
        if (argc >= 3) feat = (uint32_t)strtoul(argv[2], nullptr, 16);
        if (!Driver2AceStrengthen(h, feat)) {
            printf("[!] IOCTL 0x846 failed err=%lu\n", GetLastError());
            CloseHandle(h);
            return 9;
        }
        printf("[+] ACE strengthen 0x846 features=0x%x\n", feat);
        CloseHandle(h);
        return 0;
    }

    if (!strcmp(cmd, "ace-delta")) {
        uint32_t feat = ACE_DELTA_ALL;
        if (argc >= 3) feat = (uint32_t)strtoul(argv[2], nullptr, 16);
        if (!Driver2AceDelta(h, feat)) {
            printf("[!] IOCTL 0x84E failed err=%lu\n", GetLastError());
            CloseHandle(h);
            return 10;
        }
        printf("[+] ACE Delta 0x84E features=0x%x\n", feat);
        CloseHandle(h);
        return 0;
    }

    if (!strcmp(cmd, "eac-improve")) {
        uint32_t feat = EAC_IMP_ALL;
        if (argc >= 3) feat = (uint32_t)strtoul(argv[2], nullptr, 16);
        if (!Driver2EacImprove(h, feat)) {
            printf("[!] IOCTL 0x844 failed err=%lu\n", GetLastError());
            CloseHandle(h);
            return 7;
        }
        printf("[+] EAC improve 0x844 features=0x%x\n", feat);
        CloseHandle(h);
        return 0;
    }

    if (!strcmp(cmd, "eac-encptr")) {
        uint32_t feat = EAC_ENC_PTR_ALL;
        if (argc >= 3) feat = (uint32_t)strtoul(argv[2], nullptr, 16);
        if (!Driver2EosEncPtr(h, feat)) {
            printf("[!] IOCTL 0x851 failed err=%lu\n", GetLastError());
            CloseHandle(h);
            return 8;
        }
        printf("[+] EOS enc ptr 0x851 features=0x%x\n", feat);
        CloseHandle(h);
        return 0;
    }

    if (!strcmp(cmd, "be-improve")) {
        uint32_t feat = BE_ALL;
        if (argc >= 3) feat = (uint32_t)strtoul(argv[2], nullptr, 16);
        if (!Driver2BattleyeImprove(h, feat)) {
            printf("[!] IOCTL 0x852 failed err=%lu\n", GetLastError());
            CloseHandle(h);
            return 7;
        }
        printf("[+] BE improve 0x852 features=0x%x\n", feat);
        CloseHandle(h);
        return 0;
    }

    if (!strcmp(cmd, "probe")) {
        volatile uint64_t m = 0x1122334455667788ULL;
        uint64_t got = 0;
        if (!Driver2ReadProcess(h, GetCurrentProcessId(), (uint64_t)(uintptr_t)&m, &got, 8)) {
            printf("[!] pread failed err=%lu\n", GetLastError());
            CloseHandle(h);
            return 4;
        }
        printf("[+] probe 0x%llx\n", (unsigned long long)got);
        CloseHandle(h);
        return got == m ? 0 : 5;
    }

    if (!strcmp(cmd, "kread") && argc >= 3) {
        uint64_t kva = ParseU64(argv[2]);
        uint32_t sz = argc >= 4 ? (uint32_t)ParseU64(argv[3]) : 8;
        if (sz > 256) sz = 256;
        std::vector<uint8_t> buf(sz);
        if (!Driver2ReadKernel(h, kva, buf.data(), sz)) {
            printf("[!] kread failed err=%lu\n", GetLastError());
            CloseHandle(h);
            return 6;
        }
        printf("[+] kread %u bytes @ 0x%llx\n", sz, (unsigned long long)kva);
        for (uint32_t i = 0; i < sz; i++)
            printf("%02x", buf[i]);
        printf("\n");
        CloseHandle(h);
        return 0;
    }

    if (!strcmp(cmd, "pread") && argc >= 5) {
        uint64_t pid = ParseU64(argv[2]);
        uint64_t va = ParseU64(argv[3]);
        uint32_t sz = (uint32_t)ParseU64(argv[4]);
        if (sz > 65536) sz = 65536;
        std::vector<uint8_t> buf(sz);
        if (!Driver2ReadProcess(h, pid, va, buf.data(), sz)) {
            printf("[!] pread failed err=%lu\n", GetLastError());
            CloseHandle(h);
            return 7;
        }
        printf("[+] pread pid=%llu va=0x%llx len=%u\n",
            (unsigned long long)pid, (unsigned long long)va, sz);
        for (uint32_t i = 0; i < sz && i < 64; i++)
            printf("%02x", buf[i]);
        if (sz > 64) printf("...");
        printf("\n");
        CloseHandle(h);
        return 0;
    }

    if (!strcmp(cmd, "pwrite") && argc >= 5) {
        uint64_t pid = ParseU64(argv[2]);
        uint64_t va = ParseU64(argv[3]);
        std::vector<uint8_t> data;
        for (int i = 4; i < argc; i++) {
            unsigned v = (unsigned)strtoul(argv[i], nullptr, 16);
            data.push_back((uint8_t)v);
        }
        if (data.empty()) {
            Usage();
            CloseHandle(h);
            return 1;
        }
        if (!Driver2WriteProcess(h, pid, va, data.data(), (uint32_t)data.size())) {
            printf("[!] pwrite failed err=%lu\n", GetLastError());
            CloseHandle(h);
            return 8;
        }
        printf("[+] pwrite pid=%llu va=0x%llx len=%zu\n",
            (unsigned long long)pid, (unsigned long long)va, data.size());
        CloseHandle(h);
        return 0;
    }

    Usage();
    CloseHandle(h);
    return 1;
}
