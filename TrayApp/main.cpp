#include "TrayApp.h"
#include "resource.h"

int APIENTRY wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    // If service is not running — start it and exit.
    // The service will re-launch TrayApp in the user session.
    if (!IsServiceRunning())
    {
        StartServiceAndWait();
        return 0;
    }

    // Only allow startup when launched by the service.
    //if (!IsParentService())
      //  return 0;

    // Single instance per user session.
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, g_szMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // If launched by service, start hidden (in tray only).
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

    // Start background license polling thread.
    HANDLE hPollThread = CreateThread(nullptr, 0, LicensePollThread, hWnd, 0, nullptr);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hPollThread)
    {
        // Signal thread to stop (window is destroyed, IsWindow returns FALSE)
        WaitForSingleObject(hPollThread, 3000);
        CloseHandle(hPollThread);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
