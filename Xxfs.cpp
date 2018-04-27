#include "Common.hpp"

#include "Xxfs.hpp"

inline Xxfs::X_OpenedInodeBlock::X_OpenedInodeBlock(MapPtr<InodeBlock> &&rpBlk_) :
    rpBlk {std::move(rpBlk_)} {}

Xxfs::X_OpenedFile::X_OpenedFile(
    Xxfs *pXxfs_, Inode *pNod_,
    bool bWrite_, bool bAppend_, bool bDirect_
) : pXxfs {pXxfs_}, pNod {pNod_}, bWrite {bWrite_}, bAppend {bAppend_}, bDirect {bDirect_}
{}

template<bool kAlloc>
void Xxfs::X_OpenedFile::Seek(X_FilePointer &v, uint32_t idxNewBlkInFile) {
    if (v.rpData && v.idxBlkInFile == idxNewBlkInFile)
        return;
    v.idxBlkInFile = pXxfs->X_Seek<FileBlock>(pNod, v.rpData, idxNewBlkInFile);
}

void Xxfs::X_OpenedFile::DoRead(void *pBuf, uint64_t cbSize, uint64_t cbOff) {
    auto pBytes = (uint8_t *) pBuf;
    uint64_t cbRead = 0;
    while (cbRead < cbSize) {
        auto idxByteInBlk = cbOff % kcbBlkSize;
        auto idxBlkInFile = cbOff / kcbBlkSize;
        auto cbToRead = std::min(kcbBlkSize - idxByteInBlk, cbSize);
        Seek(vR, idxBlkInFile);
        if (vR.rpData)
            memcpy(pBytes, vR.rpData->aData + idxByteInBlk, cbToRead);
        else
            memset(pBytes, 0, cbToRead);
        pBytes += cbToRead;
        cbOff += cbToRead;
        cbRead += cbToRead;
    }
}

uint64_t Xxfs::X_OpenedFile::DoWrite(const void *pBuf, uint64_t cbSize, uint64_t cbOff) {
    auto pBytes = (const uint8_t *) pBuf;
    uint64_t cbWritten = 0;
    try {
        while (cbWritten < cbSize) {
            auto idxByteInBlk = cbOff % kcbBlkSize;
            auto idxBlkInFile = cbOff / kcbBlkSize;
            auto cbToWrite = std::min(kcbBlkSize - idxByteInBlk, cbSize);
            Seek<true>(vW, idxBlkInFile);
            memcpy(vW.rpData->aData + idxByteInBlk, pBytes, cbToWrite);
            if (bDirect)
                DoSync();
            pBytes += cbToWrite;
            cbOff += cbToWrite;
            cbWritten += cbToWrite;
        }
    }
    catch (Exception &e) {
        if (e.nErrno != ENOSPC || !cbWritten)
            throw;
    }
    return cbWritten;
}

inline void Xxfs::X_OpenedFile::DoSync() noexcept {
    if (vW.rpData)
        msync(vW.rpData.Get(), kcbBlkSize, MS_SYNC);
}

inline Xxfs::X_OpenedDir::X_OpenedDir(Xxfs *pXxfs_, Inode *pNod_) :
    pXxfs {pXxfs_}, pNod {pNod_}, rpBlks {new MapPtr<DirBlock>[pNod->cBlock + 1]}
{}

inline DirEnt *Xxfs::X_OpenedDir::GetEnt(uint32_t eno) {
    auto idxEntInBlk = eno % kcEntPerBlk;
    auto idxBlkInFile = eno / kcEntPerBlk;
    if (idxBlkInFile >= pNod->cBlock)
        throw Exception {ENOENT};
    if (!rpBlks[idxBlkInFile])
        pXxfs->X_Seek<DirBlock>(pNod, rpBlks[idxBlkInFile], idxBlkInFile);
    return &rpBlks[idxBlkInFile]->aEnts[idxEntInBlk];
}

inline uint32_t Xxfs::X_OpenedDir::DoLookup(const char *pszName) {
    auto pEnt = GetEnt(0);
    while (*pszName) {
        auto byKey = (uint8_t) *pszName;
        auto enoChild = pEnt->enoChild;
        DirEnt *pChild = nullptr;
        while (enoChild) {
            pChild = GetEnt(enoChild);
            if (pChild->byKey < byKey)
                break;
        }
        if (!enoChild)
            throw Exception {ENOENT};
        if (pChild->byKey != byKey)
            throw Exception {ENOENT};
        pEnt = pChild;
        ++pszName;
    }
    if (!pEnt->bExist)
        throw Exception {ENOENT};
    return pEnt->inoFile;
}

