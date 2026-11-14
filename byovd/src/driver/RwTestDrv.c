// Minimal lab driver: session auth + process R/W only (BYOVD map — no service load).
// Device name blends with common filter-style names; session is bound to the authenticating process.

#include <ntifs.h>
#include <intrin.h>

#define IOCTL_SESSION_AUTH      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x870, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_READ_PROCESS_MEM  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_WRITE_PROCESS_MEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_DELETE_SYMLINK    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, FILE_WRITE_DATA)

#define MAX_RW_BYTES (64 * 1024)

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

typedef NTSTATUS (NTAPI *pfnMmCopyVirtualMemory)(
    PEPROCESS SourceProcess, PVOID SourceAddress,
    PEPROCESS TargetProcess, PVOID TargetAddress,
    SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode, PSIZE_T ReturnSize);

static pfnMmCopyVirtualMemory g_MmCopyVirtualMemory;
static PDEVICE_OBJECT g_DeviceObject;
static PDRIVER_DISPATCH g_origDeviceControl;
static PDRIVER_DISPATCH g_origCreate;
static PDRIVER_DISPATCH g_origClose;
static volatile LONG g_sessionAuth;
static ULONG64 g_sessionToken;
static ULONG_PTR g_authPid;

static WCHAR g_genDevName[64];
static WCHAR g_genSymName[64];

static const WCHAR* g_devPrefixes[] = {
    L"MsFlt", L"WdFlt", L"FltMgr", L"NdPxy", L"TdiFlt",
    L"MsSecF", L"WfpDr", L"NtFlt", L"ClfsF", L"VhdFlt",
    L"SrFlt", L"FsDep", L"PcwDrv", L"MsIScsi", L"VolSnap", L"WmiLib",
};
#define DEV_PREFIX_COUNT 16

static VOID PickBlendDeviceNames(void)
{
    LARGE_INTEGER ts;
    KeQuerySystemTime(&ts);
    ULONG seed = (ULONG)(ts.LowPart ^ (ts.HighPart >> 3) ^
        (ULONG)(ULONG_PTR)&g_genDevName ^ (ULONG)__rdtsc());

    ULONG prefIdx = seed % DEV_PREFIX_COUNT;
    ULONG suffix = (seed >> 4) % 10;
    WCHAR suffixCh = L'0' + (WCHAR)suffix;

    UNICODE_STRING uDev, uSym, uPfx;
    RtlInitUnicodeString(&uDev, L"\\Device\\");
    RtlInitUnicodeString(&uSym, L"\\DosDevices\\");
    RtlInitUnicodeString(&uPfx, g_devPrefixes[prefIdx]);

    RtlZeroMemory(g_genDevName, sizeof(g_genDevName));
    RtlZeroMemory(g_genSymName, sizeof(g_genSymName));

    RtlCopyMemory(g_genDevName, uDev.Buffer, uDev.Length);
    RtlCopyMemory((PUCHAR)g_genDevName + uDev.Length, uPfx.Buffer, uPfx.Length);
    *(WCHAR*)((PUCHAR)g_genDevName + uDev.Length + uPfx.Length) = suffixCh;

    RtlCopyMemory(g_genSymName, uSym.Buffer, uSym.Length);
    RtlCopyMemory((PUCHAR)g_genSymName + uSym.Length, uPfx.Buffer, uPfx.Length);
    *(WCHAR*)((PUCHAR)g_genSymName + uSym.Length + uPfx.Length) = suffixCh;
}

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

static BOOLEAN SessionAllowsIrp(PIRP Irp)
{
    if (!g_sessionAuth || !g_authPid)
        return FALSE;
    PEPROCESS req = IoGetRequestorProcess(Irp);
    if (!req)
        return FALSE;
    return (ULONG_PTR)PsGetProcessId(req) == g_authPid;
}

static NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS Status, ULONG_PTR Info)
{
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

static NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DevObj, PIRP Irp)
{
    PIO_STACK_LOCATION sp = IoGetCurrentIrpStackLocation(Irp);
    PDRIVER_DISPATCH orig = (sp->MajorFunction == IRP_MJ_CLOSE) ? g_origClose : g_origCreate;
    if (orig && orig != DispatchCreateClose)
        return orig(DevObj, Irp);
    return CompleteIrp(Irp, STATUS_SUCCESS, 0);
}

static NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DevObj, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DevObj);
    PIO_STACK_LOCATION sp = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioctl = sp->Parameters.DeviceIoControl.IoControlCode;
    ULONG inLen = sp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = sp->Parameters.DeviceIoControl.OutputBufferLength;
    PVOID sysBuf = Irp->AssociatedIrp.SystemBuffer;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR info = 0;

    if (ioctl == IOCTL_SESSION_AUTH) {
        if (inLen < sizeof(ULONG64) || !sysBuf) {
            return CompleteIrp(Irp, STATUS_INVALID_PARAMETER, 0);
        }
        ULONG64 challenge = *(ULONG64*)sysBuf;
        if (challenge == 0) {
            return CompleteIrp(Irp, STATUS_INVALID_PARAMETER, 0);
        }
        PEPROCESS req = IoGetRequestorProcess(Irp);
        if (!req) {
            return CompleteIrp(Irp, STATUS_ACCESS_DENIED, 0);
        }
        g_authPid = (ULONG_PTR)PsGetProcessId(req);
        g_sessionToken = MixSessionToken(challenge, g_authPid);
        InterlockedExchange(&g_sessionAuth, 1);
        if (outLen >= sizeof(ULONG64)) {
            RtlCopyMemory(sysBuf, &g_sessionToken, sizeof(ULONG64));
            info = sizeof(ULONG64);
        }
        return CompleteIrp(Irp, STATUS_SUCCESS, info);
    }

    if (!SessionAllowsIrp(Irp)) {
        return CompleteIrp(Irp, STATUS_ACCESS_DENIED, 0);
    }

    switch (ioctl) {
    case IOCTL_DELETE_SYMLINK: {
        status = STATUS_SUCCESS;
        if (g_genSymName[0]) {
            UNICODE_STRING sym;
            RtlInitUnicodeString(&sym, g_genSymName);
            status = IoDeleteSymbolicLink(&sym);
        }
        break;
    }
    case IOCTL_READ_PROCESS_MEM: {
        if (inLen < sizeof(PROC_READ_REQ) || !sysBuf) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        PPROC_READ_REQ req = (PPROC_READ_REQ)sysBuf;
        if (!req->Size || req->Size > outLen || req->Size > MAX_RW_BYTES) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        PEPROCESS proc = NULL;
        status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)req->ProcessId, &proc);
        if (!NT_SUCCESS(status)) break;
        if (g_MmCopyVirtualMemory) {
            SIZE_T copied = 0;
            status = g_MmCopyVirtualMemory(
                proc, (PVOID)(ULONG_PTR)req->Address,
                PsGetCurrentProcess(), sysBuf,
                req->Size, KernelMode, &copied);
            if (NT_SUCCESS(status)) info = copied;
        } else {
            KAPC_STATE apc;
            KeStackAttachProcess(proc, &apc);
            __try {
                RtlCopyMemory(sysBuf, (PVOID)(ULONG_PTR)req->Address, req->Size);
                status = STATUS_SUCCESS;
                info = req->Size;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                status = STATUS_ACCESS_VIOLATION;
            }
            KeUnstackDetachProcess(&apc);
        }
        ObDereferenceObject(proc);
        break;
    }
    case IOCTL_WRITE_PROCESS_MEM: {
        ULONG hdr = (ULONG)FIELD_OFFSET(PROC_WRITE_REQ, Data);
        if (inLen < hdr) { status = STATUS_BUFFER_TOO_SMALL; break; }
        PPROC_WRITE_REQ req = (PPROC_WRITE_REQ)sysBuf;
        if (!req->Size || req->Size > inLen - hdr || req->Size > MAX_RW_BYTES) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        PEPROCESS proc = NULL;
        status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)req->ProcessId, &proc);
        if (!NT_SUCCESS(status)) break;
        KAPC_STATE apc;
        KeStackAttachProcess(proc, &apc);
        __try {
            RtlCopyMemory((PVOID)(ULONG_PTR)req->Address, req->Data, req->Size);
            status = STATUS_SUCCESS;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status = STATUS_ACCESS_VIOLATION;
        }
        KeUnstackDetachProcess(&apc);
        ObDereferenceObject(proc);
        break;
    }
    default:
        if (g_origDeviceControl && g_origDeviceControl != DispatchDeviceControl)
            return g_origDeviceControl(DevObj, Irp);
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return CompleteIrp(Irp, status, info);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    UNICODE_STRING fn;
    RtlInitUnicodeString(&fn, L"MmCopyVirtualMemory");
    g_MmCopyVirtualMemory = (pfnMmCopyVirtualMemory)MmGetSystemRoutineAddress(&fn);

    g_origCreate = DriverObject->MajorFunction[IRP_MJ_CREATE];
    g_origClose = DriverObject->MajorFunction[IRP_MJ_CLOSE];
    g_origDeviceControl = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    DriverObject->DriverUnload = NULL;

    g_DeviceObject = DriverObject->DeviceObject;
    if (g_DeviceObject)
        g_DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
