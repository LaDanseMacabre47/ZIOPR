#include "TrayApp.h"
#include "TrayApp.h"
#include "RpcClient.h"
#include "resource.h"

#include <tlhelp32.h>
#include <wtsapi32.h>

const wchar_t* g_szServiceName  = L"TrayService";
const wchar_t* g_szMutexName    = L"Local\\TrayAppSingleInstanceMutex";
const wchar_t* g_szWindowClass  = L"TrayAppWindowClass";
const wchar_t* g_szTitle        = L"TrayApp";

static UINT  s_wmTaskbarCreated = 0;
static HICON s_hIcon            = nullptr;

ATOM RegisterMainWindowClass(HINSTANCE hInstance)
{
    s_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wcex{};
    wcex.cbSize        = sizeof(WNDCLASSEXW);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAYAPP));
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName  = MAKEINTRESOURCEW(IDR_MAINMENU);
    wcex.lpszClassName = g_szWindowClass;
    wcex.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAYAPP));

    s_hIcon = wcex.hIcon;

    return RegisterClassExW(&wcex);
}

HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd = CreateWindowExW(
        0,
        g_szWindowClass,
        g_szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        nullptr, nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd)
        return nullptr;

    if (nCmdShow != SW_HIDE)
    {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    }

    return hWnd;
}

void AddTrayIcon(HWND hWnd, HINSTANCE hInstance)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize           = sizeof(NOTIFYICONDATAW);
    nid.hWnd             = hWnd;
    nid.uID              = 1;
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon            = s_hIcon ? s_hIcon : LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAYAPP));
    wcscpy_s(nid.szTip, L"TrayApp");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hWnd)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd   = hWnd;
    nid.uID    = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void ShowTrayContextMenu(HWND hWnd)
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, nullptr);
    DestroyMenu(hMenu);
}

void ShowMainWindow(HWND hWnd)
{
    ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);
}

static void ExitApplication(HWND hWnd)
{
    if (IsCurrentUserAdmin())
        RpcStopServiceCall();

    RemoveTrayIcon(hWnd);
    DestroyWindow(hWnd);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == s_wmTaskbarCreated && s_wmTaskbarCreated != 0)
    {
        AddTrayIcon(hWnd, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE));
        return 0;
    }

    switch (message)
    {
    case WM_TRAYICON:
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONUP:
            ShowMainWindow(hWnd);
            break;
        case WM_RBUTTONUP:
            ShowTrayContextMenu(hWnd);
            break;
        }
        return 0;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_FILE_EXIT:
        case IDM_TRAY_EXIT:
            ExitApplication(hWnd);
            break;
        case IDM_TRAY_OPEN:
            ShowMainWindow(hWnd);
            break;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
        }
        break;
    }

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

BOOL IsCurrentUserAdmin()
{
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    PSID pAdminSid = nullptr;

    if (AllocateAndInitializeSid(&ntAuth, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &pAdminSid))
    {
        CheckTokenMembership(nullptr, pAdminSid, &isAdmin);
        FreeSid(pAdminSid);
    }

    return isAdmin;
}

BOOL IsParentService()
{
    DWORD parentPid = 0;
    wchar_t parentExe[MAX_PATH]{};

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return FALSE;

    DWORD currentPid = GetCurrentProcessId();
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe))
    {
        do
        {
            if (pe.th32ProcessID == currentPid)
            {
                parentPid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }

    if (parentPid == 0)
    {
        CloseHandle(hSnap);
        return FALSE;
    }

    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe))
    {
        do
        {
            if (pe.th32ProcessID == parentPid)
            {
                wcscpy_s(parentExe, pe.szExeFile);
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    if (_wcsicmp(parentExe, L"TrayService.exe") == 0)
        return TRUE;

    return FALSE;
}

BOOL IsServiceRunning()
{
    BOOL running = FALSE;
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM)
        return FALSE;

    SC_HANDLE hSvc = OpenServiceW(hSCM, g_szServiceName, SERVICE_QUERY_STATUS);
    if (hSvc)
    {
        SERVICE_STATUS ss{};
        if (QueryServiceStatus(hSvc, &ss))
        {
            running = (ss.dwCurrentState == SERVICE_RUNNING);
        }
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
    return running;
}

BOOL StartServiceAndWait()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM)
        return FALSE;

    SC_HANDLE hSvc = OpenServiceW(hSCM, g_szServiceName, SERVICE_START | SERVICE_QUERY_STATUS);
    if (!hSvc)
    {
        CloseServiceHandle(hSCM);
        return FALSE;
    }

    if (!StartServiceW(hSvc, 0, nullptr))
    {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING)
        {
            CloseServiceHandle(hSvc);
            CloseServiceHandle(hSCM);
            return FALSE;
        }
    }

    SERVICE_STATUS ss{};
    for (int i = 0; i < 30; i++)
    {
        if (QueryServiceStatus(hSvc, &ss) && ss.dwCurrentState == SERVICE_RUNNING)
        {
            CloseServiceHandle(hSvc);
            CloseServiceHandle(hSCM);
            return TRUE;
        }
        Sleep(1000);
    }

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
    return FALSE;
}