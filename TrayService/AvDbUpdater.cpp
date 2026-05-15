#include "AvDbUpdater.h"
#include "HttpClient.h"
#include "ApiConfig.h"
#include <winhttp.h>
#include <wincrypt.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

// ---------------------------------------------------------------
// Вспомогательные функции
// ---------------------------------------------------------------

// Конвертация Base64 -> байты (для публичного ключа)
static std::vector<uint8_t> Base64Decode(const std::string& b64)
{
    DWORD needed = 0;
    if (!CryptStringToBinaryA(b64.c_str(), (DWORD)b64.size(),
        CRYPT_STRING_BASE64, nullptr, &needed,
        nullptr, nullptr))
        return {};
    std::vector<uint8_t> out(needed);
    CryptStringToBinaryA(b64.c_str(), (DWORD)b64.size(),
        CRYPT_STRING_BASE64, out.data(), &needed,
        nullptr, nullptr);
    out.resize(needed);
    return out;
}

// UTF-16 -> UTF-8
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

// UTF-8 -> UTF-16
static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(),
        nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(),
        w.data(), n);
    return w;
}

// ---------------------------------------------------------------
// Сохранить файл на диск
// ---------------------------------------------------------------
bool AvDbUpdater::SaveFile(const wchar_t* path,
    const std::vector<uint8_t>& data)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*)data.data(), data.size());
    return f.good();
}

// ---------------------------------------------------------------
// Разобрать multipart/mixed
// Content-Type: multipart/mixed; boundary=<boundary>
// Части в фиксированном порядке: manifest.bin, data.bin
// ---------------------------------------------------------------
bool AvDbUpdater::ParseMultipart(const std::vector<uint8_t>& body,
    const std::string& boundary,
    std::vector<uint8_t>& manifest,
    std::vector<uint8_t>& data)
{
    // Разделитель: "--<boundary>"
    std::string delim = "--" + boundary;
    std::string bodyStr(body.begin(), body.end());

    // Найти все части
    std::vector<std::pair<size_t, size_t>> parts; // start, end позиции тела части

    size_t pos = 0;
    while (true)
    {
        // Ищем разделитель
        size_t delimPos = bodyStr.find(delim, pos);
        if (delimPos == std::string::npos) break;

        size_t afterDelim = delimPos + delim.size();

        // Проверяем конец multipart: "--<boundary>--"
        if (afterDelim + 2 <= bodyStr.size() &&
            bodyStr[afterDelim] == '-' && bodyStr[afterDelim + 1] == '-')
            break;

        // Пропускаем CRLF после разделителя
        if (afterDelim < bodyStr.size() && bodyStr[afterDelim] == '\r') afterDelim++;
        if (afterDelim < bodyStr.size() && bodyStr[afterDelim] == '\n') afterDelim++;

        // Пропускаем заголовки части (до пустой строки \r\n\r\n)
        size_t headerEnd = bodyStr.find("\r\n\r\n", afterDelim);
        if (headerEnd == std::string::npos)
        {
            // Попробуем \n\n
            headerEnd = bodyStr.find("\n\n", afterDelim);
            if (headerEnd == std::string::npos) break;
            headerEnd += 2;
        }
        else
            headerEnd += 4;

        // Тело части — от headerEnd до следующего разделителя
        size_t nextDelim = bodyStr.find(delim, headerEnd);
        if (nextDelim == std::string::npos) nextDelim = bodyStr.size();

        // Убираем trailing CRLF перед разделителем
        size_t partEnd = nextDelim;
        if (partEnd >= 2 && bodyStr[partEnd - 2] == '\r' && bodyStr[partEnd - 1] == '\n')
            partEnd -= 2;
        else if (partEnd >= 1 && bodyStr[partEnd - 1] == '\n')
            partEnd -= 1;

        parts.push_back({ headerEnd, partEnd });
        pos = nextDelim;
    }

    if (parts.size() < 2) return false;

    // Часть 0 = manifest.bin
    manifest.assign(body.begin() + parts[0].first,
        body.begin() + parts[0].second);

    // Часть 1 = data.bin
    data.assign(body.begin() + parts[1].first,
        body.begin() + parts[1].second);

    return !manifest.empty() && !data.empty();
}

// ---------------------------------------------------------------
// Извлечь boundary из Content-Type заголовка
// "multipart/mixed; boundary=abc123" -> "abc123"
// ---------------------------------------------------------------
static std::string ExtractBoundary(const std::wstring& contentType)
{
    std::string ct = WideToUtf8(contentType);
    std::string key = "boundary=";
    size_t pos = ct.find(key);
    if (pos == std::string::npos) return {};
    pos += key.size();

    // Boundary может быть в кавычках
    if (pos < ct.size() && ct[pos] == '"')
    {
        pos++;
        size_t end = ct.find('"', pos);
        return ct.substr(pos, end - pos);
    }

    // Без кавычек — до ; или конца строки
    size_t end = ct.find(';', pos);
    if (end == std::string::npos) end = ct.size();
    std::string b = ct.substr(pos, end - pos);
    // Trim spaces
    while (!b.empty() && (b.back() == ' ' || b.back() == '\r' || b.back() == '\n'))
        b.pop_back();
    return b;
}

