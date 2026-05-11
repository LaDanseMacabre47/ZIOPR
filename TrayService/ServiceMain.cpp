#include "ServiceMain.h"
#include "SessionLauncher.h"
#include "RpcServer.h"
#include "AuthManager.h"
#include "LicenseManager.h"
#include <wtsapi32.h>

const wchar_t* g_szServiceName = L"TrayService";

SERVICE_STATUS_HANDLE g_hServiceStatus  = nullptr;
SERVICE_STATUS        g_serviceStatus{};
std::vector<HANDLE>   g_launchedProcesses;
CRITICAL_SECTION      g_csProcesses;

// ---------------------------------------------------------------
void SetServiceStatus(DWORD dwState, DWORD dwExitCode, DWORD dwWaitHint)
{
    g_serviceStatus.dwCurrentState  = dwState;
    g_serviceStatus.dwWin32ExitCode = dwExitCode;
    g_serviceStatus.dwWaitHint      = dwWaitHint;

    if (dwState == SERVICE_START_PENDING)
        g_serviceStatus.dwControlsAccepted = 0;
    else
        g_serviceStatus.dwControlsAccepted =
            SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN |
            SERVICE_ACCEPT_SESSIONCHANGE;

    static DWORD dwCheckPoint = 1;
    if (dwState == SERVICE_RUNNING || dwState == SERVICE_STOPPED)
        g_serviceStatus.dwCheckPoint = 0;
    else
        g_serviceStatus.dwCheckPoint = dwCheckPoint++;

    ::SetServiceStatus(g_hServiceStatus, &g_serviceStatus);
}

// ---------------------------------------------------------------
void StopAllLaunchedApps()
{
    EnterCriticalSection(&g_csProcesses);
    for (HANDLE hProc : g_launchedProcesses)
    {
        if (hProc && hProc != INVALID_HANDLE_VALUE)
        {
            TerminateProcess(hProc, 0);
            CloseHandle(hProc);
        }
    }
    g_launchedProcesses.clear();
    LeaveCriticalSection(&g_csProcesses);
}

// ---------------------------------------------------------------
DWORD WINAPI ServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType,
                                   LPVOID lpEventData, LPVOID lpContext)
{
    UNREFERENCED_PARAMETER(lpContext);

    switch (dwControl)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        SetServiceStatus(SERVICE_STOP_PENDING, 0, 5000);
        // Signal the RPC server to stop (which unblocks WaitForRpcServer)
        StopRpcServer();
        return NO_ERROR;

    case SERVICE_CONTROL_SESSIONCHANGE:
    {
        if (dwEventType == WTS_SESSION_LOGON)
        {
            auto* pNotif = static_cast<WTSSESSION_NOTIFICATION*>(lpEventData);
            if (pNotif && pNotif->dwSessionId != 0)
                LaunchAppInSession(pNotif->dwSessionId);
        }
        return NO_ERROR;
    }

    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;

    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

// ---------------------------------------------------------------
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

    // Launch TrayApp in all active user sessions
    LaunchAppInAllSessions();

    // Block until stop is signalled via RpcStopService or SCM STOP
    WaitForRpcServer();

    // Cleanup
    GetAuthManager().Logout();
    GetLicenseManager().ClearTicket();
    StopAllLaunchedApps();

    SetServiceStatus(SERVICE_STOPPED);
    DeleteCriticalSection(&g_csProcesses);
}
