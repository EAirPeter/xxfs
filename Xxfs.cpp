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
    auto [lin, uMode] = vParDir.Lookup(pszName, DirPolicy::kAny);
    (void) uMode;
    auto pi = x_vInoCache.IncLookup(linPar);
    FillStat(vStat, lin, pi);
    return lin;
}

void Xxfs::Forget(uint32_t lin, uint64_t cLookup) {
    x_vInoCache.DecLookup(lin, cLookup);
}

void Xxfs::GetAttr(FileStat &vStat, uint32_t lin) {
    auto pi = x_vInoCache.At(lin);
    FillStat(vStat, lin, pi);
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
    FillStat(vStat, lin, pi);
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
    pi->uMode = S_IFDIR | 0777;
    pi->cLink = 1;
    try {
        try {
            OpenedDir vDir(this, piPar);
            vDir.Insert(pszName, lin, pi->uMode, DirPolicy::kNone);
        }
        catch (...) {
            x_vInoCache.DecLookup(lin);
            throw;
        }
        FillStat(vStat, lin, pi);
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
        auto [lin, uMode] = vDir.Remove(pszName, DirPolicy::kNotDir);
        (void) uMode;
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
        auto [lin, uMode] = vDir.Lookup(pszName, DirPolicy::kDir);
        (void) uMode;
        auto pi = x_vInoCache.IncLookup(lin);
        if (pi->ccSize) {
            auto spc = Y_Map<DirCluster>(pi->lcnIdx0[0]);
            if (spc->aEnts[0].linFile != 1)
                throw Exception {ENOTEMPTY};
        }
        vDir.Remove(pszName, DirPolicy::kDir);
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
    pi->uMode = S_IFLNK | 0777;
    pi->cLink = 1;
    try {
        {
            OpenedFile vFile(this, pi, true, false, false);
            vFile.DoWrite(pszLink, (uint64_t) cbLength + 1, 0);
        }
        try {
            OpenedDir vDir(this, piPar);
            vDir.Insert(pszName, lin, pi->uMode, DirPolicy::kNone);
        }
        catch (...) {
            x_vInoCache.DecLookup(lin);
            throw;
        }
        FillStat(vStat, lin, pi);
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
        switch (uFlags) {
        case 0: {
            auto [lin, uMode] = vDir.Lookup(pszName, DirPolicy::kAny);
            auto [linOth, uOthMode] = vNewDir.Insert(pszNewName, lin, uMode, DirPolicy::kNotDir);
            (void) uOthMode;
            if (linOth) {
                auto piOth = x_vInoCache.IncLookup(linOth);
                --piOth->cLink;
                x_vInoCache.DecLookup(linOth);
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

void Xxfs::Link(FileStat &vStat, uint32_t lin, uint32_t linNewPar, const char *pszNewName) {
    if (strlen(pszNewName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto pi = x_vInoCache.At(lin);
    if (pi->IsDir())
        throw Exception {EISDIR};
    auto piNewPar = x_vInoCache.At(linNewPar);
    if (!piNewPar->IsDir())
        throw Exception {ENOTDIR};
    OpenedDir vDir(this, piNewPar);
    vDir.Insert(pszNewName, lin, pi->uMode, DirPolicy::kNone);
    x_vInoCache.IncLookup(lin);
    ++pi->cLink;
    FillStat(vStat, lin, pi);
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

std::pair<uint32_t, OpenedFile *> Xxfs::Create(FileStat &vStat, uint32_t linPar, const char *pszName) {
    if (strlen(pszName) >= kcePerClu)
        throw Exception {ENAMETOOLONG};
    auto piPar = x_vInoCache.At(linPar);
    if (!piPar->IsDir())
        throw Exception {ENOTDIR};
    auto lin = x_vInoAlloc.Alloc();
    auto pi = x_vInoCache.IncLookup(lin);
    memset(pi, 0, sizeof(Inode));
    pi->uMode = S_IFREG | 0777;
    pi->cLink = 1;
    try {
        try {
            OpenedDir vDir(this, piPar);
            vDir.Insert(pszName, lin, pi->uMode, DirPolicy::kNone);
        }
        catch (...) {
            x_vInoCache.DecLookup(lin);
            throw;
        }
        FillStat(vStat, lin, pi);
        return {lin, new OpenedFile(this, pi, true, false, false)};
    }
    catch (...) {
        Y_EraseIno(lin, pi);
        throw;
    }
}


uint32_t Xxfs::AvailClu() const noexcept {
    return x_spcMeta->ccTotal - x_spcMeta->ccUsed;
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

}
