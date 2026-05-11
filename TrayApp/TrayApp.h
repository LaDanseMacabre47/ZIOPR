#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <string>

// ---------------------------------------------------------------
// Window messages
// ---------------------------------------------------------------
#define WM_TRAYICON        (WM_USER + 1)
#define WM_LICENSE_CHANGED (WM_USER + 2)   // posted when license state changes

// ---------------------------------------------------------------
// UI state
// ---------------------------------------------------------------
enum class AppState
{
    Initializing,
    NeedLogin,
    NeedLicense,
    Active
};

// ---------------------------------------------------------------
// Globals declared in TrayApp.cpp
// ---------------------------------------------------------------
extern const wchar_t* g_szServiceName;
extern const wchar_t* g_szMutexName;
extern const wchar_t* g_szWindowClass;
extern const wchar_t* g_szTitle;

// ---------------------------------------------------------------
// Window / tray functions
// ---------------------------------------------------------------
ATOM RegisterMainWindowClass(HINSTANCE hInstance);
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void AddTrayIcon(HWND hWnd, HINSTANCE hInstance);
void RemoveTrayIcon(HWND hWnd);
void ShowTrayContextMenu(HWND hWnd);
void ShowMainWindow(HWND hWnd);

// ---------------------------------------------------------------
// Service helpers
// ---------------------------------------------------------------
BOOL IsParentService();
BOOL IsServiceRunning();
BOOL StartServiceAndWait();

// ---------------------------------------------------------------
// UI panels (created as child windows)
// ---------------------------------------------------------------
void ShowLoginPanel(HWND hParent);
void ShowLicensePanel(HWND hParent);
void ShowActivePanel(HWND hParent, const wchar_t* username, __int64 expiryUnix);

// ---------------------------------------------------------------
// App state machine
// ---------------------------------------------------------------
void TransitionToState(HWND hWnd, AppState newState,
                       const wchar_t* username = nullptr,
                       __int64 expiryUnix = 0,
                       const wchar_t* errorMsg = nullptr);

// Background polling thread
DWORD WINAPI LicensePollThread(LPVOID param);
