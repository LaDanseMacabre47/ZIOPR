#include "RpcClient.h"
#include <stdlib.h>
#include <rpc.h>

#include "TrayService.h"
#include "StopService.h"

static handle_t CreateBinding(const wchar_t* endpoint)
{
    handle_t h = nullptr;
    RPC_WSTR psz = nullptr;
    RpcStringBindingComposeW(nullptr, (RPC_WSTR)L"ncalrpc",
        nullptr, (RPC_WSTR)endpoint, nullptr, &psz);
    RpcBindingFromStringBindingW(psz, &h);
    RpcStringFreeW(&psz);
    return h;
}

long RpcClientLogin(const wchar_t* username, const wchar_t* password)
{
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    long r = (long)::RpcLogin(h, username, password);
    RpcBindingFree(&h); return r;
}

void RpcClientLogout()
{
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    ::RpcLogout(h);
    RpcBindingFree(&h);
}

long RpcClientGetCurrentUser(long* authenticated, wchar_t username[256])
{
    *authenticated = 0; username[0] = L'\0';
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    wchar_t* buf = nullptr;
    long r = (long)::RpcGetCurrentUser(h, authenticated, &buf);
    if (buf) { wcsncpy_s(username, 256, buf, _TRUNCATE); midl_user_free(buf); }
    RpcBindingFree(&h); return r;
}

long RpcClientGetLicenseStatus(long* hasLicense, __int64* expiryUnixTime)
{
    *hasLicense = 0; *expiryUnixTime = 0;
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    long r = (long)::RpcGetLicenseStatus(h, hasLicense, expiryUnixTime);
    RpcBindingFree(&h); return r;
}

long RpcClientActivateProduct(const wchar_t* activationCode)
{
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    long r = (long)::RpcActivateProduct(h, activationCode);
    RpcBindingFree(&h); return r;
}

long RpcClientGetAvDatabaseInfo(long* recordCount, wchar_t releaseDate[64])
{
    *recordCount = 0; releaseDate[0] = L'\0';
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    wchar_t* buf = nullptr;
    long r = (long)::RpcGetAvDatabaseInfo(h, recordCount, &buf);
    if (buf) { wcsncpy_s(releaseDate, 64, buf, _TRUNCATE); midl_user_free(buf); }
    RpcBindingFree(&h); return r;
}

long RpcClientScanFile(const wchar_t* filePath,
    long* isMalicious,
    wchar_t        threatName[256])
{
    *isMalicious = 0; threatName[0] = L'\0';
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    wchar_t* buf = nullptr;
    long r = (long)::RpcScanFile(h, filePath, isMalicious, &buf);
    if (buf) { wcsncpy_s(threatName, 256, buf, _TRUNCATE); midl_user_free(buf); }
    RpcBindingFree(&h); return r;
}

long RpcClientScanDirectory(const wchar_t* dirPath,
    long* filesScanned,
    long* threatsFound,
    wchar_t** threatList)
{
    *filesScanned = 0; *threatsFound = 0; *threatList = nullptr;
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    long r = (long)::RpcScanDirectory(h, dirPath,
        filesScanned, threatsFound, threatList);
    RpcBindingFree(&h); return r;
}

void RpcStopServiceCall()
{
    handle_t h = CreateBinding(L"TrayServiceALPC");
    ::RpcStopService(h);
    RpcBindingFree(&h);
}

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len) { return malloc(len); }
void __RPC_USER midl_user_free(void __RPC_FAR* ptr) { free(ptr); }