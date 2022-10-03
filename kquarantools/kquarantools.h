#pragma once 
#pragma warning(disable:4311)
#pragma warning(disable:4302)

extern "C"
{
#include <ntddk.h>
#include <wdf.h>
}


EXTERN_C
NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
);

VOID
DriverUnload(
	_In_ PDRIVER_OBJECT DriverObject
);

VOID
KnProcessNotifyRoutineEx(
	_Inout_  PEPROCESS pProcess,
	_In_ HANDLE	processId,
	_In_opt_ PPS_CREATE_NOTIFY_INFO pCreateInfo
);