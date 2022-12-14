// ufscontrol.cpp: 콘솔 응용 프로그램의 진입점을 정의합니다.
//

#include "stdafx.h"
#include "ufscontrol.h"

#define KPSMON_DEFAULT_REQUEST_COUNT       5
#define KPSMON_DEFAULT_THREAD_COUNT        2
#define KPSMON_MAX_THREAD_COUNT            64

using namespace std;

BOOL isConnectedToServer = FALSE;
SOCKET ConnectSocket = INVALID_SOCKET;
map<int, set<PKPSMON_DATA>*> data_list;

set<ULONG> block_list;

mutex m;

template <class Container>
void split(const string& str, Container& cont, char delim = ' ')
{
	stringstream ss(str);
	string token;
	while (getline(ss, token, delim)) {
		cont.push_back(token);
	}
}

VOID
Blocker() {
	while (true) {
		m.lock();
		cout << (isConnectedToServer == TRUE ? "Connected" : "Disconnected") << "> ";
		m.unlock();

		string str;
		vector<string> inputs;

		getline(cin, str);

		split(str, inputs);

		if (inputs[0] == "quit") {
			break;
		}

		if (inputs[0] == "block") {
			try {
				block_list.insert((ULONG)stoi(inputs[1]));
			} 
			catch (const invalid_argument &ia) {
				cout << "invalid argument(PID)" << endl;
			}
		}

		cin.clear();
	}
}

BOOLEAN
WorkData(
	_In_ INT pId,
	_In_ PKPSMON_DATA data
)
{
	if (data_list.find(pId) == data_list.end()) {
		data_list[pId] = new set<PKPSMON_DATA>;
	}

	data_list[pId]->insert(data);


	if (block_list.find(pId) != block_list.end()) {
		return FALSE;
	}

	return TRUE;
}

DWORD
EngineWorker(
	_In_ PKPSMON_THREAD_CONTEXT Context
) {

	PKPSMON_NOTIFICATION notification;
	KPSMON_REPLY_MESSAGE replyMessage;
	PKPSMON_MESSAGE message;
	PKPSMON_DATA data;
	LPOVERLAPPED pOvlp;
	BOOL result;
	DWORD outSize;
	HRESULT hr;
	ULONG_PTR key;

#pragma warning(push)
#pragma warning(disable:4127) // conditional expression is constant

	while (TRUE) {

#pragma warning(pop)

		result = GetQueuedCompletionStatus(Context->Completion, &outSize, &key, &pOvlp, INFINITE);

		message = CONTAINING_RECORD(pOvlp, KPSMON_MESSAGE, Ovlp);

		if (!result) {

			//
			//  An error occured.
			//

			hr = HRESULT_FROM_WIN32(GetLastError());
			break;
		}

		//printf("Received message, size %Id\n", pOvlp->InternalHigh);

		notification = &message->Notification;

		//assert(notification->BytesToScan <= SCANNER_READ_BUFFER_SIZE);
		//_Analysis_assume_(notification->BytesToScan <= SCANNER_READ_BUFFER_SIZE);

		notification = &message->Notification;

		data = (PKPSMON_DATA)malloc(sizeof(KPSMON_DATA));

		data->isWriteAccess = notification->isWriteAccess;
		data->mjCode = notification->MJCode;
		data->PrePost = notification->PrePost;
		data->BytesOfFileName = notification->BytesOfFileName;
		RtlCopyMemory(&data->FileName,
			notification->fileName,
			notification->BytesOfFileName);

		BOOLEAN allowed = WorkData(notification->pId, data);

		ZeroMemory(&replyMessage, KPSMON_REPLY_BUFFER_SIZE);

		replyMessage.ReplyHeader.Status = 0;
		replyMessage.ReplyHeader.MessageId = message->MessageHeader.MessageId;


		replyMessage.Reply.allowed = allowed;

		hr = FilterReplyMessage(Context->Port, &replyMessage.ReplyHeader, KPSMON_REPLY_BUFFER_SIZE);

		//printf("%d,0x%x,%d,%d,%S\n", notification->pId, data->mjCode, data->isWriteAccess ? 1 : 0, data->PrePost, data->FileName);
		
		if (SUCCEEDED(hr)) {

			//printf("Replied message\n");

		}
		else {

			//printf("KPSMON: Error replying message. Error = 0x%X\n", hr);
			continue;
		}

		memset(&message->Ovlp, 0, sizeof(OVERLAPPED));

		hr = FilterGetMessage(Context->Port,
			&message->MessageHeader,
			FIELD_OFFSET(KPSMON_MESSAGE, Ovlp),
			&message->Ovlp);

		if (hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {

			break;
		}
	}

	if (!SUCCEEDED(hr)) {

		if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE)) {

			//
			//  Scanner port disconncted.
			//

			printf("KPSMON: Port is disconnected, probably due to scanner filter unloading.\n");

		}
		else {

			printf("KPSMON: Unknown error occured. Error = 0x%X\n", hr);
		}
	}

	free(message);

	return hr;
}

