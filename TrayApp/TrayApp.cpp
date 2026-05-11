#include "TrayApp.h"
#include "RpcClient.h"
#include "resource.h"
#include <tlhelp32.h>
#include <wtsapi32.h>
#include <shlobj.h>
#include <commdlg.h>
#include <time.h>
#include <cstdio>

// ---------------------------------------------------------------
// Constants
// ---------------------------------------------------------------
const wchar_t* g_szServiceName = L"TrayService";
const wchar_t* g_szMutexName = L"Local\\TrayAppSingleInstanceMutex";
const wchar_t* g_szWindowClass = L"TrayAppWindowClass";
const wchar_t* g_szTitle = L"TrayApp";

static UINT  s_wmTaskbarCreated = 0;
static HICON s_hIcon = nullptr;

// ---------------------------------------------------------------
// Current state
// ---------------------------------------------------------------
static AppState s_state = AppState::Initializing;

// ---------------------------------------------------------------
// Child window IDs
// ---------------------------------------------------------------
#define IDC_LBL_STATUS      3001
#define IDC_LBL_USERNAME    3002
#define IDC_LBL_EXPIRY      3003
#define IDC_LBL_ERROR       3004
#define IDC_EDIT_USERNAME   3010
#define IDC_EDIT_PASSWORD   3011
#define IDC_EDIT_ACTCODE    3012
#define IDC_BTN_LOGIN       3020
#define IDC_BTN_LOGOUT      3021
#define IDC_BTN_ACTIVATE    3022
#define IDC_BTN_SCAN        3023

// ---------------------------------------------------------------
// Helpers to destroy all child windows
// ---------------------------------------------------------------
static BOOL CALLBACK DestroyChildProc(HWND hChild, LPARAM)
{
    DestroyWindow(hChild);
    return TRUE;
}

static void ClearChildWindows(HWND hParent)
{
    EnumChildWindows(hParent, DestroyChildProc, 0);
}

// ---------------------------------------------------------------
// Register window class
// ---------------------------------------------------------------
ATOM RegisterMainWindowClass(HINSTANCE hInstance)
{
    s_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAYAPP));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
    wcex.lpszClassName = g_szWindowClass;
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAYAPP));
    s_hIcon = wcex.hIcon;
    return RegisterClassExW(&wcex);
}

// ---------------------------------------------------------------
// Create main window
// ---------------------------------------------------------------
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd = CreateWindowExW(
        0, g_szWindowClass, g_szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 340,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return nullptr;
    if (nCmdShow != SW_HIDE)
    {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    }
    return hWnd;
}

// ---------------------------------------------------------------
// Tray icon
// ---------------------------------------------------------------
void AddTrayIcon(HWND hWnd, HINSTANCE hInstance)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = s_hIcon ? s_hIcon
        : LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAYAPP));
    wcscpy_s(nid.szTip, g_szTitle);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hWnd)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hWnd;
    nid.uID = 1;
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

// ---------------------------------------------------------------
// Helper: add a static label
// ---------------------------------------------------------------
static HWND AddLabel(HWND hParent, int id,
    const wchar_t* text,
    int x, int y, int w, int h,
    bool bold = false)
{
    HWND hLbl = CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, h,
        hParent, (HMENU)(UINT_PTR)id,
        (HINSTANCE)GetWindowLongPtrW(hParent, GWLP_HINSTANCE), nullptr);
    if (bold)
    {
        HFONT hFont = CreateFontW(18, 0, 0, 0, FW_BOLD,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI");
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    return hLbl;
}

// ---------------------------------------------------------------
// Panel: Login
// ---------------------------------------------------------------
void ShowLoginPanel(HWND hParent)
{
    ClearChildWindows(hParent);
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hParent, GWLP_HINSTANCE);

    AddLabel(hParent, IDC_LBL_STATUS, L"Login",
        20, 20, 400, 28, true);

    AddLabel(hParent, 0, L"Username:", 20, 70, 100, 20);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        120, 68, 300, 24,
        hParent, (HMENU)IDC_EDIT_USERNAME, hInst, nullptr);

    AddLabel(hParent, 0, L"Password:", 20, 110, 100, 20);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
        120, 108, 300, 24,
        hParent, (HMENU)IDC_EDIT_PASSWORD, hInst, nullptr);

    // Error label (hidden by default)
    CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 145, 400, 20,
        hParent, (HMENU)IDC_LBL_ERROR, hInst, nullptr);
    // Red text for error
    HWND hErr = GetDlgItem(hParent, IDC_LBL_ERROR);
    (void)hErr;

    CreateWindowExW(0, L"BUTTON", L"Login",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        120, 175, 120, 30,
        hParent, (HMENU)IDC_BTN_LOGIN, hInst, nullptr);
}

