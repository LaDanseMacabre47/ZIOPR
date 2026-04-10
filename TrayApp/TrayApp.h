#pragma once
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#define WM_TRAYICON (WM_USER + 1)

extern const wchar_t* g_szServiceName;
extern const wchar_t* g_szMutexName;
extern const wchar_t* g_szWindowClass;
extern const wchar_t* g_szTitle;

ATOM RegisterMainWindowClass(HINSTANCE hInstance);
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void AddTrayIcon(HWND hWnd, HINSTANCE hInstance);
void RemoveTrayIcon(HWND hWnd);
void ShowTrayContextMenu(HWND hWnd);
void ShowMainWindow(HWND hWnd);

BOOL IsCurrentUserAdmin();
BOOL IsParentService();
BOOL IsServiceRunning();
BOOL StartServiceAndWait();
