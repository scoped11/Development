// Usermode client for VmLab0 (VM lab only). Build with build-vm-lab-driver.ps1
#include <Windows.h>
#include <stdint.h>
#include <stdio.h>

#define IOCTL_SESSION_AUTH      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x870, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_READ_PROCESS_MEM  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_WRITE_PROCESS_MEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_WRITE_DATA)

#pragma pack(push, 1)
typedef struct _PROC_READ_REQ {
    uint64_t ProcessId;
    uint64_t Address;
    uint32_t Size;
    uint32_t Pad;
} PROC_READ_REQ;
#pragma pack(pop)

static bool SessionAuth(HANDLE h)
{
    uint64_t challenge = GetTickCount64() ^ ((uint64_t)GetCurrentProcessId() << 32);
    if (!challenge) challenge = 1;
    DWORD ret = 0;
  return DeviceIoControl(h, IOCTL_SESSION_AUTH, &challenge, sizeof(challenge),
                         &challenge, sizeof(challenge), &ret, nullptr) != FALSE;
}

static bool ReadMarker(HANDLE h)
{
    volatile uint64_t marker = 0x1122334455667788ULL;
    PROC_READ_REQ req = {};
    req.ProcessId = GetCurrentProcessId();
    req.Address = (uint64_t)&marker;
    req.Size = (uint32_t)sizeof(marker);

    uint64_t got = 0;
    DWORD ret = 0;
    if (!DeviceIoControl(h, IOCTL_READ_PROCESS_MEM, &req, sizeof(req),
                         &got, sizeof(got), &ret, nullptr))
        return false;
    printf("VmLab read 0x%llx (expect 0x%llx)\n",
           (unsigned long long)got, (unsigned long long)marker);
    return got == marker;
}

int main()
{
    HANDLE h = CreateFileW(L"\\\\.\\VmLab0",
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        printf("Open \\\\.\\VmLab0 failed err=%lu — run install-vm-lab-driver.ps1\n", GetLastError());
        return 1;
    }
    if (!SessionAuth(h)) {
        printf("Session auth failed err=%lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    int rc = ReadMarker(h) ? 0 : 2;
    CloseHandle(h);
    return rc;
}