// ---------------------------------------------------------------
// Низкоуровневый HTTP запрос с получением тела и заголовков
// ---------------------------------------------------------------
struct HttpRawResponse
{
    DWORD              statusCode = 0;
    std::wstring       contentType;
    std::vector<uint8_t> body;
};

static HttpRawResponse DoRawRequest(const wchar_t* host,
    INTERNET_PORT port,
    bool https,
    const wchar_t* verb,
    const wchar_t* path,
    const wchar_t* bearerToken)
{
    HttpRawResponse result;

    HINTERNET hSession = WinHttpOpen(L"TrayService/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return result;

    HINTERNET hConnect = WinHttpConnect(hSession, host, port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return result; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, verb, path,
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Игнорируем ошибки SSL для localhost
    if (https)
    {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
            SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
            SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
            SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
            &secFlags, sizeof(secFlags));
    }

    std::wstring headers;
    if (bearerToken && bearerToken[0])
    {
        headers = L"Authorization: Bearer ";
        headers += bearerToken;
        headers += L"\r\n";
    }

    BOOL ok = WinHttpSendRequest(hRequest,
        headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        headers.empty() ? 0 : (DWORD)headers.size(),
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    if (ok)
    {
        // Status code
        DWORD sc = 0, sz = sizeof(sc);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &sc, &sz,
            WINHTTP_NO_HEADER_INDEX);
        result.statusCode = sc;

        // Content-Type
        wchar_t ctBuf[512]{};
        DWORD ctLen = sizeof(ctBuf);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_CONTENT_TYPE,
            WINHTTP_HEADER_NAME_BY_INDEX,
            ctBuf, &ctLen, WINHTTP_NO_HEADER_INDEX);
        result.contentType = ctBuf;

        // Body (бинарное)
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0)
        {
            size_t old = result.body.size();
            result.body.resize(old + avail);
            DWORD read = 0;
            WinHttpReadData(hRequest, result.body.data() + old, avail, &read);
            result.body.resize(old + read);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

// ---------------------------------------------------------------
// Скачать публичный ключ
// GET /licenses/public-key -> Base64 DER строка
// ---------------------------------------------------------------
bool AvDbUpdater::DownloadPublicKey(const wchar_t* exeDir)
{
    auto resp = DoRawRequest(API_HOST, API_PORT, API_USE_HTTPS,
        L"GET", L"/licenses/public-key", nullptr);

    if (resp.statusCode != 200 || resp.body.empty())
        return false;

    // Тело — Base64 строка (может быть с пробелами/переносами)
    std::string b64(resp.body.begin(), resp.body.end());

    // Убираем возможные кавычки и пробелы
    b64.erase(std::remove(b64.begin(), b64.end(), '"'), b64.end());
    b64.erase(std::remove(b64.begin(), b64.end(), '\r'), b64.end());
    b64.erase(std::remove(b64.begin(), b64.end(), '\n'), b64.end());
    b64.erase(std::remove(b64.begin(), b64.end(), ' '), b64.end());

    // Декодируем Base64 -> DER байты
    auto derBytes = Base64Decode(b64);
    if (derBytes.empty()) return false;

    // Сохраняем как avdb_public.key
    std::wstring path = std::wstring(exeDir);
    if (!path.empty() && path.back() != L'\\') path += L'\\';
    path += L"avdb_public.key";

    return SaveFile(path.c_str(), derBytes);
}

// ---------------------------------------------------------------
// Скачать полную базу сигнатур
// GET /api/binary/signatures/full -> multipart/mixed
// ---------------------------------------------------------------
bool AvDbUpdater::DownloadFullDatabase(const wchar_t* accessToken,
    const wchar_t* exeDir)
{
    auto resp = DoRawRequest(API_HOST, API_PORT, API_USE_HTTPS,
        L"GET", L"/api/binary/signatures/full",
        accessToken);

    if (resp.statusCode != 200 || resp.body.empty())
        return false;

    // Извлекаем boundary из Content-Type
    std::string boundary = ExtractBoundary(resp.contentType);
    if (boundary.empty()) return false;

    // Парсим multipart
    std::vector<uint8_t> manifest, data;
    if (!ParseMultipart(resp.body, boundary, manifest, data))
        return false;

    std::wstring dir = exeDir;
    if (!dir.empty() && dir.back() != L'\\') dir += L'\\';

    // Делаем резервные копии существующих файлов
    std::wstring mPath = dir + L"manifest.bin";
    std::wstring dPath = dir + L"data.bin";
    std::wstring mBak = dir + L"manifest.bin.bak";
    std::wstring dBak = dir + L"data.bin.bak";

    CopyFileW(mPath.c_str(), mBak.c_str(), FALSE);
    CopyFileW(dPath.c_str(), dBak.c_str(), FALSE);

    // Сохраняем новые файлы
    if (!SaveFile(mPath.c_str(), manifest) ||
        !SaveFile(dPath.c_str(), data))
    {
        // Откатываем из резервной копии
        CopyFileW(mBak.c_str(), mPath.c_str(), FALSE);
        CopyFileW(dBak.c_str(), dPath.c_str(), FALSE);
        return false;
    }

    return true;
}