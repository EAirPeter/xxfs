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
    // load root inode, keep it forever
    x_vInoCache.IncLookup(0);
}

uint32_t Xxfs::Lookup(struct stat &vStat, uint32_t linPar, const char *pszName) {
    auto piPar = x_vInoCache.At(linPar);
    OpenedDir vParDir(this, piPar);
    auto lin = vParDir.Lookup(pszName);
    auto pi = x_vInoCache.IncLookup(linPar);
    X_FillStat(vStat, lin, pi);
    return lin;
}

void Xxfs::Forget(uint32_t lin, uint64_t cLookup) {
    x_vInoCache.DecLookup(lin, cLookup);
}

void Xxfs::GetAttr(struct stat &vStat, uint32_t lin) {
    auto pi = x_vInoCache.At(lin);
    X_FillStat(vStat, lin, pi);
}

ShrPtr<char []> Xxfs::ReadLink(uint32_t lin) {
    auto pi = x_vInoCache.At(lin);
    auto lcn = pi->lcnIdx0[0];
    if (!lcn)
        return {};
    return Y_Map<char []>(lcn);
}

uint32_t Xxfs::MkDir(struct stat &vStat, uint32_t linPar, const char *pszName) {
    auto pi = x_vInoCache.At(linPar);
    // TODO: write dir
    (void) vStat;
    (void) pi;
    (void) pszName;
    return 0;
}
void Xxfs::Unlink(uint32_t linPar, const char *pszName) {
    auto pi = x_vInoCache.At(linPar);
    // TODO: unlink
    (void) pi;
    (void) pszName;
}

void Xxfs::RmDir(uint32_t linPar, const char *pszName) {
    auto pi = x_vInoCache.At(linPar);
    // TODO: rmdir
    (void) pi;
    (void) pszName;
}

uint32_t Xxfs::SymLink(struct stat &vStat, const char *pszLink, uint32_t linPar, const char *pszName) {
    auto pi = x_vInoCache.At(linPar);
    // TODO: symlink
    (void) vStat;
    (void) pszLink;
    (void) pi;
    (void) pszName;
    return 0;
}

void Xxfs::Rename(
    uint32_t linPar, const char *pszName,
    uint32_t inoNewPar, const char *pszNewName,
    unsigned uFlags
) {
    auto pi = x_vInoCache.At(linPar);
    auto pNewNod = X_GetInode(inoNewPar);
    // TODO: rename
    (void) pi;
    (void) pNewNod;
    (void) pszName;
    (void) pszNewName;
    (void) uFlags;
}

void Xxfs::Link(struct stat &vStat, uint32_t lin, uint32_t inoNewPar, const char *pszNewName) {
    auto pNewNod = X_GetInode(inoNewPar);
    auto pi = X_GetInode<true>(lin);
    X_FillStat(vStat, lin, pi);
    // TODO: link
    (void) pNewNod;
    (void) pszNewName;
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
    return new OpenedFile(this, pi, bWrite, bAppend, bDirect);
}

uint64_t Xxfs::Read(OpenedFile *pFile, std::unique_ptr<char []> &upBuf, uint64_t cbSize, uint64_t cbOff) {
    if (!cbSize)
        return 0;
    if (cbOff >= pFile->pi->cbSize)
        return 0;
    cbSize = std::min(pFile->pi->cbSize - cbOff, cbSize);
    upBuf = std::make_unique<char []>(cbSize);
    pFile->Read(upBuf.get(), cbSize, cbOff);
    return cbSize;
}

uint64_t Xxfs::Write(OpenedFile *pFile, const void *pBuf, uint64_t cbSize, uint64_t cbOff) {
    if (!cbSize)
        return 0;
    if (!pFile->bWrite)
        throw Exception {EACCES};
    if (pFile->bAppend)
        cbOff = pFile->pi->cbSize;
    auto cbRes = pFile->Write(pBuf, cbSize, cbOff);
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
    pFile->Sync();
}

OpenedDir *Xxfs::OpenDir(uint32_t lin) {
    auto pi = x_vInoCache.At(lin);
    if (!pi->IsDir())
        throw Exception {ENOTDIR};
    return new OpenedDir(this, pi);
}

size_t Xxfs::ReadDir(OpenedDir *pDir, fuse_req_t pReq, char *pBuf, size_t cbSize, off_t vOff) {
    pDir->IterSeek(vOff);
    FileStat vStat;
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
void Xxfs::X_FillStat(struct stat &vStat, uint32_t lin, Inode *pi) {
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
