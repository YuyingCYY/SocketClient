#define WIN32_LEAN_AND_MEAN

#include "pch.h"
#include "SocketClient.h"
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <chrono>
#include <string>
#include <fstream>
#include "json.hpp"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "443"
#define DEFAULT_ADDRESS "nenweb.supreme.com.tw"

using json = nlohmann::json;

SOCKET ConnectSocket = INVALID_SOCKET;
bool isInitialized = false;

bool InitializeClient(
	const char* stationType, 
	const char* stationName, 
	const char* stationId,
	const char* operatorId
) {
	if (isInitialized) return true;

	WSAData wsaData;
	struct addrinfo* result = NULL, * ptr = NULL, hints;
	int iResult;

	// 初始化 Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		return false;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// 解析服務器地址和端口
	iResult = getaddrinfo(DEFAULT_ADDRESS, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		WSACleanup();
		return false;
	}

	// 嘗試連接到服務器
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			WSACleanup();
			return false;
		}

		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		WSACleanup();
		return false;
	}

	isInitialized = true;
	return true;
}

int SendData(const char* askId, const char* productSeries, const char* applicableProjects, bool isGetFile = false) {
	if (!isInitialized || ConnectSocket == INVALID_SOCKET) {
		return -1;
	}

	// 獲取當前時間戳
	auto now = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point time_point = now;
	std::time_t timestamp = std::chrono::system_clock::to_time_t(time_point);

	json askContent = { {"productSeries", productSeries}, {"applicableProjects", applicableProjects} };
	json data = {
		{"timestamp", timestamp},
		{"askId", askId},
		{"askContent", askContent},
		{"isGetFile", isGetFile}
	};

	std::string jsonStr = data.dump();

	int iResult = send(ConnectSocket, jsonStr.c_str(), jsonStr.length(), 0);
	if (iResult == SOCKET_ERROR) {
		return -1;
	}

	return iResult;
}

FileInfo* ReceiveFileInfo()
{
	if (!isInitialized || ConnectSocket == INVALID_SOCKET) {
		return nullptr;
	}

	// 先接收 JSON header
	char headerBuffer[4096];
	int headerResult = recv(ConnectSocket, headerBuffer, 4096, 0);
	if (headerResult <= 0) {
		return nullptr;
	}
	headerBuffer[headerResult] = '\0';

	try {
		// 解析 JSON header
		auto headerJson = json::parse(headerBuffer);
		if (headerJson["status"] == "error") {
			return nullptr;
		}

		size_t fileSize = headerJson["fileSize"];
		std::string fileName = headerJson["fileName"];

		// 建立 FileInfo 結構
		FileInfo* fileInfo = new FileInfo();
		fileInfo->data = new char[fileSize];
		fileInfo->size = fileSize;
		strncpy_s(fileInfo->fileName, fileName.c_str(), sizeof(fileInfo->fileName) - 1);

		// 接收檔案內容
		size_t totalReceived = 0;
		char buffer[4096];

		while (totalReceived < fileSize) {
			int bytesReceived = recv(ConnectSocket, buffer,
				min(sizeof(buffer), fileSize - totalReceived), 0);
			if (bytesReceived <= 0) {
				delete[] fileInfo->data;
				delete fileInfo;
				return nullptr;
			}

			memcpy(fileInfo->data + totalReceived, buffer, bytesReceived);
			totalReceived += bytesReceived;
		}

		return fileInfo;
	}
	catch (const json::exception& e) {
		return nullptr;
	}
}

void FreeFileInfo(FileInfo* fileInfo)
{
	if (fileInfo) {
		delete[] fileInfo->data;
		delete fileInfo;
	}
}

FileInfo* GetBinFileInfo(const char* askId, const char* productSeries, const char* applicableProjects)
{
	if (!isInitialized || ConnectSocket == INVALID_SOCKET) {
		return nullptr;
	}

	// 發送請求資訊
	int sendResult = SendData(askId, productSeries, applicableProjects, true);
	if (sendResult <= 0) {
		return nullptr;
	}

	// 接收檔案資訊
	FileInfo* fileInfo = ReceiveFileInfo();
	if (fileInfo == nullptr) {
		return nullptr;
	}

	return fileInfo;
}

int ReceiveData(char* buffer, int bufferSize) {
	if (!isInitialized || ConnectSocket == INVALID_SOCKET) {
		return -1;
	}

	int iResult = recv(ConnectSocket, buffer, bufferSize, 0);
	if (iResult > 0) {
		// 確保字符串終止
		buffer[iResult] = '\0';
	}
	return iResult;
}

void CloseConnection() {
	if (ConnectSocket != INVALID_SOCKET) {
		shutdown(ConnectSocket, SD_SEND);
		closesocket(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
	}
	if (isInitialized) {
		WSACleanup();
		isInitialized = false;
	}
}

//BOOL APIENTRY DllMain(
//	HMODULE hModule,
//	DWORD reason,
//	LPVOID lpReserved
//) {
//	switch (reason)
//	{
//	case DLL_PROCESS_ATTACH:
//		DisableThreadLibraryCalls(hModule);
//		break;
//	case DLL_PROCESS_DETACH:
//		if (isInitialized) {
//			CloseConnection();
//		}
//		break;
//	}
//	return TRUE;
//}