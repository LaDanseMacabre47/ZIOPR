#include "ScanEngine.h"
#include <bcrypt.h>
#include <cstring>
#pragma comment(lib, "bcrypt.lib")

// SHA-256
static bool Sha256(const uint8_t* data, size_t len, uint8_t out[32])
{
    BCRYPT_ALG_HANDLE  hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return false;
    DWORD objSz = 0, cb = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objSz, sizeof(DWORD), &cb, 0);
    std::vector<uint8_t> obj(objSz);
    if (BCryptCreateHash(hAlg, &hHash, obj.data(), objSz, nullptr, 0, 0) == 0) {
        BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
        ok = BCryptFinishHash(hHash, out, 32, 0) == 0;
        BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

// ---------------------------------------------------------------
// јлгоритм сканировани€ (п.3 требований)
// јдаптирован под новый формат AvRecord:
//   - PrefixKey     = первые 8 байт firstBytes как uint64
//   - FirstBytes    = сырые байты первой части сигнатуры
//   - RemainderHash = SHA-256 хвоста
//   - RemainderLength = длина хвоста
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

    while (pos + 2 <= size)  // минимум 2 байта
    {
        // п.3.2 Ч считать 8 байт (с нулевым дополнением) и найти в дереве
        uint8_t prefixBuf[8]{};
        size_t avail = std::min((size_t)8, size - pos);
        memcpy(prefixBuf, data + pos, avail);
        uint64_t prefix = 0;
        memcpy(&prefix, prefixBuf, 8);

        auto it = db.find(prefix);
        if (it == db.end()) { pos++; continue; }

        std::vector<const AvRecord*> candidates;
        for (auto& rec : it->second)
            candidates.push_back(&rec);

        // п.3.3.1 Ч проверка типа объекта
        {
            std::vector<const AvRecord*> f;
            for (auto* r : candidates)
                if (r->Type == type) f.push_back(r);
            candidates = f;
        }
        if (candidates.empty()) { pos++; continue; }

        // п.3.3.2 Ч проверка диапазона смещени€
        {
            std::vector<const AvRecord*> f;
            for (auto* r : candidates)
            {
                bool inRange = ((int64_t)pos >= r->OffsetStart) &&
                    (r->OffsetEnd == 0 || r->OffsetEnd == INT64_MAX ||
                        (int64_t)pos <= r->OffsetEnd);
                if (inRange) f.push_back(r);
            }
            candidates = f;
        }
        if (candidates.empty()) { pos++; continue; }

        // п.3.3.3-5 Ч считать firstBytes полностью, затем хвост, проверить хеш
        {
            std::vector<const AvRecord*> f;
            for (auto* r : candidates)
            {
                size_t fbLen = r->FirstBytes.size();

                // ѕровер€ем firstBytes полностью
                if (pos + fbLen > size) continue;
                if (memcmp(data + pos, r->FirstBytes.data(), fbLen) != 0) continue;
                // ≈сли есть хвост Ч провер€ем SHA-256
                if (r->RemainderLength > 0 && r->RemainderHashLen == 32)
                {
                    size_t remStart = pos + fbLen;
                    if (remStart + r->RemainderLength > size) continue;

                    uint8_t hash[32]{};
                    Sha256(data + remStart, r->RemainderLength, hash);
                    if (memcmp(hash, r->RemainderHash, 32) != 0) continue;
                }

                f.push_back(r);
            }
            candidates = f;
        }

        if (candidates.empty()) { pos++; continue; }

        // п.3.6 Ч объект вредоносен
        result.IsMalicious = true;
        result.ThreatName = candidates[0]->ThreatName;
        return true;
    }

    return false;
}