#include "LicenseManager.h"
#include "HttpClient.h"
#include "AuthManager.h"
#include "DeviceId.h"
#include "ApiConfig.h"
#include <vector>

// ---------------------------------------------------------------
// Минимальный JSON-парсер
// ---------------------------------------------------------------
static std::wstring LicJsonStr(const std::wstring& json, const wchar_t* key)
{
    std::wstring k = L"\""; k += key; k += L"\"";
    auto p = json.find(k);
    if (p == std::wstring::npos) return {};
    p = json.find(L':', p);
    if (p == std::wstring::npos) return {};
    size_t vp = p + 1;
    while (vp < json.size() && json[vp] == L' ') vp++;
    if (json[vp] == L'"')
    {
        auto e = json.find(L'"', vp + 1);
        if (e == std::wstring::npos) return {};
        return json.substr(vp + 1, e - vp - 1);
    }
    auto e = json.find_first_of(L",}\n\r", vp);
    return json.substr(vp, e == std::wstring::npos ? std::wstring::npos : e - vp);
}

static LONGLONG ParseIso8601(const std::wstring& s)
{
    if (s.size() < 19) return 0;
    SYSTEMTIME st{};
    swscanf_s(s.c_str(), L"%4hd-%2hd-%2hdT%2hd:%2hd:%2hd",
        &st.wYear, &st.wMonth, &st.wDay,
        &st.wHour, &st.wMinute, &st.wSecond);
    FILETIME ft{};
    SystemTimeToFileTime(&st, &ft);
    ULARGE_INTEGER ul; ul.LowPart = ft.dwLowDateTime; ul.HighPart = ft.dwHighDateTime;
    return (LONGLONG)((ul.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

static LONGLONG LicUnixNow()
{
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER ul; ul.LowPart = ft.dwLowDateTime; ul.HighPart = ft.dwHighDateTime;
    return (LONGLONG)((ul.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

static DWORD WINAPI LicRefreshThreadProc(LPVOID p)
{
    reinterpret_cast<LicenseManager*>(p)->RefreshLoop();
    return 0;
}

// ---------------------------------------------------------------
LicenseManager::LicenseManager()
{
    InitializeCriticalSection(&m_cs);
    m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

LicenseManager::~LicenseManager()
{
    ClearTicket();
    if (m_hStopEvent) CloseHandle(m_hStopEvent);
    DeleteCriticalSection(&m_cs);
}

// ---------------------------------------------------------------
BOOL LicenseManager::HasTicket()
{
    EnterCriticalSection(&m_cs);
    bool has = m_hasLicense && !m_blocked;
    LeaveCriticalSection(&m_cs);
    return has ? TRUE : FALSE;
}

// ---------------------------------------------------------------
// П.11: остановить фоновые задачи если тикет пропал
// ---------------------------------------------------------------
void LicenseManager::StopBackgroundTasks()
{
    StopRefreshThread();
}

// ---------------------------------------------------------------
LONG LicenseManager::CheckLicense(const wchar_t* accessToken)
{
    std::wstring mac = GetDeviceIdentifier();
    std::wstring body = L"{\"deviceIdentifier\":\"";
    body += mac;
    body += L"\",\"productId\":\"";
    body += API_PRODUCT_ID;
    body += L"\"}";

    HttpClient http(API_HOST, API_PORT, API_USE_HTTPS);
    auto resp = http.Post(API_LICENSE_CHECK_PATH, body, accessToken);

    if (resp.statusCode == 404)
    {
        // Лицензия не найдена — останавливаем фоновые задачи (п.11)
        StopBackgroundTasks();
        EnterCriticalSection(&m_cs);
        m_hasLicense = false;
        m_ticketJson.clear();
        LeaveCriticalSection(&m_cs);
        return ERROR_NOT_FOUND;
    }
    if (resp.statusCode == 401) return ERROR_NOT_AUTHENTICATED;
    if (!resp.ok())             return ERROR_INVALID_DATA;

    EnterCriticalSection(&m_cs);
    BOOL ok = ParseTicket(resp.body);
    LeaveCriticalSection(&m_cs);

    if (!ok) return ERROR_INVALID_DATA;

    // Запускаем фоновое обновление если ещё не запущено
    if (!m_hThread)
    {
        ResetEvent(m_hStopEvent);
        m_hThread = CreateThread(nullptr, 0, LicRefreshThreadProc, this, 0, nullptr);
    }
    return ERROR_SUCCESS;
}

// ---------------------------------------------------------------
LONG LicenseManager::ActivateProduct(const wchar_t* licenseKey,
    const wchar_t* accessToken)
{
    std::wstring mac = GetDeviceIdentifier();
    std::wstring body = L"{\"licenseKey\":\"";
    body += licenseKey;
    body += L"\",\"deviceIdentifier\":\"";
    body += mac;
    body += L"\"}";

    HttpClient http(API_HOST, API_PORT, API_USE_HTTPS);
    auto resp = http.Post(API_LICENSE_ACTIVATE_PATH, body, accessToken);

    if (resp.statusCode == 403) return ERROR_ACCESS_DENIED;
    if (resp.statusCode == 404) return ERROR_NOT_FOUND;
    if (resp.statusCode == 409) return ERROR_TOO_MANY_SESS;
    if (!resp.ok())             return ERROR_INVALID_PARAMETER;

    EnterCriticalSection(&m_cs);
    BOOL ok = ParseTicket(resp.body);
    LeaveCriticalSection(&m_cs);

    if (!ok) return ERROR_INVALID_DATA;

    if (!m_hThread)
    {
        ResetEvent(m_hStopEvent);
        m_hThread = CreateThread(nullptr, 0, LicRefreshThreadProc, this, 0, nullptr);
    }
    return ERROR_SUCCESS;
}

// ---------------------------------------------------------------
BOOL LicenseManager::GetLicenseInfo(LONGLONG* expiryUnixTime)
{
    EnterCriticalSection(&m_cs);
    bool has = m_hasLicense && !m_blocked;
    if (expiryUnixTime) *expiryUnixTime = m_expiryUnixTime;
    LeaveCriticalSection(&m_cs);
    return has ? TRUE : FALSE;
}

// ---------------------------------------------------------------
void LicenseManager::ClearTicket()
{
    StopRefreshThread();
    EnterCriticalSection(&m_cs);
    m_ticketJson.clear();
    m_expiryUnixTime = 0;
    m_nextRefresh = 0;
    m_hasLicense = false;
    m_blocked = false;
    LeaveCriticalSection(&m_cs);
}

// ---------------------------------------------------------------
BOOL LicenseManager::ParseTicket(const std::wstring& json)
{
    auto licEnd = LicJsonStr(json, L"licenseEnd");
    auto serverTimeStr = LicJsonStr(json, L"serverTime");
    auto lifetimeStr = LicJsonStr(json, L"lifetime");
    auto blockedStr = LicJsonStr(json, L"blocked");

    m_expiryUnixTime = licEnd.empty() ? 0 : ParseIso8601(licEnd);

    LONGLONG serverTime = serverTimeStr.empty() ? LicUnixNow()
        : ParseIso8601(serverTimeStr);
    LONGLONG lifetime = lifetimeStr.empty() ? 300 : _wtoi64(lifetimeStr.c_str());
    m_nextRefresh = serverTime + lifetime;

    m_blocked = (blockedStr == L"true");
    m_hasLicense = !licEnd.empty() && !m_blocked;
    m_ticketJson = json;
    return TRUE;
}

// ---------------------------------------------------------------
void LicenseManager::StopRefreshThread()
{
    if (m_hStopEvent) SetEvent(m_hStopEvent);
    if (m_hThread)
    {
        WaitForSingleObject(m_hThread, 5000);
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
    if (m_hStopEvent) ResetEvent(m_hStopEvent);
}

// ---------------------------------------------------------------
// Фоновый поток: обновляет тикет каждые lifetime секунд.
// При потере лицензии — останавливает себя (п.11).
// ---------------------------------------------------------------
void LicenseManager::RefreshLoop()
{
    while (true)
    {
        EnterCriticalSection(&m_cs);
        LONGLONG nextRefresh = m_nextRefresh;
        LeaveCriticalSection(&m_cs);

        LONGLONG now = LicUnixNow();
        LONGLONG waitSecs = nextRefresh - now;
        if (waitSecs < 5) waitSecs = 5;

        DWORD waitMs = (DWORD)(min(waitSecs, (LONGLONG)30) * 1000); // макс 30 сек
        if (WaitForSingleObject(m_hStopEvent, waitMs) == WAIT_OBJECT_0)
            break;

        now = LicUnixNow();
        EnterCriticalSection(&m_cs);
        nextRefresh = m_nextRefresh;
        LeaveCriticalSection(&m_cs);
        if (now < nextRefresh) continue;

        std::wstring tok = GetAuthManager().GetAccessToken();
        if (tok.empty()) continue;

        std::wstring mac = GetDeviceIdentifier();
        std::wstring body = L"{\"deviceIdentifier\":\"";
        body += mac;
        body += L"\",\"productId\":\"";
        body += API_PRODUCT_ID;
        body += L"\"}";

        HttpClient http(API_HOST, API_PORT, API_USE_HTTPS);
        auto resp = http.Post(API_LICENSE_CHECK_PATH, body, tok.c_str());

        if (resp.ok())
        {
            EnterCriticalSection(&m_cs);
            ParseTicket(resp.body);
            LeaveCriticalSection(&m_cs);
        }
        else
        {
            // Лицензия пропала или отозвана — п.11: останавливаем фоновые задачи
            EnterCriticalSection(&m_cs);
            m_hasLicense = false;
            m_ticketJson.clear();
            LeaveCriticalSection(&m_cs);

            // Выходим из потока — фоновые задачи остановлены
            break;
        }
    }
}

// ---------------------------------------------------------------
LicenseManager& GetLicenseManager()
{
    static LicenseManager instance;
    return instance;
}