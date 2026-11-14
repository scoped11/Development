// Lab example: open mapped Betest, handshake, read 8 bytes from this process.
// Build (after map): cl /EHsc /std:c++17 process_rw.cpp /I..\include /Fe..\bin\process_rw.exe
#include "betest_ioctl.h"
#include <cstdio>

int main(int argc, char** argv)
{
    bool emitManifest = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--emit-manifest") == 0)
            emitManifest = true;
    }

    HANDLE h = BetestOpenDevice();
    if (h == INVALID_HANDLE_VALUE) {
        printf("Betest device not found. Map first: run-map-stable.cmd\n");
        return 1;
    }
    if (!BetestSessionAuth(h)) {
        printf("Session auth failed err=%u\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    volatile uint64_t marker = 0x1122334455667788ULL;
    uint64_t got = 0;
    if (!BetestReadProcess(h, GetCurrentProcessId(), (uint64_t)&marker, &got, sizeof(got))) {
        printf("Read failed err=%u\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    printf("read 0x%llx (expect 0x1122334455667788)\n", (unsigned long long)got);
    if (emitManifest && got == marker) {
        printf("{\"lane_d_kernel_proof\":{\"marker_ok\":true,\"pid\":%lu,\"method\":\"ioctl_0x805\"}}\n",
            (unsigned long)GetCurrentProcessId());
    }
    CloseHandle(h);
    return got == marker ? 0 : 2;
}
