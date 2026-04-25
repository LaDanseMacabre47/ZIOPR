#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

class LicenseManager
{
public:
    LicenseManager();
    ~LicenseManager();

    LONG CheckLicense(const wchar_t* accessToken);
    LONG ActivateProduct(const wchar_t* licenseKey, const wchar_t* accessToken);

    // Возвращает TRUE если лицензия есть и не заблокирована
    BOOL GetLicenseInfo(LONGLONG* expiryUnixTime);

    // Удалить тикет (при logout)
    void ClearTicket();

    // Остановить фоновые задачи (при отсутствии тикета — п.11)
    void StopBackgroundTasks();

    // Проверить есть ли тикет
    BOOL HasTicket();

    void RefreshLoop();

private:
    void StopRefreshThread();
    BOOL ParseTicket(const std::wstring& json);

    std::wstring m_ticketJson;
    LONGLONG     m_expiryUnixTime = 0;
    LONGLONG     m_nextRefresh = 0;
    bool         m_hasLicense = false;
    bool         m_blocked = false;

    CRITICAL_SECTION m_cs;
    HANDLE           m_hStopEvent = nullptr;
    HANDLE           m_hThread = nullptr;
};

LicenseManager& GetLicenseManager();