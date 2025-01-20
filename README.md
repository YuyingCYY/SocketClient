# 後勤平台系統 Clinet 介面函式庫

## 資料結構定義

### 1. FileInfo

FileInfo 結構在調用 GetBinFileInfo 函式時被回傳。

```cpp
struct FileInfo {
    char* data;
    size_t size;
    char fileName[256];
}
```

成員：
- data: 資料內容。
- size: 檔案大小。
- fileName: 檔案名稱。

## 介面函式庫說明

### 1. InitialzeClient

初始化客戶端並連接至Server

```cpp
SOCKETCLIENT_API bool InitializeClient(const char* stationType, const char* stationName,
    const char* stationId, const char* operatorId);
```

參數：
- stationType: 本站類型
- stationName: 本站名稱
- stationId: 本站號碼
- operatorId: 登入操作人員

返回值：
  true 表示連接成功，false 表示失敗。

範例：
```cpp
#include "SocketClient.h"

const char* stationType = "type";
const char* stationName = "name";
const char* stationId = "Id";
const char* operatorId = "oId";

bool init = InitializeClient(stationType, stationName, stationId, operatorId);

if (!init) {
    std::cerr << "Failed to initialize client connection" << std::endl;
    return 1;
}

std::cout << "Connected to server successfully" << std::endl;
```

### 2. CloseConnection

關閉連接。

```cpp
SOCKETCLIENT_API void CloseConnection();
```

無參數無返回，在處理例外時使用，手動按鈕關閉時使用。

### 3. GetBinFileInfo

```cpp
SOCKETCLIENT_API bool GetBinFileInfo(const char* askId, const char* productSeries, const char* applicableProjects);
```

參數：
- askId: 站點類型
- productSeries: 產品系列
- applicableProjects: 適用專案

返回值：
  FileInfo* 結構

範例：
```cpp
#include <fstream>
#include <filesystem>
#include "SocketClient.h"

const char* askId = "MainApp";
const char* askId = "BMS";
const char* applicableProjects = "Thai";

FileInfo* fileInfo = GetBinFileInfo(askId, productSeries, applicableProjects);
if (!fileInfo) {
    std::cerr << "Failed to get file info" << std::endl;
    CloseConnection();
    return 1;
}

std::cout << "File received successfully:" << std::endl;
std::cout << "Name: " << fileInfo->fileName << std::endl;
std::cout << "Size: " << fileInfo->size << " bytes" << std::endl;

// 做成 stream
std::filesystem::path tempPath = std::filesystem::temp_directory_path() / fileInfo->fileName;
{
    std::ofstream tempFile(tempPath, std::ios::binary);
    if (!tempFile) {
        std::cerr << "Failed to create temporary file: " << tempPath << std::endl;
        FreeFileInfo(fileInfo);
        CloseConnection();
        return 1;
    }

    std::cout << "Saving file to: " << tempPath << std::endl;
    tempFile.write(fileInfo->data, fileInfo->size);
    tempFile.close();
}
```

### 4. FreeFileInfo

釋放資源，若有獲取檔案需釋放資源。

```cpp
SOCKETCLIENT_API void FreeFileInfo(FileInfo* fileInfo);
```

參數：
- fileInfo: FileInfo* 結構，獲取到的資源





