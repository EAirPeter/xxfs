#include "Common.hpp"

#include "Raii.hpp"

#include "Xxfs.hpp"

namespace xxfs {

Xxfs::Xxfs(RaiiFile &&vRf, ShrPtr<MetaCluster> &&spcMeta) :
    x_vRf(std::move(vRf)),
    x_spcMeta(std::move(spcMeta)),
    x_vCluCache(x_vRf.Get()),
    x_vInoCluCache(x_vRf.Get()),
    x_vCluAlloc(x_vRf.Get(), x_spcMeta->lcnCluBmp, x_spcMeta->ccCluBmp),
    x_vInoAlloc(x_vRf.Get(), x_spcMeta->lcnInoBmp, x_spcMeta->ccInoBmp)
{}

uint32_t Xxfs::LinAt(const char *pszPath) {
    auto lin = LinPar(pszPath);
    if (*pszPath) {
        auto pi = X_GetInode(lin);
        {
            OpenedDir vDir(this, pi, lin);
            lin = vDir.Lookup(pszPath, DirPolicy::kAny).first;
        }
    }
    return lin;
}

uint32_t Xxfs::LinPar(const char *&pszPath) {
    uint32_t lin = 0;
    auto pszDelim = strchr(++pszPath, '/');
    while (pszDelim) {
        auto pi = X_GetInode(lin);
        {
            OpenedDir vDir(this, pi, lin);
            lin = vDir.Lookup(std::string(pszPath, pszDelim).c_str(), DirPolicy::kDir).first;
        }
        pszPath = pszDelim;
        pszDelim = strchr(++pszPath, '/');
    }
    return lin;
}

/*uint32_t Xxfs::Lookup(FileStat &vStat, uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = X_GetInode(linPar);
    OpenedDir vParDir(this, piPar);
    auto [lin, uMode] = vParDir.Lookup(pszName, DirPolicy::kAny);
    (void) uMode;
    auto pi = X_GetInode(lin);
    FillStat(vStat, lin, pi);
    return lin;
}

void Xxfs::Forget(uint32_t lin, uint64_t cLookup) {
    x_vInoCache.DecLookup(lin, cLookup);
}*/

void Xxfs::GetAttr(FileStat &vStat, uint32_t lin) {
    auto pi = X_GetInode(lin);
    FillStat(vStat, lin, pi);
}

/*void Xxfs::SetAttr(FileStat &vStat, FileStat *pStat, uint32_t lin, int nFlags, fuse_file_info *pInfo) {
    auto pi = X_GetInode(lin);
    if (nFlags & FUSE_SET_ATTR_SIZE) {
        if (pi->IsDir())
            throw Exception {EISDIR};
        if ((size_t) pStat->st_size >= kcbMaxSize)
            throw Exception {EINVAL};
        pi->cbSize = (uint64_t) pStat->st_size;
        if (!pInfo)
            Y_FileShrink(pi);
    }
    FillStat(vStat, lin, pi);
}*/

void Xxfs::ReadLink(uint32_t lin, char *pBuf, size_t cbSize) {
    auto pi = X_GetInode(lin);
    auto lcn = pi->lcnIdx0[0];
    strncpy(pBuf, lcn ? Y_Map<char>(lcn).get() : "", cbSize);
}

void Xxfs::MkDir(uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = X_GetInode(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    auto lin = Y_AllocIno();
    auto pi = X_GetInode(lin);
    memset(pi, 0, sizeof(Inode));
    pi->uMode = S_IFDIR | 0777;
    pi->cLink = 1;
    try {
        OpenedDir vDir(this, piPar, linPar);
        vDir.Insert(pszName, lin, pi->uMode, DirPolicy::kNone);
    }
    catch (...) {
        Y_UnlinkIno(lin, pi);
        throw;
    }
}
void Xxfs::Unlink(uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = X_GetInode(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    {
        OpenedDir vDir(this, piPar, linPar);
        auto [lin, uMode] = vDir.Remove(pszName, DirPolicy::kNotDir);
        (void) uMode;
        Y_UnlinkIno(lin, X_GetInode(lin));
        vDir.Shrink();
    }
    Y_FileShrink(piPar);
}

void Xxfs::RmDir(uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = X_GetInode(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    {
        OpenedDir vDir(this, piPar, linPar);
        auto [lin, uMode] = vDir.Lookup(pszName, DirPolicy::kDir);
        (void) uMode;
        auto pi = X_GetInode(lin);
        if (pi->ccSize) {
            auto spc = Y_Map<DirCluster>(pi->lcnIdx0[0]);
            if (spc->aEnts[0].linFile != 1)
                throw Exception {ENOTEMPTY};
        }
        vDir.Remove(pszName, DirPolicy::kDir);
        Y_UnlinkIno(lin, pi);
        vDir.Shrink();
    }
    Y_FileShrink(piPar);
}

void Xxfs::SymLink(const char *pszLink, uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto cbLength = strlen(pszLink);
    if (cbLength >= kcbCluSize)
        throw Exception {ENAMETOOLONG};
    auto piPar = X_GetInode(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    auto lin = Y_AllocIno();
    auto pi = X_GetInode(lin);
    memset(pi, 0, sizeof(Inode));
    pi->uMode = S_IFLNK | 0777;
    pi->cLink = 1;
    try {
        {
            OpenedFile vFile(this, pi, lin, true, false, false);
            vFile.DoWrite(pszLink, (uint64_t) cbLength + 1, 0);
        }
        OpenedDir vDir(this, piPar, linPar);
        vDir.Insert(pszName, lin, pi->uMode, DirPolicy::kNone);
    }
    catch (...) {
        Y_UnlinkIno(lin, pi);
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
    auto piPar = X_GetInode(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    auto piNewPar = X_GetInode(linNewPar);
    if (!piNewPar->IsDir())
        throw Exception {ENOTDIR};
    {
        OpenedDir vDir(this, piPar, linPar);
        OpenedDir vNewDir(this, piNewPar, linNewPar);
        switch (uFlags) {
        case 0: {
            auto [lin, uMode] = vDir.Lookup(pszName, DirPolicy::kAny);
            auto [linOth, uOthMode] = vNewDir.Insert(pszNewName, lin, uMode, DirPolicy::kNotDir);
            (void) uOthMode;
            if (linOth) {
                auto piOth = X_GetInode(linOth);
                Y_UnlinkIno(linOth, piOth);
            }
            vDir.Remove(pszName, DirPolicy::kAny);
            break;
        }
        case RENAME_NOREPLACE: {
            auto [lin, uMode] = vDir.Lookup(pszName, DirPolicy::kAny);
            vNewDir.Insert(pszNewName, lin, uMode, DirPolicy::kNone);
            vDir.Remove(pszName, DirPolicy::kAny);
            break;
        }
        case RENAME_EXCHANGE: {
            auto [lin, uMode] = vDir.Lookup(pszName, DirPolicy::kAny);
            auto [linOth, uOthMode] = vNewDir.Insert(pszNewName, lin, uMode, DirPolicy::kAny);
            if (linOth)
                vDir.Insert(pszName, lin, uOthMode, DirPolicy::kAny);
            else
                vDir.Remove(pszName, DirPolicy::kAny);
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

void Xxfs::Link(uint32_t lin, uint32_t linNewPar, const char *pszNewName) {
    if (strlen(pszNewName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto pi = X_GetInode(lin);
    if (pi->IsDir())
        throw Exception {EISDIR};
    auto piNewPar = X_GetInode(linNewPar);
    if (!piNewPar->IsDir())
        throw Exception {ENOTDIR};
    OpenedDir vDir(this, piNewPar, linNewPar);
    vDir.Insert(pszNewName, lin, pi->uMode, DirPolicy::kNone);
    ++pi->cLink;
}

void Xxfs::Truncate(uint32_t lin, off_t cbNewSize) {
    auto pi = X_GetInode(lin);
    if (pi->IsDir())
        throw Exception {EISDIR};
    if ((size_t) cbNewSize>= kcbMaxSize)
        throw Exception {EINVAL};
    pi->cbSize = (uint64_t) cbNewSize;
    Y_FileShrink(pi);
}

OpenedFile *Xxfs::Open(uint32_t lin, fuse_file_info *pInfo) {
    if (pInfo->flags & O_DIRECTORY)
        throw Exception {ENOTSUP};
    auto pi = X_GetInode(lin);
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
    return new OpenedFile(this, pi, lin, bWrite, bAppend, bDirect);
}

uint64_t Xxfs::Read(OpenedFile *pFile, void *pBuf, uint64_t cbSize, uint64_t cbOff) {
    if (!cbSize)
        return 0;
    if (cbOff >= pFile->pi->cbSize)
        return 0;
    cbSize = std::min(pFile->pi->cbSize - cbOff, cbSize);
    pFile->DoRead(pBuf, cbSize, cbOff);
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
    auto pi = X_GetInode(lin);
    if (!pi->IsDir())
        throw Exception {ENOTDIR};
    return new OpenedDir(this, pi, lin);
}

void Xxfs::ReadDir(OpenedDir *pDir, void *pBuf, fuse_fill_dir_t fnFill, off_t vOff) {
    auto vNextOff = pDir->IterSeek(vOff);
    while (vNextOff != OpenedDir::kItEnd) {
        FileStat vStat;
        auto pszName = pDir->IterGet(vStat);
        char szName[kcePerClu];
        strcpy(szName, pszName);
        vNextOff = pDir->IterNext();
        if (fnFill(pBuf, szName, &vStat, vNextOff, {}))
            break;
    }
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

OpenedFile *Xxfs::Create(uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = X_GetInode(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    auto lin = Y_AllocIno();
    auto pi = X_GetInode(lin);
    memset(pi, 0, sizeof(Inode));
    pi->uMode = S_IFREG | 0777;
    pi->cLink = 1;
    try {
        OpenedDir vDir(this, piPar, linPar);
        vDir.Insert(pszName, lin, pi->uMode, DirPolicy::kNone);
        return new OpenedFile(this, pi, lin, true, false, false);
    }
    catch (...) {
        Y_UnlinkIno(lin, pi);
        throw;
    }
}

uint32_t Xxfs::AvailClu() const noexcept {
    return x_spcMeta->ccTotal - x_spcMeta->ccUsed;
}

inline Inode *Xxfs::X_GetInode(uint32_t lin) noexcept {
    auto vin = lin % kciPerClu;
    auto vcn = lin / kciPerClu;
    return &x_vInoCluCache.At<InodeCluster>(x_spcMeta->lcnIno + vcn)->aInos[vin];
}

ShrPtr<InodeCluster> Xxfs::Y_MapInoClu(uint32_t vcn) noexcept {
    return x_vCluCache.At<InodeCluster>(x_spcMeta->lcnIno + vcn);
}

inline uint32_t Xxfs::Y_AllocIno() {
    auto lin = x_vInoAlloc.Alloc();
    ++x_spcMeta->ciUsed;
    return lin;
}

void Xxfs::Y_UnlinkIno(uint32_t lin, Inode *pi) noexcept {
    if (--pi->cLink)
        return;
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

}
