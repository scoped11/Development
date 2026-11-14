// VmLab — IOCTL process R/W lab driver (VM only, service load).
// Fixed device \\.\VmLab0 — GH-tutorial shape without AC bypass IOCTLs.

#include <ntifs.h>

#define DEVICE_NAME     L"\\Device\\VmLab0"
#define SYMLINK_NAME    L"\\DosDevices\\VmLab0"

#define IOCTL_SESSION_AUTH      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x870, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_READ_PROCESS_MEM  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_WRITE_PROCESS_MEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_WRITE_DATA)

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
static volatile LONG g_sessionAuth;
static ULONG64 g_sessionToken;
static ULONG_PTR g_authPid;

static ULONG64 MixSessionToken(ULONG64 challenge, ULONG_PTR pid)
{
    ULONG64 x = challenge ^ (ULONG64)pid ^ 0xC0FFEE5EEDBEE5ULL;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x ? x : 1;
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
    UNREFERENCED_PARAMETER(DevObj);
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
        if (inLen < sizeof(ULONG64) || !sysBuf)
            return CompleteIrp(Irp, STATUS_INVALID_PARAMETER, 0);
        ULONG64 challenge = *(ULONG64*)sysBuf;
        if (!challenge)
            return CompleteIrp(Irp, STATUS_INVALID_PARAMETER, 0);
        PEPROCESS req = IoGetRequestorProcess(Irp);
        if (!req)
            return CompleteIrp(Irp, STATUS_ACCESS_DENIED, 0);
        g_authPid = (ULONG_PTR)PsGetProcessId(req);
        g_sessionToken = MixSessionToken(challenge, g_authPid);
        InterlockedExchange(&g_sessionAuth, 1);
        if (outLen >= sizeof(ULONG64)) {
            RtlCopyMemory(sysBuf, &g_sessionToken, sizeof(ULONG64));
            info = sizeof(ULONG64);
        }
        return CompleteIrp(Irp, STATUS_SUCCESS, info);
    }

    if (!SessionAllowsIrp(Irp))
        return CompleteIrp(Irp, STATUS_ACCESS_DENIED, 0);

    switch (ioctl) {
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
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return CompleteIrp(Irp, status, info);
}

static VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING sym;
    RtlInitUnicodeString(&sym, SYMLINK_NAME);
    IoDeleteSymbolicLink(&sym);
    if (g_DeviceObject)
        IoDeleteDevice(g_DeviceObject);
    UNREFERENCED_PARAMETER(DriverObject);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    UNICODE_STRING fn;
    RtlInitUnicodeString(&fn, L"MmCopyVirtualMemory");
    g_MmCopyVirtualMemory = (pfnMmCopyVirtualMemory)MmGetSystemRoutineAddress(&fn);

    UNICODE_STRING devName, symName;
    RtlInitUnicodeString(&devName, DEVICE_NAME);
    RtlInitUnicodeString(&symName, SYMLINK_NAME);

    NTSTATUS status = IoCreateDevice(
        DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN, FALSE, &g_DeviceObject);
    if (!NT_SUCCESS(status))
        return status;

    status = IoCreateSymbolicLink(&symName, &devName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = NULL;
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    g_DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