// ---------------------------------------------------------------
// Panel: License activation
// ---------------------------------------------------------------
void ShowLicensePanel(HWND hParent)
{
    ClearChildWindows(hParent);
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hParent, GWLP_HINSTANCE);

    AddLabel(hParent, IDC_LBL_STATUS, L"Product Activation",
        20, 20, 400, 28, true);
    AddLabel(hParent, 0,
        L"No license found. Enter activation code:",
        20, 60, 420, 20);

    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 90, 380, 24,
        hParent, (HMENU)IDC_EDIT_ACTCODE, hInst, nullptr);

    CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 125, 420, 20,
        hParent, (HMENU)IDC_LBL_ERROR, hInst, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Activate",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        20, 155, 150, 30,
        hParent, (HMENU)IDC_BTN_ACTIVATE, hInst, nullptr);
}

// ---------------------------------------------------------------
// Panel: Active (authenticated + licensed)
// ---------------------------------------------------------------
void ShowActivePanel(HWND hParent, const wchar_t* username, __int64 expiryUnix)
{
    ClearChildWindows(hParent);
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hParent, GWLP_HINSTANCE);

    AddLabel(hParent, IDC_LBL_STATUS, L"Antivirus Active",
        20, 20, 400, 28, true);

    // Username
    wchar_t userLine[300]{};
    swprintf_s(userLine, L"User: %s", username ? username : L"");
    AddLabel(hParent, IDC_LBL_USERNAME, userLine, 20, 70, 420, 22);

    // Expiry date
    wchar_t expiryLine[200]{};
    if (expiryUnix > 0)
    {
        time_t t = (time_t)expiryUnix;
        struct tm tm {};
        gmtime_s(&tm, &t);
        swprintf_s(expiryLine,
            L"License valid until: %04d-%02d-%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    }
    else
    {
        wcscpy_s(expiryLine, L"License active");
    }
    AddLabel(hParent, IDC_LBL_EXPIRY, expiryLine, 20, 100, 420, 22);

    // AV database info
    long dbCount = 0;
    wchar_t dbDate[64]{};
    RpcClientGetAvDatabaseInfo(&dbCount, dbDate);

    wchar_t dbLine[200]{};
    swprintf_s(dbLine, L"AV Database: %s | Records: %ld",
        dbDate[0] ? dbDate : L"unknown", dbCount);
    AddLabel(hParent, 0, dbLine, 20, 122, 420, 18);

    CreateWindowExW(0, L"BUTTON", L"Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 170, 120, 30,
        hParent, (HMENU)IDC_BTN_SCAN, hInst, nullptr);

    // Метка результата сканирования (пустая по умолчанию)
    CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        155, 177, 260, 20,
        hParent, (HMENU)IDC_LBL_STATUS + 1, hInst, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Logout",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 215, 120, 30,
        hParent, (HMENU)IDC_BTN_LOGOUT, hInst, nullptr);
}

// ---------------------------------------------------------------
// State machine
// ---------------------------------------------------------------
void TransitionToState(HWND hWnd, AppState newState,
    const wchar_t* username,
    __int64 expiryUnix,
    const wchar_t* errorMsg)
{
    s_state = newState;
    switch (newState)
    {
    case AppState::NeedLogin:
        ShowLoginPanel(hWnd);
        if (errorMsg && errorMsg[0])
        {
            HWND hErr = GetDlgItem(hWnd, IDC_LBL_ERROR);
            if (hErr) SetWindowTextW(hErr, errorMsg);
        }
        break;

    case AppState::NeedLicense:
        ShowLicensePanel(hWnd);
        if (errorMsg && errorMsg[0])
        {
            HWND hErr = GetDlgItem(hWnd, IDC_LBL_ERROR);
            if (hErr) SetWindowTextW(hErr, errorMsg);
        }
        break;

    case AppState::Active:
        ShowActivePanel(hWnd, username, expiryUnix);
        break;

    default:
        break;
    }
    InvalidateRect(hWnd, nullptr, TRUE);
}

