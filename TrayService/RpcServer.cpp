#include "RpcServer.h"
#include "AuthManager.h"
#include "LicenseManager.h"
#include <stdlib.h>
#include <rpc.h>

#include "TrayService.h"
#include "StopService.h"

static HANDLE s_hStopEvent = nullptr;

// ---------------------------------------------------------------
// Вспомогательная проверка п.11:
// Если тикета нет — останавливаем фоновые задачи и возвращаем ошибку.
// ---------------------------------------------------------------
static LONG CheckLicenseTicket()
{
    if (!GetLicenseManager().HasTicket())
    {
        // П.11: останавливаем фоновые задачи
        GetLicenseManager().StopBackgroundTasks();
        return ERROR_LICENSE_QUOTA_EXCEEDED; // используем как "нет лицензии"
    }
    return ERROR_SUCCESS;
}

// ---------------------------------------------------------------
// Auth методы — не требуют лицензии
// ---------------------------------------------------------------
long RpcLogin(const wchar_t* username, const wchar_t* password)
{
    return GetAuthManager().Login(username, password);
}

void RpcLogout()
{
    GetAuthManager().Logout();
    GetLicenseManager().ClearTicket();
}

long RpcGetCurrentUser(long* authenticated, wchar_t** username)
{
    *authenticated = 0;
    *username = nullptr;

    wchar_t buf[256]{};
    BOOL ok = GetAuthManager().GetCurrentUser(buf);
    *authenticated = ok ? 1 : 0;

    if (ok)
    {
        size_t len = (wcslen(buf) + 1) * sizeof(wchar_t);
        *username = (wchar_t*)midl_user_allocate(len);
        if (*username) wcscpy_s(*username, wcslen(buf) + 1, buf);
    }
    return 0;
}

// ---------------------------------------------------------------
// Лицензионные методы — требуют аутентификации + тикета (п.11)
// ---------------------------------------------------------------
long RpcGetLicenseStatus(long* hasLicense, __int64* expiryUnixTime)
{
    *hasLicense = 0;
    *expiryUnixTime = 0;

    wchar_t user[256]{};
    if (!GetAuthManager().GetCurrentUser(user))
        return ERROR_NOT_AUTHENTICATED;

    std::wstring tok = GetAuthManager().GetAccessToken();
    LONG r = GetLicenseManager().CheckLicense(tok.c_str());

    if (r == ERROR_NOT_FOUND)
    {
        // Лицензия не найдена — п.11: останавливаем фоновые задачи
        GetLicenseManager().StopBackgroundTasks();
        *hasLicense = 0;
        return ERROR_SUCCESS; // возвращаем SUCCESS но hasLicense=0
    }

    if (r != ERROR_SUCCESS)
        return r;

    LONG check = CheckLicenseTicket();
    if (check != ERROR_SUCCESS)
    {
        *hasLicense = 0;
        return ERROR_SUCCESS; // нет тикета — hasLicense=0
    }

    LONGLONG exp = 0;
    GetLicenseManager().GetLicenseInfo(&exp);
    *hasLicense = 1;
    *expiryUnixTime = (__int64)exp;
    return 0;
}

long RpcActivateProduct(const wchar_t* activationCode)
{
    // Требуем аутентификацию
    wchar_t user[256]{};
    if (!GetAuthManager().GetCurrentUser(user))
        return ERROR_NOT_AUTHENTICATED;

    std::wstring tok = GetAuthManager().GetAccessToken();
    LONG r = GetLicenseManager().ActivateProduct(activationCode, tok.c_str());

    // После активации проверяем тикет (п.11)
    if (r == ERROR_SUCCESS)
    {
        LONG check = CheckLicenseTicket();
        if (check != ERROR_SUCCESS) return check;
    }
    return r;
}

// ---------------------------------------------------------------
// Легаси stop
// ---------------------------------------------------------------
void RpcStopService()
{
    if (s_hStopEvent) SetEvent(s_hStopEvent);
}

// ---------------------------------------------------------------
BOOL StartRpcServer()
{
    s_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!s_hStopEvent) return FALSE;

    RPC_STATUS status;
    status = RpcServerUseProtseqEpW(
        (RPC_WSTR)L"ncalrpc",
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        (RPC_WSTR)L"TrayServiceALPC2",
        nullptr);
    if (status != RPC_S_OK) return FALSE;

    status = RpcServerRegisterIf(TrayService_v1_0_s_ifspec, nullptr, nullptr);
    if (status != RPC_S_OK) return FALSE;

    RpcServerUseProtseqEpW(
        (RPC_WSTR)L"ncalrpc",
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        (RPC_WSTR)L"TrayServiceALPC",
        nullptr);
    RpcServerRegisterIf(StopService_v1_0_s_ifspec, nullptr, nullptr);

    status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);
    return (status == RPC_S_OK) ? TRUE : FALSE;
}

void StopRpcServer()
{
    RpcMgmtStopServerListening(nullptr);
    RpcServerUnregisterIf(TrayService_v1_0_s_ifspec, nullptr, FALSE);
    RpcServerUnregisterIf(StopService_v1_0_s_ifspec, nullptr, FALSE);
    if (s_hStopEvent) { CloseHandle(s_hStopEvent); s_hStopEvent = nullptr; }
}

void WaitForRpcServer()
{
    if (s_hStopEvent) WaitForSingleObject(s_hStopEvent, INFINITE);
    StopRpcServer();
}

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len) { return malloc(len); }
void __RPC_USER midl_user_free(void __RPC_FAR* ptr) { free(ptr); }