int _cdecl
main(
	_In_ int argc,
	_In_reads_(argc) char *argv[]
)
{
	DWORD requestCount = KPSMON_DEFAULT_REQUEST_COUNT;
	DWORD threadCount = KPSMON_DEFAULT_THREAD_COUNT;
	HANDLE threads[KPSMON_MAX_THREAD_COUNT];
	KPSMON_THREAD_CONTEXT context;
	HANDLE port, completion;
	PKPSMON_MESSAGE msg;
	DWORD threadId;
	HRESULT hr;
	DWORD i;

	hr = FilterConnectCommunicationPort(KPSMON_PORT_NAME,
		0,
		NULL,
		0,
		NULL,
		&port);

	if (IS_ERROR(hr)) {

		printf("ERROR: Connecting to filter port: 0x%08x\n", hr);
		return 2;
	}

	//
	//  Create a completion port to associate with this handle.
	//

	completion = CreateIoCompletionPort(port,
		NULL,
		0,
		threadCount);

	if (completion == NULL) {

		printf("ERROR: Creating completion port: %d\n", GetLastError());
		CloseHandle(port);
		return 3;
	}

	printf("KPSMON: Port = 0x%p Completion = 0x%p\n", port, completion);
	isConnectedToServer = TRUE;

	context.Port = port;
	context.Completion = completion;

	//
	//  Create specified number of threads.
	//

	for (i = 0; i < threadCount; i++) {

		threads[i] = CreateThread(NULL,
			0,
			(LPTHREAD_START_ROUTINE)EngineWorker,
			&context,
			0,
			&threadId);

		if (threads[i] == NULL) {

			//
			//  Couldn't create thread.
			//

			hr = GetLastError();
			printf("ERROR: Couldn't create thread: %d\n", hr);
			goto main_cleanup;
		}

		
		//
		//  Allocate the message.
		//

#pragma prefast(suppress:__WARNING_MEMORY_LEAK, "msg will not be leaked because it is freed in EngineWorker")
		msg = (PKPSMON_MESSAGE)malloc(sizeof(KPSMON_MESSAGE));

		if (msg == NULL) {

			hr = ERROR_NOT_ENOUGH_MEMORY;
			goto main_cleanup;
		}

		memset(&msg->Ovlp, 0, sizeof(OVERLAPPED));

		//
		//  Request messages from the filter driver.
		//

		hr = FilterGetMessage(port,
			&msg->MessageHeader,
			FIELD_OFFSET(KPSMON_MESSAGE, Ovlp),
			&msg->Ovlp);

		if (hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {

			free(msg);
			goto main_cleanup;
		}
		
	}

	Blocker();

	hr = S_OK;

	WaitForMultipleObjectsEx(i, threads, TRUE, INFINITE, FALSE);

main_cleanup:

	CloseHandle(port);
	CloseHandle(completion);

	return hr;
}