// ---------------------------------------------------------------
// Handle Login button click
// ---------------------------------------------------------------
static void OnLoginClick(HWND hWnd)
{
    wchar_t user[256]{}, pass[256]{};
    GetDlgItemTextW(hWnd, IDC_EDIT_USERNAME, user, 256);
    GetDlgItemTextW(hWnd, IDC_EDIT_PASSWORD, pass, 256);

    if (!user[0] || !pass[0])
    {
        HWND hErr = GetDlgItem(hWnd, IDC_LBL_ERROR);
        if (hErr) SetWindowTextW(hErr, L"Enter username and password.");
        return;
    }

    // Disable button during request
    EnableWindow(GetDlgItem(hWnd, IDC_BTN_LOGIN), FALSE);

    long result = RpcClientLogin(user, pass);
    if (result != 0)
    {
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_LOGIN), TRUE);
        TransitionToState(hWnd, AppState::NeedLogin, nullptr, 0,
            L"Authentication failed. Check username and password.");
        return;
    }

    // Authenticated — check license
    long hasLicense = 0;
    __int64 expiry = 0;
    RpcClientGetLicenseStatus(&hasLicense, &expiry);

    if (!hasLicense)
        TransitionToState(hWnd, AppState::NeedLicense);
    else
        TransitionToState(hWnd, AppState::Active, user, expiry);
}

// ---------------------------------------------------------------
// Handle Activate button click
// ---------------------------------------------------------------
static void OnActivateClick(HWND hWnd)
{
    wchar_t code[256]{};
    GetDlgItemTextW(hWnd, IDC_EDIT_ACTCODE, code, 256);
    if (!code[0])
    {
        HWND hErr = GetDlgItem(hWnd, IDC_LBL_ERROR);
        if (hErr) SetWindowTextW(hErr, L"Enter activation code.");
        return;
    }

    EnableWindow(GetDlgItem(hWnd, IDC_BTN_ACTIVATE), FALSE);
    long result = RpcClientActivateProduct(code);
    if (result != 0)
    {
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_ACTIVATE), TRUE);
        TransitionToState(hWnd, AppState::NeedLicense, nullptr, 0,
            L"Activation failed. Check the code.");
        return;
    }

    // Get updated license info
    long hasLicense = 0;
    __int64 expiry = 0;
    RpcClientGetLicenseStatus(&hasLicense, &expiry);

    // Get username
    long auth = 0;
    wchar_t user[256]{};
    RpcClientGetCurrentUser(&auth, user);

    TransitionToState(hWnd, AppState::Active, user, expiry);
}

// ---------------------------------------------------------------
// Handle Logout button click
// ---------------------------------------------------------------
static void OnLogoutClick(HWND hWnd)
{
    RpcClientLogout();
    TransitionToState(hWnd, AppState::NeedLogin);
}

// ---------------------------------------------------------------
// Exit application
// ---------------------------------------------------------------
static void ExitApplication(HWND hWnd)
{
    RpcStopServiceCall();
    RemoveTrayIcon(hWnd);
    DestroyWindow(hWnd);
}

