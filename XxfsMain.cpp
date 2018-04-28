#include "Common.hpp"

#include "Raii.hpp"
#include "Xxfs.hpp"

namespace xxfs {

namespace {

inline Xxfs *GetXxfs(fuse_req_t pReq) {
    return (Xxfs *) fuse_req_userdata(pReq);
}

constexpr uint32_t GetLcn(fuse_ino_t ino) {
    return (uint32_t) (ino - FUSE_ROOT_ID);
}

constexpr fuse_ino_t GetFino(uint32_t ino) {
    return (fuse_ino_t) ino + FUSE_ROOT_ID;
}

}

void XxfsLookup(fuse_req_t pReq, fuse_ino_t inoPar, const char *pszName) {
    try {
        auto pXxfs = GetXxfs(pReq);
        fuse_entry_param vEntry {};
        vEntry.ino = GetFino(pXxfs->Lookup(vEntry.attr, GetLcn(inoPar), pszName));
        vEntry.entry_timeout = std::numeric_limits<double>::infinity();
        vEntry.attr_timeout = std::numeric_limits<double>::infinity();
        fuse_reply_entry(pReq, &vEntry);
    }
    catch (Exception &e) {
        fprintf(stderr, "lookup: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsForget(fuse_req_t pReq, fuse_ino_t ino, uint64_t cLookup) {
    try {
        auto pXxfs = GetXxfs(pReq);
        pXxfs->Forget(GetLcn(ino), cLookup);
        fuse_reply_none(pReq);
    }
    catch (Exception &e) {
        fprintf(stderr, "forget: %d\n", e.nErrno);
        fuse_reply_none(pReq);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsGetAttr(fuse_req_t pReq, fuse_ino_t ino, fuse_file_info *) {
    try {
        auto pXxfs = GetXxfs(pReq);
        struct stat vStat {};
        pXxfs->GetAttr(vStat, GetLcn(ino));
        fuse_reply_attr(pReq, &vStat, std::numeric_limits<double>::infinity());
    }
    catch (Exception &e) {
        fprintf(stderr, "getattr: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsReadLink(fuse_req_t pReq, fuse_ino_t ino) {
    try {
        auto pXxfs = GetXxfs(pReq);
        auto spRes = pXxfs->ReadLink(GetLcn(ino));
        fuse_reply_readlink(pReq, spRes.get());
    }
    catch (Exception &e) {
        fprintf(stderr, "readlink: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsMkDir(fuse_req_t pReq, fuse_ino_t inoPar, const char *pszName, mode_t) {
    // mode is ignored
    try {
        auto pXxfs = GetXxfs(pReq);
        fuse_entry_param vEntry {};
        vEntry.ino = GetFino(pXxfs->MkDir(vEntry.attr, GetLcn(inoPar), pszName));
        vEntry.entry_timeout = std::numeric_limits<double>::infinity();
        vEntry.attr_timeout = std::numeric_limits<double>::infinity();
        fuse_reply_entry(pReq, &vEntry);
    }
    catch (Exception &e) {
        fprintf(stderr, "mkdir: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsUnlink(fuse_req_t pReq, fuse_ino_t inoPar, const char *pszName) {
    try {
        auto pXxfs = GetXxfs(pReq);
        pXxfs->Unlink(GetLcn(inoPar), pszName);
        fuse_reply_err(pReq, 0);
    }
    catch (Exception &e) {
        fprintf(stderr, "unlink: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsRmDir(fuse_req_t pReq, fuse_ino_t inoPar, const char *pszName) {
    try {
        auto pXxfs = GetXxfs(pReq);
        pXxfs->RmDir(GetLcn(inoPar), pszName);
        fuse_reply_err(pReq, 0);
    }
    catch (Exception &e) {
        fprintf(stderr, "rmdir: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsSymLink(fuse_req_t pReq, const char *pszLink, fuse_ino_t inoPar, const char *pszName) {
    try {
        auto pXxfs = GetXxfs(pReq);
        fuse_entry_param vEntry {};
        vEntry.ino = GetFino(pXxfs->SymLink(vEntry.attr, pszLink, GetLcn(inoPar), pszName));
        vEntry.entry_timeout = std::numeric_limits<double>::infinity();
        vEntry.attr_timeout = std::numeric_limits<double>::infinity();
        fuse_reply_entry(pReq, &vEntry);
    }
    catch (Exception &e) {
        fprintf(stderr, "symlink: %d\n", e.nErrno);
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
    try {
        auto pXxfs = GetXxfs(pReq);
        pXxfs->Rename(GetLcn(inoPar), pszName, GetLcn(inoNewPar), pszNewName, uFlags);
        fuse_reply_err(pReq, 0);
    }
    catch (Exception &e) {
        fprintf(stderr, "rename: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsLink(fuse_req_t pReq, fuse_ino_t ino, fuse_ino_t inoNewPar, const char *pszNewName) {
    try {
        auto pXxfs = GetXxfs(pReq);
        fuse_entry_param vEntry {};
        pXxfs->Link(vEntry.attr, GetLcn(ino), GetLcn(inoNewPar), pszNewName);
        vEntry.ino = ino;
        vEntry.entry_timeout = std::numeric_limits<double>::infinity();
        vEntry.attr_timeout = std::numeric_limits<double>::infinity();
        fuse_reply_entry(pReq, &vEntry);
    }
    catch (Exception &e) {
        fprintf(stderr, "link: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsOpen(fuse_req_t pReq, fuse_ino_t ino, fuse_file_info *pInfo) {
    try {
        auto pXxfs = GetXxfs(pReq);
        pInfo->fh = (uint64_t) (uintptr_t) pXxfs->Open(GetLcn(ino), pInfo);
        fuse_reply_open(pReq, pInfo);
    }
    catch (Exception &e) {
        fprintf(stderr, "open: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsRead(fuse_req_t pReq, fuse_ino_t, size_t cbSize, off_t cbOff, fuse_file_info *pInfo) {
    try {
        auto pXxfs = GetXxfs(pReq);
        std::unique_ptr<char []> upBuf;
        auto cbRes = pXxfs->Read((OpenedFile *) (uintptr_t) pInfo->fh, upBuf, (uint64_t) cbSize, (uint64_t) cbOff);
        fuse_reply_buf(pReq, upBuf.get(), (size_t) cbRes);
    }
    catch (Exception &e) {
        fprintf(stderr, "read: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsWrite(fuse_req_t pReq, fuse_ino_t, const char *pBuf, size_t cbSize, off_t cbOff, fuse_file_info *pInfo) {
    try {
        auto pXxfs = GetXxfs(pReq);
        auto cbRes = pXxfs->Write(
            (OpenedFile *) (uintptr_t) pInfo->fh, pBuf,
            (uint64_t) cbSize, (uint64_t) cbOff
        );
        fuse_reply_write(pReq, (size_t) cbRes);
    }
    catch (Exception &e) {
        fprintf(stderr, "write: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsRelease(fuse_req_t pReq, fuse_ino_t, fuse_file_info *pInfo) {
    //try {
        auto pXxfs = GetXxfs(pReq);
        pXxfs->Release((OpenedFile *) (uintptr_t) pInfo->fh);
        fuse_reply_err(pReq, 0);
    /*}
    catch (Exception &e) {
        fprintf(stderr, "release: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }*/
}

void XxfsFSync(fuse_req_t pReq, fuse_ino_t, int, fuse_file_info *pInfo) {
    //try {
        auto pXxfs = GetXxfs(pReq);
        pXxfs->FSync((OpenedFile *) (uintptr_t) pInfo->fh);
        fuse_reply_err(pReq, 0);
    /*}
    catch (Exception &e) {
        fprintf(stderr, "fsync: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }*/
}

void XxfsOpenDir(fuse_req_t pReq, fuse_ino_t ino, fuse_file_info *pInfo) {
    try {
        auto pXxfs = GetXxfs(pReq);
        pInfo->fh = (uint64_t) (uintptr_t) pXxfs->OpenDir(GetLcn(ino));
        fuse_reply_open(pReq, pInfo);
    }
    catch (Exception &e) {
        fprintf(stderr, "opendir: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsReadDir(fuse_req_t pReq, fuse_ino_t, size_t cbSize, off_t vOff, fuse_file_info *pInfo) {
    try {
        auto pXxfs = GetXxfs(pReq);
        auto upBuf = std::make_unique<char[]>(cbSize);
        auto cbRes = pXxfs->ReadDir((OpenedDir *) (uintptr_t) pInfo->fh, pReq, upBuf.get(), cbSize, vOff);
        fuse_reply_buf(pReq, upBuf.get(), cbRes);
    }
    catch (Exception &e) {
        fprintf(stderr, "readdir: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsReleaseDir(fuse_req_t pReq, fuse_ino_t, fuse_file_info *pInfo) {
    try {
        auto pXxfs = GetXxfs(pReq);
        pXxfs->ReleaseDir((OpenedDir *) (uintptr_t) pInfo->fh);
        fuse_reply_err(pReq, 0);
    }
    catch (Exception &e) {
        fprintf(stderr, "releasedir: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsStatFs(fuse_req_t pReq, fuse_ino_t) {
    try {
        auto pXxfs = GetXxfs(pReq);
        struct statvfs vStat {};
        pXxfs->StatFs(vStat);
        fuse_reply_statfs(pReq, &vStat);
    }
    catch (Exception &e) {
        fprintf(stderr, "releasedir: %d\n", e.nErrno);
        fuse_reply_err(pReq, e.nErrno);
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        exit(-1);
    }
}

void XxfsCreate(fuse_req_t pReq, fuse_ino_t inoPar, const char *pszName, mode_t, fuse_file_info *pInfo) {
    // mode is ignored
    try {
        auto pXxfs = GetXxfs(pReq);
        fuse_entry_param vEntry {};
        pInfo->fh = (uint64_t) (uintptr_t) pXxfs->Create(vEntry, GetLcn(inoPar), pszName);
        pInfo->direct_io = false;
        pInfo->keep_cache = false;
        fuse_reply_entry(pReq, &vEntry);
    }
    catch (Exception &e) {
        fprintf(stderr, "releasedir: %d\n", e.nErrno);
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
    //  .setattr
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
    //  .flush
    vOps.release = &XxfsRelease;
    vOps.fsync = &XxfsFSync;
    vOps.opendir = &XxfsOpenDir;
    vOps.readdir = &XxfsReadDir;
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

constexpr static fuse_opt f_aXxfsOpts[] {
    {"--file=%s", 0, 0},
    FUSE_OPT_END
};

}

static void ShowHelp(const char *pszExec) {
    printf(
        "\n"
        "Usage: %s -x path mountpoint\n"
        "\n"
        "Options:\n"
        "    -x path          to a device holding XXFS\n"
        "    -h   --help      print help\n"
        "    -V   --version   print version\n",
        pszExec
    );
}

int main(int ncArg, char *ppszArgs[]) {
    using namespace xxfs;
    fuse_args vArgs = FUSE_ARGS_INIT(ncArg, ppszArgs);
    fuse_cmdline_opts vFuseOpts;
    if (fuse_parse_cmdline(&vArgs, &vFuseOpts)) {
        fprintf(stderr, "Incorrect argument.\n");
        ShowHelp(ppszArgs[0]);
        return -1;
    }
    RAII_GUARD(vFuseOpts.mountpoint, &free);
    RAII_GUARD(&vArgs, &fuse_opt_free_args);
    char *pszPath = nullptr;
    if (fuse_opt_parse(&vArgs, &pszPath, f_aXxfsOpts, nullptr)) {
        fprintf(stderr, "Incorrect argument.\n");
        ShowHelp(ppszArgs[0]);
        return -1;
    }
    if (vFuseOpts.show_help) {
        ShowHelp(ppszArgs[0]);
        return 0;
    }
    if (vFuseOpts.show_version) {
        fuse_lowlevel_version();
        return 0;
    }
    if (!pszPath) {
        fprintf(stderr, "No file specified.\n");
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
    auto pSession = fuse_session_new(&vArgs, &vOps, sizeof(vOps), &vXxfs);
    if (!pSession)
        return -1;
    RAII_GUARD(pSession, &fuse_session_destroy);
    if (fuse_set_signal_handlers(pSession))
        return -1;
    RAII_GUARD(pSession, &fuse_remove_signal_handlers);
    if (fuse_session_mount(pSession, vFuseOpts.mountpoint))
        return -1;
    RAII_GUARD(pSession, &fuse_session_unmount);
    // vFuseOpts.foreground is ignored
    fuse_daemonize(1);
    // vFuseOpts.singlethread is ignored
    return fuse_session_loop(pSession);
}
