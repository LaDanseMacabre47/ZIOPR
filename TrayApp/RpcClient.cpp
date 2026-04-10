#include "RpcClient.h"
#include "RpcClient.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <rpc.h>
#include "StopService.h"

void RpcStopServiceCall()
{
    RPC_WSTR pszStringBinding = nullptr;
    RPC_STATUS status;

    status = RpcStringBindingCompose(
        nullptr,
        (RPC_WSTR)L"ncalrpc",
        nullptr,
        (RPC_WSTR)L"TrayServiceALPC",
        nullptr,
        &pszStringBinding
    );

    if (status != RPC_S_OK)
        return;

    status = RpcBindingFromStringBinding(pszStringBinding, &StopService_IfHandle);
    RpcStringFree(&pszStringBinding);

    if (status != RPC_S_OK)
        return;

    RpcTryExcept
    {
        ::RpcStopService();
    }
    RpcExcept(1)
    {
    }
    RpcEndExcept

    RpcBindingFree(&StopService_IfHandle);
}

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len)
{
    return malloc(len);
}

void __RPC_USER midl_user_free(void __RPC_FAR* ptr)
{
    free(ptr);
}
