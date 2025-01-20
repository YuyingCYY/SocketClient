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
    /// ��l�ƫȤ��
    /// </summary>
    /// <param name="stationType">��������</param>
    /// <param name="stationName">�����W��</param>
    /// <param name="stationId">�������X</param>
    /// <param name="operatorId">�n�J�ާ@�H��</param>
    /// <returns></returns>
    SOCKETCLIENT_API bool InitializeClient(
        const char* stationType,
        const char* stationName,
        const char* stationId,
        const char* operatorId
    );

    // �����ƾ�
    SOCKETCLIENT_API int ReceiveData(char* buffer, int bufferSize);

    /// <summary>
    /// �����s��
    /// </summary>
    SOCKETCLIENT_API void CloseConnection();

    /// <summary>
    /// �ǰe���
    /// </summary>
    /// <param name="askId">���I����</param>
    /// <param name="productSeries">���~�t�C</param>
    /// <param name="applicableProjects">�A�αM��</param>
    /// <returns></returns>
    SOCKETCLIENT_API int SendData(const char* askId, const char* productSeries, const char* applicableProjects, bool isGetFile);

    /// <summary>
    /// ���.bin�ɮ׸�T
    /// </summary>
    /// <param name="askId">���I����</param>
    /// <param name="productSeries">���~�t�C</param>
    /// <param name="applicableProjects">�A�αM��</param>
    /// <returns>�ɮ׸�T</returns>
    SOCKETCLIENT_API FileInfo* GetBinFileInfo(const char* askId, const char* productSeries, const char* applicableProjects);

    /// <summary>
    /// ����귽�A�Y������ɮ׻�����귽
    /// </summary>
    /// <param name="fileInfo"></param>
    SOCKETCLIENT_API void FreeFileInfo(FileInfo* fileInfo);

    /// <summary>
    /// �u�W��s�����ɸ�T
    /// </summary>
    /// <param name="productSeries"></param>
    /// <param name="applicableProjects"></param>
    SOCKETCLIENT_API MainAppInfo* GetMainAppInfo(const char* productSeries, const char* applicableProjects);

    SOCKETCLIENT_API DefaultParametersInfo* GetDefaultParametersInfo(const char* productSeries, const char* applicableProjects);
}