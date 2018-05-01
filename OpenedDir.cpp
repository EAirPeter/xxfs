#include "Common.hpp"

#include "OpenedDir.hpp"
#include "Xxfs.hpp"

namespace xxfs {

std::pair<uint32_t, uint16_t> OpenedDir::Lookup(const char *pszName, DirPolicy vPolicy) {
    if (!pi->ccSize)
        throw Exception {ENOENT};
    auto pe = X_GetEnt(0);
    while (*pszName) {
        auto byKey = (uint8_t) *pszName;
        auto lenChild = pe->lenChild;
        DirEnt *pChild = nullptr;
        while (lenChild) {
            pChild = X_GetEnt(lenChild);
            if (pChild->byKey >= byKey)
                break;
            lenChild = pChild->lenNext;
        }
        if (!lenChild || pChild->byKey != byKey)
            throw Exception {ENOENT};
        pe = pChild;
        ++pszName;
    }
    if (!pe->bExist)
        throw Exception {ENOENT};
    switch (vPolicy) {
    case DirPolicy::kAny:
        break;
    case DirPolicy::kDir:
        if (!S_ISDIR(pe->uMode))
            throw Exception {ENOTDIR};
        break;
    case DirPolicy::kNotDir:
        if (S_ISDIR(pe->uMode))
            throw Exception {EISDIR};
        break;
    case DirPolicy::kNone:
        throw Exception {EINVAL};
    }
    return {pe->linFile, pe->uMode};
}

// begin (denoted by off_t {0}): the first entry with bExist == true
// end (denoted by ~off_t {0}): empty stack
off_t OpenedDir::IterSeek(off_t vOff) {
    if (!pi->ccSize || vOff == kItEnd) {
        x_cStkSize = 0;
        return kItEnd;
    }
    if (vOff == kItBegin) {
        x_cStkSize = 0;
        auto lenBegin = X_GetEnt(0)->lenChild;
        if (!lenBegin)
            return kItEnd;
        X_Push(lenBegin);
        while (x_cStkSize) {
            if (X_GetEnt(X_Top())->bExist)
                return (off_t) X_Top();
            X_Next();
        }
        return x_cStkSize ? (off_t) X_Top() : kItEnd;
    }
    auto len = (uint32_t) vOff;
    if (x_cStkSize) {
        auto lenPrev = X_Top();
        while (x_cStkSize) {
            if (X_Top() == len)
                return (off_t) X_Top();
            X_Next();
        }
        auto lenBegin = X_GetEnt(0)->lenChild;
        if (!lenBegin)
            return kItEnd;
        X_Push(lenBegin);
        while (x_cStkSize && X_Top() != lenPrev) {
            if (X_Top() == len)
                return (off_t) X_Top();
            X_Next();
        }
        throw Exception {ENOENT};
    }
    else {
        auto lenBegin = X_GetEnt(0)->lenChild;
        if (!lenBegin)
            return kItEnd;
        X_Push(lenBegin);
        while (x_cStkSize) {
            if (X_Top() == len)
                return (off_t) X_Top();
            X_Next();
        }
        throw Exception {ENOENT};
    }
}

const char *OpenedDir::IterGet(FileStat &vStat) noexcept {
    if (!x_cStkSize)
        return nullptr;
    auto pe = X_GetEnt(X_Top());
    vStat.st_ino = (ino_t) pe->linFile;
    vStat.st_mode = (mode_t) pe->uMode;
    x_szName[x_cStkSize] = '\0';
    return x_szName;
}

off_t OpenedDir::IterNext() noexcept {
    if (!x_cStkSize)
        return kItEnd;
    X_Next();
    while (x_cStkSize) {
        if (X_GetEnt(X_Top())->bExist)
            return (off_t) X_Top();
        X_Next();
    }
    return kItEnd;
}

std::pair<uint32_t, uint16_t> OpenedDir::Insert(const char *pszName, uint32_t lin, uint16_t uMode, DirPolicy vPolicy) {
    X_PrepareRoot();
    auto pe = X_GetEnt(0);
    auto ceNeed = (uint32_t) strlen(pszName);
    auto ceFree = kcePerClu * pi->ccSize - pe->linFile;
    if (ceNeed > ceFree && px->AvailClu() < 4)
        throw Exception {ENOSPC};
    while (*pszName) {
        auto byKey = (uint8_t) *pszName;
        auto *pLenChild = &pe->lenChild;
        DirEnt *pChild = nullptr;
        while (*pLenChild) {
            pChild = X_GetEnt(*pLenChild);
            if (pChild->byKey >= byKey)
                break;
            pLenChild = &pChild->lenNext;
        }
        if (!*pLenChild || pChild->byKey != byKey) {
            auto lenNext = *pLenChild;
            *pLenChild = X_Alloc();
            pChild = X_GetEnt(*pLenChild);
            pChild->lenNext = lenNext;
            pChild->lenChild = 0;
            pChild->byKey = byKey;
            pChild->bExist = false;
        }
        pe = pChild;
        ++pszName;
    }
    if (pe->bExist) {
        switch (vPolicy) {
        case DirPolicy::kAny:
            break;
        case DirPolicy::kDir:
            if (!S_ISDIR(pe->uMode))
                throw Exception {ENOTDIR};
            break;
        case DirPolicy::kNotDir:
            if (S_ISDIR(pe->uMode))
                throw Exception {EISDIR};
            break;
        case DirPolicy::kNone:
            throw Exception {EEXIST};
        }
        auto linOld = pe->linFile;
        auto uModeOld = pe->uMode;
        pe->linFile = lin;
        pe->uMode = uMode;
        return {linOld, uModeOld};
    }
    pe->linFile = lin;
    pe->uMode = uMode;
    pe->bExist = true;
    return {0, 0};
}

namespace {
struct MappedStack : NoCopyMove {
    inline void Push(const ShrPtr<DirCluster> &spc, uint32_t *pLcn) noexcept {
        x_aData[x_cSize].spc = spc;
        x_aData[x_cSize].pLcn = pLcn;
        ++x_cSize;
    }