void Xxfs::X_OpenedDir::IterSeek(off_t vOff) {
    if (!pNod->cBlock || !vOff) {
        cStkSize = 0;
        return;
    }
    auto enoReq = (uint32_t) vOff;
    if (cStkSize && StkTop() == enoReq)
        return;
    cStkSize = 0;
    StkPush(0);
    while (cStkSize && enoReq != StkTop())
        TrieNext();
}

const char *Xxfs::X_OpenedDir::IterNext(struct stat &vStat, off_t &vOff) {
    TrieNext();
    while (cStkSize && !GetEnt(StkTop())->bExist)
        TrieNext();
    if (!cStkSize)
        return nullptr;
    auto pEnt = GetEnt(StkTop());
    vStat.st_ino = (ino_t) pEnt->inoFile;
    vStat.st_mode = (mode_t) pEnt->uMode;
}

void Xxfs::X_OpenedDir::TrieNext() {
    auto pEnt = GetEnt(StkTop());
    if (pEnt->enoChild) {
        StkPush(pEnt->enoChild);
        return;
    }
    StkPop();
    while (cStkSize && !pEnt->enoNext) {
        pEnt = GetEnt(StkTop());
        StkPop();
    }
    if (cStkSize)
        StkPush(pEnt->enoNext);
}

Xxfs::Xxfs(RaiiFile &&roFile, MapPtr<SuperBlock> &&rpSupBlk) :
    x_roFile {std::move(roFile)},
    x_rpSupBlk {std::move(rpSupBlk)},
    x_bmpBno(
        x_roFile.Get(), x_rpSupBlk->cUsedBlk, x_rpSupBlk->bnoBnoBmpBmp,
        x_rpSupBlk->cBnoBmpBmpBlk, x_rpSupBlk->bnoBnoBmp
    ),
    x_bmpIno(
        x_roFile.Get(), x_rpSupBlk->cUsedNod, x_rpSupBlk->bnoInoBmpBmp,
        x_rpSupBlk->cInoBmpBmpBlk, x_rpSupBlk->bnoInoBmp
    )
{
    X_GetInode<true>(0);
}

uint32_t Xxfs::Lookup(struct stat &vStat, uint32_t inoPar, const char *pszName) {
    auto pParNod = X_GetInode(inoPar);
    X_OpenedDir vParDir(this, pParNod);
    auto ino = vParDir.DoLookup(pszName);
    auto pNod = X_GetInode<true>(ino);
    X_FillStat(vStat, ino, pNod);
    return ino;
}

void Xxfs::Forget(uint32_t ino, uint64_t cLookup) {
    if (ino >= x_rpSupBlk->cInode)
        throw Exception {ENOENT};
    auto idxNodInBlk = ino % kcNodPerBlk;
    auto idxBlk = ino / kcNodPerBlk;
    auto it = x_mapNodBlks.find(idxBlk);
    if (it == x_mapNodBlks.end())
        throw Exception {ENOENT};
    auto pRef = &it->second;
    if (!(pRef->cNodLookup[idxNodInBlk] -= cLookup)) {
        if (!pRef->rpBlk->aNods[idxNodInBlk].cLink) {
            // TODO: really erase the file
        }
    }
    if (!(pRef->cBlkLookup -= cLookup))
        x_mapNodBlks.erase(it);
}

void Xxfs::GetAttr(struct stat &vStat, uint32_t ino) {
    auto pNod = X_GetInode(ino);
    X_FillStat(vStat, ino, pNod);
}

MapPtr<char> Xxfs::ReadLink(uint32_t ino) {
    auto pNod = X_GetInode(ino);
    auto idxBlk = pNod->bnoIdx0[0];
    if (!idxBlk)
        return {};
    return X_Map<char>(idxBlk);
}

uint32_t Xxfs::MkDir(struct stat &vStat, uint32_t inoPar, const char *pszName) {
    auto pNod = X_GetInode(inoPar);
    // TODO: write dir
    (void) vStat;
    (void) pNod;
    (void) pszName;
    return 0;
}
void Xxfs::Unlink(uint32_t inoPar, const char *pszName) {
    auto pNod = X_GetInode(inoPar);
    // TODO: unlink
    (void) pNod;
    (void) pszName;
}

