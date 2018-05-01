#include "Common.hpp"

#include "Raii.hpp"
#include "Xxfs.hpp"

namespace xxfs { namespace {

bool f_bVerbose = false;

inline Xxfs *GetXxfs(fuse_req_t pReq) {
    return (Xxfs *) fuse_req_userdata(pReq);
}

constexpr uint32_t GetLin(fuse_ino_t ino) {
    return (uint32_t) (ino - FUSE_ROOT_ID);
}

constexpr fuse_ino_t GetFino(uint32_t ino) {
    return (fuse_ino_t) ino + FUSE_ROOT_ID;
}

void XxfsLookup(fuse_req_t pReq, fuse_ino_t inoPar, const char *pszName) {
    if (f_bVerbose)
        printf("lookup: inoPar = %" PRIu32 ", pszName = %s\n", GetLin(inoPar), pszName);
    try {
        auto px = GetXxfs(pReq);
        fuse_entry_param vEntry {};
        vEntry.ino = GetFino(px->Lookup(vEntry.attr, GetLin(inoPar), pszName));
        vEntry.entry_timeout = std::numeric_limits<double>::infinity();
        vEntry.attr_timeout = std::numeric_limits<double>::infinity();
        fuse_reply_entry(pReq, &vEntry);
    }
    catch (Exception &e) {
        fprintf(stderr, "lookup: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsForget(fuse_req_t pReq, fuse_ino_t ino, uint64_t cLookup) {
    if (f_bVerbose)
        printf("forget: ino = %" PRIu32 ", cLookup = %" PRIu64 "\n", GetLin(ino), cLookup);
    try {
        auto px = GetXxfs(pReq);
        px->Forget(GetLin(ino), cLookup);
        fuse_reply_none(pReq);
    }
    catch (Exception &e) {
        fprintf(stderr, "forget: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_none(pReq);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsGetAttr(fuse_req_t pReq, fuse_ino_t ino, fuse_file_info *) {
    if (f_bVerbose)
        printf("getattr: ino = %" PRIu32 "\n", GetLin(ino));
    try {
        auto px = GetXxfs(pReq);
        FileStat vStat {};
        px->GetAttr(vStat, GetLin(ino));
        fuse_reply_attr(pReq, &vStat, std::numeric_limits<double>::infinity());
    }
    catch (Exception &e) {
        fprintf(stderr, "getattr: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsSetAttr(fuse_req_t pReq, fuse_ino_t ino, FileStat *pStat, int nFlags, fuse_file_info *pInfo) {
    if (f_bVerbose)
        printf("setattr: ino = %" PRIu32 "\n", GetLin(ino));
    // ignore   FUSE_SET_ATTR_MODE
    // ignore   FUSE_SET_ATTR_UID
    // ignore   FUSE_SET_ATTR_GID
    // truncate FUSE_SET_ATTR_SIZE
    // ignore   FUSE_SET_ATTR_ATIME
    // ingore   FUSE_SET_ATTR_MTIME
    // ignore   FUSE_SET_ATTR_ATIME_NOW
    // ignore   FUSE_SET_ATTR_MTIME_NOW
    // ignore   FUSE_SET_ATTR_CTIME
    try {
        auto px = GetXxfs(pReq);
        FileStat vStat {};
        px->SetAttr(vStat, pStat, GetLin(ino), nFlags, pInfo);
        fuse_reply_attr(pReq, &vStat, std::numeric_limits<double>::infinity());
    }
    catch (Exception &e) {
        fprintf(stderr, "setattr: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsReadLink(fuse_req_t pReq, fuse_ino_t ino) {
    if (f_bVerbose)
        printf("readlink: ino = %" PRIu32 "\n", GetLin(ino));
    try {
        auto px = GetXxfs(pReq);
        auto pszLink = px->ReadLink(GetLin(ino));
        fuse_reply_readlink(pReq, pszLink);
    }
    catch (Exception &e) {
        fprintf(stderr, "readlink: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsMkDir(fuse_req_t pReq, fuse_ino_t inoPar, const char *pszName, mode_t) {
    if (f_bVerbose)
        printf("mkdir: inoPar = %" PRIu32 ", pszName = %s\n", GetLin(inoPar), pszName);
    // mode is ignored
    try {
        auto px = GetXxfs(pReq);
        fuse_entry_param vEntry {};
        vEntry.ino = GetFino(px->MkDir(vEntry.attr, GetLin(inoPar), pszName));
        vEntry.entry_timeout = std::numeric_limits<double>::infinity();
        vEntry.attr_timeout = std::numeric_limits<double>::infinity();
        fuse_reply_entry(pReq, &vEntry);
    }
    catch (Exception &e) {
        fprintf(stderr, "mkdir: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsUnlink(fuse_req_t pReq, fuse_ino_t inoPar, const char *pszName) {
    if (f_bVerbose)
        printf("unlink: inoPar = %" PRIu32 ", pszName = %s\n", GetLin(inoPar), pszName);
    try {
        auto px = GetXxfs(pReq);
        px->Unlink(GetLin(inoPar), pszName);
        fuse_reply_err(pReq, 0);
    }
    catch (Exception &e) {
        fprintf(stderr, "unlink: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsRmDir(fuse_req_t pReq, fuse_ino_t inoPar, const char *pszName) {
    if (f_bVerbose)
        printf("rmdir: inoPar = %" PRIu32 ", pszName = %s\n", GetLin(inoPar), pszName);
    try {
        auto px = GetXxfs(pReq);
        px->RmDir(GetLin(inoPar), pszName);
        fuse_reply_err(pReq, 0);
    }
    catch (Exception &e) {
        fprintf(stderr, "rmdir: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsSymLink(fuse_req_t pReq, const char *pszLink, fuse_ino_t inoPar, const char *pszName) {
    if (f_bVerbose)
        printf("symlink: pszLink = %s, inoPar = %" PRIu32 ", pszName = %s\n", pszLink, GetLin(inoPar), pszName);
    try {
        auto px = GetXxfs(pReq);
        fuse_entry_param vEntry {};
        vEntry.ino = GetFino(px->SymLink(vEntry.attr, pszLink, GetLin(inoPar), pszName));
        vEntry.entry_timeout = std::numeric_limits<double>::infinity();
        vEntry.attr_timeout = std::numeric_limits<double>::infinity();
        fuse_reply_entry(pReq, &vEntry);
    }
    catch (Exception &e) {
        fprintf(stderr, "symlink: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsRename(
    fuse_req_t pReq,
    fuse_ino_t inoPar, const char *pszName,
    fuse_ino_t inoNewPar, const char *pszNewName,
    unsigned uFlags
) {
    if (f_bVerbose) {
        printf("rename: inoPar = %" PRIu32 ", pszName = %s\n", GetLin(inoPar), pszName);
        printf("        inoNewPar = %" PRIu32 ", pszNewName = %s\n", GetLin(inoNewPar), pszNewName);
        printf("        uFlags = %u\n", uFlags);
    }
    try {
        auto px = GetXxfs(pReq);
        px->Rename(GetLin(inoPar), pszName, GetLin(inoNewPar), pszNewName, uFlags);
        fuse_reply_err(pReq, 0);
    }
    catch (Exception &e) {
        fprintf(stderr, "rename: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsLink(fuse_req_t pReq, fuse_ino_t ino, fuse_ino_t inoNewPar, const char *pszNewName) {
    if (f_bVerbose) {
        printf("link: ino = %" PRIu32 ", inoNewPar = %" PRIu32 "\n", GetLin(ino), GetLin(inoNewPar));
        printf("      pszNewName = %s\n", pszNewName);
    }
    try {
        auto px = GetXxfs(pReq);
        fuse_entry_param vEntry {};
        px->Link(vEntry.attr, GetLin(ino), GetLin(inoNewPar), pszNewName);
        vEntry.ino = ino;
        vEntry.entry_timeout = std::numeric_limits<double>::infinity();
        vEntry.attr_timeout = std::numeric_limits<double>::infinity();
        fuse_reply_entry(pReq, &vEntry);
    }
    catch (Exception &e) {
        fprintf(stderr, "link: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsOpen(fuse_req_t pReq, fuse_ino_t ino, fuse_file_info *pInfo) {
    if (f_bVerbose)
        printf("open: ino = %" PRIu32 "\n", GetLin(ino));
    try {
        auto px = GetXxfs(pReq);
        pInfo->fh = (uint64_t) (uintptr_t) px->Open(GetLin(ino), pInfo);
        fuse_reply_open(pReq, pInfo);
    }
    catch (Exception &e) {
        fprintf(stderr, "open: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsRead(fuse_req_t pReq, fuse_ino_t ino, size_t cbSize, off_t cbOff, fuse_file_info *pInfo) {
    if (f_bVerbose)
        printf("read: ino = %" PRIu32 ", cbSize = %zu, cbOff = %zd\n", GetLin(ino), cbSize, cbOff);
    try {
        auto px = GetXxfs(pReq);
        std::unique_ptr<char []> upBuf;
        auto cbRes = px->Read((OpenedFile *) (uintptr_t) pInfo->fh, upBuf, (uint64_t) cbSize, (uint64_t) cbOff);
        fuse_reply_buf(pReq, upBuf.get(), (size_t) cbRes);
    }
    catch (Exception &e) {
        fprintf(stderr, "read: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsWrite(fuse_req_t pReq, fuse_ino_t ino, const char *pBuf, size_t cbSize, off_t cbOff, fuse_file_info *pInfo) {
    if (f_bVerbose)
        printf("write: ino = %" PRIu32 ", cbSize = %zu, cbOff = %zd\n", GetLin(ino), cbSize, cbOff);
    try {
        auto px = GetXxfs(pReq);
        auto cbRes = px->Write(
            (OpenedFile *) (uintptr_t) pInfo->fh, pBuf,
            (uint64_t) cbSize, (uint64_t) cbOff
        );
        fuse_reply_write(pReq, (size_t) cbRes);
    }
    catch (Exception &e) {
        fprintf(stderr, "write: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsRelease(fuse_req_t pReq, fuse_ino_t ino, fuse_file_info *pInfo) {
    if (f_bVerbose)
        printf("release: ino = %" PRIu32 "\n", GetLin(ino));
    try {
        auto px = GetXxfs(pReq);
        px->Release((OpenedFile *) (uintptr_t) pInfo->fh);
        fuse_reply_err(pReq, 0);
    }
    catch (Exception &e) {
        fprintf(stderr, "release: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsFSync(fuse_req_t pReq, fuse_ino_t ino, int, fuse_file_info *pInfo) {
    if (f_bVerbose)
        printf("fsync: ino = %" PRIu32 "\n", GetLin(ino));
    try {
        auto px = GetXxfs(pReq);
        px->FSync((OpenedFile *) (uintptr_t) pInfo->fh);
        fuse_reply_err(pReq, 0);
    }
    catch (Exception &e) {
        fprintf(stderr, "fsync: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsOpenDir(fuse_req_t pReq, fuse_ino_t ino, fuse_file_info *pInfo) {
    //if (f_bVerbose)
    //    printf("opendir: ino = %" PRIu32 "\n", GetLin(ino));
    try {
        auto px = GetXxfs(pReq);
        pInfo->fh = (uint64_t) (uintptr_t) px->OpenDir(GetLin(ino));
        fuse_reply_open(pReq, pInfo);
    }
    catch (Exception &e) {
        fprintf(stderr, "opendir: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsReadDir(fuse_req_t pReq, fuse_ino_t ino, size_t cbSize, off_t vOff, fuse_file_info *pInfo) {
    //if (f_bVerbose)
    //    printf("readdir: ino = %" PRIu32 ", cbSize = %zu, vOff = %zd\n", GetLin(ino), cbSize, vOff);
    try {
        auto px = GetXxfs(pReq);
        auto upBuf = std::make_unique<char[]>(cbSize);
        auto cbRes = px->ReadDir((OpenedDir *) (uintptr_t) pInfo->fh, pReq, upBuf.get(), cbSize, vOff);
        fuse_reply_buf(pReq, upBuf.get(), cbRes);
    }
    catch (Exception &e) {
        fprintf(stderr, "readdir: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsReleaseDir(fuse_req_t pReq, fuse_ino_t ino, fuse_file_info *pInfo) {
    //if (f_bVerbose)
    //    printf("releasedir: ino = %" PRIu32 "\n", GetLin(ino));
    try {
        auto px = GetXxfs(pReq);
        px->ReleaseDir((OpenedDir *) (uintptr_t) pInfo->fh);
        fuse_reply_err(pReq, 0);
    }
    catch (Exception &e) {
        fprintf(stderr, "releasedir: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsStatFs(fuse_req_t pReq, fuse_ino_t ino) {
    if (f_bVerbose)
        printf("statfs: ino = %" PRIu32 "\n", GetLin(ino));
    try {
        auto px = GetXxfs(pReq);
        VfsStat vStat {};
        px->StatFs(vStat);
        fuse_reply_statfs(pReq, &vStat);
    }
    catch (Exception &e) {
        fprintf(stderr, "statfs: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsCreate(fuse_req_t pReq, fuse_ino_t inoPar, const char *pszName, mode_t, fuse_file_info *pInfo) {
    if (f_bVerbose)
        printf("create: inoPar = %" PRIu32 ", pszName = %s\n", GetLin(inoPar), pszName);
    // mode is ignored
    try {
        auto px = GetXxfs(pReq);
        fuse_entry_param vEntry {};
        auto [lin, pFile] = px->Create(vEntry.attr, GetLin(inoPar), pszName);
        vEntry.ino = lin;
        vEntry.entry_timeout = std::numeric_limits<double>::infinity();
        vEntry.attr_timeout = std::numeric_limits<double>::infinity();
        pInfo->fh = (uint64_t) (uintptr_t) pFile;
        pInfo->direct_io = false;
        pInfo->keep_cache = false;
        fuse_reply_create(pReq, &vEntry, pInfo);
    }
    catch (Exception &e) {
        fprintf(stderr, "create: [%d] %s\n", e.nErrno, strerror(e.nErrno));
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

constexpr static fuse_lowlevel_ops XxfsOps() {
    fuse_lowlevel_ops vOps {};
    //  .init
    //  .destroy
    vOps.lookup = &XxfsLookup;
    vOps.forget = &XxfsForget;
    vOps.getattr = &XxfsGetAttr;
    vOps.setattr = &XxfsSetAttr;
    vOps.readlink = &XxfsReadLink;
    //  .mknod
    vOps.mkdir = &XxfsMkDir;
    vOps.unlink = &XxfsUnlink;
    vOps.rmdir = &XxfsRmDir;
    vOps.symlink = &XxfsSymLink;
    vOps.rename = &XxfsRename;
    vOps.link = &XxfsLink;
    vOps.open = &XxfsOpen;
    vOps.read = &XxfsRead;
    vOps.write = &XxfsWrite;
    //  .flush
    vOps.release = &XxfsRelease;
    vOps.fsync = &XxfsFSync;
    vOps.opendir = &XxfsOpenDir;
    vOps.readdir = &XxfsReadDir;
    vOps.releasedir = &XxfsReleaseDir;
    //  .fsyncdir
    vOps.statfs = &XxfsStatFs;
    //  .setxattr
    //  .getxattr
    //  .listxattr
    //  .removexattr
    //  .access
    vOps.create = &XxfsCreate;
    //  .getlk
    //  .setlk
    //  .bmap
    //  .ioctl
    //  .poll
    //  .write_buf
    //  .retrieve_reply
    //  .forget_multi
    //  .flock
    //  .fallocate
    //  .readdirplus
    return vOps;
}

constexpr fuse_opt f_aXxfsOpts[] {
    {"--file=%s", 0, 0},
    FUSE_OPT_END
};

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
    // if (f_bVerbose)
    //     fuse_opt_add_arg(&vArgs, "-d");
    auto pSession = fuse_session_new(&vArgs, &vOps, sizeof(vOps), &vXxfs);
    if (!pSession)
        return -1;
    RAII_GUARD(pSession, &fuse_session_destroy);
    if (fuse_set_signal_handlers(pSession))
        return -1;
    RAII_GUARD(pSession, &fuse_remove_signal_handlers);
    if (fuse_session_mount(pSession, pszMountPoint))
        return -1;
    RAII_GUARD(pSession, &fuse_session_unmount);
    // vFuseOpts.foreground is ignored
    fuse_daemonize(1);
    // vFuseOpts.singlethread is ignored
    return fuse_session_loop(pSession);
}
