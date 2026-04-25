#pragma once
#include <winhttp.h>

// ================================================================
//  Конфигурация Web-сервиса — отредактируйте при смене хоста.
// ================================================================

constexpr const wchar_t* API_HOST     = L"localhost";
constexpr INTERNET_PORT  API_PORT     = 8443;
constexpr bool           API_USE_HTTPS = true;

// Эндпоинты
constexpr const wchar_t* API_LOGIN_PATH            = L"/auth/login";
constexpr const wchar_t* API_REFRESH_PATH          = L"/auth/refresh";
constexpr const wchar_t* API_LICENSE_CHECK_PATH    = L"/licenses/check";
constexpr const wchar_t* API_LICENSE_ACTIVATE_PATH = L"/licenses/activate";

// UUID продукта (productId на сервере)
constexpr const wchar_t* API_PRODUCT_ID = L"a0000000-0000-0000-0000-000000000001";

// Сроки действия токенов (мс) — берём из конфига сервера как fallback
constexpr LONGLONG JWT_ACCESS_EXPIRATION_MS  = 3600000LL;    // 1 час
constexpr LONGLONG JWT_REFRESH_EXPIRATION_MS = 604800000LL;  // 7 дней
