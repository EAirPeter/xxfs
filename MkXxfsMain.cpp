#include "Common.hpp"

#include "Raii.hpp"

uint32_t WriteBlock(int fd, uint32_t bno, uint32_t cBlk, int nVal) {
    for (uint32_t i = 0; i < cBlk; ++i) {
        auto rpBlk = MapAt<BitmapBlock>(fd, bno + i);
        if (!rpBlk)
            return i;
        memset(rpBlk->aBmp, nVal, kcbBlkSize);
    }
    return 0;
}

uint32_t WriteBitmap(int fd, uint32_t bnoBmp, uint32_t cBit, int nVal) {
    auto idxBitInQwd = cBit % 64;
    auto idxQwd = cBit / 64;
    auto idxQwdInBlk = idxQwd % kcQwdPerBlk;
    auto idxBlk = idxQwd / kcQwdPerBlk;
    for (uint32_t i = 0; i < idxBlk; ++i) {
        auto rpBlk = MapAt<BitmapBlock>(fd, bnoBmp + i);
        if (!rpBlk)
            return i;
        memset(rpBlk->aBmp, nVal, kcbBlkSize);
    }
    auto rpBlk = MapAt<BitmapBlock>(fd, bnoBmp + idxBlk);
    if (!rpBlk)
        return idxBlk;
    memset(rpBlk->aBmp, nVal, sizeof(uint64_t) * idxQwdInBlk);
    auto uMask = (uint64_t {1} << idxBitInQwd) - 1;
    if (nVal)
        rpBlk->aBmp[idxQwdInBlk] |= uMask;
    else
        rpBlk->aBmp[idxQwdInBlk] &= ~uMask;
    return 0;
}

static uint8_t f_blkAllOne[kcbBlkSize];

int BitmapAlloc(int fd, uint32_t bnoBmp, uint32_t &idx) {
    auto idxBitInQwd = idx % 64;
    auto idxQwd = idx / 64;
    auto idxQwdInBlk = idxQwd % kcQwdPerBlk;
    idx = idxQwd / kcQwdPerBlk;
    auto rpBlk = MapAt<BitmapBlock>(fd, bnoBmp + idx);
    if (!rpBlk)
        return -1;
    rpBlk->aBmp[idxQwdInBlk] |= uint64_t {1} << idxBitInQwd;
    if (memcmp(f_blkAllOne, rpBlk.Get(), kcbBlkSize))
        return 0;
    return 1;
}