void Xxfs::RmDir(uint32_t inoPar, const char *pszName) {
    auto pNod = X_GetInode(inoPar);
    // TODO: rmdir
    (void) pNod;
    (void) pszName;
}

uint32_t Xxfs::SymLink(struct stat &vStat, const char *pszLink, uint32_t inoPar, const char *pszName) {
    auto pNod = X_GetInode(inoPar);
    // TODO: symlink
    (void) vStat;
    (void) pszLink;
    (void) pNod;
    (void) pszName;
    return 0;
}

void Xxfs::Rename(
    uint32_t inoPar, const char *pszName,
    uint32_t inoNewPar, const char *pszNewName,
    unsigned uFlags
) {
    auto pNod = X_GetInode(inoPar);
    auto pNewNod = X_GetInode(inoNewPar);
    // TODO: rename
    (void) pNod;
    (void) pNewNod;
    (void) pszName;
    (void) pszNewName;
    (void) uFlags;
}

void Xxfs::Link(struct stat &vStat, uint32_t ino, uint32_t inoNewPar, const char *pszNewName) {
    auto pNewNod = X_GetInode(inoNewPar);
    auto pNod = X_GetInode<true>(ino);
    X_FillStat(vStat, ino, pNod);
    // TODO: link
    (void) pNewNod;
    (void) pszNewName;
}

void *Xxfs::Open(uint32_t ino, fuse_file_info *pInfo) {
    if (pInfo->flags & O_DIRECTORY)
        throw Exception {ENOTSUP};
    auto pNod = X_GetInode(ino);
    if (pNod->IsDir())
        throw Exception {EISDIR};
    bool bWrite = false;
    switch (pInfo->flags & O_ACCMODE) {
    case O_RDONLY:
        break;
    case O_WRONLY:
    case O_RDWR:
        bWrite = true;
        break;
    default:
        throw Exception {EINVAL};
    }
    bool bAppend = false;
    if (pInfo->flags & O_APPEND) {
        if (!bWrite)
            throw Exception {EINVAL};
        bAppend = true;
    }
    if (pInfo->flags & O_TRUNC) {
        // TODO: truncate
    }
    bool bDirect = false;
    if (pInfo->flags & O_DIRECT) {
        pInfo->direct_io = true;
        bDirect = true;
    }
    else
        pInfo->direct_io = false;
    return new X_OpenedFile(this, pNod, bWrite, bAppend, bDirect);
}

uint64_t Xxfs::Read(void *pParam, PodPtr<uint8_t> &rpBuf, uint64_t cbSize, uint64_t cbOff) {
    if (!cbSize)
        return 0;
    auto pFile = (X_OpenedFile *) pParam;
    if (cbOff >= pFile->pNod->cbSize)
        return 0;
    cbSize = std::min(pFile->pNod->cbSize - cbOff, cbSize);
    rpBuf = (uint8_t *) ::operator new(cbSize);
    pFile->DoRead(rpBuf.Get(), cbSize, cbOff);
    return cbSize;
}

uint64_t Xxfs::Write(void *pParam, const uint8_t *pBuf, uint64_t cbSize, uint64_t cbOff) {
    if (!cbSize)
        return 0;
    auto pFile = (X_OpenedFile *) pParam;
    if (!pFile->bWrite)
        throw Exception {EACCES};
    if (pFile->bAppend)
        cbOff = pFile->pNod->cbSize;
    auto cbRes = pFile->DoWrite(pBuf, cbSize, cbOff);
    if (cbOff + cbRes > pFile->pNod->cbSize)
        pFile->pNod->cbSize = cbOff + cbRes;
    return cbRes;
}

void Xxfs::Release(void *pParam) noexcept {
    auto pFile = (X_OpenedFile *) pParam;
    delete pFile;
}

void Xxfs::FSync(void *pParam) noexcept {
    auto pFile = (X_OpenedFile *) pParam;
    pFile->DoSync();
}

void *Xxfs::OpenDir(uint32_t ino) {
    auto pNod = X_GetInode(ino);
    return new X_OpenedDir(this, pNod);
}

