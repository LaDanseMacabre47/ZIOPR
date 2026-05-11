#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Auth
long  RpcClientLogin(const wchar_t* username, const wchar_t* password);
void  RpcClientLogout();
long  RpcClientGetCurrentUser(long* authenticated, wchar_t username[256]);

// License
long  RpcClientGetLicenseStatus(long* hasLicense, __int64* expiryUnixTime);
long  RpcClientActivateProduct(const wchar_t* activationCode);

// AV Database
long  RpcClientGetAvDatabaseInfo(long* recordCount, wchar_t releaseDate[64]);

// Scan
long  RpcClientScanFile(const wchar_t* filePath,
    long* isMalicious,
    wchar_t threatName[256]);

long  RpcClientScanDirectory(const wchar_t* dirPath,
    long* filesScanned,
    long* threatsFound,
    wchar_t** threatList);

// Legacy stop
void  RpcStopServiceCall();