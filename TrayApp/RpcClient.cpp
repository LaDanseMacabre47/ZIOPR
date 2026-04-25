#include "RpcClient.h"
#include <stdlib.h>
#include <rpc.h>

#include "TrayService.h"
#include "StopService.h"

// TrayService_IfHandle и StopService_IfHandle объ€влены в сгенерированных файлах
// и используютс€ как implicit handle

static void BindTo(const wchar_t* endpoint, handle_t* phBinding)
{
    RPC_WSTR psz = nullptr;
    RpcStringBindingComposeW(nullptr, (RPC_WSTR)L"ncalrpc",
        nullptr, (RPC_WSTR)endpoint, nullptr, &psz);
    RpcBindingFromStringBindingW(psz, phBinding);
    RpcStringFreeW(&psz);
}

// ---------------------------------------------------------------
long RpcClientLogin(const wchar_t* username, const wchar_t* password)
{
    BindTo(L"TrayServiceALPC2", &TrayService_IfHandle);
    long result = -1;
    RpcTryExcept{ result = ::RpcLogin(username, password); }
    RpcExcept(1) { result = (long)RpcExceptionCode(); }
    RpcEndExcept
        RpcBindingFree(&TrayService_IfHandle);
    return result;
}

void RpcClientLogout()
{
    BindTo(L"TrayServiceALPC2", &TrayService_IfHandle);
    RpcTryExcept{ ::RpcLogout(); }
        RpcExcept(1) {}
    RpcEndExcept
        RpcBindingFree(&TrayService_IfHandle);
}

long RpcClientGetCurrentUser(long* authenticated, wchar_t username[256])
{
    *authenticated = 0;
    username[0] = L'\0';

    BindTo(L"TrayServiceALPC2", &TrayService_IfHandle);
    long result = -1;
    wchar_t* buf = nullptr;
    RpcTryExcept{ result = ::RpcGetCurrentUser(authenticated, &buf); }
    RpcExcept(1) { result = (long)RpcExceptionCode(); }
    RpcEndExcept

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
    long result = -1;
    RpcTryExcept{ result = ::RpcGetLicenseStatus(hasLicense, expiryUnixTime); }
    RpcExcept(1) { result = (long)RpcExceptionCode(); }
    RpcEndExcept
        RpcBindingFree(&TrayService_IfHandle);
    return result;
}

long RpcClientActivateProduct(const wchar_t* activationCode)
{
    BindTo(L"TrayServiceALPC2", &TrayService_IfHandle);
    long result = -1;
    RpcTryExcept{ result = ::RpcActivateProduct(activationCode); }
    RpcExcept(1) { result = (long)RpcExceptionCode(); }
    RpcEndExcept
        RpcBindingFree(&TrayService_IfHandle);
    return result;
}

void RpcStopServiceCall()
{
    BindTo(L"TrayServiceALPC", &StopService_IfHandle);
    RpcTryExcept{ ::RpcStopService(); }
        RpcExcept(1) {}
    RpcEndExcept
        RpcBindingFree(&StopService_IfHandle);
}

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len) { return malloc(len); }
void __RPC_USER midl_user_free(void __RPC_FAR* ptr) { free(ptr); }