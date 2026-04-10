#include "TrayApp.h"
#include "TrayApp.h"
#include "resource.h"

int APIENTRY wWinMain(
    _In_ HINSTANCE     hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR        lpCmdLine,
    _In_ int           nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    // Requirement: If service is NOT running, start it and exit.
    if (!IsServiceRunning())
    {
        StartServiceAndWait();
        return 0;
    }

    // Requirement: If parent process is not the service, exit.
    if (!IsParentService())
        return 0;

    // Requirement: Single instance per user (named mutex).
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, g_szMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // Support hidden-start mode: if launched by the service, start hidden.
    // The service passes SW_HIDE via the command-line parameter "--hidden"
    // or the nCmdShow may already be SW_HIDE.
    BOOL startHidden = FALSE;
    if (lpCmdLine && wcsstr(lpCmdLine, L"--hidden"))
        startHidden = TRUE;

    RegisterMainWindowClass(hInstance);

    int showCmd = startHidden ? SW_HIDE : nCmdShow;
    HWND hWnd = CreateMainWindow(hInstance, showCmd);
    if (!hWnd)
    {
        CloseHandle(hMutex);
        return 0;
    }

    AddTrayIcon(hWnd, hInstance);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