size_t Xxfs::ReadDir(void *pParam, fuse_req_t pReq, char *pBuf, size_t cbSize, off_t vOff) {
    auto pDir = (X_OpenedDir *) pParam;
    pDir->IterSeek(vOff);
    struct stat vStat;
    auto pszName = pDir->IterNext(vStat, vOff);
    size_t cbRes = 0;
    while (pszName) {
        auto cbNeeded = fuse_add_direntry(pReq, pBuf, cbSize, pszName, &vStat, vOff);
        if (cbNeeded < cbSize)
            break;
        cbRes += cbNeeded;
        cbSize -= cbNeeded;
    }
    return cbRes;
}

/*
uint32_t Xxfs::X_Lookup(Inode *pParNod, const char *pszName) {
    X_OpenedFile vParFile {{}, {}, this, pParNod, false, false, false};
    PodPtr<DirEnt> rpEnts = (DirEnt *) ::operator new(pParNod->cbSize);
    vParFile.DoRead((uint8_t *) rpEnts.Get(), pParNod->cbSize, 0);
    uint32_t enoCur = 0;
    while (*pszName) {
        auto enoChild = rpEnts[enoCur].enoChild;
        while (enoChild && rpEnts[enoChild].byKey < (uint8_t) *pszName)
            enoChild = rpEnts[enoChild].enoNext;
        if (!enoChild)
            throw Exception {ENOENT};
        if (rpEnts[enoChild].byKey != (uint8_t) *pszName)
            throw Exception {ENOENT};
        enoCur = enoChild;
    }
    if (!rpEnts[enoCur].bExist)
        throw Exception {ENOENT};
    return rpEnts[enoCur].inoFile;
}

template<bool kReserve>
inline PodPtr<DirEnt> Xxfs::X_DirOpen(Inode *pNod) {
    if (!pNod->IsDir())
        throw Exception {ENOTDIR};
    PodPtr<DirEnt> rpEnts;
    if (pNod->cbSize) {
        if constexpr (kReserve)
            rpEnts = (DirEnt *) ::operator new(pParNod->cbSize + kcbBlkSize);
        else
            rpEnts = (DirEnt *) ::operator new(pParNod->cbSize);
        X_OpenedFile vFile {{}, {}, this, pNod, false, false, false};
        vFile.DoRead(rpEnts.Get(), pNod->cbSize, 0);
    }
    else if constexpr (kReserve)
        rpEnts = (DirEnt *) ::operator new(kcbBlkSize);
    return std::move(rpEnts);
}

template<bool kCreate>
inline DirEnt *Xxfs::X_DirLookup(DirEnt *pEnts, const char *pszName) {
    uint32_t eno = 0;
    while (*pszName) {
        auto enoPrev = 0;
        auto enoChild = rpEnts[enoCur].enoChild;
        while (enoChild && rpEnts[enoChild].byKey < (uint8_t) *pszName) {
            enoPrev = enoChild;
            enoChild = rpEnts[enoChild].enoNext;
        }
        if (!enoChild) {
            if constexpr (kCreate) {
                // TOOD: alloc
            }
            else
                throw Exception {ENOENT};
        }
        if (rpEnts[enoChild].byKey != (uint8_t) *pszName)
            throw Exception {ENOENT};
        enoCur = enoChild;
    }
    if (!rpEnts[enoCur].bExist)
        throw Exception {ENOENT};
    return rpEnts[enoCur].inoFile;
}
*/

template<bool kAddRef>
Inode *Xxfs::X_GetInode(uint32_t ino) {
    if (ino >= x_rpSupBlk->cInode)
        throw Exception {ENOENT};
    auto idxNodInBlk = ino % kcNodPerBlk;
    auto idxBlk = ino / kcNodPerBlk;
    auto it = x_mapNodBlks.find(idxBlk);
    if (it == x_mapNodBlks.end()) {
        if constexpr (!kAddRef)
            throw Exception {ENOENT};
        auto &&res = x_mapNodBlks.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(idxBlk),
            std::forward_as_tuple(X_Map<InodeBlock>(x_rpSupBlk->bnoNod + idxBlk))
        );
        it = res.first;
    }
    if constexpr (kAddRef) {
        ++it->second.cBlkLookup;
        ++it->second.cNodLookup[idxNodInBlk];
    }
    return &it->second.rpBlk->aNods[idxNodInBlk];
}

