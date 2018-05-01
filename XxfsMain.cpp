#include "Common.hpp"

#include "Raii.hpp"
#include "Xxfs.hpp"

namespace xxfs { namespace {

bool f_bVerbose = false;

inline Xxfs *GetXxfs() noexcept {
    return (Xxfs *) fuse_get_context()->private_data;
}

inline void PutHandle(fuse_file_info *pInfo, OpenedFile *pFile) {
    pInfo->fh = (uint64_t) (uintptr_t) pFile;
}

inline OpenedFile *GetFile(fuse_file_info *pInfo) {
    if (!pInfo)
        return nullptr;
    return (OpenedFile *) pInfo->fh;
}

inline OpenedFile *GetNdir(fuse_file_info *pInfo) {
    if (!pInfo)
        return nullptr;
    auto pFile = (OpenedFile *) pInfo->fh;
    if (pFile->pi->IsDir())
        throw Exception {EISDIR};
    return pFile;
}

constexpr OpenedDir *GetDir(fuse_file_info *pInfo) {
    if (!pInfo)
        return nullptr;
    auto pFile = (OpenedFile *) pInfo->fh;
    if (!pFile->pi->IsDir())
        throw Exception {ENOTDIR};
    return static_cast<OpenedDir *>(pFile);
}

int XxfsGetAttr(const char *pszPath, FileStat *pStat, fuse_file_info *pInfo) {
    try {
        auto pFile = GetFile(pInfo);
        if (pFile) {
            FillStat(*pStat, pFile->lin, pFile->pi);
            return 0;
        }
        auto px = GetXxfs();
        auto lin = px->LinAt(pszPath);
        px->GetAttr(*pStat, lin);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsReadLink(const char *pszPath, char *pBuf, size_t cbSize) {
    try {
        auto px = GetXxfs();
        auto lin = px->LinAt(pszPath);
        px->ReadLink(lin, pBuf, cbSize);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsMkDir(const char *pszPath, mode_t) {
    try {
        auto px = GetXxfs();
        auto linPar = px->LinPar(pszPath);
        px->MkDir(linPar, pszPath);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsUnlink(const char *pszPath) {
    try {
        auto px = GetXxfs();
        auto linPar = px->LinPar(pszPath);
        px->Unlink(linPar, pszPath);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsRmDir(const char *pszPath) {
    try {
        auto px = GetXxfs();
        auto linPar = px->LinPar(pszPath);
        px->RmDir(linPar, pszPath);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsSymLink(const char *pszLink, const char *pszPath) {
    try {
        auto px = GetXxfs();
        auto linPar = px->LinPar(pszPath);
        px->SymLink(pszLink, linPar, pszPath);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsRename(const char *pszPath, const char *pszNewPath, unsigned uFlags) {
    try {
        auto px = GetXxfs();
        auto linPar = px->LinPar(pszPath);
        auto linNewPar = px->LinPar(pszNewPath);
        px->Rename(linPar, pszPath, linNewPar, pszNewPath, uFlags);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsLink(const char *pszPath, const char *pszNewPath) {
    try {
        auto px = GetXxfs();
        auto lin = px->LinAt(pszPath);
        auto linNewPar = px->LinPar(pszNewPath);
        px->Link(lin, linNewPar, pszNewPath);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

// TODO: move part of the code into Xxfs
int XxfsTruncate(const char *pszPath, off_t cbNewSize, fuse_file_info *pInfo) {
    try {
        auto pFile = GetNdir(pInfo);
        if (pFile) {
            if ((size_t) cbNewSize >= kcbMaxSize)
                throw Exception {EINVAL};
            pFile->pi->cbSize = (uint64_t) cbNewSize;
            return 0;
        }
        auto px = GetXxfs();
        auto lin = px->LinAt(pszPath);
        px->Truncate(lin, cbNewSize);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsOpen(const char *pszPath, fuse_file_info *pInfo) {
    try {
        auto px = GetXxfs();
        auto lin = px->LinAt(pszPath);
        PutHandle(pInfo, px->Open(lin, pInfo));
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsRead(const char *, char *pBuf, size_t cbSize, off_t cbOff, fuse_file_info *pInfo) {
    try {
        auto px = GetXxfs();
        return (int) px->Read(GetNdir(pInfo), pBuf, (uint64_t) cbSize, (uint64_t) cbOff);
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsWrite(const char *, const char *pBuf, size_t cbSize, off_t cbOff, fuse_file_info *pInfo) {
    try {
        auto px = GetXxfs();
        return (int) px->Write(GetNdir(pInfo), pBuf, (uint64_t) cbSize, (uint64_t) cbOff);
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsStatFs(const char *, VfsStat *pStat) {
    try {
        auto px = GetXxfs();
        px->StatFs(*pStat);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsRelease(const char *, fuse_file_info *pInfo) {
    try {
        auto px = GetXxfs();
        px->Release(GetNdir(pInfo));
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsFSync(const char *, int, fuse_file_info *pInfo) {
    try {
        auto px = GetXxfs();
        px->FSync(GetNdir(pInfo));
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsOpenDir(const char *pszPath, fuse_file_info *pInfo) {
    try {
        auto px = GetXxfs();
        auto lin = px->LinAt(pszPath);
        PutHandle(pInfo, px->OpenDir(lin));
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsReadDir(
    const char *, void *pBuf, fuse_fill_dir_t fnFill,
    off_t vOff, fuse_file_info *pInfo, fuse_readdir_flags
) {
    try {
        auto px = GetXxfs();
        px->ReadDir(GetDir(pInfo), pBuf, fnFill, vOff);
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsReleaseDir(const char *, fuse_file_info *pInfo) {
    try {
        auto px = GetXxfs();
        px->ReleaseDir(GetDir(pInfo));
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

int XxfsCreate(const char *pszPath, mode_t, fuse_file_info *pInfo) {
    try {
        auto px = GetXxfs();
        auto linPar = px->LinPar(pszPath);
        PutHandle(pInfo, px->Create(linPar, pszPath));
        return 0;
    }
    catch (Exception &e) {
        fprintf(stderr, "%s failed: [%d] %s\n", __func__, e.nErrno, strerror(e.nErrno));
        return -e.nErrno;
    }
    catch (FatalException &e) {
        fprintf(stderr, "%s failed: ", __func__);
        e.ShowWhat(stderr);
        exit(-1);
    }
}

constexpr static fuse_operations XxfsOps() {
    fuse_operations vOps {};
    vOps.getattr = &XxfsGetAttr;
    vOps.readlink = &XxfsReadLink;
    //  .mknod
    vOps.mkdir = &XxfsMkDir;
    vOps.unlink = &XxfsUnlink;
    vOps.rmdir = &XxfsRmDir;
    vOps.symlink = &XxfsSymLink;
    vOps.rename = &XxfsRename;
    vOps.link = &XxfsLink;
    //  .chmod
    //  .chown
    vOps.truncate = &XxfsTruncate;
    vOps.open = &XxfsOpen;
    vOps.read = &XxfsRead;
    vOps.write = &XxfsWrite;
    vOps.statfs = &XxfsStatFs;
    //  .flush
    vOps.release = &XxfsRelease;
    vOps.fsync = &XxfsFSync;
    //  .setxattr
    //  .getxattr
    //  .listxattr
    //  .removexattr
    vOps.opendir = &XxfsOpenDir;
    vOps.readdir = &XxfsReadDir;
    vOps.releasedir = &XxfsReleaseDir;
    //  .fsyncdir
    //  .init
    //  .destroy
    //  .access
    vOps.create = &XxfsCreate;
    //  .lock
    //  .utimens
    //  .bmap
    //  .ioctl
    //  .poll
    //  .write_buf
    //  .read_buf
    //  .flock
    //  .fallocate
    return vOps;
}

void ShowHelp(const char *pszExec) {
    printf(
        "\n"
        "Usage: %s [-v] filepath mountpoint\n"
        "\n"
        "Options:\n"
        "    -v       enable verbose mode\n"
        "    -h       print this help\n",
        pszExec
    );
}

}}

int main(int ncArg, char *ppszArgs[]) {
    using namespace xxfs;
    const char *pszPath = nullptr;
    const char *pszMountPoint = nullptr;
    bool bHelp = false;
    bool bIncorrect = false;
    int chOpt;
    while ((chOpt = getopt(ncArg, ppszArgs, ":hv")) != -1) {
        switch (chOpt) {
        case 'v':
            f_bVerbose = true;
            break;
        case 'h':
            bHelp = true;
            break;
        default:
            bIncorrect = true;
            break;
        }
    }
    if (optind < ncArg)
        pszPath = ppszArgs[optind++];
    if (optind < ncArg)
        pszMountPoint = ppszArgs[optind++];
    if (bIncorrect) {
        fprintf(stderr, "Incorrect argument.\n");
        ShowHelp(ppszArgs[0]);
        return -1;
    }
    if (bHelp) {
        ShowHelp(ppszArgs[0]);
        return 0;
    }
    if (!pszPath) {
        fprintf(stderr, "No file specified.\n");
        ShowHelp(ppszArgs[0]);
        return -1;
    }
    if (!pszMountPoint) {
        fprintf(stderr, "No mount point specified.\n");
        ShowHelp(ppszArgs[0]);
        return -1;
    }
    auto fd = open(pszPath, O_RDWR | O_DIRECT);
    if (fd == -1) {
        fprintf(stderr, "Failed to open file: %s.\n", pszPath);
        return -1;
    }
    RaiiFile vRf(fd);
    FileStat vStat;
    if (fstat(fd, &vStat)) {
        fprintf(stderr, "Failed to get the file\'s attributes.\n");
        return -1;
    }
    MetaCluster cluMeta;
    if (FillMeta(&cluMeta, (size_t) vStat.st_size) != MetaResult::kSuccess) {
        fprintf(stderr, "The filesystem is corrupt.\n");
        return -1;
    }
    ShrPtr<MetaCluster> spcMeta;
    try {
        spcMeta = ShrMap<MetaCluster>(fd, 0);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        return -1;
    }
    if (memcmp(&cluMeta, spcMeta.get(), kcbMetaStatic)) {
        fprintf(stderr, "The filesystem is corrupt.\n");
        return -1;
    }
    std::aligned_storage_t<sizeof(Xxfs)> vXxfs;
    try {
        ::new(&vXxfs) Xxfs {std::move(vRf), std::move(spcMeta)};
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        return -1;
    }
    constexpr auto vOps = XxfsOps();
    fuse_args vArgs {};
    fuse_opt_add_arg(&vArgs, ppszArgs[0]);
    fuse_opt_add_arg(&vArgs, "-s");
    fuse_opt_add_arg(&vArgs, "-f");
    fuse_opt_add_arg(&vArgs, pszMountPoint);
    if (f_bVerbose)
        fuse_opt_add_arg(&vArgs, "-d");
    return fuse_main(vArgs.argc, vArgs.argv, &vOps, &vXxfs);
}
