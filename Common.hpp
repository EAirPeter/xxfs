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

// nod => inode
// ino => inode number
// blk => block
// bno => block number

constexpr size_t kcbMaxSize = 1ULL << 44; // 16 TiB
constexpr size_t kcbMinSize = 6ULL << 12; // 24 KiB

constexpr uint32_t kSign = 0x0000000053465858; // "XXFS"
constexpr uint64_t kMagic0 = 0xfc6f4dfb3784ee9c;
constexpr uint64_t kMagic1 = 0xbf602ab60041f70c;
constexpr uint64_t kMagic2 = 0x612fcf459c80cfa2;

constexpr uint32_t kcBlkBit = 12;
constexpr uint32_t kcbBlkSize = 4096;

// Res: [SuperBlock] [BlockBitmapBitmap] [InodeBitmapBitmap]
// Dyn: [BlockBitmap] [InodeBitmap] [FileBlocks]

struct SuperBlock {
    // specified at creation
    uint64_t uSign;
    uint64_t cbSize;
    uint32_t bnoBnoBmp;
    uint32_t cBlock;
    uint32_t cbBnoBmpSize;
    uint32_t cBnoBmpBlk;
    uint32_t bnoBnoBmpBmp;
    uint32_t cbBnoBmpBmpSize;
    uint32_t cBnoBmpBmpBlk;
    uint32_t bnoInoBmp;
    uint32_t cInode;
    uint32_t cbInoBmpSize;
    uint32_t cInoBmpBlk;
    uint32_t bnoInoBmpBmp;
    uint32_t cbInoBmpBmpSize;
    uint32_t cInoBmpBmpBlk;
    uint32_t bnoNod;
    uint32_t cNodBlk;
    // updated dynamically
    uint32_t cUsedBlk;
    uint32_t cUsedNod;
    uint8_t aZero[4008];
};

constexpr size_t kcbSupBlkStatic = offsetof(SuperBlock, cUsedBlk);

constexpr uint32_t kcQwdPerBlk = kcbBlkSize / sizeof(uint64_t);

struct BitmapBlock {
    uint64_t aBmp[kcQwdPerBlk];
};

constexpr uint32_t kcBnoPerBlk = kcbBlkSize / sizeof(uint32_t);

struct IndexBlock {
    uint32_t aBnos[kcBnoPerBlk];
};

constexpr uint32_t kcIdx0Blk = 8;
constexpr uint32_t kcIdx1Blk = kcBnoPerBlk;
constexpr uint32_t kcIdx2Blk = kcBnoPerBlk * kcIdx1Blk;
constexpr uint32_t kcOffIdx1Blk = kcIdx0Blk;
constexpr uint32_t kcOffIdx2Blk = kcOffIdx1Blk + kcIdx1Blk;
constexpr uint32_t kcOffIdx3Blk = kcOffIdx2Blk + kcIdx2Blk;

struct Inode {
    uint64_t cbSize;
    uint32_t cBlock;
    uint32_t uMode;
    uint32_t cLink;
    uint32_t bnoIdx0[kcIdx0Blk];
    uint32_t bnoIdx1;
    uint32_t bnoIdx2;
    uint32_t bnoIdx3;

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

constexpr uint32_t kcNodPerBlk = kcbBlkSize / sizeof(Inode);

struct InodeBlock {
    Inode aNods[kcNodPerBlk];
};

struct DirEnt{
    // inode number to the entry if not root
    // number of the first available entry if root
    // available entries are linked using inoFile instead of enoNext
    uint32_t inoFile;
    uint32_t enoNext;
    uint32_t enoChild;
    uint16_t uMode; // enough to hold perm and type
    uint8_t byKey;
    bool bExist;
};

constexpr uint32_t kcEntPerBlk = kcbBlkSize / sizeof(DirEnt);
constexpr uint32_t kcEntPerStack = 255; // used in dir operations

struct DirBlock {
    DirEnt aEnts[kcEntPerBlk];
};

struct FileBlock {
    uint8_t aData[kcbBlkSize];
};

enum class SuperBlockResult {
    kSuccess,   // succeed to fill the block
    kTooLarge,  // the size is too large
    kTooSmall,  // the size is too small
    kPartial,   // the size is not dividable by 4 KiB
};

SuperBlockResult FillSuperBlock(SuperBlock *pSuperBlk, size_t cbSize);
void CheckPageSize();

#endif