template<class tBlock>
uint32_t Xxfs::X_AllocFor(Inode *pNod, MapPtr<tBlock> &rpBlk) {
    auto bno = x_bmpBno.Alloc();
    ++pNod->cBlock;
    rpBlk = X_Map<tBlock>(bno);
    memset(rpBlk.Get(), 0, kcbBlkSize);
    return bno;
}

template<class tBlock, bool kAlloc>
uint32_t Xxfs::X_Seek(Inode *pNod, MapPtr<tBlock> &rpBlk, uint32_t idxBlkInFile) {
    rpBlk.Release();
    uint32_t *pBnoIdx0 = nullptr;
    uint32_t *pBnoIdx1 = nullptr;
    uint32_t *pBnoIdx2 = nullptr;
    MapPtr<IndexBlock> rpIdx1, rpIdx2, rpIdx3;
    if (idxBlkInFile >= kcOffIdx3Blk) {
        idxBlkInFile -= kcOffIdx3Blk;
        auto &bnoIdx3 = pNod->bnoIdx3;
        if (bnoIdx3)
            rpIdx3 = X_Map<IndexBlock>(bnoIdx3);
        else if constexpr (kAlloc)
            bnoIdx3 = X_AllocFor<IndexBlock>(pNod, rpIdx3);
        else
            return 0;
        pBnoIdx2 = &rpIdx3->aBnos[idxBlkInFile / kcIdx2Blk];
        idxBlkInFile %= kcIdx2Blk;
    }
    if (idxBlkInFile >= kcOffIdx2Blk || pBnoIdx2) {
        if (!pBnoIdx2) {
            idxBlkInFile -= kcOffIdx2Blk;
            pBnoIdx2 = &pNod->bnoIdx2;
        }
        auto &bnoIdx2 = *pBnoIdx2;
        if (bnoIdx2)
            rpIdx2 = X_Map<IndexBlock>(bnoIdx2);
        else if constexpr (kAlloc)
            bnoIdx2 = X_AllocFor<IndexBlock>(pNod, rpIdx2);
        else
            return 0;
        pBnoIdx1 = &rpIdx3->aBnos[idxBlkInFile / kcIdx1Blk];
        idxBlkInFile %= kcIdx1Blk;
    }
    if (idxBlkInFile >= kcOffIdx1Blk || pBnoIdx1) {
        if (!pBnoIdx1) {
            idxBlkInFile -= kcOffIdx1Blk;
            pBnoIdx1 = &pNod->bnoIdx1;
        }
        auto &bnoIdx1 = *pBnoIdx1;
        if (bnoIdx1)
            rpIdx1 = X_Map<IndexBlock>(bnoIdx1);
        else if constexpr (kAlloc)
            bnoIdx1 = X_AllocFor<IndexBlock>(pNod, rpIdx1);
        else
            return 0;
        pBnoIdx0 = &rpIdx3->aBnos[idxBlkInFile];
    }
    if (!pBnoIdx0)
        pBnoIdx0 = &pNod->bnoIdx0[idxBlkInFile];
    auto &bnoIdx0 = *pBnoIdx0;
    if (bnoIdx0)
        rpBlk = X_Map<FileBlock>(bnoIdx0);
    else if constexpr (kAlloc)
        bnoIdx0 = X_AllocFor<FileBlock>(pNod, rpBlk);
    return bnoIdx0;
}

template<class tBlock>
inline MapPtr<tBlock> Xxfs::X_Map(uint32_t bno) {
    return MapAt<tBlock>(x_roFile.Get(), bno);
}

void Xxfs::X_FillStat(struct stat &vStat, uint32_t ino, Inode *pNod) {
    vStat.st_dev = (dev_t) 0;
    vStat.st_ino = (ino_t) ino;
    vStat.st_mode = (mode_t) pNod->uMode;
    vStat.st_nlink = (nlink_t) pNod->cLink;
    vStat.st_uid = (uid_t) 0;
    vStat.st_gid = (gid_t) 0;
    vStat.st_rdev = 0;
    vStat.st_size = (off_t) pNod->cbSize;
    vStat.st_blksize = (blksize_t) kcbBlkSize;
    vStat.st_blocks = (blkcnt_t) pNod->cBlock;
    vStat.st_atim = {};
    vStat.st_mtim = {};
    vStat.st_ctim = {};
}
