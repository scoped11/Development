// Minimal cheat-dev loop: connect driver2, read your game's PID + VA.
// Build: see driver2\build.ps1 (-BuildExamples)
//
// Usage (after map):
//   cheat_dev_minimal.exe <pid> <hex_va>

#include "../sdk/Driver2Session.hpp"
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
    if (argc < 3) {
        printf("Usage: cheat_dev_minimal.exe <pid> <hex_va>\n");
        printf("Map first: Driver2Runner.exe --profile deltaforce --mode bypass\n");
        return 1;
    }

    uint64_t pid = (uint64_t)_strtoui64(argv[1], nullptr, 0);
    uint64_t va = (uint64_t)_strtoui64(argv[2], nullptr, 0);

    Driver2Session drv;
    if (!drv.Connect()) {
        printf("[!] driver not available — run Driver2Runner on lab VM\n");
        return 2;
    }
    if (!drv.ProbeSelfMarker()) {
        printf("[!] session probe failed\n");
        return 3;
    }
    printf("[+] driver session OK\n");

    uint64_t value = 0;
    if (!drv.ReadProcess(pid, va, &value, sizeof(value))) {
        printf("[!] ReadProcess failed err=%lu\n", GetLastError());
        return 4;
    }
    printf("[+] pid=%llu va=0x%llx -> 0x%llx\n",
        (unsigned long long)pid, (unsigned long long)va, (unsigned long long)value);
    return 0;
}
