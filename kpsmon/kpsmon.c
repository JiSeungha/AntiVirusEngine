
#include "kpsmon.h"


/*************************************************************************
Filter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Description:

This is the initialization routine for this miniFilter driver. This
registers the miniFilter with FltMgr and initializes all
its global data structures.

Arguments:

DriverObject - Pointer to driver object created by the system to
represent this driver.
RegistryPath - Unicode string identifying where the parameters for this
driver are located in the registry.

Return Value:

Returns STATUS_SUCCESS.

--*/
{
	NTSTATUS status = STATUS_SUCCESS;
	PSECURITY_DESCRIPTOR sd;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING uniString;

	UNREFERENCED_PARAMETER(RegistryPath);

	//
	//  Register with FltMgr
	//

	RtlInitUnicodeString(&uniString, KPSMON_PORT_NAME);


	try
	{
		status = FltBuildDefaultSecurityDescriptor(&sd,
			FLT_PORT_ALL_ACCESS);

		if (!NT_SUCCESS(status))
		{
			leave;
		}

		status = FltRegisterFilter(DriverObject,
			&FilterRegistration,
			&kpsmonData.Filter);

		if (!NT_SUCCESS(status))
		{
			leave;
		}

		InitializeObjectAttributes(&oa,
			&uniString,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
			NULL,
			sd);

		status = FltCreateCommunicationPort(
			kpsmonData.Filter,
			&kpsmonData.ServerPort,
			&oa,
			NULL,
			kpsmonPortConnect,
			kpsmonPortDisconnect,
			kpsmonMessageCallback,
			1);

		if (!NT_SUCCESS(status))
		{
			leave;
		}

		FltFreeSecurityDescriptor(sd);

		if (!NT_SUCCESS(status))
		{
			leave;
		}

		status = FltStartFiltering(kpsmonData.Filter);

		if (!NT_SUCCESS(status))
		{
			leave;
		}

		//status = PsSetCreateProcessNotifyRoutineEx(KnProcessNotifyRoutineEx, FALSE);

		/*if (!NT_SUCCESS(status))
		{
			DbgPrint("kpsmon: Error on PsSetCreateProcessNotifyRoutineEx(FALSE). status : 0x%X\n", status);
			leave;
		}*/

	}
	finally
	{
		if (!NT_SUCCESS(status))
		{
			if (kpsmonData.Filter != NULL)
			{
				FltUnregisterFilter(kpsmonData.Filter);
			}

			if (kpsmonData.ServerPort != NULL)
			{
				FltCloseCommunicationPort(kpsmonData.ServerPort);
			}

			DbgPrint("kpsmon: Load failed");
		}
	}

	return status;
}

NTSTATUS
kpsmonUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
/*++

Routine Description:

This is the unload routine for this miniFilter driver. This is called
when the minifilter is about to be unloaded. We can fail this unload
request if this is not a mandatory unloaded indicated by the Flags
parameter.

Arguments:

Flags - Indicating if this is a mandatory unload.

Return Value:

Returns the final status of this operation.

--*/
{
	UNREFERENCED_PARAMETER(Flags);
	NTSTATUS status = STATUS_SUCCESS;

	FltCloseCommunicationPort(kpsmonData.ServerPort);

	FltUnregisterFilter(kpsmonData.Filter);

	//status = PsSetCreateProcessNotifyRoutineEx(KnProcessNotifyRoutineEx, TRUE);

	return status;
}


NTSTATUS
kpsmonPortConnect(
	_In_ PFLT_PORT ClientPort,
	_In_opt_ PVOID ServerPortCookie,
	_In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
	_In_ ULONG SizeOfContext,
	_Outptr_result_maybenull_ PVOID *ConnectionCookie
)
{
	PAGED_CODE();

	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionCookie = NULL);

	FLT_ASSERT(kpsmonData.ClientPort == NULL);
	FLT_ASSERT(kpsmonData.UserProcess == NULL);

	//
	//  Set the user process and port. In a production filter it may
	//  be necessary to synchronize access to such fields with port
	//  lifetime. For instance, while filter manager will synchronize
	//  FltCloseClientPort with FltSendMessage's reading of the port 
	//  handle, synchronizing access to the UserProcess would be up to
	//  the filter.
	//

	kpsmonData.UserProcess = PsGetCurrentProcess();
	kpsmonData.ClientPort = ClientPort;

	isConnected = TRUE;

	DbgPrint("kpsmon: Port Connected");

	return STATUS_SUCCESS;
}

