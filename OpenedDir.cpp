#include "Common.hpp"

#include "OpenedDir.hpp"

namespace xxfs {

inline uint32_t OpenedDir::Lookup(const char *pszName) {
    auto pEnt = X_GetEnt(0);
    while (*pszName) {
        auto byKey = (uint8_t) *pszName;
        auto lenChild = pEnt->lenChild;
        DirEnt *pChild = nullptr;
        while (lenChild) {
            pChild = X_GetEnt(lenChild);
            if (pChild->byKey < byKey)
                break;
        }
        if (!lenChild)
            throw Exception {ENOENT};
        if (pChild->byKey != byKey)
            throw Exception {ENOENT};
        pEnt = pChild;
        ++pszName;
    }
    if (!pEnt->bExist)
        throw Exception {ENOENT};
    return pEnt->linFile;
}


void OpenedDir::IterSeek(off_t vOff) {
    if (!pi->ccSize || !vOff) {
        cStkSize = 0;
        return;
    }
    auto lenReq = (uint32_t) vOff;
    if (cStkSize && X_Top() == lenReq)
        return;
    aStk[0] = 0;
    cStkSize = 1;
    while (cStkSize && lenReq != X_Top())
        X_Next();
}

const char *OpenedDir::IterNext(FileStat &vStat, off_t &vOff) {
    X_Next();
    while (cStkSize && !X_GetEnt(X_Top())->bExist)
        X_Next();
    if (!cStkSize)
        return nullptr;
    auto pEnt = X_GetEnt(X_Top());
    vStat.st_ino = (ino_t) pEnt->linFile;
    vStat.st_mode = (mode_t) pEnt->uMode;
    vOff = (off_t) X_Top();
    szName[cStkSize - 1] = '\0';
    return szName;
}

DirEnt *OpenedDir::X_GetEnt(uint32_t len) {
    auto ven = len % kcePerClu;
    auto vcn = len / kcePerClu;
    auto spc = fpR.Seek<DirCluster>(px, pi, vcn);
    if (!spc)
        throw Exception {ENOENT};
    return &spc->aEnts[ven];
}

void OpenedDir::X_Next() {
    auto pEnt = X_GetEnt(X_Top());
    if (pEnt->lenChild) {
        X_Push(pEnt->lenChild);
        return;
    }
    X_Pop();
    while (cStkSize && !pEnt->lenNext) {
        pEnt = X_GetEnt(X_Top());
        X_Pop();
    }
    if (cStkSize)
        X_Push(pEnt->lenNext);
}

inline uint32_t OpenedDir::X_Top() const noexcept {
    return aStk[cStkSize - 1];
}

inline void OpenedDir::X_Push(uint32_t len) noexcept {
    szName[cStkSize - 1] = (char) X_GetEnt(len)->byKey;
    aStk[cStkSize++] = len;
}
inline void OpenedDir::X_Pop() noexcept {
    --cStkSize;
}

}
