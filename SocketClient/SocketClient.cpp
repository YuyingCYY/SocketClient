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
#include <mutex>
#include "json.hpp"

using json = nlohmann::json;

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "443"
#define DEFAULT_ADDRESS "nenweb.supreme.com.tw"
#define LOG_FILE_PATH "socket_client.log"
#define MAX_LOG_SIZE 10 * 1024 * 1024 // 10MB

class Logger
{
public:
	static void log(const std::string& level, const std::string& message) {
		std::lock_guard<std::mutex> lock(logMutex);
		std::ofstream logFile(LOG_FILE_PATH, std::ios::app);

		if (logFile.is_open()) {
			// Check file size
			logFile.seekp(0, std::ios::end);
			if (logFile.tellp() > MAX_LOG_SIZE) {
				logFile.close();
				// Rename old log file
				std::string backupPath = LOG_FILE_PATH + std::string(".old");
				remove(backupPath.c_str());
				rename(LOG_FILE_PATH, backupPath.c_str());
				logFile.open(LOG_FILE_PATH);
			}

			logFile << "[" << getTimestamp() << "][" << level << "] " << message << std::endl;
			logFile.close();
		}
	}

	static void info(const std::string& message) {
		log("INFO", message);
	}

	static void error(const std::string& message) {
		log("ERROR", message);
	}

	static void debug(const std::string& message) {
		log("DEBUG", message);
	}

private:
	static std::mutex logMutex;
	static std::string getTimestamp() {
		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		struct tm timeinfo;
		localtime_s(&timeinfo, &time);
		char buffer[80];
		strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
		return std::string(buffer);
	}
};

std::mutex Logger::logMutex;

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
	Logger::info("Initializing client...");
	Logger::debug("Station Type: " + std::string(stationType) +
		", Station Name: " + std::string(stationName) +
		", Station ID: " + std::string(stationId) +
		", Operator ID: " + std::string(operatorId));

	if (isInitialized) {
		Logger::info("Client already initialized");
		return true;
	}

	WSAData wsaData;
	struct addrinfo* result = NULL, * ptr = NULL, hints;
	int iResult;

	// 初始化 Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		Logger::error("WSAStartup failed with error: " + std::to_string(iResult));
		return false;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// 解析服務器地址和端口
	iResult = getaddrinfo(DEFAULT_ADDRESS, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		Logger::error("getaddrinfo failed with error: " + std::to_string(iResult));
		WSACleanup();
		return false;
	}

	// 嘗試連接到服務器
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			Logger::error("Socket creation failed with error: " + std::to_string(WSAGetLastError()));
			WSACleanup();
			return false;
		}

		Logger::debug("Attempting to connect to server...");
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			Logger::debug("Connection attempt failed, trying next address...");
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		Logger::error("Unable to connect to server");
		WSACleanup();
		return false;
	}

	Logger::info("Client initialized successfully");
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
		Logger::error("Attempt to send data while not initialized");
		return -1;
	}

	Logger::info("Sending data to server...");

	std::time_t timestamp = getCurrentTimestamp();
	json askContent = { {"productSeries", productSeries}, {"applicableProjects", applicableProjects}, {"customizeId", customizeId} };
	json data = {
		{"timestamp", timestamp},
		{"askId", askId},
		{"askContent", askContent},
		{"isGetFile", isGetFile}
	};

	std::string jsonStr = data.dump();

	int iResult = send(ConnectSocket, jsonStr.c_str(), (int)jsonStr.length(), 0);
	if (iResult == SOCKET_ERROR) {
		Logger::error("Send failed with error: " + std::to_string(WSAGetLastError()));
		return -1;
	}

	Logger::info("Data sent successfully: " + std::to_string(iResult) + " bytes");
	return iResult;
}