VOID
kpsmonPortDisconnect(
	_In_opt_ PVOID ConnectionCookie
)
{
	PAGED_CODE();

	UNREFERENCED_PARAMETER(ConnectionCookie);

	DbgPrint("kpsmon: Port Disconnected");

	FltCloseClientPort(kpsmonData.Filter, &kpsmonData.ClientPort);

	isConnected = FALSE;

	kpsmonData.UserProcess = NULL;

};

NTSTATUS
kpsmonMessageCallback(
	_In_ PVOID ConnectionCookie,
	_In_reads_bytes_opt_(InputBufferSize) PVOID InputBuffer,
	_In_ ULONG InputBufferSize,
	_Out_writes_bytes_to_opt_(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
	_In_ ULONG OutputBufferSize,
	_Out_ PULONG ReturnOutputBufferLength
)
{

	PAGED_CODE();

	UNREFERENCED_PARAMETER(InputBuffer);

	UNREFERENCED_PARAMETER(ConnectionCookie);

	UNREFERENCED_PARAMETER(InputBufferSize);

	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(OutputBuffer);


	//OutputBuffer = NULL;
	*ReturnOutputBufferLength = 0;

	//DbgPrint("clear");



	return STATUS_SUCCESS;
}

FLT_PREOP_CALLBACK_STATUS
kpsmonPreOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(CompletionContext);

	BOOLEAN allowed;

	kpsmonSendInformation(FltObjects, Data, PRE, &allowed);

	if (allowed != TRUE) {
		Data->IoStatus.Status = STATUS_ACCESS_DENIED;
		Data->IoStatus.Information = 0;
		return FLT_PREOP_COMPLETE;
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
kpsmonPostOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	FLT_POSTOP_CALLBACK_STATUS RetPostOperationStatus = FLT_POSTOP_FINISHED_PROCESSING;
	BOOLEAN ret;

	ret = FltDoCompletionProcessingWhenSafe(
		Data,
		FltObjects,
		CompletionContext,
		Flags,
		kpsmonSafePostOperation,
		&RetPostOperationStatus);

	if (ret == FALSE && RetPostOperationStatus == FLT_POSTOP_FINISHED_PROCESSING)
	{
		DbgPrint("Failed");
	}

	return RetPostOperationStatus;
}

FLT_POSTOP_CALLBACK_STATUS
kpsmonSafePostOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	BOOLEAN allowed;

	kpsmonSendInformation(FltObjects, Data, POST, &allowed);

	if (allowed != TRUE) {
		Data->IoStatus.Status = STATUS_ACCESS_DENIED;
		Data->IoStatus.Information = 0;
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS
kpsmonSendInformation(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ INT PreOrPost,
	_Inout_ PBOOLEAN Allowed
)
{
	PKPSMON_NOTIFICATION notification = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	ULONG userPid;

	*Allowed = TRUE;

	try
	{
		//DbgPrint("pID = %u, isWriteAccess = %as, PreOrPost = %s", FltGetRequestorProcessId(Data), FltObjects->FileObject->WriteAccess ? "true" : "false", (PreOrPost == 0) ? "PRE" : "POST");

		if (isConnected)
		{
			userPid = PsGetProcessId(kpsmonData.UserProcess);

			if (FltGetRequestorProcessId(Data) == userPid)
			{
				DbgPrint("My data");

				return status;
			}

			notification = ExAllocatePool(
				NonPagedPool,
				sizeof(KPSMON_NOTIFICATION));

			if (!NT_SUCCESS(status))
			{
				leave;
			}

			notification->MJCode = Data->Iopb->MajorFunction;

			notification->pId = FltGetRequestorProcessId(Data);

			notification->isWriteAccess = FltObjects->FileObject->WriteAccess;

			notification->PrePost = PreOrPost;

			notification->BytesOfFileName = min(FltObjects->FileObject->FileName.MaximumLength, 256);

			RtlCopyMemory(&notification->fileName,
				FltObjects->FileObject->FileName.Buffer,
				notification->BytesOfFileName);


			KPSMON_REPLY reply;
			ULONG replyLength = sizeof(reply);

			LARGE_INTEGER timeout = { 0 };

			timeout.QuadPart = -(1 * 1000000); //1ms

			status = FltSendMessage(
				kpsmonData.Filter,
				&kpsmonData.ClientPort,
				notification,
				sizeof(KPSMON_NOTIFICATION),
				&reply,
				&replyLength,
				&timeout);

			if (reply.allowed) DbgPrint("%d", reply.allowed);

			*Allowed = reply.allowed;

			if (status == STATUS_TIMEOUT) {
				*Allowed = TRUE;
			}

			if (!NT_SUCCESS(status))
			{
				leave;
			}

		}
		else
		{
			//DbgPrint("kpsmon: Not Connected!");
		}
	}
	finally
	{
		if (!NT_SUCCESS(status))
		{
			DbgPrint("kpsmon: Send Message Failed, status 0x%X", status);
		}
	}

	return status;
}

VOID
KnProcessNotifyRoutineEx(
	_Inout_  PEPROCESS pProcess,
	_In_ HANDLE	processId,
	_In_opt_ PPS_CREATE_NOTIFY_INFO pCreateInfo
)
{
	UNREFERENCED_PARAMETER(pProcess);
	UNREFERENCED_PARAMETER(processId);

	if (pCreateInfo == NULL) // Process exiting
	{

	}

}

VOID
kpsmonReturnIrpMjCodeString(
	_In_  UCHAR MjCode,
	_Outptr_ PCHAR MjCodeString
)
{
	switch (MjCode)
	{
	case IRP_MJ_CLEANUP:
		MjCodeString = "IRP_MJ_CLEANUP";
		break;
	case IRP_MJ_CLOSE:
		MjCodeString = "IRP_MJ_CLOSE";
		break;
	case IRP_MJ_CREATE:
		MjCodeString = "IRP_MJ_CREATE";
		break;
	case IRP_MJ_DEVICE_CONTROL:
		MjCodeString = "IRP_MJ_DEVICE_CONTROL";
		break;
	case IRP_MJ_FILE_SYSTEM_CONTROL:
		MjCodeString = "IRP_MJ_FILE_SYSTEM_CONTROL";
		break;
	case IRP_MJ_FLUSH_BUFFERS:
		MjCodeString = "IRP_MJ_FLUSH_BUFFERS";
		break;
	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
		MjCodeString = "IRP_MJ_INTERNAL_DEVICE_CONTROL";
		break;
	case IRP_MJ_PNP:
		MjCodeString = "IRP_MJ_PNP";
		break;
	case IRP_MJ_POWER:
		MjCodeString = "IRP_MJ_POWER";
		break;
	case IRP_MJ_QUERY_INFORMATION:
		MjCodeString = "IRP_MJ_QUERY_INFORMATION";
		break;
	case IRP_MJ_READ:
		MjCodeString = "IRP_MJ_READ";
		break;
	case IRP_MJ_SET_INFORMATION:
		MjCodeString = "IRP_MJ_SET_INFORMATION";
		break;
	case IRP_MJ_SHUTDOWN:
		MjCodeString = "IRP_MJ_SHUTDOWN";
		break;
	case IRP_MJ_SYSTEM_CONTROL:
		MjCodeString = "IRP_MJ_SYSTEM_CONTROL";
		break;
	case IRP_MJ_WRITE:
		MjCodeString = "IRP_MJ_WRITE";
		break;

	}
}
