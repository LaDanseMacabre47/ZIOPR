#include "HttpClient.h"
#pragma comment(lib, "winhttp.lib")

// UTF-8 bytes -> wstring
static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// wstring -> UTF-8 bytes
static std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                        s.data(), n, nullptr, nullptr);
    return s;
}

HttpClient::HttpClient(const wchar_t* host, INTERNET_PORT port, bool useHttps)
    : m_host(host), m_port(port), m_https(useHttps)
{}

HttpClient::~HttpClient() = default;

static HttpResponse DoRequest(
    const std::wstring& host,
    INTERNET_PORT       port,
    bool                https,
    const wchar_t*      verb,
    const wchar_t*      path,
    const std::wstring& body,
    const wchar_t*      bearerToken)
{
    HttpResponse result;

    HINTERNET hSession = WinHttpOpen(
        L"TrayService/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return result;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return result; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, verb, path,
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Для localhost с самоподписанным сертификатом — отключаем проверку
    if (https)
    {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA
                       | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
                       | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
                       | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                         &secFlags, sizeof(secFlags));
    }

    // Заголовки
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (bearerToken && bearerToken[0])
    {
        headers += L"Authorization: Bearer ";
        headers += bearerToken;
        headers += L"\r\n";
    }

    std::string bodyUtf8 = WideToUtf8(body);

    BOOL ok = WinHttpSendRequest(
        hRequest,
        headers.c_str(), (DWORD)headers.size(),
        bodyUtf8.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)bodyUtf8.data(),
        (DWORD)bodyUtf8.size(),
        (DWORD)bodyUtf8.size(), 0);

    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    if (ok)
    {
        DWORD sc = 0, sz = sizeof(sc);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &sc, &sz,
            WINHTTP_NO_HEADER_INDEX);
        result.statusCode = sc;

        std::string raw;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0)
        {
            std::string chunk(avail, '\0');
            DWORD read = 0;
            WinHttpReadData(hRequest, chunk.data(), avail, &read);
            chunk.resize(read);
            raw += chunk;
        }
        result.body = Utf8ToWide(raw);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

HttpResponse HttpClient::Post(const wchar_t* path,
                               const std::wstring& body,
                               const wchar_t* bearerToken)
{
    return DoRequest(m_host, m_port, m_https, L"POST", path, body, bearerToken);
}

HttpResponse HttpClient::Get(const wchar_t* path,
                              const wchar_t* bearerToken)
{
    return DoRequest(m_host, m_port, m_https, L"GET", path, {}, bearerToken);
}
