#include "ScanEngine.h"
#include <cstring>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")

// ---------------------------------------------------------------
// SHA-256 через WinCrypt (локальна€ копи€)
// ---------------------------------------------------------------
static bool Sha256Local(const uint8_t* data, size_t len, uint8_t out[32])
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    bool ok = false;

    if (!CryptAcquireContextW(&hProv, nullptr, nullptr,
        PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return false;

    if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
    {
        if (CryptHashData(hHash, data, (DWORD)len, 0))
        {
            DWORD hashLen = 32;
            ok = CryptGetHashParam(hHash, HP_HASHVAL, out, &hashLen, 0) != 0;
        }
        CryptDestroyHash(hHash);
    }
    CryptReleaseContext(hProv, 0);
    return ok;
}

// ---------------------------------------------------------------
// јлгоритм сканировани€ (п.3 требований)
// O(n * log k) где n Ч размер файла, k Ч число уникальных префиксов
// ---------------------------------------------------------------
bool ScanEngine::Scan(const uint8_t* data,
    size_t         size,
    ObjectType     type,
    const AvDatabase& db,
    ScanResult& result)
{
    result.IsMalicious = false;
    result.ThreatName.clear();

    if (size < 8) return false;

    // п.3.1 Ч позици€ считывани€ = 0
    size_t pos = 0;

    while (pos + 8 <= size)
    {
        // п.3.2 Ч считать 8 байт и найти в красно-чЄрном дереве (std::map)
        uint64_t prefix = 0;
        memcpy(&prefix, data + pos, 8);

        auto it = db.find(prefix);
        if (it == db.end())
        {
            // п.3.5 Ч префикс не найден, сдвиг на 1 байт
            pos++;
            continue;
        }

        //  опируем список записей дл€ фильтрации
        std::vector<const AvRecord*> candidates;
        for (auto& rec : it->second)
            candidates.push_back(&rec);

        // п.3.3 Ч проверки от лЄгкой к т€жЄлой

        // п.3.3.1 Ч проверка типа объекта
        {
            std::vector<const AvRecord*> filtered;
            for (auto* rec : candidates)
                if (rec->Type == type)
                    filtered.push_back(rec);
            candidates = filtered;
        }

        if (candidates.empty()) { pos++; continue; }

        // п.3.3.2 Ч проверка диапазона смещени€
        {
            std::vector<const AvRecord*> filtered;
            for (auto* rec : candidates)
            {
                bool inRange = (pos >= rec->OffsetBegin) &&
                    (rec->OffsetEnd == 0 || pos <= rec->OffsetEnd);
                if (inRange) filtered.push_back(rec);
            }
            candidates = filtered;
        }

        if (candidates.empty()) { pos++; continue; }

        // п.3.3.3 Ч считать дополнительные байты (ObjectSignatureLength - 8)
        // п.3.3.4 Ч подсчитать SHA-256 от prefix + доп.байты
        // п.3.3.5 Ч сравнить с ObjectSignature
        {
            std::vector<const AvRecord*> filtered;
            for (auto* rec : candidates)
            {
                uint32_t extraLen = rec->ObjectSignatureLength > 8
                    ? rec->ObjectSignatureLength - 8 : 0;

                // ѕровер€ем что достаточно данных
                if (pos + 8 + extraLen > size)
                    continue;

                // Ѕуфер: prefix (8 байт) + extra
                std::vector<uint8_t> buf(8 + extraLen);
                memcpy(buf.data(), data + pos, 8 + extraLen);

                // SHA-256
                uint8_t hash[32]{};
                if (!Sha256Local(buf.data(), buf.size(), hash))
                    continue;

                // —равниваем с ObjectSignature
                if (memcmp(hash, rec->ObjectSignature, 32) == 0)
                    filtered.push_back(rec);
            }
            candidates = filtered;
        }

        // п.3.4 Ч если список пуст Ч сдвиг
        if (candidates.empty()) { pos++; continue; }

        // п.3.6 Ч объект вредоносен
        result.IsMalicious = true;
        result.ThreatName = candidates[0]->ThreatName;
        return true;
    }

    return false;
}