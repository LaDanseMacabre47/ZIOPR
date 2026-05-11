#include "RpcServer.h"
#include "AuthManager.h"
#include "LicenseManager.h"
#include "AvDatabase.h"
#include "ScanManager.h"
#include <stdlib.h>
#include <rpc.h>

#include "TrayService.h"
#include "StopService.h"

static HANDLE s_hStopEvent = nullptr;

static LONG CheckLicenseTicket()
{
    if (!GetLicenseManager().HasTicket())
    {
        GetLicenseManager().StopBackgroundTasks();
        return ERROR_LICENSE_QUOTA_EXCEEDED;
    }
    return ERROR_SUCCESS;
}

// Вспомогательная функция загрузки баз
static void LoadAvDatabase()
{
    if (GetAvDatabaseManager().IsLoaded()) return;

    // Определяем путь к exe
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    // Убираем имя файла
    wchar_t* slash = wcsrchr(exePath, L'\\');
    if (slash) *(slash + 1) = L'\0';

    std::wstring avdbPath = std::wstring(exePath) + L"bases.avdb";
    std::wstring pubkPath = std::wstring(exePath) + L"avdb_public.key";

    // Пробуем загрузить из файла
    if (!GetAvDatabaseManager().Load(avdbPath.c_str(), pubkPath.c_str()))
    {
        // Файл не найден или подпись не прошла — используем хардкод
        GetAvDatabaseManager().LoadHardcoded();
    }
}
long RpcLogin(handle_t, const wchar_t* username, const wchar_t* password)
{
    return GetAuthManager().Login(username, password);
}

void RpcLogout(handle_t)
{
    GetAuthManager().Logout();
    GetLicenseManager().ClearTicket();
}

long RpcGetCurrentUser(handle_t, long* authenticated, wchar_t** username)
{
    *authenticated = 0;
    *username = nullptr;

    wchar_t buf[256]{};
    BOOL ok = GetAuthManager().GetCurrentUser(buf);
    *authenticated = ok ? 1 : 0;

    if (ok)
    {
        size_t len = (wcslen(buf) + 1) * sizeof(wchar_t);
        *username = (wchar_t*)midl_user_allocate(len);
        if (*username) wcscpy_s(*username, wcslen(buf) + 1, buf);
    }
    return 0;
}

// ---------------------------------------------------------------
// License
// ---------------------------------------------------------------
long RpcGetLicenseStatus(handle_t, long* hasLicense, __int64* expiryUnixTime)
{
    *hasLicense = 0;
    *expiryUnixTime = 0;

    wchar_t user[256]{};
    if (!GetAuthManager().GetCurrentUser(user))
        return ERROR_NOT_AUTHENTICATED;

    std::wstring tok = GetAuthManager().GetAccessToken();
    LONG r = GetLicenseManager().CheckLicense(tok.c_str());

    if (r == ERROR_NOT_FOUND)
    {
        GetLicenseManager().StopBackgroundTasks();
        *hasLicense = 0;
        return ERROR_SUCCESS;
    }
    if (r != ERROR_SUCCESS) return r;

    LONG check = CheckLicenseTicket();
    if (check != ERROR_SUCCESS) { *hasLicense = 0; return ERROR_SUCCESS; }

    LONGLONG exp = 0;
    GetLicenseManager().GetLicenseInfo(&exp);
    *hasLicense = 1;
    *expiryUnixTime = (__int64)exp;
    return 0;
}

long RpcActivateProduct(handle_t, const wchar_t* activationCode)
{
    wchar_t user[256]{};
    if (!GetAuthManager().GetCurrentUser(user))
        return ERROR_NOT_AUTHENTICATED;

    std::wstring tok = GetAuthManager().GetAccessToken();
    LONG r = GetLicenseManager().ActivateProduct(activationCode, tok.c_str());

    if (r == ERROR_SUCCESS)
    {
        // Загружаем антивирусные базы после активации (п.1 требований)
        LoadAvDatabase();

        LONG check = CheckLicenseTicket();
        if (check != ERROR_SUCCESS) return check;
    }
    return r;
}

// ---------------------------------------------------------------
// AV Database info
// ---------------------------------------------------------------
long RpcGetAvDatabaseInfo(handle_t, long* recordCount, wchar_t** releaseDate)
{
    *recordCount = 0;
    *releaseDate = nullptr;

    wchar_t user[256]{};
    if (!GetAuthManager().GetCurrentUser(user))
        return ERROR_NOT_AUTHENTICATED;

    if (!GetAvDatabaseManager().IsLoaded())
        LoadAvDatabase();

    auto info = GetAvDatabaseManager().GetInfo();
    *recordCount = (long)info.RecordCount;

    size_t len = (info.ReleaseDate.size() + 1) * sizeof(wchar_t);
    *releaseDate = (wchar_t*)midl_user_allocate(len);
    if (*releaseDate)
        wcscpy_s(*releaseDate, info.ReleaseDate.size() + 1,
            info.ReleaseDate.c_str());
    return 0;
}

