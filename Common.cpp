#include "Common.hpp"

template<class tObj>
constexpr static bool IsBlock = std::is_pod_v<tObj> && sizeof(tObj) == kcbBlkSize;

static_assert(IsBlock<SuperBlock>);
static_assert(IsBlock<BitmapBlock>);
static_assert(IsBlock<IndexBlock>);
static_assert(IsBlock<InodeBlock>);
static_assert(IsBlock<DirBlock>);
static_assert(IsBlock<FileBlock>);

static_assert(kcbBlkSize >= PATH_MAX);
static_assert(kcEntPerStack >= NAME_MAX);
static_assert(sizeof(uint64_t) >= sizeof(uintptr_t));

void FatalException::ShowWhat(FILE *pFile) const noexcept {
    fprintf(
        pFile, "At %s:%ld [%s] [errno %d]: %s.\n",
        x_pszFile, x_nLine, x_pszFunc, x_nErrno, x_pszWhat
    );
}

SuperBlockResult FillSuperBlock(SuperBlock *pSuperBlk, size_t cbSize) {
    if (cbSize > kcbMaxSize)
        return SuperBlockResult::kTooLarge;
    if (cbSize < kcbMinSize)
        return SuperBlockResult::kTooSmall;
    if (cbSize % kcbBlkSize)
        return SuperBlockResult::kPartial;
    auto cBlock = (uint32_t) (cbSize / kcbBlkSize);
    auto cbBnoBmpSize = (cBlock + 7) / 8;
    auto cBnoBmpBlk = (cbBnoBmpSize + kcbBlkSize - 1) / kcbBlkSize;
    auto cbBnoBmpBmpSize = (cBnoBmpBlk + 7) / 8;
    auto cBnoBmpBmpBlk = (cbBnoBmpBmpSize + kcbBlkSize - 1) / kcbBlkSize;
    auto cInodeUpper = cBlock - cbBnoBmpSize;
    uint32_t cInode = 0;
    while (cInode + 1 < cInodeUpper) {
        auto cInodeMid = (cInode + cInodeUpper) / 2;
        auto cbInoBmpSize =  (cInodeMid + 7) / 8;
        auto cInoBmpBlk = (cbInoBmpSize + kcbBlkSize - 1) / kcbBlkSize;
        auto cbInoBmpBmpSize = (cInoBmpBlk + 7) / 8;
        auto cInoBmpBmpBlk = (cbInoBmpBmpSize + kcbBlkSize - 1) / kcbBlkSize;
        auto cNodBlk =  (cInodeMid + kcNodPerBlk - 1) / kcNodPerBlk;
        auto cUsedBlk = 1 + cBnoBmpBmpBlk + cInoBmpBmpBlk + cBnoBmpBlk + cInoBmpBlk + cNodBlk;
        if (cUsedBlk + cInodeMid - 1 <= cBlock)
            cInode = cInodeMid;
        else
            cInodeUpper = cInodeMid;
    }
    auto cbInoBmpSize = (cInode + 7) / 8;
    auto cInoBmpBlk = (cbInoBmpSize + kcbBlkSize - 1) / kcbBlkSize;
    auto cbInoBmpBmpSize = (cInoBmpBlk + 7) / 8;
    auto cInoBmpBmpBlk = (cbInoBmpBmpSize + kcbBlkSize - 1) / kcbBlkSize;
    auto cNodBlk =  (cInode + kcNodPerBlk - 1) / kcNodPerBlk;
    auto bnoBnoBmpBmp = 1;
    auto bnoInoBmpBmp = cBnoBmpBmpBlk + bnoBnoBmpBmp;
    auto bnoBnoBmp = cInoBmpBmpBlk + bnoInoBmpBmp;
    auto bnoInoBmp = cBnoBmpBlk + bnoBnoBmp;
    auto bnoNod = cInoBmpBlk + bnoInoBmp;
    auto cUsedBlk = 1 + cBnoBmpBmpBlk + cInoBmpBmpBlk + cBnoBmpBlk + cInoBmpBlk + cNodBlk;
    auto cUsedNod = 1;
    pSuperBlk->uSign = kSign;
    pSuperBlk->cbSize = (uint64_t) cbSize;
    pSuperBlk->bnoBnoBmp = bnoBnoBmp;
    pSuperBlk->cBlock = cBlock;
    pSuperBlk->cbBnoBmpSize = cbBnoBmpSize;
    pSuperBlk->cBnoBmpBlk = cBnoBmpBlk;
    pSuperBlk->bnoBnoBmpBmp = bnoBnoBmpBmp;
    pSuperBlk->cbBnoBmpBmpSize = cbBnoBmpBmpSize;
    pSuperBlk->cBnoBmpBmpBlk = cBnoBmpBmpBlk;
    pSuperBlk->bnoInoBmp = bnoInoBmp;
    pSuperBlk->cInode = cInode;
    pSuperBlk->cbInoBmpSize = cbInoBmpSize;
    pSuperBlk->cInoBmpBlk = cInoBmpBlk;
    pSuperBlk->bnoInoBmpBmp = bnoInoBmpBmp;
    pSuperBlk->cbInoBmpBmpSize = cbInoBmpBmpSize;
    pSuperBlk->cInoBmpBmpBlk = cInoBmpBmpBlk;
    pSuperBlk->bnoNod = bnoNod;
    pSuperBlk->cNodBlk = cNodBlk;
    pSuperBlk->cUsedBlk = cUsedBlk;
    pSuperBlk->cUsedNod = cUsedNod;
    memset(pSuperBlk->aZero, 0, sizeof(pSuperBlk->aZero));
    return SuperBlockResult::kSuccess;
}

void CheckPageSize() {
    auto ncbPageSize = sysconf(_SC_PAGESIZE);
    if (ncbPageSize != (long) kcbBlkSize) {
        fprintf(
            stderr,
            "Warning: Actual page size (%ld B) does not equal to predefined page size (%ld B)",
            ncbPageSize,
            (long) kcbBlkSize
        );
    }
}
