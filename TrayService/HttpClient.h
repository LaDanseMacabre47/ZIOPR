#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <string>

struct HttpResponse
{
    DWORD        statusCode = 0;
    std::wstring body;
    bool ok() const { return statusCode >= 200 && statusCode < 300; }
};

class HttpClient
{
public:
    // host     — e.g. L"localhost"
    // port     — e.g. 8443
    // useHttps — true для HTTPS
    HttpClient(const wchar_t* host, INTERNET_PORT port, bool useHttps = true);
    ~HttpClient();

    HttpResponse Post(const wchar_t* path,
                      const std::wstring& jsonBody,
                      const wchar_t* bearerToken = nullptr);

    HttpResponse Get(const wchar_t* path,
                     const wchar_t* bearerToken = nullptr);

private:
    std::wstring  m_host;
    INTERNET_PORT m_port;
    bool          m_https;
};
