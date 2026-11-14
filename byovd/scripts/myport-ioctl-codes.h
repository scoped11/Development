// MyPortIO_x64.sys — IOCTL codes from IDA 0e7cd77a (main PC copy in byovd/vehicles/)
#pragma once
#include <stdint.h>

#define MYPORT_IOCTL_PHYS_READ_DWORD  0x9C406680u
#define MYPORT_IOCTL_PHYS_WRITE_DWORD 0x9C40A684u

#pragma pack(push, 1)
typedef struct MYPORT_PHYS_READ_INOUT {
    uint64_t Phys;
    uint32_t Size; /* must be 4 */
} MYPORT_PHYS_READ_INOUT; /* 12 bytes in/out */

typedef struct MYPORT_PHYS_WRITE_IN {
    uint64_t Phys;
    uint32_t Size;  /* must be 4 */
    uint32_t Value;
} MYPORT_PHYS_WRITE_IN; /* 16 bytes in */
#pragma pack(pop)
