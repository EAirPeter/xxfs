#include "Common.hpp"

namespace xxfs {

namespace {

template<class tObj>
constexpr static bool IsCluster = std::is_pod_v<tObj> && sizeof(tObj) == kcbCluSize;

static_assert(IsCluster<MetaCluster>);
static_assert(IsCluster<BitmapCluster>);
static_assert(IsCluster<IndexCluster>);
static_assert(IsCluster<InodeCluster>);
static_assert(IsCluster<DirCluster>);
static_assert(IsCluster<ByteCluster>);

static_assert(kcbCluSize >= PATH_MAX);
static_assert(kcePerClu >= NAME_MAX);
static_assert(sizeof(uint64_t) >= sizeof(uintptr_t));

}

void FatalException::ShowWhat(FILE *pFile) const noexcept {
    fprintf(
        pFile, "At %s:%ld [%s] [errno %d]: %s.\n",
        x_pszFile, x_nLine, x_pszFunc, x_nErrno, x_pszWhat
    );
}

void CheckPageSize() {
    auto ncbPageSize = sysconf(_SC_PAGESIZE);
    if (ncbPageSize != (long) kcbCluSize) {
        fprintf(
            stderr,
            "Warning: Actual page size (%ld B) does not equal to predefined page size (%ld B)",
            ncbPageSize,
            (long) kcbCluSize
        );
    }
}

MetaResult FillMeta(MetaCluster *pcMeta, size_t cbSize) {
    if (cbSize > kcbMaxSize)
        return MetaResult::kTooLarge;
    if (cbSize < kcbMinSize)
        return MetaResult::kTooSmall;
    if (cbSize % kcbCluSize)
        return MetaResult::kPartial;
    auto ccTotal = (uint32_t) (cbSize / kcbCluSize);
    auto cqCluBmp = (ccTotal + 63) / 64;
    auto ccCluBmp = (cqCluBmp + kcqPerClu - 1) / kcqPerClu;
    auto ciUpper = ccTotal - ccCluBmp;
    uint32_t ciTotal = 0;
    while (ciTotal + 1 < ciUpper) {
        auto ci = (ciTotal + ciUpper) / 2;
        auto cqInoBmp =  (ci + 63) / 64;
        auto ccInoBmp = (cqInoBmp + kcqPerClu - 1) / kcqPerClu;
        auto ccIno =  (ci + kciPerClu - 1) / kciPerClu;
        auto ccOther = 1 + ccCluBmp + ccInoBmp + ccIno;
        if (ccOther + ci - 1 <= ccTotal)
            ciTotal = ci;
        else
            ciUpper = ci;
    }
    ciTotal = (ciTotal + kciPerClu - 1) / kciPerClu * kciPerClu;
    auto cqInoBmp =  (ciTotal + 63) / 64;
    auto ccInoBmp = (cqInoBmp + kcqPerClu - 1) / kcqPerClu;
    auto ccIno =  (ciTotal + kciPerClu - 1) / kciPerClu;
    auto lcnCluBmp = 1;
    auto lcnInoBmp = ccCluBmp + lcnCluBmp;
    auto lcnIno = ccInoBmp + lcnInoBmp;
    auto ccUsed = ccIno + lcnIno;
    auto ciUsed = 1;
    pcMeta->uSign = kSign;
    pcMeta->cbSize = (uint64_t) cbSize;
    pcMeta->ccTotal = ccTotal;
    pcMeta->ciTotal = ciTotal;
    pcMeta->lcnCluBmp = lcnCluBmp;
    pcMeta->ccCluBmp = ccCluBmp;
    pcMeta->lcnInoBmp = lcnInoBmp;
    pcMeta->ccInoBmp = ccInoBmp;
    pcMeta->lcnIno = lcnIno;
    pcMeta->ccIno = ccIno;
    pcMeta->ccUsed = ccUsed;
    pcMeta->ciUsed = ciUsed;
    memset(pcMeta->aZeros, 0, sizeof(pcMeta->aZeros));
    return MetaResult::kSuccess;
}

void FillStat(FileStat &vStat, uint32_t lin, Inode *pi) noexcept {
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
