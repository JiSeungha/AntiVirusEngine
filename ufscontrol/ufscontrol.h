#pragma once

#include "stdafx.h"

#define KPSMON_PORT_NAME L"\\kpsmonPort"

#define IRP_MJ_CREATE                   0x00
#define IRP_MJ_CREATE_NAMED_PIPE        0x01
#define IRP_MJ_CLOSE                    0x02
#define IRP_MJ_READ                     0x03
#define IRP_MJ_WRITE                    0x04
#define IRP_MJ_QUERY_INFORMATION        0x05
#define IRP_MJ_SET_INFORMATION          0x06
#define IRP_MJ_QUERY_EA                 0x07
#define IRP_MJ_SET_EA                   0x08
#define IRP_MJ_FLUSH_BUFFERS            0x09
#define IRP_MJ_QUERY_VOLUME_INFORMATION 0x0a
#define IRP_MJ_SET_VOLUME_INFORMATION   0x0b
#define IRP_MJ_DIRECTORY_CONTROL        0x0c
#define IRP_MJ_FILE_SYSTEM_CONTROL      0x0d
#define IRP_MJ_DEVICE_CONTROL           0x0e
#define IRP_MJ_INTERNAL_DEVICE_CONTROL  0x0f
#define IRP_MJ_SHUTDOWN                 0x10
#define IRP_MJ_LOCK_CONTROL             0x11
#define IRP_MJ_CLEANUP                  0x12
#define IRP_MJ_CREATE_MAILSLOT          0x13
#define IRP_MJ_QUERY_SECURITY           0x14
#define IRP_MJ_SET_SECURITY             0x15
#define IRP_MJ_POWER                    0x16
#define IRP_MJ_SYSTEM_CONTROL           0x17
#define IRP_MJ_DEVICE_CHANGE            0x18
#define IRP_MJ_QUERY_QUOTA              0x19
#define IRP_MJ_SET_QUOTA                0x1a
#define IRP_MJ_PNP                      0x1b
#define IRP_MJ_PNP_POWER                IRP_MJ_PNP      // Obsolete....
#define IRP_MJ_MAXIMUM_FUNCTION         0x1b


typedef struct _KPSMON_NOTIFICATION {

	ULONG pId;
	UCHAR MJCode;
	BOOLEAN isWriteAccess;
	INT PrePost;
	ULONG BytesOfFileName;
	WCHAR fileName[256];

}KPSMON_NOTIFICATION, *PKPSMON_NOTIFICATION;

typedef struct _KPSMON_THREAD_CONTEXT {

	HANDLE Port;
	HANDLE Completion;

} KPSMON_THREAD_CONTEXT, *PKPSMON_THREAD_CONTEXT;

typedef struct _KPSMON_MESSAGE {

	FILTER_MESSAGE_HEADER MessageHeader;
	KPSMON_NOTIFICATION Notification;
	OVERLAPPED Ovlp;

}KPSMON_MESSAGE, *PKPSMON_MESSAGE;

typedef struct _KPSMON_DATA {

	BOOL isWriteAccess;

	UCHAR mjCode;

	INT PrePost;

	WCHAR FileName[256];

	ULONG BytesOfFileName;

}KPSMON_DATA, *PKPSMON_DATA;

typedef struct _KPSMON_DATA_HEADER {

	ULONG pId;

	PKPSMON_DATA data;

	struct _KPSMON_DATA_HEADER* link;

}KPSMON_DATA_HEADER, *PKPSMON_DATA_HEADER;

typedef struct _KPSMON_COMMAND {

	ULONG pId;

	INT Op;

}KPSMON_COMMAND, *PKPSMON_COMMAND;

typedef struct _KPSMON_DATA_FORSERVER {

	ULONG pId;

	UCHAR MJCode[100];
	BOOLEAN isWriteAccess[100];
	INT PrePost[100];
	ULONG BytesOfFileName[100];
	WCHAR fileName[100][256];

}KPSMON_DATA_FORSERVER, *PKPSMON_DATA_FORSERVER;

enum OpCode {
	ignore,
	block,
	pause,
	kill
};

typedef struct _KPSMON_REPLY {

	BOOLEAN allowed;

}KPSMON_REPLY, *PKPSMON_REPLY;

typedef struct _KPSMON_REPLY_MESSAGE {

	FILTER_REPLY_HEADER ReplyHeader;

	KPSMON_REPLY Reply;

}KPSMON_REPLY_MESSAGE, *PKPSMON_REPLY_MESSAGE;

#define KPSMON_REPLY_BUFFER_SIZE (sizeof(FILTER_REPLY_HEADER) + sizeof(KPSMON_REPLY))