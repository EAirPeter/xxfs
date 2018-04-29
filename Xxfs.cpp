#include "Common.hpp"

#include "Raii.hpp"

#include "Xxfs.hpp"

namespace xxfs {

Xxfs::Xxfs(RaiiFile &&vRf, ShrPtr<MetaCluster> &&spcMeta) :
    x_vRf(std::move(vRf)),
    x_spcMeta(std::move(spcMeta)),
    x_vCluCache(x_vRf.Get()),
    x_vInoCache(this),
    x_vCluAlloc(x_vRf.Get(), x_spcMeta->lcnCluBmp, x_spcMeta->ccCluBmp),
    x_vInoAlloc(x_vRf.Get(), x_spcMeta->lcnInoBmp, x_spcMeta->ccInoBmp)
{
    // load root directory inode, keep it forever
    x_vInoCache.IncLookup(0);
}

uint32_t Xxfs::Lookup(FileStat &vStat, uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = x_vInoCache.At(linPar);
    OpenedDir vParDir(this, piPar);
    uint16_t uMode = S_IFDIR | S_IFLNK | S_IFREG;
    auto lin = vParDir.Lookup(pszName, uMode);
    auto pi = x_vInoCache.IncLookup(linPar);
    X_FillStat(vStat, lin, pi);
    return lin;
}

void Xxfs::Forget(uint32_t lin, uint64_t cLookup) {
    x_vInoCache.DecLookup(lin, cLookup);
}

void Xxfs::GetAttr(FileStat &vStat, uint32_t lin) {
    auto pi = x_vInoCache.At(lin);
    X_FillStat(vStat, lin, pi);
}

void Xxfs::SetAttr(FileStat &vStat, FileStat *pStat, uint32_t lin, int nFlags, fuse_file_info *pInfo) {
    auto pi = x_vInoCache.At(lin);
    if (nFlags & FUSE_SET_ATTR_SIZE) {
        if (pi->IsDir())
            throw Exception {EISDIR};
        if ((size_t) pStat->st_size >= kcbMaxSize)
            throw Exception {EINVAL};
        pi->cbSize = (uint64_t) pStat->st_size;
        if (!pInfo)
            Y_FileShrink(pi);
    }
    X_FillStat(vStat, lin, pi);
}

const char *Xxfs::ReadLink(uint32_t lin) {
    auto pi = x_vInoCache.At(lin);
    auto lcn = pi->lcnIdx0[0];
    if (!lcn)
        return "";
    return Y_Map<char>(lcn).get();
}

uint32_t Xxfs::MkDir(FileStat &vStat, uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = x_vInoCache.At(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    auto lin = x_vInoAlloc.Alloc();
    auto pi = x_vInoCache.IncLookup(lin);
    memset(pi, 0, sizeof(Inode));
    uint16_t uMode = S_IFDIR | 0777;
    pi->uMode = uMode;
    pi->cLink = 1;
    try {
        try {
            OpenedDir vDir(this, piPar);
            vDir.Insert(pszName, lin, uMode);
        }
        catch (...) {
            x_vInoCache.DecLookup(lin);
            throw;
        }
        X_FillStat(vStat, lin, pi);
        return lin;
    }
    catch (...) {
        Y_EraseIno(lin, pi);
        throw;
    }
}
void Xxfs::Unlink(uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = x_vInoCache.At(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    {
        OpenedDir vDir(this, piPar);
        uint16_t uMode = S_IFLNK | S_IFREG;
        auto lin = vDir.Remove(pszName, uMode);
        auto pi = x_vInoCache.IncLookup(lin);
        --pi->cLink;
        x_vInoCache.DecLookup(lin);
        vDir.Shrink();
    }
    Y_FileShrink(piPar);
}

void Xxfs::RmDir(uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = x_vInoCache.At(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    {
        OpenedDir vDir(this, piPar);
        uint16_t uMode = S_IFDIR;
        auto lin = vDir.Lookup(pszName, uMode);
        auto pi = x_vInoCache.IncLookup(lin);
        if (pi->ccSize) {
            auto spc = Y_Map<DirCluster>(pi->lcnIdx0[0]);
            if (spc->aEnts[0].linFile != 1)
                throw Exception {ENOTEMPTY};
        }
        vDir.Remove(pszName, uMode);
        --pi->cLink;
        x_vInoCache.DecLookup(lin);
        vDir.Shrink();
    }
    Y_FileShrink(piPar);
}

uint32_t Xxfs::SymLink(FileStat &vStat, const char *pszLink, uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto cbLength = strlen(pszLink);
    if (cbLength >= kcbCluSize)
        throw Exception {ENAMETOOLONG};
    auto piPar = x_vInoCache.At(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    auto lin = x_vInoAlloc.Alloc();
    auto pi = x_vInoCache.IncLookup(lin);
    memset(pi, 0, sizeof(Inode));
    uint16_t uMode = S_IFLNK | 0777;
    pi->uMode = uMode;
    pi->cLink = 1;
    try {
        {
            OpenedFile vFile(this, pi, false, false, false);
            vFile.DoWrite(pszLink, (uint64_t) cbLength + 1, 0);
        }
        try {
            OpenedDir vDir(this, piPar);
            vDir.Insert(pszName, lin, uMode);
        }
        catch (...) {
            x_vInoCache.DecLookup(lin);
            throw;
        }
        X_FillStat(vStat, lin, pi);
        return lin;
    }
    catch (...) {
        Y_EraseIno(lin, pi);
        throw;
    }
}

void Xxfs::Rename(
    uint32_t linPar, const char *pszName,
    uint32_t linNewPar, const char *pszNewName,
    unsigned uFlags
) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    if (strlen(pszNewName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = x_vInoCache.At(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    auto piNewPar = x_vInoCache.At(linNewPar);
    if (!piNewPar->IsDir())
        throw Exception {ENOTDIR};
    {
        OpenedDir vDir(this, piPar);
        OpenedDir vNewDir(this, piNewPar);
        uint16_t uReplaceMode = 0;
        uint16_t uMode = S_IFDIR | S_IFLNK | S_IFREG;
        switch (uFlags) {
        case 0:
            uReplaceMode = S_IFLNK | S_IFREG;
            [[fallthrough]];
        case RENAME_NOREPLACE: {
            auto lin = vDir.Lookup(pszName, uMode);
            auto uOthMode = uMode;
            auto linOth = vNewDir.Insert(pszNewName, lin, uOthMode, uReplaceMode);
            if (linOth) {
                auto piOth = x_vInoCache.IncLookup(linOth);
                --piOth->cLink;
                x_vInoCache.DecLookup(linOth);
            }
            vDir.Remove(pszName, uMode);
            break;
        }
        case RENAME_EXCHANGE: {
            uReplaceMode = S_IFDIR | S_IFLNK | S_IFREG;
            auto lin = vDir.Lookup(pszName, uMode);
            auto uOthMode = uMode;
            auto linOth = vNewDir.Insert(pszNewName, lin, uOthMode, uReplaceMode);
            if (linOth)
                vDir.Insert(pszName, lin, uOthMode, uReplaceMode);
            else
                vDir.Remove(pszName, uMode);
            break;
        }
        default:
            throw Exception {EINVAL};
        }
        vDir.Shrink();
        vNewDir.Shrink();
    }
    Y_FileShrink(piPar);
    Y_FileShrink(piNewPar);
}

void Xxfs::Link(FileStat &vStat, uint32_t lin, uint32_t linNewPar, const char *pszNewName) {
    if (strlen(pszNewName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto pi = x_vInoCache.At(lin);
    if (pi->IsDir())
        throw Exception {EISDIR};
    auto piNewPar = x_vInoCache.At(linNewPar);
    if (!piNewPar->IsDir())
        throw Exception {ENOTDIR};
    auto uMode = pi->uMode;
    OpenedDir vDir(this, piNewPar);
    vDir.Insert(pszNewName, lin, uMode);
    x_vInoCache.IncLookup(lin);
    ++pi->cLink;
    X_FillStat(vStat, lin, pi);
}

OpenedFile *Xxfs::Open(uint32_t lin, fuse_file_info *pInfo) {
    if (pInfo->flags & O_DIRECTORY)
        throw Exception {ENOTSUP};
    auto pi = x_vInoCache.At(lin);
    if (pi->IsDir())
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
    if (pInfo->flags & O_TRUNC)
        pi->cbSize = 0;
    bool bDirect = false;
    if (pInfo->flags & O_DIRECT) {
        pInfo->direct_io = true;
        bDirect = true;
    }
    else
        pInfo->direct_io = false;
    return new OpenedFile(this, pi, bWrite, bAppend, bDirect);
}

uint64_t Xxfs::Read(OpenedFile *pFile, std::unique_ptr<char []> &upBuf, uint64_t cbSize, uint64_t cbOff) {
    if (!cbSize)
        return 0;
    if (cbOff >= pFile->pi->cbSize)
        return 0;
    cbSize = std::min(pFile->pi->cbSize - cbOff, cbSize);
    upBuf = std::make_unique<char []>(cbSize);
    pFile->DoRead(upBuf.get(), cbSize, cbOff);
    return cbSize;
}

uint64_t Xxfs::Write(OpenedFile *pFile, const void *pBuf, uint64_t cbSize, uint64_t cbOff) {
    if (!cbSize)
        return 0;
    if (!pFile->bWrite)
        throw Exception {EACCES};
    if (pFile->bAppend)
        cbOff = pFile->pi->cbSize;
    auto cbRes = pFile->DoWrite(pBuf, cbSize, cbOff);
    if (cbOff + cbRes > pFile->pi->cbSize)
        pFile->pi->cbSize = cbOff + cbRes;
    return cbRes;
}

void Xxfs::Release(OpenedFile *pFile) noexcept {
    auto pi = pFile->pi;
    delete pFile;
    Y_FileShrink(pi);
}

void Xxfs::FSync(OpenedFile *pFile) noexcept {
    pFile->DoSync();
}

OpenedDir *Xxfs::OpenDir(uint32_t lin) {
    auto pi = x_vInoCache.At(lin);
    if (!pi->IsDir())
        throw Exception {ENOTDIR};
    return new OpenedDir(this, pi);
}

size_t Xxfs::ReadDir(OpenedDir *pDir, fuse_req_t pReq, char *pBuf, size_t cbSize, off_t vOff) {
    size_t cbRes = 0;
    auto vNextOff = pDir->IterSeek(vOff);
    while (vNextOff != OpenedDir::kItEnd) {
        FileStat vStat;
        auto pszName = pDir->IterGet(vStat);
        vNextOff = pDir->IterNext();
        auto cbNeed = fuse_add_direntry(pReq, pBuf, cbSize, pszName, &vStat, vNextOff);
        if (cbNeed > cbSize)
            break;
        cbRes += cbNeed;
        cbSize -= cbNeed;
    }
    return cbRes;
}

void Xxfs::ReleaseDir(OpenedDir *pDir) noexcept {
    pDir->Shrink();
    delete pDir;
}

void Xxfs::StatFs(VfsStat &vStat) const noexcept {
    vStat.f_bsize = (unsigned long) kcbCluSize;
    vStat.f_frsize = (unsigned long) kcbCluSize;
    vStat.f_blocks = (fsblkcnt_t) x_spcMeta->ccTotal;
    vStat.f_bfree = (fsblkcnt_t) (x_spcMeta->ccTotal - x_spcMeta->ccUsed);
    vStat.f_bavail = (fsblkcnt_t) (x_spcMeta->ccTotal - x_spcMeta->ccUsed);
    vStat.f_files = (fsfilcnt_t) x_spcMeta->ciTotal;
    vStat.f_ffree = (fsfilcnt_t) (x_spcMeta->ciTotal - x_spcMeta->ciUsed);
    vStat.f_favail = (fsfilcnt_t) (x_spcMeta->ciTotal - x_spcMeta->ciUsed);
    vStat.f_namemax = (unsigned long) (kcePerClu - 1);
}

ShrPtr<InodeCluster> Xxfs::Y_MapInoClu(uint32_t vcn) noexcept {
    return x_vCluCache.At<InodeCluster>(x_spcMeta->lcnIno + vcn);
}

void Xxfs::Y_EraseIno(uint32_t lin, Inode *pi) noexcept {
    pi->cbSize = 0;
    Y_FileShrink(pi);
    assert(!pi->ccSize);
    x_vInoAlloc.Free(lin);
    --x_spcMeta->ciUsed;
}

void Xxfs::Y_FileAllocClu(Inode *pi, uint32_t &lcn) {
    if (x_spcMeta->ccUsed >= x_spcMeta->ccTotal)
        throw Exception {ENOSPC};
    lcn = x_vCluAlloc.Alloc();
    ++x_spcMeta->ccUsed;
    ++pi->ccSize;
}

void Xxfs::Y_FileFreeClu(Inode *pi, uint32_t &lcn) noexcept {
    if (!lcn)
        return;
    x_vCluAlloc.Free(lcn);
    lcn = 0;
    --pi->ccSize;
    --x_spcMeta->ccUsed;
}

void Xxfs::Y_FileFreeIdx1(Inode *pi, uint32_t &lcn, uint32_t vcnFrom) noexcept {
    if (!lcn)
        return;
    {
        auto spc = Y_Map<IndexCluster>(lcn);
        for (uint32_t i = vcnFrom; i < kcnPerClu; ++i)
            Y_FileFreeClu(pi, spc->aLcns[i]);
    }
    Y_FileFreeClu(pi, lcn);
}

void Xxfs::Y_FileFreeIdx2(Inode *pi, uint32_t &lcn, uint32_t vcnFrom) noexcept {
    if (!lcn)
        return;
    {
        auto vcn1 = vcnFrom % kccIdx1;
        auto idx1 = vcnFrom / kccIdx1;
        auto spc = Y_Map<IndexCluster>(lcn);
        Y_FileFreeIdx1(pi, spc->aLcns[idx1], vcn1);
        for (uint32_t i = idx1 + 1; i < kcnPerClu; ++i)
            Y_FileFreeIdx1(pi, spc->aLcns[i]);
    }
    Y_FileFreeClu(pi, lcn);
}

void Xxfs::Y_FileFreeIdx3(Inode *pi, uint32_t &lcn, uint32_t vcnFrom) noexcept {
    if (!lcn)
        return;
    {
        auto vcn2 = vcnFrom % kccIdx2;
        auto idx2 = vcnFrom / kccIdx2;
        auto spc = Y_Map<IndexCluster>(lcn);
        Y_FileFreeIdx2(pi, spc->aLcns[idx2], vcn2);
        for (uint32_t i = idx2 + 1; i < kcnPerClu; ++i)
            Y_FileFreeIdx2(pi, spc->aLcns[i]);
    }
    Y_FileFreeClu(pi, lcn);
}

void Xxfs::Y_FileShrink(Inode *pi) noexcept {
    auto vcnEnd = (uint32_t) (pi->cbSize + kcbCluSize - 1) / kcbCluSize;
    if (vcnEnd <= kvcnIdx1) {
        for (uint32_t i = vcnEnd; i < kvcnIdx1; ++i)
            Y_FileFreeClu(pi, pi->lcnIdx0[i]);
        Y_FileFreeIdx1(pi, pi->lcnIdx1);
        Y_FileFreeIdx2(pi, pi->lcnIdx2);
        Y_FileFreeIdx3(pi, pi->lcnIdx3);
    }
    else if (vcnEnd <= kvcnIdx2) {
        Y_FileFreeIdx1(pi, pi->lcnIdx1, vcnEnd - kvcnIdx1);
        Y_FileFreeIdx2(pi, pi->lcnIdx2);
        Y_FileFreeIdx3(pi, pi->lcnIdx3);
    }
    else if (vcnEnd <= kvcnIdx3) {
        Y_FileFreeIdx2(pi, pi->lcnIdx2, vcnEnd - kvcnIdx2);
        Y_FileFreeIdx3(pi, pi->lcnIdx3);
    }
    else {
        Y_FileFreeIdx3(pi, pi->lcnIdx3);
    }
}

/*
template<bool kAddRef>
Inode *Xxfs::X_GetInode(uint32_t lin) {
    if (lin >= x_spcMeta->ciTotal)
        throw Exception {ENOENT};
    auto idxNodInBlk = lin % kciPerClu;
    auto lcn = lin / kciPerClu;
    auto it = x_mapNodBlks.find(lcn);
    if (it == x_mapNodBlks.end()) {
        if constexpr (!kAddRef)
            throw Exception {ENOENT};
        auto &&res = x_mapNodBlks.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(lcn),
            std::forward_as_tuple(X_Map<InodeBlock>(x_spcMeta->bnoNod + lcn))
        );
        it = res.first;
    }
    if constexpr (kAddRef) {
        ++it->second.cBlkLookup;
        ++it->second.cNodLookup[idxNodInBlk];
    }
    return &it->second.mpBlk->aNods[idxNodInBlk];
}

template<class tClu>
uint32_t Xxfs::X_AllocFor(Inode *pi, ShrPtr<tClu> &mpBlk) {
    auto bno = x_vCluAlloc.Alloc();
    ++pi->ccTotal;
    mpBlk = X_Map<tClu>(bno);
    memset(mpBlk.Get(), 0, kcbCluSize);
    return bno;
}

template<class tClu, bool kAlloc>
uint32_t Xxfs::X_Seek(Inode *pi, ShrPtr<tClu> &mpBlk, uint32_t vcn) {
    mpBlk.Release();
    uint32_t *pBnoIdx0 = nullptr;
    uint32_t *pBnoIdx1 = nullptr;
    uint32_t *pBnoIdx2 = nullptr;
    ShrPtr<IndexBlock> mpIdx1, mpIdx2, mpIdx3;
    if (vcn >= kcOffIdx3Blk) {
        vcn -= kcOffIdx3Blk;
        auto &bnoIdx3 = pi->bnoIdx3;
        if (bnoIdx3)
            mpIdx3 = X_Map<IndexBlock>(bnoIdx3);
        else if constexpr (kAlloc)
            bnoIdx3 = X_AllocFor<IndexBlock>(pi, mpIdx3);
        else
            return 0;
        pBnoIdx2 = &mpIdx3->aBnos[vcn / kcIdx2Blk];
        vcn %= kcIdx2Blk;
    }
    if (vcn >= kcOffIdx2Blk || pBnoIdx2) {
        if (!pBnoIdx2) {
            vcn -= kcOffIdx2Blk;
            pBnoIdx2 = &pi->bnoIdx2;
        }
        auto &bnoIdx2 = *pBnoIdx2;
        if (bnoIdx2)
            mpIdx2 = X_Map<IndexBlock>(bnoIdx2);
        else if constexpr (kAlloc)
            bnoIdx2 = X_AllocFor<IndexBlock>(pi, mpIdx2);
        else
            return 0;
        pBnoIdx1 = &mpIdx3->aBnos[vcn / kcIdx1Blk];
        vcn %= kcIdx1Blk;
    }
    if (vcn >= kcOffIdx1Blk || pBnoIdx1) {
        if (!pBnoIdx1) {
            vcn -= kcOffIdx1Blk;
            pBnoIdx1 = &pi->bnoIdx1;
        }
        auto &bnoIdx1 = *pBnoIdx1;
        if (bnoIdx1)
            mpIdx1 = X_Map<IndexBlock>(bnoIdx1);
        else if constexpr (kAlloc)
            bnoIdx1 = X_AllocFor<IndexBlock>(pi, mpIdx1);
        else
            return 0;
        pBnoIdx0 = &mpIdx3->aBnos[vcn];
    }
    if (!pBnoIdx0)
        pBnoIdx0 = &pi->lcnIdx0[vcn];
    auto &lcnIdx0 = *pBnoIdx0;
    if (lcnIdx0)
        mpBlk = X_Map<tClu>(lcnIdx0);
    else if constexpr (kAlloc)
        lcnIdx0 = X_AllocFor<tClu>(pi, mpBlk);
    return lcnIdx0;
}

template<class tClu>
inline ShrPtr<tClu> Xxfs::X_Map(uint32_t lcn) {
    return x_vCluCache.At<tClu>(lcn);
}
*/
constexpr void Xxfs::X_FillStat(FileStat &vStat, uint32_t lin, Inode *pi) noexcept {
    vStat.st_dev = (dev_t) 0;
    vStat.st_ino = (ino_t) lin;
    vStat.st_mode = (mode_t) pi->uMode;
    vStat.st_nlink = (nlink_t) pi->cLink;
    vStat.st_uid = (uid_t) 0;
    vStat.st_gid = (gid_t) 0;
    vStat.st_rdev = 0;
    vStat.st_size = (off_t) pi->cbSize;
    vStat.st_blksize = (blksize_t) kcbCluSize;
    vStat.st_blocks = (blkcnt_t) pi->ccSize;
    vStat.st_atim = {};
    vStat.st_mtim = {};
    vStat.st_ctim = {};
}

}
