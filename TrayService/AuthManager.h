#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// Хранит JWT-токены только в оперативной памяти.
// Потокобезопасен.
class AuthManager
{
public:
    AuthManager();
    ~AuthManager();

    // Аутентификация. Возвращает 0 при успехе, иначе код ошибки.
    LONG Login(const wchar_t* username, const wchar_t* password);

    // Выход: удаляет токены из памяти.
    void Logout();

    // Заполняет username[256] если есть активная сессия.
    BOOL GetCurrentUser(wchar_t username[256]);

    // Возвращает копию access-токена (для HTTP-запросов).
    std::wstring GetAccessToken();

    // Вызывается фоновым потоком.
    void RefreshLoop();

private:
    void StopRefreshThread();

    // Парсит ответ {"accessToken":"...","refreshToken":"...","tokenType":"Bearer"}
    BOOL ParseLoginResponse(const std::wstring& json);

    // Токены — только в памяти, никогда на диск
    std::wstring m_accessToken;
    std::wstring m_refreshToken;
    std::wstring m_username;
    LONGLONG     m_accessExpiry  = 0;   // Unix timestamp (секунды)
    LONGLONG     m_refreshExpiry = 0;

    CRITICAL_SECTION m_cs;
    HANDLE           m_hStopEvent = nullptr;
    HANDLE           m_hThread    = nullptr;
};

AuthManager& GetAuthManager();