FileInfo* GetBinFileInfo(
	const char* askId,
	const char* productSeries,
	const char* applicableProjects,
	const char* customizeId
) {
	if (!isInitialized || ConnectSocket == INVALID_SOCKET) {
		Logger::error("GetBinFileInfo called while not initialized");
		return nullptr;
	}

	Logger::info("Getting binary file info...");

	// 發送請求資訊
	int sendResult = SendData(askId, productSeries, applicableProjects, customizeId, true);
	if (sendResult <= 0) {
		Logger::error("Failed to send data request for binary file info");
		return nullptr;
	}

	// 先接收 JSON header
	char headerBuffer[4096];
	int headerResult = recv(ConnectSocket, headerBuffer, 4096, 0);
	if (headerResult <= 0) {
		Logger::error("Failed to receive header data. Result: " + std::to_string(headerResult));
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

		Logger::info("Starting file content reception");
		while (totalReceived < fileSize) {
			int bytesReceived = recv(ConnectSocket, buffer,
				min(sizeof(buffer), (int)fileSize - (int)totalReceived), 0);
			if (bytesReceived <= 0) {
				Logger::error("Failed to receive file content. Bytes received: " +
					std::to_string(bytesReceived) +
					", Total received so far: " + std::to_string(totalReceived) +
					" of " + std::to_string(fileSize));
				delete[] fileInfo->data;
				delete fileInfo;
				return nullptr;
			}

			memcpy(fileInfo->data + totalReceived, buffer, bytesReceived);
			totalReceived += bytesReceived;
		}

		Logger::info("Successfully received file: " + fileName);
		return fileInfo;
	}
	catch (const json::exception& e) {
		Logger::error("JSON parsing error in GetBinFileInfo: " + std::string(e.what()) +
			"\nHeader content: " + std::string(headerBuffer));
		return nullptr;
	}
	catch (const std::exception& e) {
		Logger::error("Unexpected error in GetBinFileInfo: " + std::string(e.what()));
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
		Logger::error("GetMainAppInfo called while not initialized");
		return nullptr;
	}

	Logger::info("Getting main app info...");

	// 發送請求資訊
	int sendResult = SendData("MainApp", productSeries, applicableProjects, customizeId, false);
	if (sendResult <= 0) {
		Logger::error("Failed to send data request for main app info");
		return nullptr;
	}

	// 先接收 JSON header
	char headerBuffer[4096];
	int headerResult = recv(ConnectSocket, headerBuffer, 4096, 0);
	if (headerResult <= 0) {
		Logger::error("Failed to receive header data. Result: " + std::to_string(headerResult));
		return nullptr;
	}
	headerBuffer[headerResult] = '\0';

	try {
		// 解析 JSON header
		auto headerJson = json::parse(headerBuffer);
		if (headerJson["status"] == "error") {
			Logger::error("Server returned error status in header");
			return nullptr;
		}

		std::string version = headerJson["Version"];
		std::string blVersion = headerJson["BLVersion"];
		int calibrationOffset = headerJson["CalibrationOffset"];

		MainAppInfo* mainAppInfo = new MainAppInfo();
		strncpy_s(mainAppInfo->version, version.c_str(), sizeof(mainAppInfo->version) - 1);
		strncpy_s(mainAppInfo->blVersion, blVersion.c_str(), sizeof(mainAppInfo->blVersion) - 1);
		mainAppInfo->calibrationOffset = calibrationOffset;

		Logger::info("Successfully retrieved main app info");
		return mainAppInfo;
	}
	catch (const json::exception& e) {
		Logger::error("JSON parsing error in GetMainAppInfo: " + std::string(e.what()) +
			"\nHeader content: " + std::string(headerBuffer));
		return nullptr;
	}
	catch (const std::exception& e) {
		Logger::error("Unexpected error in GetMainAppInfo: " + std::string(e.what()));
		return nullptr;
	}
}

DefaultParametersInfo* GetDefaultParametersInfo(const char* productSeries, const char* applicableProjects, const char* customizeId) {
	if (!isInitialized || ConnectSocket == INVALID_SOCKET) {
		Logger::error("GetDefaultParametersInfo called while not initialized");
		return nullptr;
	}

	Logger::info("Getting default parameters info...");

	// 發送請求資訊
	int sendResult = SendData("DefaultParameters", productSeries, applicableProjects, customizeId, false);
	if (sendResult <= 0) {
		Logger::error("Failed to send data request for default parameters");
		return nullptr;
	}

	// 先接收 JSON header
	char headerBuffer[4096];
	int headerResult = recv(ConnectSocket, headerBuffer, 4096, 0);
	if (headerResult <= 0) {
		Logger::error("Failed to receive header data. Result: " + std::to_string(headerResult));
		return nullptr;
	}
	headerBuffer[headerResult] = '\0';

	try {
		// 解析 JSON header
		auto headerJson = json::parse(headerBuffer);
		if (headerJson["status"] == "error") {
			Logger::error("Server returned error status in header");
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

		Logger::info("Successfully retrieved default parameters info");
		return defaultParaInfo;
	}
	catch (const json::exception& e) {
		Logger::error("JSON parsing error in GetDefaultParametersInfo: " + std::string(e.what()) +
			"\nHeader content: " + std::string(headerBuffer));
		return nullptr;
	}
	catch (const std::exception& e) {
		Logger::error("Unexpected error in GetDefaultParametersInfo: " + std::string(e.what()));
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

bool CloseConnection() {
	Logger::info("Closing connection...");
	if (ConnectSocket != INVALID_SOCKET) {
		// 發送斷開連接請求
		json disconnectRequest;
		disconnectRequest["command"] = "disconnect";
		std::string jsonStr = disconnectRequest.dump();

		Logger::debug("Sending disconnect request");
		int sendResult = send(ConnectSocket, jsonStr.c_str(), (int)jsonStr.length(), 0);

		if (sendResult == SOCKET_ERROR) {
			// 發送失敗，關閉連接時發生錯誤
			Logger::error("Failed to send disconnect request: " + std::to_string(WSAGetLastError()));
			return false;
		}

		// 等待一小段時間，確保服務器收到請求
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		if (shutdown(ConnectSocket, SD_SEND) == SOCKET_ERROR) {
			// 關閉發送通道時發生錯誤
			Logger::error("shutdown failed with error: " + std::to_string(WSAGetLastError()));
			return false;
		}

		if (closesocket(ConnectSocket) == SOCKET_ERROR) {
			// 關閉 socket 時發生錯誤
			Logger::error("closesocket failed with error: " + std::to_string(WSAGetLastError()));
			return false;
		}

		ConnectSocket = INVALID_SOCKET;
	}
	if (isInitialized) {
		if (WSACleanup() == SOCKET_ERROR) {
			// 清理 Windows Sockets 時發生錯誤
			Logger::error("WSACleanup failed with error: " + std::to_string(WSAGetLastError()));
			return false;
		}
		isInitialized = false;
	}

	// 完成關閉
	Logger::info("Connection closed successfully");
	return true;
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