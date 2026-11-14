#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <cstdio>

static constexpr DWORD TS_READ = 0x80006498;
static constexpr DWORD TS_MAP = 0x8000645C;
static constexpr DWORD TS_UNMAP = 0x80006460;

static void Mark(const char* s) {
    printf("%s", s);
    fflush(stdout);
    FILE* z = fopen("Z:\\driver-lab\\vm\\bg\\ts-live.log", "a");
    if (z) { fputs(s, z); fclose(z); }
}

static void TryRead(HANDLE h, uint64_t pa, DWORD sz) {
    uint8_t buf[8] = {};
    DWORD got = 0;
    BOOL ok = DeviceIoControl(h, TS_READ, &pa, 8, buf, sz, &got, nullptr);
    char line[160];
    snprintf(line, sizeof(line), "read pa=%llx sz=%lu ok=%d gle=%lu got=%lu b=%02x%02x%02x%02x\n",
             (unsigned long long)pa, sz, (int)ok, GetLastError(), got, buf[0], buf[1], buf[2], buf[3]);
    Mark(line);
}

static void TryMap(HANDLE h, uint64_t pa, uint32_t sz) {
    struct { uint64_t pa; uint32_t size; } in{ pa, sz };
    uint64_t va = 0;
    DWORD got = 0;
    char line[180];
    snprintf(line, sizeof(line), "before map pa=%llx sz=%x\n", (unsigned long long)pa, sz);
    Mark(line);
    BOOL ok = DeviceIoControl(h, TS_MAP, &in, 12, &va, 8, &got, nullptr);
    snprintf(line, sizeof(line), "map pa=%llx sz=%x ok=%d gle=%lu va=%llx\n",
             (unsigned long long)pa, sz, (int)ok, GetLastError(), (unsigned long long)va);
    Mark(line);
    if (ok && va) {
        auto* p = (volatile uint8_t*)(uintptr_t)va;
        snprintf(line, sizeof(line), "  bytes %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        Mark(line);
        DeviceIoControl(h, TS_UNMAP, &va, 8, nullptr, 0, &got, nullptr);
        Mark("unmap\n");
    }
}

int main() {
    HANDLE h = CreateFileW(L"\\\\.\\TsSvc", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    char line[80];
    snprintf(line, sizeof(line), "open %p gle=%lu\n", h, GetLastError());
    Mark(line);
    if (h == INVALID_HANDLE_VALUE) return 1;
    const uint64_t cr3 = 0x1ae000;
    TryRead(h, cr3, 4);
    TryRead(h, cr3 + 0xF80, 8);
    TryRead(h, 0x1000, 4);
    TryMap(h, cr3, 0x20);
    TryMap(h, cr3, 0x100);
    CloseHandle(h);
    Mark("done\n");
    return 0;
}
