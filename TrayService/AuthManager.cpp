#include "AuthManager.h"
#include "HttpClient.h"
#include "ApiConfig.h"

// ---------------------------------------------------------------
// Минимальный JSON-парсер (без зависимостей)
// ---------------------------------------------------------------
static std::wstring JsonStr(const std::wstring& json, const wchar_t* key)
{
    std::wstring k = L"\""; k += key; k += L"\"";
    auto p = json.find(k);
    if (p == std::wstring::npos) return {};
    p = json.find(L':', p);
    if (p == std::wstring::npos) return {};
    p = json.find(L'"', p);
    if (p == std::wstring::npos) return {};
    auto e = json.find(L'"', p + 1);
    if (e == std::wstring::npos) return {};
    return json.substr(p + 1, e - p - 1);
}

// ---------------------------------------------------------------
// Текущее время Unix (секунды)
// ---------------------------------------------------------------
static LONGLONG UnixNow()
{
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER ul; ul.LowPart = ft.dwLowDateTime; ul.HighPart = ft.dwHighDateTime;
    return (LONGLONG)((ul.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

// ---------------------------------------------------------------
// Поток обновления токенов
// ---------------------------------------------------------------
static DWORD WINAPI AuthRefreshThreadProc(LPVOID p)
{
    reinterpret_cast<AuthManager*>(p)->RefreshLoop();
    return 0;
}

// ---------------------------------------------------------------
AuthManager::AuthManager()
{
    InitializeCriticalSection(&m_cs);
    m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

AuthManager::~AuthManager()
{
    Logout();
    if (m_hStopEvent) CloseHandle(m_hStopEvent);
    DeleteCriticalSection(&m_cs);
}

// ---------------------------------------------------------------
LONG AuthManager::Login(const wchar_t* username, const wchar_t* password)
{
    // POST /auth/login  {"username":"...","password":"..."}
    std::wstring body = L"{\"username\":\"";
    body += username;
    body += L"\",\"password\":\"";
    body += password;
    body += L"\"}";

    HttpClient http(API_HOST, API_PORT, API_USE_HTTPS);
    auto resp = http.Post(API_LOGIN_PATH, body);

    if (resp.statusCode == 401 || resp.statusCode == 403)
        return ERROR_LOGON_FAILURE;          // 1326

    if (!resp.ok())
        return ERROR_SERVICE_NOT_ACTIVE;     // 1062 — сервер недоступен

    EnterCriticalSection(&m_cs);
    BOOL ok = ParseLoginResponse(resp.body);
    if (ok) m_username = username;
    LeaveCriticalSection(&m_cs);

    if (!ok) return ERROR_INVALID_DATA;

    // Запускаем поток обновления токенов
    if (!m_hThread)
    {
        ResetEvent(m_hStopEvent);
        m_hThread = CreateThread(nullptr, 0, AuthRefreshThreadProc, this, 0, nullptr);
    }
    return ERROR_SUCCESS;
}

// ---------------------------------------------------------------
void AuthManager::Logout()
{
    StopRefreshThread();
    EnterCriticalSection(&m_cs);
    m_accessToken.clear();
    m_refreshToken.clear();
    m_username.clear();
    m_accessExpiry = 0;
    m_refreshExpiry = 0;
    LeaveCriticalSection(&m_cs);
}

// ---------------------------------------------------------------
BOOL AuthManager::GetCurrentUser(wchar_t username[256])
{
    EnterCriticalSection(&m_cs);
    bool auth = !m_username.empty() && !m_accessToken.empty();
    if (auth) wcsncpy_s(username, 256, m_username.c_str(), _TRUNCATE);
    LeaveCriticalSection(&m_cs);
    return auth ? TRUE : FALSE;
}

// ---------------------------------------------------------------
std::wstring AuthManager::GetAccessToken()
{
    EnterCriticalSection(&m_cs);
    std::wstring t = m_accessToken;
    LeaveCriticalSection(&m_cs);
    return t;
}

// ---------------------------------------------------------------
// Парсим ответ сервера:
// {"accessToken":"...","refreshToken":"...","tokenType":"Bearer"}
// Сроки действия берём из ApiConfig (JWT_ACCESS_EXPIRATION_MS и т.д.)
// ---------------------------------------------------------------
BOOL AuthManager::ParseLoginResponse(const std::wstring& json)
{
    auto at = JsonStr(json, L"accessToken");
    auto rt = JsonStr(json, L"refreshToken");
    if (at.empty()) return FALSE;

    m_accessToken = at;
    m_refreshToken = rt;

    LONGLONG now = UnixNow();
    // Сервер не возвращает expiry — считаем из конфига (мс → с)
    m_accessExpiry = now + JWT_ACCESS_EXPIRATION_MS / 1000;
    m_refreshExpiry = now + JWT_REFRESH_EXPIRATION_MS / 1000;
    return TRUE;
}

// ---------------------------------------------------------------
void AuthManager::StopRefreshThread()
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
// Фоновый поток: обновляет access-токен за 60 с до истечения.
// Когда refresh-токен истекает — разлогиниваемся.
//
// NOTE: уточните у бэкенда точный формат /auth/refresh и обновите
//       ParseLoginResponse ниже если поля ответа отличаются от login.
// ---------------------------------------------------------------
void AuthManager::RefreshLoop()
{
    while (true)
    {
        EnterCriticalSection(&m_cs);
        LONGLONG accessExp = m_accessExpiry;
        LONGLONG refreshExp = m_refreshExpiry;
        std::wstring rt = m_refreshToken;
        LeaveCriticalSection(&m_cs);

        if (rt.empty()) break;

        // Если refresh-токен истёк — выходим
        LONGLONG now = UnixNow();
        if (refreshExp > 0 && now >= refreshExp - 60)
        {
            Logout();
            break;
        }

        // Ждём до момента когда нужно обновить access-токен (за 60 с до конца)
        LONGLONG waitSecs = (accessExp - 60) - now;
        if (waitSecs < 5) waitSecs = 5;

        DWORD waitMs = (DWORD)(std::min(waitSecs, (LONGLONG)3600) * 1000);
        if (WaitForSingleObject(m_hStopEvent, waitMs) == WAIT_OBJECT_0)
            break;

        // Проверяем ещё раз после ожидания
        now = UnixNow();
        EnterCriticalSection(&m_cs);
        accessExp = m_accessExpiry;
        rt = m_refreshToken;
        LeaveCriticalSection(&m_cs);

        if (rt.empty() || now < accessExp - 60) continue;

        // POST /auth/refresh
        // TODO: уточните тело запроса у бэкенда.
        // Предположительно: {"refreshToken":"..."}
        std::wstring body = L"{\"refreshToken\":\"";
        body += rt;
        body += L"\"}";

        HttpClient http(API_HOST, API_PORT, API_USE_HTTPS);
        auto resp = http.Post(API_REFRESH_PATH, body);

        if (resp.ok())
        {
            EnterCriticalSection(&m_cs);
            ParseLoginResponse(resp.body);  // ожидаем тот же формат что и /login
            LeaveCriticalSection(&m_cs);
        }
        else
        {
            // Refresh не удался — разлогиниваемся
            Logout();
            break;
        }
    }
}

// ---------------------------------------------------------------
AuthManager& GetAuthManager()
{
    static AuthManager instance;
    return instance;
}