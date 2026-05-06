#include "RpcClient.h"
#include <stdlib.h>
#include <rpc.h>

#include "TrayService.h"
#include "StopService.h"

static void BindTo(const wchar_t* endpoint, handle_t* phBinding)
{
    RPC_WSTR psz = nullptr;
    RpcStringBindingComposeW(nullptr, (RPC_WSTR)L"ncalrpc",
        nullptr, (RPC_WSTR)endpoint, nullptr, &psz);
    RpcBindingFromStringBindingW(psz, phBinding);
    RpcStringFreeW(&psz);
}

// mingw не поддерживает __try/__except Ч используем setjmp или просто вызываем напр€мую
// RPC ошибки будут возвращены как коды возврата функций

long RpcClientLogin(const wchar_t* username, const wchar_t* password)
{
    BindTo(L"TrayServiceALPC2", &TrayService_IfHandle);
    long result = (long)::RpcLogin(username, password);
    RpcBindingFree(&TrayService_IfHandle);
    return result;
}

void RpcClientLogout()
{
    BindTo(L"TrayServiceALPC2", &TrayService_IfHandle);
    ::RpcLogout();
    RpcBindingFree(&TrayService_IfHandle);
}

long RpcClientGetCurrentUser(long* authenticated, wchar_t username[256])
{
    *authenticated = 0;
    username[0] = L'\0';

    BindTo(L"TrayServiceALPC2", &TrayService_IfHandle);
    wchar_t* buf = nullptr;
    long result = (long)::RpcGetCurrentUser(authenticated, &buf);

    if (buf)
    {
        wcsncpy_s(username, 256, buf, _TRUNCATE);
        midl_user_free(buf);
    }
    RpcBindingFree(&TrayService_IfHandle);
    return result;
}

long RpcClientGetLicenseStatus(long* hasLicense, __int64* expiryUnixTime)
{
    *hasLicense = 0; *expiryUnixTime = 0;
    BindTo(L"TrayServiceALPC2", &TrayService_IfHandle);
    long result = (long)::RpcGetLicenseStatus(hasLicense, expiryUnixTime);
    RpcBindingFree(&TrayService_IfHandle);
    return result;
}

long RpcClientActivateProduct(const wchar_t* activationCode)
{
    BindTo(L"TrayServiceALPC2", &TrayService_IfHandle);
    long result = (long)::RpcActivateProduct(activationCode);
    RpcBindingFree(&TrayService_IfHandle);
    return result;
}

void RpcStopServiceCall()
{
    handle_t hBinding = nullptr;
    BindTo(L"TrayServiceALPC", &hBinding);
    ::RpcStopService();
    RpcBindingFree(&hBinding);
}

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len) { return malloc(len); }
void __RPC_USER midl_user_free(void __RPC_FAR* ptr) { free(ptr); }