#include "kquarantools.h"

DRIVER_INITIALIZE DriverEntry;

EXTERN_C
NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	DriverObject->DriverUnload = DriverUnload;

	NTSTATUS status = STATUS_SUCCESS;

	status = PsSetCreateProcessNotifyRoutineEx(KnProcessNotifyRoutineEx, FALSE);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("Error on PsSetCreateProcessNotifyRoutineEx(FALSE). status : 0x%X\n", status);
	}

	return status;
}

VOID
DriverUnload(
	_In_ PDRIVER_OBJECT DriverObject
)
{
	UNREFERENCED_PARAMETER(DriverObject);

	PsSetCreateProcessNotifyRoutineEx(KnProcessNotifyRoutineEx, TRUE);
}

VOID
KnProcessNotifyRoutineEx(
	_Inout_  PEPROCESS pProcess,
	_In_ HANDLE	processId,
	_In_opt_ PPS_CREATE_NOTIFY_INFO pCreateInfo
)
{
	UNREFERENCED_PARAMETER(pProcess);
	/*
	UNICODE_STRING uniString;

	RtlInitUnicodeString(&uniString, L"notepad.exe");

	if (RtlCompareUnicodeString(pCreateInfo->ImageFileName, &uniString, FALSE))
	{
		pCreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
	}
	*/

	if (pCreateInfo == NULL)
	{
		DbgPrint("%d exiting", (ULONG)processId);
	}
	else
	{
		DbgPrint("%wZ Created", pCreateInfo->ImageFileName);
	}

}