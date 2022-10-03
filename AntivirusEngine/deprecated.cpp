#include "AntivirusEngine.h"

using namespace std;

#define THREAD_COUNT 1

BOOL isConnectedToServer = FALSE;
SOCKET ConnectSocket = INVALID_SOCKET;
map<int, set<PKPSMON_DATA>*> data_list;

set<ULONG> block_list;

mutex m;

DWORD
SendOpCode(
	_In_ HANDLE port,
	_In_ ULONG pId,
	_In_ OpCode OpCode
)
{
	HRESULT hr = S_OK;
	CHAR   returnChar = NULL;
	ULONG    bytesReturned = 0;
	KPSMON_COMMAND command = { 0 };

	command.Op = OpCode;
	command.pId = pId;

	hr = FilterSendMessage(
		port,
		&command,
		sizeof(KPSMON_COMMAND),
		NULL,
		0,
		&bytesReturned);

	if (FAILED(hr)) {
		printf("[!] Data send Failed. Error = 0x%X\n", hr);
	}

	return hr;
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
		return TRUE;
	}

	return FALSE;
}

DWORD
EngineWorker(
	_In_ PKPSMON_THREAD_CONTEXT Context
)
{
	PKPSMON_NOTIFICATION notification;
	KPSMON_REPLY_MESSAGE replyMessage;
	PKPSMON_MESSAGE message;
	BOOL result = true;
	HRESULT hr;
	PKPSMON_DATA data;
	BOOLEAN code;

	while (TRUE)
	{
		message = (PKPSMON_MESSAGE)malloc(sizeof(KPSMON_MESSAGE));
		if (!result) {
			hr = HRESULT_FROM_WIN32(GetLastError());
			break;
		}

		hr = FilterGetMessage(Context->Port,
					&message->MessageHeader,
					sizeof(KPSMON_MESSAGE),
					NULL);

		
		notification = &message->Notification;

		data = (PKPSMON_DATA)malloc(sizeof(KPSMON_DATA));

		data->isWriteAccess = notification->isWriteAccess;
		data->mjCode = notification->MJCode;
		data->PrePost = notification->PrePost;
		data->BytesOfFileName = notification->BytesOfFileName;
		RtlCopyMemory(&data->FileName,
			notification->fileName,
			notification->BytesOfFileName);

		code = WorkData(notification->pId, data);

		//printf("%d,0x%x,%d,%d,%S,\n", notification->pId, data->mjCode, data->isWriteAccess ? 1 : 0, data->PrePost, data->FileName);
		
		ZeroMemory(&replyMessage, KPSMON_REPLY_BUFFER_SIZE);

		replyMessage.ReplyHeader.MessageId = message->MessageHeader.MessageId;

		replyMessage.Reply.allowed = code;
				
		FilterReplyMessage(Context->Port, &replyMessage.ReplyHeader, KPSMON_REPLY_BUFFER_SIZE);
	}

	if (!SUCCEEDED(hr)) {

		if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE)) {

			printf("kpsmon: Port is disconnected, probably due to kpsmon filter unloading.\n");
			
			isConnectedToServer = FALSE;
		}
		else {

			printf("kpsmon: Unknown error occured. Error = 0x%X\n", hr);
		}
	}

	free(message);

	return hr;
}

template <class Container>
void split(const string& str, Container& cont, char delim = ' ')
{
	stringstream ss(str);
	string token;
	while (getline(ss, token, delim)) {
		cont.push_back(token);
	}
}

void worker() {
	while (true) {
		m.lock();
		cout << (isConnectedToServer==TRUE?"Connected":"Disconnected") << "> ";
		m.unlock();
		
		string str;
		vector<string> inputs;

		getline(cin, str);

		split(str, inputs);

		if (inputs[0] == "quit") {
			break;
		}
		cin.clear();
	}
}

int _cdecl
main(
	_In_ int argc,
	_In_reads_(argc) char *argv[]
)
{
	HRESULT hr;
	HANDLE port, completion;
	KPSMON_THREAD_CONTEXT context;
	INT i = 0, j = 0;

	hr = FilterConnectCommunicationPort(
		KPSMON_PORT_NAME,
		0,
		NULL,
		0,
		NULL,
		&port);

	if (IS_ERROR(hr)){
		printf("[!] Error = 0x%X\n", hr);
		return 2;
	}

	completion = CreateIoCompletionPort(
		port,
		NULL,
		0,
		0);

	if (completion == NULL){
		CloseHandle(port);
		printf("[!] Error = 0x%X\n", hr);
		return 3;
	}

	isConnectedToServer = TRUE;

	context.Port = port;
	context.Completion = completion;
	
	
	thread *threads[THREAD_COUNT];

	for (i = 0; i < THREAD_COUNT; i++) {
		threads[i] = new thread(&EngineWorker, &context);
	}

	while (true) {
		cout << (isConnectedToServer == TRUE ? "Connected" : "Disconnected") << "> ";

		string str;
		vector<string> inputs;

		getline(cin, str);

		if (str.length() < 2) {
			continue;
		}
		
		split(str, inputs);

		if (inputs[0] == "quit") {
			goto memory_clean;
		}
		else if (inputs[0] == "block") {
			try {
				block_list.insert(stoi(inputs[1]));
			}
			catch (const invalid_argument &ia) {
				cout << "Wrong pID" << endl;
				continue;
			}
		}
		
		cin.clear();
	}
	
	for (i = 0; i < THREAD_COUNT; i++) {
		threads[i]->join();
	}

	hr = S_OK;

memory_clean:
	//clean memory
	CloseHandle(port);
	CloseHandle(completion);

	return hr;
}
