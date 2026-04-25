#include "SessionLauncher.h"
#include "ServiceMain.h"

#include <wtsapi32.h>
#include <userenv.h>

void LaunchAppInSession(DWORD sessionId)
{
    if (sessionId == 0)
        return;

    HANDLE hToken = nullptr;
    if (!WTSQueryUserToken(sessionId, &hToken))
        return;

    HANDLE hDupToken = nullptr;
    if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, nullptr,
                          SecurityImpersonation, TokenPrimary, &hDupToken))
    {
        CloseHandle(hToken);
        return;
    }

    LPVOID pEnv = nullptr;
    if (!CreateEnvironmentBlock(&pEnv, hDupToken, FALSE))
    {
        CloseHandle(hDupToken);
        CloseHandle(hToken);
        return;
    }

    wchar_t szPath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, szPath, MAX_PATH);

    wchar_t* lastSlash = wcsrchr(szPath, L'\\');
    if (lastSlash)
        *(lastSlash + 1) = L'\0';

    wcscat_s(szPath, L"TrayApp.exe");

    wchar_t szCmdLine[MAX_PATH + 32]{};
    swprintf_s(szCmdLine, L"\"%s\" --hidden", szPath);

    STARTUPINFOW si{};
    si.cb          = sizeof(si);
    si.lpDesktop   = const_cast<LPWSTR>(L"WinSta0\\Default");
    si.wShowWindow = SW_HIDE;
    si.dwFlags     = STARTF_USESHOWWINDOW;

    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessAsUserW(
        hDupToken,
        szPath,
        szCmdLine,
        nullptr,
        nullptr,
        FALSE,
        CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE,
        pEnv,
        nullptr,
        &si,
        &pi
    );

    DestroyEnvironmentBlock(pEnv);
    CloseHandle(hDupToken);
    CloseHandle(hToken);

    if (ok)
    {
        CloseHandle(pi.hThread);

        EnterCriticalSection(&g_csProcesses);
        g_launchedProcesses.push_back(pi.hProcess);
        LeaveCriticalSection(&g_csProcesses);
    }
}

void LaunchAppInAllSessions()
{
    WTS_SESSION_INFOW* pSessions = nullptr;
    DWORD count = 0;

    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessions, &count))
        return;

    for (DWORD i = 0; i < count; i++)
    {
        if (pSessions[i].SessionId == 0)
            continue;

        if (pSessions[i].State == WTSActive)
        {
            LaunchAppInSession(pSessions[i].SessionId);
        }
    }

    WTSFreeMemory(pSessions);
}