    inline ShrPtr<DirCluster> Pop() noexcept {
        return std::move(x_aData[--x_cSize].spc);
    }

    constexpr uint32_t *Top() const noexcept {
        return x_aData[x_cSize - 1].pLcn;
    }

    constexpr bool IsEmpty() const noexcept {
        return !x_cSize;
    }

private:
    struct {
        ShrPtr<DirCluster> spc;
        uint32_t *pLcn;
    } x_aData[kcePerClu];
    uint32_t x_cSize = 0;

};

}

std::pair<uint32_t, uint16_t> OpenedDir::Remove(const char *pszName, DirPolicy vPolicy) {
    if (!pi->ccSize)
        throw Exception {ENOENT};
    MappedStack vStk;
    auto pe = X_GetEnt(0);
    while (*pszName) {
        auto byKey = (uint8_t) *pszName;
        auto *pLenChild = &pe->lenChild;
        DirEnt *pChild = nullptr;
        while (*pLenChild) {
            pChild = X_GetEnt(*pLenChild);
            if (pChild->byKey >= byKey)
                break;
            pLenChild = &pChild->lenNext;
        }
        if (!*pLenChild || pChild->byKey != byKey)
            throw Exception {ENOENT};
        vStk.Push(x_fpR.Get<DirCluster>(), pLenChild);
        pe = pChild;
        ++pszName;
    }
    if (!pe->bExist)
        throw Exception {ENOENT};
    switch (vPolicy) {
    case DirPolicy::kAny:
        break;
    case DirPolicy::kDir:
        if (!S_ISDIR(pe->uMode))
            throw Exception {ENOTDIR};
        break;
    case DirPolicy::kNotDir:
        if (S_ISDIR(pe->uMode))
            throw Exception {EISDIR};
        break;
    case DirPolicy::kNone:
        throw Exception {EINVAL};
    }
    auto lin = pe->linFile;
    auto uMode = pe->uMode;
    pe->bExist = false;
    while (!vStk.IsEmpty()) {
        auto *pLcn = vStk.Top();
        auto spc = vStk.Pop();
        pe = X_GetEnt(*pLcn);
        if (pe->bExist || pe->lenChild)
            break;
        auto lenNext = pe->lenNext;
        X_Free(*pLcn);
        *pLcn = lenNext;
    }
    return {lin, uMode};
}

void OpenedDir::Shrink(bool bForce) noexcept {
    if (!pi->ccSize)
        return;
    auto peRoot = X_GetEnt(0);
    auto ceTotal = pi->ccSize * kcePerClu;
    auto ceUsed = peRoot->linFile;
    if (!bForce && ceUsed * 2 >= ceTotal)
        return;
    auto ccSizeNew = (ceUsed + kcePerClu - 1) / kcePerClu;
    auto ceNew = ccSizeNew * kcePerClu;
    for (auto pLenNext = &peRoot->lenNext; *pLenNext; ) {
        auto pe = X_GetEnt(*pLenNext);
        if (*pLenNext < ceNew)
            pLenNext = &pe->lenNext;
        else
            *pLenNext = pe->lenNext;
    }
    if (peRoot->lenChild) {
        MappedStack vStk;
        FilePtrR fp;
        vStk.Push({}, &peRoot->lenChild);
        while (!vStk.IsEmpty()) {
            auto &len = *vStk.Top();
            DirEnt *pe;
            if (len < ceNew)
                pe = X_MapEnt(fp, len);
            else {
                auto lenOld = len;
                auto peOld = X_GetEnt(lenOld);
                len = peRoot->lenNext;
                pe = X_MapEnt(fp, len);
                peRoot->lenNext = pe->lenNext;
                memcpy(pe, peOld, sizeof(DirEnt));
            }
            if (pe->lenChild) {
                vStk.Push(fp.Get<DirCluster>(), &pe->lenChild);
                continue;
            }
            auto spc = vStk.Pop();
            while (!vStk.IsEmpty() && !pe->lenNext) {
                pe = X_MapEnt(fp, *vStk.Top());
                spc = vStk.Pop();
            }
            if (pe->lenNext)
                vStk.Push(spc, &pe->lenNext);
        }
    }
    pi->cbSize = (uint64_t) kcbCluSize * ccSizeNew;
}

void OpenedDir::X_Next() noexcept {
    auto pe = X_GetEnt(X_Top());
    if (pe->lenChild) {
        X_Push(pe->lenChild);
        return;
    }
    X_Pop();
    while (x_cStkSize && !pe->lenNext) {
        pe = X_GetEnt(X_Top());
        X_Pop();
    }
    if (pe->lenNext)
        X_Push(pe->lenNext);
}

void OpenedDir::X_PrepareRoot() {
    if (!pi->ccSize) {
        auto spc = x_fpW.Seek<DirCluster>(px, pi, 0);
        pi->cbSize = (uint64_t) kcbCluSize * pi->ccSize;
        spc->aEnts[0].linFile = 1;
        spc->aEnts[0].lenChild = 0;
        spc->aEnts[0].bExist = false;
        spc->aEnts[0].byKey = '\0';
        for (uint32_t i = 0; i < kcePerClu; ++i)
            spc->aEnts[i].lenNext = i + 1;
        spc->aEnts[kcePerClu - 1].lenNext = 0;
    }
}

uint32_t OpenedDir::X_Alloc() {
    auto peRoot = X_GetEnt(0);
    if (!peRoot->lenNext) {
        auto len = pi->ccSize * kcePerClu;
        auto spc = x_fpW.Seek<DirCluster>(px, pi, pi->ccSize);
        pi->cbSize = (uint64_t) kcbCluSize * pi->ccSize;
        peRoot->lenNext = len;
        for (uint32_t i = 0; i < kcePerClu; ++i)
            spc->aEnts[i].lenNext = len + i + 1;
        spc->aEnts[kcePerClu - 1].lenNext = 0;
    }
    auto len = peRoot->lenNext;
    auto pe = X_GetEnt(len);
    peRoot->lenNext = pe->lenNext;
    ++peRoot->linFile;
    return len;
}

void OpenedDir::X_Free(uint32_t len) noexcept {
    auto peRoot = X_GetEnt(0);
    auto pe = X_GetEnt(len);
    pe->lenNext = peRoot->lenNext;
    peRoot->lenNext = len;
    --peRoot->linFile;
}

inline DirEnt *OpenedDir::X_GetEnt(uint32_t len) noexcept {
    return X_MapEnt(x_fpR, len);
}

template<bool kAlloc>
inline DirEnt *OpenedDir::X_MapEnt(FilePointer<kAlloc> &fp, uint32_t len) noexcept(!kAlloc) {
    auto ven = len % kcePerClu;
    auto vcn = len / kcePerClu;
    auto spc = fp.template Seek<DirCluster>(px, pi, vcn);
    return &spc->aEnts[ven];
}

inline uint32_t OpenedDir::X_Top() const noexcept {
    return x_alenStk[x_cStkSize - 1];
}

inline void OpenedDir::X_Push(uint32_t len) noexcept {
    x_szName[x_cStkSize] = (char) X_GetEnt(len)->byKey;
    x_alenStk[x_cStkSize++] = len;
}

inline void OpenedDir::X_Pop() noexcept {
    --x_cStkSize;
}

}
