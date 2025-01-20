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
#include <thread>
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

static std::time_t getCurrentTimestamp()
{
	auto now = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point time_point = now;
	return std::chrono::system_clock::to_time_t(time_point);
}

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

int SendData(
	const char* askId,
	const char* productSeries,
	const char* applicableProjects,
	const char* customizeId,
	bool isGetFile = false
) {
	if (!isInitialized || ConnectSocket == INVALID_SOCKET) {
		return -1;
	}

	std::time_t timestamp = getCurrentTimestamp();
	json askContent = { {"productSeries", productSeries}, {"applicableProjects", applicableProjects}, {"customizeId", customizeId} };
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

FileInfo* GetBinFileInfo(
	const char* askId,
	const char* productSeries,
	const char* applicableProjects,
	const char* customizeId
) {
	if (!isInitialized || ConnectSocket == INVALID_SOCKET) {
		return nullptr;
	}

	// 發送請求資訊
	int sendResult = SendData(askId, productSeries, applicableProjects, customizeId, true);
	if (sendResult <= 0) {
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

MainAppInfo* GetMainAppInfo(const char* productSeries, const char* applicableProjects, const char* customizeId) {
	if (!isInitialized || ConnectSocket == INVALID_SOCKET) {
		return nullptr;
	}

	// 發送請求資訊
	int sendResult = SendData("MainApp", productSeries, applicableProjects, customizeId, false);
	if (sendResult <= 0) {
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

		std::string version = headerJson["Version"];
		std::string blVersion = headerJson["BLVersion"];
		int calibrationOffset = headerJson["CalibrationOffset"];

		MainAppInfo* mainAppInfo = new MainAppInfo();
		strncpy_s(mainAppInfo->version, version.c_str(), sizeof(mainAppInfo->version) - 1);
		strncpy_s(mainAppInfo->blVersion, blVersion.c_str(), sizeof(mainAppInfo->blVersion) - 1);
		mainAppInfo->calibrationOffset = calibrationOffset;

		return mainAppInfo;
	}
	catch (const json::exception& e) {
		return nullptr;
	}
}

DefaultParametersInfo* GetDefaultParametersInfo(const char* productSeries, const char* applicableProjects, const char* customizeId) {
	if (!isInitialized || ConnectSocket == INVALID_SOCKET) {
		return nullptr;
	}

	// 發送請求資訊
	int sendResult = SendData("DefaultParameters", productSeries, applicableProjects, customizeId, false);
	if (sendResult <= 0) {
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

		std::string version = headerJson["Version"];
		std::string blVersion = headerJson["BLVersion"];

		DefaultParametersInfo* defaultParaInfo = new DefaultParametersInfo();
		strncpy_s(defaultParaInfo->version, version.c_str(), sizeof(defaultParaInfo->version) - 1);
		strncpy_s(defaultParaInfo->blVersion, blVersion.c_str(), sizeof(defaultParaInfo->blVersion) - 1);
		defaultParaInfo->calibrationOffset = headerJson["CalibrationOffset"];
		
		defaultParaInfo->shieldedZoneCount = headerJson["ShieldedZoneCount"];
		auto& zones = headerJson["ShieldedZone"];
		for (int i = 0; i < defaultParaInfo->shieldedZoneCount && i < 50; i++) {
			defaultParaInfo->shieldedZone[i].start = zones[i]["start"];
			defaultParaInfo->shieldedZone[i].end = zones[i]["end"];
		}

		return defaultParaInfo;
	}
	catch (const json::exception& e) {
		return nullptr;
	}
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
		// 發送斷開連接請求
		json disconnectRequest;
		disconnectRequest["command"] = "disconnect";
		std::string jsonStr = disconnectRequest.dump();
		send(ConnectSocket, jsonStr.c_str(), jsonStr.length(), 0);

		// 等待一小段時間，確保服務器收到請求
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

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