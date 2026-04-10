#include "ServiceMain.h"
#include "SessionLauncher.h"
#include "RpcServer.h"

#include <wtsapi32.h>

const wchar_t* g_szServiceName = L"TrayService";

SERVICE_STATUS_HANDLE g_hServiceStatus = nullptr;
SERVICE_STATUS        g_serviceStatus{};
std::vector<LaunchedProcess> g_launchedProcesses;
CRITICAL_SECTION      g_csProcesses;

static BOOL IsSessionAdmin(DWORD sessionId)
{
    HANDLE hToken = nullptr;
    if (!WTSQueryUserToken(sessionId, &hToken))
        return FALSE;

    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    PSID pAdminSid = nullptr;

    if (AllocateAndInitializeSid(&ntAuth, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &pAdminSid))
    {
        CheckTokenMembership(hToken, pAdminSid, &isAdmin);
        FreeSid(pAdminSid);
    }

    CloseHandle(hToken);
    return isAdmin;
}

void SetServiceStatus(DWORD dwState, DWORD dwExitCode, DWORD dwWaitHint)
{
    g_serviceStatus.dwCurrentState  = dwState;
    g_serviceStatus.dwWin32ExitCode = dwExitCode;
    g_serviceStatus.dwWaitHint      = dwWaitHint;

    if (dwState == SERVICE_START_PENDING)
        g_serviceStatus.dwControlsAccepted = 0;
    else
        g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_SESSIONCHANGE;

    static DWORD dwCheckPoint = 1;
    if (dwState == SERVICE_RUNNING || dwState == SERVICE_STOPPED)
        g_serviceStatus.dwCheckPoint = 0;
    else
        g_serviceStatus.dwCheckPoint = dwCheckPoint++;

    ::SetServiceStatus(g_hServiceStatus, &g_serviceStatus);
}

void StopAllLaunchedApps()
{
    EnterCriticalSection(&g_csProcesses);

    std::vector<LaunchedProcess> kept;

    for (auto& lp : g_launchedProcesses)
    {
        if (!lp.hProcess || lp.hProcess == INVALID_HANDLE_VALUE)
            continue;

        if (IsSessionAdmin(lp.sessionId))
        {
            kept.push_back(lp);
            continue;
        }

        TerminateProcess(lp.hProcess, 0);
        CloseHandle(lp.hProcess);
    }

    g_launchedProcesses = std::move(kept);

    LeaveCriticalSection(&g_csProcesses);
}

DWORD WINAPI ServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType,
                                   LPVOID lpEventData, LPVOID lpContext)
{
    UNREFERENCED_PARAMETER(lpContext);

    switch (dwControl)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        return NO_ERROR;

    case SERVICE_CONTROL_SESSIONCHANGE:
    {
        if (dwEventType == WTS_SESSION_LOGON)
        {
            WTSSESSION_NOTIFICATION* pSessionNotif =
                static_cast<WTSSESSION_NOTIFICATION*>(lpEventData);
            if (pSessionNotif && pSessionNotif->dwSessionId != 0)
            {
                LaunchAppInSession(pSessionNotif->dwSessionId);
            }
        }
        return NO_ERROR;
    }

    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;

    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI ServiceMain(DWORD argc, LPWSTR* argv)
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    InitializeCriticalSection(&g_csProcesses);

    g_hServiceStatus = RegisterServiceCtrlHandlerExW(
        g_szServiceName, ServiceCtrlHandlerEx, nullptr);

    if (!g_hServiceStatus)
    {
        DeleteCriticalSection(&g_csProcesses);
        return;
    }

    g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

    SetServiceStatus(SERVICE_START_PENDING, 0, 3000);

    if (!StartRpcServer())
    {
        SetServiceStatus(SERVICE_STOPPED, GetLastError());
        DeleteCriticalSection(&g_csProcesses);
        return;
    }

    SetServiceStatus(SERVICE_RUNNING);

    LaunchAppInAllSessions();

    WaitForRpcServer();

    StopAllLaunchedApps();

    SetServiceStatus(SERVICE_STOPPED);

    DeleteCriticalSection(&g_csProcesses);
}
