#include "ServiceMain.h"
#include <cstdio>

static BOOL InstallService()
{
    wchar_t szPath[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, szPath, MAX_PATH))
        return FALSE;

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM)
    {
        wprintf(L"OpenSCManager failed: %lu\n", GetLastError());
        return FALSE;
    }

    SC_HANDLE hSvc = CreateServiceW(
        hSCM,
        g_szServiceName,
        L"TrayService",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        szPath,
        nullptr, nullptr, nullptr, nullptr, nullptr
    );

    if (!hSvc)
    {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS)
            wprintf(L"Service already exists.\n");
        else
            wprintf(L"CreateService failed: %lu\n", err);
        CloseServiceHandle(hSCM);
        return FALSE;
    }

    wprintf(L"Service installed successfully.\n");
    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
    return TRUE;
}

static BOOL UninstallService()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM)
    {
        wprintf(L"OpenSCManager failed: %lu\n", GetLastError());
        return FALSE;
    }

    SC_HANDLE hSvc = OpenServiceW(hSCM, g_szServiceName, DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!hSvc)
    {
        wprintf(L"OpenService failed: %lu\n", GetLastError());
        CloseServiceHandle(hSCM);
        return FALSE;
    }

    SERVICE_STATUS ss{};
    if (QueryServiceStatus(hSvc, &ss) && ss.dwCurrentState != SERVICE_STOPPED)
    {
        ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
        Sleep(1000);
    }

    if (!DeleteService(hSvc))
    {
        wprintf(L"DeleteService failed: %lu\n", GetLastError());
        CloseServiceHandle(hSvc);
        CloseServiceHandle(hSCM);
        return FALSE;
    }

    wprintf(L"Service uninstalled successfully.\n");
    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
    return TRUE;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc >= 2)
    {
        if (_wcsicmp(argv[1], L"--install") == 0)
            return InstallService() ? 0 : 1;

        if (_wcsicmp(argv[1], L"--uninstall") == 0)
            return UninstallService() ? 0 : 1;
    }

    SERVICE_TABLE_ENTRYW serviceTable[] =
    {
        { const_cast<LPWSTR>(g_szServiceName), ServiceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcherW(serviceTable))
    {
        return GetLastError();
    }

    return 0;
}
