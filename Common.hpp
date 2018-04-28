#ifndef XXFS_COMMON_HPP_
#define XXFS_COMMON_HPP_

#define FUSE_USE_VERSION 31

#include <errno.h>
#include <fcntl.h>
#include <fuse_lowlevel.h>
#include <inttypes.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#define CONCAT_(a_, b_) a_ ## b_
#define CONCAT(a_, b_) CONCAT_(a_, b_)

namespace xxfs {

using FileStat = struct stat;
using VfsStat = struct statvfs;

template<class tObj>
using ShrPtr = std::shared_ptr<tObj>;

struct NoCopy {
    constexpr NoCopy() noexcept = default;
    constexpr NoCopy(const NoCopy &) noexcept = delete;
    constexpr NoCopy(NoCopy &&) noexcept = default;
    constexpr NoCopy &operator =(const NoCopy &) noexcept = delete;
    constexpr NoCopy &operator =(NoCopy &&) noexcept = default;
};

struct NoCopyMove : NoCopy {
    constexpr NoCopyMove() noexcept = default;
    constexpr NoCopyMove(NoCopyMove &&) noexcept = delete;
    constexpr NoCopyMove &operator =(NoCopyMove &&) noexcept = delete;
};

struct Exception {
    int nErrno;
};

class FatalException {
public:
    constexpr FatalException(
        const char *pszFunc,
        const char *pszFile, long nLine,
        const char *pszWhat = "Unknown", int nErrno = 0
    ) noexcept :
        x_pszFunc {pszFunc},
        x_pszFile {pszFile}, x_nLine {nLine},
        x_nErrno {nErrno}, x_pszWhat {pszWhat}
    {}
    void ShowWhat(FILE *pFile) const noexcept;

private:
    const char *x_pszFunc;
    const char *x_pszFile;
    long x_nLine;
    int x_nErrno;
    const char *x_pszWhat;

};

#define RAISE(...) throw FatalException {__PRETTY_FUNCTION__, __FILE__, __LINE__, __VA_ARGS__}

// clu => cluster
// nod => linde
// ent => dirent (directory entry)
// bit => bit
// qwd => qword
// by => byte
// lin => index of linde in filesystem
// vin => index of linde in linde cluster
// lcn => index of cluster in filesystem
// vcn => index of cluster in file/bitmap/inode-cluster-list
// len => index of dirent in directory file
// ven => index of dirent in cluster
// lqw => index of qword in bitmap
// vqw => index of qword in cluster
// lbi => index of bit in bitmap
// vbi => index of bit in qword
// lby => index of byte in file
// vby => index of byte in cluster
// cc => count of cluster
// ci => count of inode
// ce => count of dirent
// cq => count of qword
// cn => count of index number (dword)
// cb => count of byte
// cbi => count of bit

constexpr size_t kcbMaxSize = 1ULL << 44; // 16 TiB
constexpr size_t kcbMinSize = 6ULL << 12; // 24 KiB

constexpr uint32_t kSign = 0x0000000053465858; // "XXFS"
constexpr uint64_t kMagic0 = 0xfc6f4dfb3784ee9c;
constexpr uint64_t kMagic1 = 0xbf602ab60041f70c;
constexpr uint64_t kMagic2 = 0x612fcf459c80cfa2;

constexpr uint32_t kcbCluSize = 4096;

// Res: [MetaCluster] [ClusterBitmap] [InodeBitmap]

struct MetaCluster {
    // specified at creation
    uint64_t uSign;
    uint64_t cbSize;
    uint32_t ccTotal;
    uint32_t ciTotal;
    uint32_t lcnCluBmp;
    uint32_t ccCluBmp;
    uint32_t lcnInoBmp;
    uint32_t ccInoBmp;
    uint32_t lcnIno;
    uint32_t ccIno;
    // updated dynamically
    uint32_t ccUsed;
    uint32_t ciUsed;
    uint8_t aZeros[4040];
};

constexpr size_t kcbMetaStatic = offsetof(MetaCluster, ccUsed);

constexpr uint32_t kcqPerClu = kcbCluSize / sizeof(uint64_t);

struct BitmapCluster {
    uint64_t aBmp[kcqPerClu];
};

constexpr uint32_t kcnPerClu = kcbCluSize / sizeof(uint32_t);

struct IndexCluster {
    uint32_t aLcns[kcnPerClu];
};

constexpr uint32_t kccIdx0 = 8;
constexpr uint32_t kccIdx1 = kcnPerClu;
constexpr uint32_t kccIdx2 = kcnPerClu * kccIdx1;
constexpr uint32_t kvcnIdx1 = kccIdx0;
constexpr uint32_t kvcnIdx2 = kvcnIdx1 + kccIdx1;
constexpr uint32_t kvcnIdx3 = kvcnIdx2 + kccIdx2;

struct Inode {
    uint64_t cbSize;
    uint32_t ccSize;
    uint32_t uMode;
    uint32_t cLink;
    uint32_t lcnIdx0[kccIdx0];
    uint32_t lcnIdx1;
    uint32_t lcnIdx2;
    uint32_t lcnIdx3;

    constexpr bool IsDir() const noexcept {
        return S_ISDIR(uMode);
    }

    constexpr bool IsReg() const noexcept {
        return S_ISREG(uMode);
    }

    constexpr bool IsSym() const noexcept {
        return S_ISLNK(uMode);
    }

};

constexpr uint32_t kciPerClu = kcbCluSize / sizeof(Inode);

struct InodeCluster {
    Inode aInos[kciPerClu];
};

struct DirEnt{
    // inode number to the entry if not root
    // number of the first available entry if root
    // available entries are linked using linFile instead of lenNext
    uint32_t linFile;
    uint32_t lenNext;
    uint32_t lenChild;
    uint16_t uMode; // enough to hold perm and type
    uint8_t byKey;
    bool bExist;
};

constexpr uint32_t kcePerClu = kcbCluSize / sizeof(DirEnt);
// constexpr uint32_t kcDirStkSize = 256; // used in dir operations

struct DirCluster {
    DirEnt aEnts[kcePerClu];
};

struct ByteCluster {
    uint8_t aData[kcbCluSize];
};

enum class MetaResult {
    kSuccess,   // succeed to fill the meta cluster
    kTooLarge,  // the size is too large
    kTooSmall,  // the size is too small
    kPartial,   // the size is not dividable by 4 KiB
};

MetaResult FillMeta(MetaCluster *pcMeta, size_t cbSize);
void CheckPageSize();

}

#endif
