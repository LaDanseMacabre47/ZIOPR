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
    long result = (long)::RpcLogin(h, username, password);
    RpcBindingFree(&h);
    return result;
}

void RpcClientLogout()
{
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    ::RpcLogout(h);
    RpcBindingFree(&h);
}

long RpcClientGetCurrentUser(long* authenticated, wchar_t username[256])
{
    *authenticated = 0;
    username[0] = L'\0';

    handle_t h = CreateBinding(L"TrayServiceALPC2");
    wchar_t* buf = nullptr;
    long result = (long)::RpcGetCurrentUser(h, authenticated, &buf);

    if (buf)
    {
        wcsncpy_s(username, 256, buf, _TRUNCATE);
        midl_user_free(buf);
    }
    RpcBindingFree(&h);
    return result;
}

long RpcClientGetLicenseStatus(long* hasLicense, __int64* expiryUnixTime)
{
    *hasLicense = 0; *expiryUnixTime = 0;
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    long result = (long)::RpcGetLicenseStatus(h, hasLicense, expiryUnixTime);
    RpcBindingFree(&h);
    return result;
}

long RpcClientActivateProduct(const wchar_t* activationCode)
{
    handle_t h = CreateBinding(L"TrayServiceALPC2");
    long result = (long)::RpcActivateProduct(h, activationCode);
    RpcBindingFree(&h);
    return result;
}

void RpcStopServiceCall()
{
    handle_t h = CreateBinding(L"TrayServiceALPC");
    ::RpcStopService(h);
    RpcBindingFree(&h);
}

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len) { return malloc(len); }
void __RPC_USER midl_user_free(void __RPC_FAR* ptr) { free(ptr); }