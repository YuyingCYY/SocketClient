#pragma once

#ifdef SOCKETCLIENT_EXPORTS
#define SOCKETCLIENT_API __declspec(dllexport)
#else
#define SOCKETCLIENT_API __declspec(dllimport)
#endif // SOCKETCLIENT_EXPORTS

#include <string>
#include <fstream>

extern "C" {
    struct FileInfo
    {
        char* data;
        size_t size;
        char fileName[256];
    };

    struct MainAppInfo {
        char version[256];
        char blVersion[256];
        int calibrationOffset;
        //bool IsSuccess;
    };

    struct ShieldedZoneInfo {
        int start;
        int end;
    };

    struct DefaultParametersInfo
    {
        char version[256];
        char blVersion[256];
        int calibrationOffset;
        int shieldedZoneCount;
        ShieldedZoneInfo shieldedZone[50];
    };

    /// <summary>
    /// 初始化客戶端
    /// </summary>
    /// <param name="stationType">本站類型</param>
    /// <param name="stationName">本站名稱</param>
    /// <param name="stationId">本站號碼</param>
    /// <param name="operatorId">登入操作人員</param>
    /// <returns></returns>
    SOCKETCLIENT_API bool InitializeClient(
        const char* stationType,
        const char* stationName,
        const char* stationId,
        const char* operatorId
    );

    // 接收數據
    SOCKETCLIENT_API int ReceiveData(char* buffer, int bufferSize);

    /// <summary>
    /// 關閉連接
    /// </summary>
    SOCKETCLIENT_API void CloseConnection();

    /// <summary>
    /// 傳送資料
    /// </summary>
    /// <param name="askId">站點類型</param>
    /// <param name="productSeries">產品系列</param>
    /// <param name="applicableProjects">適用專案</param>
    /// <param name="customizeId">客製化ID</param>
    /// <returns></returns>
    SOCKETCLIENT_API int SendData(
        const char* askId,
        const char* productSeries,
        const char* applicableProjects,
        const char* customizeId,
        bool isGetFile
    );

    /// <summary>
    /// 獲取.bin檔案資訊
    /// </summary>
    /// <param name="askId">站點類型</param>
    /// <param name="productSeries">產品系列</param>
    /// <param name="applicableProjects">適用專案</param>
    /// <param name="customizeId">客製化ID</param>
    /// <returns>檔案資訊</returns>
    SOCKETCLIENT_API FileInfo* GetBinFileInfo(
        const char* askId,
        const char* productSeries,
        const char* applicableProjects,
        const char* customizeId
    );

    /// <summary>
    /// 釋放資源，若有獲取檔案需釋放資源
    /// </summary>
    /// <param name="fileInfo"></param>
    SOCKETCLIENT_API void FreeFileInfo(FileInfo* fileInfo);

    /// <summary>
    /// 線上更新執行檔資訊
    /// </summary>
    /// <param name="productSeries">產品系列</param>
    /// <param name="applicableProjects">適用專案</param>
    /// <param name="customizeId">客製化ID</param>
    SOCKETCLIENT_API MainAppInfo* GetMainAppInfo(const char* productSeries, const char* applicableProjects, const char* customizeId);

    /// <summary>
    /// 線上更新參數資訊
    /// </summary>
    /// <param name="productSeries">產品系列</param>
    /// <param name="applicableProjects">適用專案</param>
    /// <param name="customizeId">客製化ID</param>
    /// <returns></returns>
    SOCKETCLIENT_API DefaultParametersInfo* GetDefaultParametersInfo(const char* productSeries, const char* applicableProjects, const char* customizeId);
}