// ---------------------------------------------------------------
// WndProc
// ---------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == s_wmTaskbarCreated && s_wmTaskbarCreated != 0)
    {
        AddTrayIcon(hWnd, (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE));
        return 0;
    }

    switch (message)
    {
        // ---- Tray ----
    case WM_TRAYICON:
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONUP: ShowMainWindow(hWnd);     break;
        case WM_RBUTTONUP: ShowTrayContextMenu(hWnd); break;
        }
        return 0;

        // ---- Commands ----
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        switch (id)
        {
        case IDM_FILE_EXIT:
        case IDM_TRAY_EXIT:
            ExitApplication(hWnd);
            break;

        case IDM_TRAY_OPEN:
            ShowMainWindow(hWnd);
            break;

        case IDC_BTN_LOGIN:
            OnLoginClick(hWnd);
            break;

        case IDC_BTN_ACTIVATE:
            OnActivateClick(hWnd);
            break;

        case IDC_BTN_SCAN:
        {
            // Показываем меню выбора: файл или папка
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDC_BTN_SCAN + 10, L"Scan File...");
            AppendMenuW(hMenu, MF_STRING, IDC_BTN_SCAN + 11, L"Scan Directory...");
            RECT rc{}; GetWindowRect(GetDlgItem(hWnd, IDC_BTN_SCAN), &rc);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN,
                rc.left, rc.bottom, 0, hWnd, nullptr);
            DestroyMenu(hMenu);
            break;
        }

        case IDC_BTN_SCAN + 10:  // Scan File
        {
            wchar_t path[MAX_PATH]{};
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = L"Select file to scan";
            ofn.Flags = OFN_FILEMUSTEXIST;
            if (GetOpenFileNameW(&ofn))
            {
                long isMalicious = 0;
                wchar_t threatName[256]{};
                long r = RpcClientScanFile(path, &isMalicious, threatName);

                wchar_t msg[512]{};
                if (r != 0)
                    swprintf_s(msg, L"Scan error: %ld", r);
                else if (isMalicious)
                    swprintf_s(msg, L"THREAT DETECTED!\n%s\n\nFile: %s",
                        threatName, path);
                else
                    swprintf_s(msg, L"File is clean.\n%s", path);

                MessageBoxW(hWnd, msg,
                    isMalicious ? L"Threat Found!" : L"Scan Result",
                    isMalicious ? MB_ICONWARNING : MB_ICONINFORMATION);
            }
            break;
        }

        case IDC_BTN_SCAN + 11:  // Scan Directory
        {
            wchar_t path[MAX_PATH]{};
            BROWSEINFOW bi{};
            bi.hwndOwner = hWnd;
            bi.lpszTitle = L"Select directory to scan";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl && SHGetPathFromIDListW(pidl, path))
            {
                // Показываем что идёт сканирование
                HWND hResult = GetDlgItem(hWnd, IDC_LBL_STATUS + 1);
                if (hResult) SetWindowTextW(hResult, L"Scanning...");
                EnableWindow(GetDlgItem(hWnd, IDC_BTN_SCAN), FALSE);

                // Запускаем сканирование в отдельном потоке
                struct ScanParams {
                    HWND    hWnd;
                    wchar_t path[MAX_PATH];
                };
                auto* params = new ScanParams();
                params->hWnd = hWnd;
                wcscpy_s(params->path, path);

                CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
                    auto* sp = static_cast<ScanParams*>(p);

                    long filesScanned = 0, threatsFound = 0;
                    wchar_t* threatList = nullptr;
                    long r = RpcClientScanDirectory(sp->path,
                        &filesScanned,
                        &threatsFound,
                        &threatList);

                    wchar_t msg[1024]{};
                    if (r != 0)
                        swprintf_s(msg, L"Scan error: %ld", r);
                    else
                        swprintf_s(msg,
                            L"Scan complete.\nFiles scanned: %ld\nThreats found: %ld\n\n%s",
                            filesScanned, threatsFound,
                            threatList ? threatList : L"");

                    if (threatList) midl_user_free(threatList);

                    // Показываем результат в UI потоке
                    MessageBoxW(sp->hWnd, msg,
                        threatsFound > 0 ? L"Threats Found!" : L"Scan Result",
                        threatsFound > 0 ? MB_ICONWARNING : MB_ICONINFORMATION);

                    // Разблокируем кнопку
                    EnableWindow(GetDlgItem(sp->hWnd, IDC_BTN_SCAN), TRUE);
                    HWND hLbl = GetDlgItem(sp->hWnd, IDC_LBL_STATUS + 1);
                    if (hLbl) SetWindowTextW(hLbl, L"");

                    delete sp;
                    return 0;
                    }, params, 0, nullptr);
            }
            if (pidl) CoTaskMemFree(pidl);
            break;
        }

        case IDC_BTN_LOGOUT:
            OnLogoutClick(hWnd);
            break;

        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
        }
        break;
    }

    // ---- License state changed (from polling thread) ----
    case WM_LICENSE_CHANGED:
    {
        long hasLicense = (long)wParam;
        __int64 expiry = (__int64)lParam;

        if (!hasLicense && expiry == (__int64)-1)
        {
            // Сессия пропала — требуем повторного логина
            TransitionToState(hWnd, AppState::NeedLogin, nullptr, 0,
                L"Session expired. Please log in again.");
        }
        else if (hasLicense)
        {
            long auth = 0;
            wchar_t user[256]{};
            RpcClientGetCurrentUser(&auth, user);
            TransitionToState(hWnd, AppState::Active, user, expiry);
        }
        else
        {
            // Лицензия истекла — показываем форму активации без логаута
            TransitionToState(hWnd, AppState::NeedLicense, nullptr, 0,
                L"License expired. Enter a new activation code.");
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == 1)
        {
            KillTimer(hWnd, 1);
            HWND hResult = GetDlgItem(hWnd, IDC_LBL_STATUS + 1);
            if (hResult) SetWindowTextW(hResult, L"No threats found.");
        }
        return 0;

    case WM_CREATE:
    {
        // Initial state: query current user from service
        long auth = 0;
        wchar_t user[256]{};
        long r = RpcClientGetCurrentUser(&auth, user);

        if (r != 0 || !auth)
        {
            // Not authenticated
            TransitionToState(hWnd, AppState::NeedLogin);
        }
        else
        {
            // Authenticated — check license
            long hasLicense = 0;
            __int64 expiry = 0;
            long r = RpcClientGetLicenseStatus(&hasLicense, &expiry);

            if (r == 0 && hasLicense)
                TransitionToState(hWnd, AppState::Active, user, expiry);
            else
                TransitionToState(hWnd, AppState::NeedLicense);
        }
        return 0;
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

// ---------------------------------------------------------------
// Background license polling thread
// Polls every 60 seconds and posts WM_LICENSE_CHANGED if state changes.
// ---------------------------------------------------------------
DWORD WINAPI LicensePollThread(LPVOID param)
{
    HWND hWnd = reinterpret_cast<HWND>(param);

    bool lastHadLicense = false;
    int  sessionTick = 0;  // счётчик для проверки сессии раз в 5 минут

    while (IsWindow(hWnd))
    {
        Sleep(10000);  // лицензия каждые 10 секунд
        if (!IsWindow(hWnd)) break;

        // Проверяем сессию раз в 5 минут (30 тиков по 10 сек)
        sessionTick++;
        if (sessionTick >= 30)
        {
            sessionTick = 0;
            long auth = 0;
            wchar_t user[256]{};
            long authResult = RpcClientGetCurrentUser(&auth, user);
            if (authResult != 0 || !auth)
            {
                PostMessageW(hWnd, WM_LICENSE_CHANGED, 0, (LPARAM)-1);
                lastHadLicense = false;
                continue;
            }
        }

        // Проверяем лицензию каждые 10 секунд
        long hasLicense = 0;
        __int64 expiry = 0;
        long r = RpcClientGetLicenseStatus(&hasLicense, &expiry);

        // r=0 hasLicense=0 — нет лицензии, нужна активация
        // r=0 hasLicense=1 — лицензия есть
        // r!=0             — ошибка связи, не меняем состояние
        if (r == 0)
        {
            bool nowHas = (hasLicense != 0);
            if (nowHas != lastHadLicense)
            {
                lastHadLicense = nowHas;
                PostMessageW(hWnd, WM_LICENSE_CHANGED,
                    (WPARAM)hasLicense, (LPARAM)expiry);
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------
// Service helpers (unchanged from original)
// ---------------------------------------------------------------
BOOL IsParentService()
{
    DWORD parentPid = 0;
    wchar_t parentExe[MAX_PATH]{};
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return FALSE;

    DWORD currentPid = GetCurrentProcessId();
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe))
        do {
            if (pe.th32ProcessID == currentPid) { parentPid = pe.th32ParentProcessID; break; }
        } while (Process32NextW(hSnap, &pe));

    if (parentPid)
    {
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe))
            do {
                if (pe.th32ProcessID == parentPid) { wcscpy_s(parentExe, pe.szExeFile); break; }
            } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return (_wcsicmp(parentExe, L"TrayService.exe") == 0) ? TRUE : FALSE;
}

BOOL IsServiceRunning()
{
    BOOL running = FALSE;
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return FALSE;
    SC_HANDLE hSvc = OpenServiceW(hSCM, g_szServiceName, SERVICE_QUERY_STATUS);
    if (hSvc)
    {
        SERVICE_STATUS ss{};
        if (QueryServiceStatus(hSvc, &ss))
            running = (ss.dwCurrentState == SERVICE_RUNNING);
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
    return running;
}

BOOL StartServiceAndWait()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return FALSE;
    SC_HANDLE hSvc = OpenServiceW(hSCM, g_szServiceName,
        SERVICE_START | SERVICE_QUERY_STATUS);
    if (!hSvc) { CloseServiceHandle(hSCM); return FALSE; }

    if (!StartServiceW(hSvc, 0, nullptr))
    {
        DWORD err = GetLastError();
        CloseServiceHandle(hSvc);
        CloseServiceHandle(hSCM);
        return (err == ERROR_SERVICE_ALREADY_RUNNING) ? TRUE : FALSE;
    }

    // Wait up to 10 s
    for (int i = 0; i < 20; i++)
    {
        SERVICE_STATUS ss{};
        if (QueryServiceStatus(hSvc, &ss) &&
            ss.dwCurrentState == SERVICE_RUNNING)
            break;
        Sleep(500);
    }

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
    return TRUE;
}