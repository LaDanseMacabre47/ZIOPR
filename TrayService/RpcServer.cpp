#include "RpcServer.h"
#include "ServiceMain.h"

#include <stdlib.h>
#include <rpc.h>
#include "StopService.h"

static HANDLE s_hStopEvent = nullptr;

void RpcStopService()
{
    if (s_hStopEvent)
        SetEvent(s_hStopEvent);
}

BOOL StartRpcServer()
{
    RPC_STATUS status;

    s_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!s_hStopEvent)
        return FALSE;

    status = RpcServerUseProtseqEpW(
        (RPC_WSTR)L"ncalrpc",
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        (RPC_WSTR)L"TrayServiceALPC",
        nullptr
    );

    if (status != RPC_S_OK)
        return FALSE;

    status = RpcServerRegisterIf(StopService_v1_0_s_ifspec, nullptr, nullptr);
    if (status != RPC_S_OK)
        return FALSE;

    status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);
    if (status != RPC_S_OK)
        return FALSE;

    return TRUE;
}

void StopRpcServer()
{
    RpcMgmtStopServerListening(nullptr);
    RpcServerUnregisterIf(StopService_v1_0_s_ifspec, nullptr, FALSE);

    if (s_hStopEvent)
    {
        CloseHandle(s_hStopEvent);
        s_hStopEvent = nullptr;
    }
}

void WaitForRpcServer()
{
    if (s_hStopEvent)
        WaitForSingleObject(s_hStopEvent, INFINITE);

    StopRpcServer();
}

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len)
{
    return malloc(len);
}

void __RPC_USER midl_user_free(void __RPC_FAR* ptr)
{
    free(ptr);
}