// ---------------------------------------------------------------
// Scan file
// ---------------------------------------------------------------
long RpcScanFile(handle_t,
    const wchar_t* filePath,
    long* isMalicious,
    wchar_t** threatName)
{
    *isMalicious = 0;
    *threatName = nullptr;

    wchar_t user[256]{};
    if (!GetAuthManager().GetCurrentUser(user))
        return ERROR_NOT_AUTHENTICATED;

    LONG check = CheckLicenseTicket();
    if (check != ERROR_SUCCESS) return check;

    if (!GetAvDatabaseManager().IsLoaded())
        LoadAvDatabase();

    auto result = ScanManager::ScanFile(filePath);
    *isMalicious = result.IsMalicious ? 1 : 0;

    std::wstring name = result.IsMalicious ? result.ThreatName : L"Clean";
    size_t len = (name.size() + 1) * sizeof(wchar_t);
    *threatName = (wchar_t*)midl_user_allocate(len);
    if (*threatName)
        wcscpy_s(*threatName, name.size() + 1, name.c_str());

    return 0;
}

// ---------------------------------------------------------------
// Scan directory
// ---------------------------------------------------------------
long RpcScanDirectory(handle_t,
    const wchar_t* dirPath,
    long* filesScanned,
    long* threatsFound,
    wchar_t** threatList)
{
    *filesScanned = 0;
    *threatsFound = 0;
    *threatList = nullptr;

    wchar_t user[256]{};
    if (!GetAuthManager().GetCurrentUser(user))
        return ERROR_NOT_AUTHENTICATED;

    LONG check = CheckLicenseTicket();
    if (check != ERROR_SUCCESS) return check;

    if (!GetAvDatabaseManager().IsLoaded())
        LoadAvDatabase();

    auto result = ScanManager::ScanDirectory(dirPath);
    *filesScanned = (long)result.FilesScanned;
    *threatsFound = (long)result.ThreatsFound;

    // Формируем список угроз через \n
    std::wstring list;
    for (auto& t : result.Threats)
    {
        list += t.FilePath;
        list += L" -> ";
        list += t.ThreatName;
        list += L"\n";
    }
    if (list.empty()) list = L"No threats found";

    size_t len = (list.size() + 1) * sizeof(wchar_t);
    *threatList = (wchar_t*)midl_user_allocate(len);
    if (*threatList)
        wcscpy_s(*threatList, list.size() + 1, list.c_str());

    return 0;
}

// ---------------------------------------------------------------
void RpcStopService(handle_t)
{
    if (s_hStopEvent) SetEvent(s_hStopEvent);
}

// ---------------------------------------------------------------
BOOL StartRpcServer()
{
    s_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!s_hStopEvent) return FALSE;

    // Загружаем базы при старте
    LoadAvDatabase();

    RPC_STATUS status;
    status = RpcServerUseProtseqEpW(
        (RPC_WSTR)L"ncalrpc",
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        (RPC_WSTR)L"TrayServiceALPC2",
        nullptr);
    if (status != RPC_S_OK) return FALSE;

    status = RpcServerRegisterIf(TrayService_v1_0_s_ifspec, nullptr, nullptr);
    if (status != RPC_S_OK) return FALSE;

    RpcServerUseProtseqEpW(
        (RPC_WSTR)L"ncalrpc",
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        (RPC_WSTR)L"TrayServiceALPC",
        nullptr);
    RpcServerRegisterIf(StopService_v1_0_s_ifspec, nullptr, nullptr);

    status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);
    return (status == RPC_S_OK) ? TRUE : FALSE;
}

void StopRpcServer()
{
    RpcMgmtStopServerListening(nullptr);
    RpcServerUnregisterIf(TrayService_v1_0_s_ifspec, nullptr, FALSE);
    RpcServerUnregisterIf(StopService_v1_0_s_ifspec, nullptr, FALSE);
    if (s_hStopEvent) { CloseHandle(s_hStopEvent); s_hStopEvent = nullptr; }
}

void WaitForRpcServer()
{
    if (s_hStopEvent) WaitForSingleObject(s_hStopEvent, INFINITE);
    StopRpcServer();
}

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len) { return malloc(len); }
void __RPC_USER midl_user_free(void __RPC_FAR* ptr) { free(ptr); }