int main(int ncArg, char *ppszArgs[]) {
    if (ncArg != 2) {
        fprintf(stderr, "Incorrect argument.\n");
        printf("\nUsage: %s path\n", ppszArgs[0]);
        return -1;
    }
    CheckPageSize();
    memset(f_blkAllOne, 0xff, kcbBlkSize);
    auto fd = open(ppszArgs[1], O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "Failed to open the file.\n");
        return -1;
    }
    RAII_GUARD_INVALID(fd, &close, -1);
    struct stat vStat;
    if (fstat(fd, &vStat)) {
        fprintf(stderr, "Failed to get the file\'s size.\n");
        return -1;
    }
    auto rpSuperBlk = MapAt<SuperBlock>(fd, 0);
    if (!rpSuperBlk) {
        fprintf(stderr, "Failed to map super block.\n");
        return -1;
    }
    auto vRes = FillSuperBlock(rpSuperBlk.Get(), (size_t) vStat.st_size);
    switch (vRes) {
    case SuperBlockResult::kTooLarge:
        fprintf(stderr, "The file is too large (%zu B over %zu B).\n", (size_t) vStat.st_size, kcbMaxSize);
        return -1;
    case SuperBlockResult::kTooSmall:
        fprintf(stderr, "The file is too small (%zu B less than %zu B).\n", (size_t) vStat.st_size, kcbMinSize);
        return -1;
    case SuperBlockResult::kPartial:
        fprintf(stderr, "The file\'s size is not dividable by 4KiB block size.\n");
        return -1;
    default:
        break;
    }
    uint32_t uRes;
    if ((uRes = WriteBlock(fd, rpSuperBlk->bnoBnoBmpBmp, rpSuperBlk->cBnoBmpBmpBlk, 0xff))) {
        fprintf(stderr, "Failed to map block bitmap bitmap block %" PRIu32 ".\n", uRes);
        return -1;
    }
    if ((uRes = WriteBitmap(fd, rpSuperBlk->bnoBnoBmpBmp, rpSuperBlk->cBnoBmpBlk, 0x00))) {
        fprintf(stderr, "Failed to map block bitmap bitmap block %" PRIu32 ".\n", uRes);
        return -1;
    }
    if ((uRes = WriteBlock(fd, rpSuperBlk->bnoInoBmpBmp, rpSuperBlk->cInoBmpBmpBlk, 0xff))) {
        fprintf(stderr, "Failed to map inode bitmap bitmap block %" PRIu32 ".\n", uRes);
        return -1;
    }
    if ((uRes = WriteBitmap(fd, rpSuperBlk->bnoInoBmpBmp, rpSuperBlk->cInoBmpBlk, 0x00))) {
        fprintf(stderr, "Failed to map inode bitmap bitmap block %" PRIu32 ".\n", uRes);
        return -1;
    }
    if ((uRes = WriteBlock(fd, rpSuperBlk->bnoBnoBmp, rpSuperBlk->cBnoBmpBlk, 0xff))) {
        fprintf(stderr, "Failed to map block bitmap block %" PRIu32 ".\n", uRes);
        return -1;
    }
    if ((uRes = WriteBitmap(fd, rpSuperBlk->bnoBnoBmp, rpSuperBlk->cBlock, 0x00))) {
        fprintf(stderr, "Failed to map block bitmap block %" PRIu32 ".\n", uRes);
        return -1;
    }
    if ((uRes = WriteBlock(fd, rpSuperBlk->bnoInoBmp, rpSuperBlk->cInoBmpBlk, 0xff))) {
        fprintf(stderr, "Failed to map inode bitmap block %" PRIu32 ".\n", uRes);
        return -1;
    }
    if ((uRes = WriteBitmap(fd, rpSuperBlk->bnoInoBmp, rpSuperBlk->cInode, 0x00))) {
        fprintf(stderr, "Failed to map inode bitmap block %" PRIu32 ".\n", uRes);
        return -1;
    }
    for (uint32_t i = 0; i < rpSuperBlk->cUsedBlk; ++i) {
        uint32_t idx = i;
        auto nRes = BitmapAlloc(fd, rpSuperBlk->bnoBnoBmp, idx);
        if (nRes == -1) {
            fprintf(stderr, "Failed to map block bitmap block %" PRIu32 ".\n", idx);
            return -1;
        }
        if (nRes == 1 && BitmapAlloc(fd, rpSuperBlk->bnoBnoBmpBmp, idx) == -1) {
            fprintf(stderr, "Failed to map block bitmap bitmap block %" PRIu32 ".\n", idx);
            return -1;
        }
    }
    for (uint32_t i = 0; i < rpSuperBlk->cUsedNod; ++i) {
        uint32_t idx = i;
        auto nRes = BitmapAlloc(fd, rpSuperBlk->bnoInoBmp, idx);
        if (nRes == -1) {
            fprintf(stderr, "Failed to map inode bitmap block %" PRIu32 ".\n", idx);
            return -1;
        }
        if (nRes == 1 && BitmapAlloc(fd, rpSuperBlk->bnoInoBmpBmp, idx) == -1) {
            fprintf(stderr, "Failed to map inode bitmap bitmap block %" PRIu32 ".\n", idx);
            return -1;
        }
        idx = i / kcNodPerBlk;
        auto rpBlk = MapAt<InodeBlock>(fd, rpSuperBlk->bnoNod + idx);
        if (!rpBlk) {
            fprintf(stderr, "Failed to map inode block %" PRIu32 ".\n", idx);
            return -1;
        }
        idx = i % kcNodPerBlk;
        memset(&rpBlk->aNods[i], 0x00, sizeof(Inode));
        rpBlk->aNods[i].uMode = 0777 | S_IFDIR;
    }
    printf("    %" PRIu64 " bytes in total\n", rpSuperBlk->cbSize);
    printf("    %" PRIu64 " bytes allocated \n", (uint64_t) kcbBlkSize * rpSuperBlk->cUsedBlk);
    printf("    %" PRIu32 " blocks in total\n", rpSuperBlk->cBlock);
    printf("    %" PRIu32 " blocks allocated\n", rpSuperBlk->cUsedBlk);
    printf("    %" PRIu32 " inodes in total\n", rpSuperBlk->cInode);
    printf("    %" PRIu32 " inodes allocated\n", rpSuperBlk->cUsedNod);
    printf("Done.\n");
    return 0;
}
