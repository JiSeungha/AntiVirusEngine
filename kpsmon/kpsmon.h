#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")
#pragma warning(disable:4047) //disable warning during to cast HANDLE to ULONG

//---------------------------------------------------------------------------
//      Global variables
//---------------------------------------------------------------------------

#define KPSMON_FILTER_NAME     L"kpsmon"

#define KPSMON_PORT_NAME L"\\kpsmonPort"

#define KPSMON_STRING_TAG 'Kpst'

#define PRE 0
#define POST 1

BOOLEAN isConnected = FALSE;

typedef struct KPSMON_DATA {

	//
	//  The filter handle that results from a call to
	//  FltRegisterFilter.
	//
	PDRIVER_OBJECT DriverObject;

	PFLT_FILTER Filter;

	PFLT_PORT ServerPort;

	PFLT_PORT ClientPort;

	PEPROCESS UserProcess;

} KPSMON_DATA, *PKPSMON_DATA;

typedef struct _KPSMON_NOTIFICATION {

	ULONG pId;
	UCHAR MJCode;
	BOOLEAN isWriteAccess;
	INT PrePost;
	ULONG BytesOfFileName;
	WCHAR fileName[256];

}KPSMON_NOTIFICATION, *PKPSMON_NOTIFICATION;

typedef struct _KPSMON_STREAM_HANDLE_CONTEXT {

	BOOLEAN allowed;

}KPSMON_STREAM_HANDLE_CONTEXT, *PKPSMON_STREAM_HANDLE_CONTEXT;

typedef struct _KPSMON_CMD_DATA {

	ULONG pId;

	INT OpCode;

	struct _KPSMON_CMD_DATA* link;

}KPSMON_CMD_DATA, *PKPSMON_CMD_DATA;

typedef struct _KPSMON_COMMAND {

	ULONG pId;

	INT Op;

}KPSMON_COMMAND, *PKPSMON_COMMAND;

typedef struct _KPSMON_REPLY {

	BOOLEAN allowed;

}KPSMON_REPLY, *PKPSMON_REPLY;

typedef struct _KPSMON_REPLY_MESSAGE {

	FILTER_REPLY_HEADER ReplyHeader;

	KPSMON_REPLY reply;

}KPSMON_REPLY_MESSAGE, *PKPSMON_REPLY_MESSAGE;

#define KPSMON_REPLY_BUFFER_SIZE (sizeof(FILTER_REPLY_HEADER) + sizeof(KPSMON_REPLY))

DRIVER_INITIALIZE DriverEntry;

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
kpsmonUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
kpsmonPreOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS
kpsmonPostOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_POSTOP_CALLBACK_STATUS
kpsmonSafePostOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);

NTSTATUS
kpsmonPortConnect(
	_In_ PFLT_PORT ClientPort,
	_In_opt_ PVOID ServerPortCookie,
	_In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
	_In_ ULONG SizeOfContext,
	_Outptr_result_maybenull_ PVOID *ConnectionCookie
);

VOID
kpsmonPortDisconnect(
	_In_opt_ PVOID ConnectionCookie
);

NTSTATUS
kpsmonMessageCallback(
	_In_ PVOID ConnectionCookie,
	_In_reads_bytes_opt_(InputBufferSize) PVOID InputBuffer,
	_In_ ULONG InputBufferSize,
	_Out_writes_bytes_to_opt_(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
	_In_ ULONG OutputBufferSize,
	_Out_ PULONG ReturnOutputBufferLength
);

NTSTATUS
kpsmonSendInformation(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PFLT_CALLBACK_DATA Data,
	_In_ INT PreOrPost,
	_Out_ PBOOLEAN Allowed
);

VOID
kpsmonReturnIrpMjCodeString(
	_In_  UCHAR MjCode,
	_Outptr_ PCHAR MjCodeString
);

VOID
KnProcessNotifyRoutineEx(
	_Inout_  PEPROCESS pProcess,
	_In_ HANDLE	processId,
	_In_opt_ PPS_CREATE_NOTIFY_INFO pCreateInfo
);

//
//  Structure that contains all the global data structures
//  used throughout NullFilter.
//

KPSMON_DATA kpsmonData;

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, kpsmonUnload)
#pragma alloc_text(PAGE, kpsmonPreOperation)
#pragma alloc_text(PAGE, kpsmonPostOperation)
#pragma alloc_text(PAGE, kpsmonPortConnect)
#pragma alloc_text(PAGE, kpsmonPortDisconnect)
#pragma alloc_text(PAGE, kpsmonMessageCallback)
#pragma alloc_text(PAGE, kpsmonSendInformation)
#endif


//
//  This defines what we want to filter with FltMgr
//

const FLT_CONTEXT_REGISTRATION ContextRegistration[] = {

	{ FLT_STREAMHANDLE_CONTEXT,
	0,
	NULL,
	sizeof(KPSMON_STREAM_HANDLE_CONTEXT),
	'ghat' },

	{ FLT_CONTEXT_END }
};

const FLT_OPERATION_REGISTRATION Callbacks[] = {

	{ IRP_MJ_CREATE,
	0,
	kpsmonPreOperation,
	kpsmonPostOperation },

	{ IRP_MJ_WRITE,
	0,
	kpsmonPreOperation,
	kpsmonPostOperation },

	{ IRP_MJ_READ,
	0,
	kpsmonPreOperation,
	kpsmonPostOperation },

	{ IRP_MJ_CLEANUP,
	0,
	kpsmonPreOperation,
	kpsmonPostOperation },

	{ IRP_MJ_CLOSE,
	0,
	kpsmonPreOperation,
	kpsmonPostOperation },

	{ IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION FilterRegistration = {

	sizeof(FLT_REGISTRATION),         //  Size
	FLT_REGISTRATION_VERSION,           //  Version
	0,                                  //  Flags

	NULL,                               //  Context
	Callbacks,                               //  Operation callbacks

	kpsmonUnload,                         //  FilterUnload

	NULL,                               //  InstanceSetup
	NULL,                  //  InstanceQueryTeardown
	NULL,                               //  InstanceTeardownStart
	NULL,                               //  InstanceTeardownComplete

	NULL,                               //  GenerateFileName
	NULL,                               //  GenerateDestinationFileName
	NULL                                //  NormalizeNameComponent

};