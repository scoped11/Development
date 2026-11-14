#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <cstdio>

static constexpr DWORD TS_MAP = 0x8000645C;
static constexpr DWORD TS_UNMAP = 0x80006460;
static constexpr DWORD TS_READ = 0x80006498;

#pragma pack(push, 1)
struct MapIn { uint64_t pa; uint32_t size; };
#pragma pack(pop)

static void Mark(const char* s) {
    FILE* f = fopen("C:\\DriverLab\\map4k-live.txt", "a");
    if (!f) return;
    fputs(s, f);
    fclose(f);
    FILE* z = fopen("Z:\\driver-lab\\vm\\bg\\ts-live.log", "a");
    if (z) { fputs(s, z); fclose(z); }
}

int main() {
    HANDLE h = CreateFileW(L"\\\\.\\TsSvc", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    printf("open %p gle=%lu\n", h, GetLastError());
    if (h == INVALID_HANDLE_VALUE) return 1;

    DWORD got = 0;
    const uint64_t pas[] = { 0x1000, 0x10000, 0x100000, 0x1000000, 0x2000000, 0x10000000 };
    const uint32_t sizes[] = { 0x100, 0x1000 };
    int wins = 0;
    for (uint64_t pa : pas) {
        uint32_t smoke = 0;
        BOOL rok = DeviceIoControl(h, TS_READ, &pa, 8, &smoke, 4, &got, nullptr);
        char line[160];
        snprintf(line, sizeof(line), "read8 pa=%llx ok=%d gle=%lu val=%08x\n",
            (unsigned long long)pa, (int)rok, GetLastError(), smoke);
        printf("%s", line);
        Mark(line);
        for (uint32_t sz : sizes) {
            snprintf(line, sizeof(line), "before map pa=%llx sz=%x\n", (unsigned long long)pa, sz);
            Mark(line);
            MapIn in{ pa, sz };
            uint64_t va = 0;
            LARGE_INTEGER t0, t1, f;
            QueryPerformanceFrequency(&f);
            QueryPerformanceCounter(&t0);
            BOOL mok = DeviceIoControl(h, TS_MAP, &in, sizeof(in), &va, 8, &got, nullptr);
            QueryPerformanceCounter(&t1);
            double ms = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart;
            snprintf(line, sizeof(line), "map pa=%llx sz=%x ok=%d gle=%lu va=%llx ms=%.2f\n",
                (unsigned long long)pa, sz, (int)mok, GetLastError(),
                (unsigned long long)va, ms);
            printf("%s", line);
            Mark(line);
            if (mok && va) {
                wins++;
                auto* p = (volatile uint8_t*)(uintptr_t)va;
                snprintf(line, sizeof(line), "  bytes %02x %02x %02x %02x\n", p[0], p[1], p[2], p[3]);
                printf("%s", line);
                Mark(line);
                uint64_t u = va;
                DeviceIoControl(h, TS_UNMAP, &u, 8, nullptr, 0, &got, nullptr);
            }
        }
    }
    printf("wins=%d\n", wins);
    CloseHandle(h);
    return wins ? 0 : 2;
}
