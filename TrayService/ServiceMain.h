#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>

extern const wchar_t* g_szServiceName;

void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);
DWORD WINAPI ServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType,
                                   LPVOID lpEventData, LPVOID lpContext);

void SetServiceStatus(DWORD dwState,
                      DWORD dwExitCode = 0,
                      DWORD dwWaitHint = 0);
void StopAllLaunchedApps();

extern SERVICE_STATUS_HANDLE g_hServiceStatus;
extern std::vector<HANDLE>   g_launchedProcesses;
extern CRITICAL_SECTION      g_csProcesses;
